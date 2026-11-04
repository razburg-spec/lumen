#include "PluginProcessor.h"
#include "PluginEditor.h"

LumenProcessor::LumenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    routingParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamID::routing));
    delayOnParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamID::delayOn));
    shimOnParam  = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamID::shimOn));
    revOnParam   = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamID::revOn));
    mixParam     = apvts.getRawParameterValue (ParamID::mix);
    outputParam  = apvts.getRawParameterValue (ParamID::output);
    router.attach (apvts);
}

bool LumenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    // Mono in -> stereo out is allowed (mono-to-wide).
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void LumenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    router.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, numCh });
    dryBuffer.setSize ((int) numCh, samplesPerBlock);

    mixSmoothed.reset (sampleRate, 0.05);
    outputSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() / 100.0f);
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load(), -60.0f));

    gapPeriod = juce::roundToInt (sampleRate * 40.0);
    gapLength = juce::roundToInt (sampleRate * 1.5);
    evaluationClock = 0;
}

void LumenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Mono input feeding a stereo output: duplicate to both sides.
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    const int filled = (numIn == 1 && numOut == 2) ? 2 : numIn;
    for (int ch = filled; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    for (int ch = 0; ch < numOut; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    Router::Settings settings;
    settings.mode = static_cast<Router::Mode> (routingParam->getIndex());
    settings.delayOn = delayOnParam->get();
    settings.shimmerOn = shimOnParam->get();
    settings.reverbOn = revOnParam->get();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                router.setTempo (*bpm);
    router.process (buffer, settings);

    mixSmoothed.setTargetValue (mixParam->load() / 100.0f);
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load(), -60.0f));

    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = mixSmoothed.getNextValue();
        const float gain = outputSmoothed.getNextValue();
        for (int ch = 0; ch < numOut; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            out[i] = (dry[i] * (1.0f - wet) + out[i] * wet) * gain;
        }
    }

    if (licence->grant() != expectedGrant)
        applyEvaluationGaps (buffer, numSamples);
}

void LumenProcessor::applyEvaluationGaps (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (gapPeriod <= 0)
        return;

    // The plugin falls back to the dry signal (silence on a 100% wet aux) for
    // gapLength samples out of every gapPeriod, starting 20 s in.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto phase = (evaluationClock + i) % gapPeriod;
        if (phase >= gapPeriod / 2 && phase < gapPeriod / 2 + gapLength)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, dryBuffer.getSample (ch, i) * (1.0f - mixParam->load() / 100.0f));
    }
    evaluationClock += numSamples;
}

void LumenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LumenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* LumenProcessor::createEditor()
{
    return new LumenEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumenProcessor();
}
