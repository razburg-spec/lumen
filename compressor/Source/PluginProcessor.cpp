#include "PluginProcessor.h"
#include "PluginEditor.h"

ChromaProcessor::ChromaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false))   // the host's native key input
{
    character  = raw (ParamID::character);
    input      = raw (ParamID::input);
    output     = raw (ParamID::output);
    attack     = raw (ParamID::attack);
    release    = raw (ParamID::release);
    ratio      = raw (ParamID::ratio);
    saturation = raw (ParamID::saturation);
    softClip   = raw (ParamID::softClip);
    parallel   = raw (ParamID::parallel);
    keyOn      = raw (ParamID::keyOn);
    keySource  = raw (ParamID::keySource);
    keyFreq    = raw (ParamID::keyFreq);
    keyListen  = raw (ParamID::keyListen);
}

bool ChromaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;

    // Key input: off, mono or stereo (Pro Tools offers a mono key, other hosts usually stereo).
    if (layouts.inputBuses.size() > 1)
    {
        const auto key = layouts.getChannelSet (true, 1);
        if (! key.isDisabled() && key != juce::AudioChannelSet::mono() && key != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

bool ChromaProcessor::hasSidechainInput() const
{
    if (auto* bus = getBus (true, 1))
        return bus->isEnabled() && bus->getNumberOfChannels() > 0;
    return false;
}

void ChromaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = getMainBusNumOutputChannels();
    engine.prepare (sampleRate, samplesPerBlock, numCh);
    dryBuffer.setSize (juce::jmax (1, numCh), samplesPerBlock);
    setLatencySamples (0);

    gapPeriod = juce::roundToInt (sampleRate * 40.0);
    gapLength = juce::roundToInt (sampleRate * 1.5);
    evaluationClock = 0;
}

void ChromaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    auto main = getBusBuffer (buffer, true, 0);
    for (int ch = getMainBusNumInputChannels(); ch < getMainBusNumOutputChannels(); ++ch)
        main.clear (ch, 0, numSamples);

    const bool unlicensed = licence->grant() != expectedGrant;
    if (unlicensed)
    {
        dryBuffer.setSize (main.getNumChannels(), numSamples, false, false, true);
        for (int ch = 0; ch < main.getNumChannels(); ++ch)
            dryBuffer.copyFrom (ch, 0, main, ch, 0, numSamples);
    }

    ChromaEngine::Params p;
    p.character  = juce::roundToInt (character->load());
    p.inputDb    = input->load();
    p.outputDb   = output->load();
    p.attackMs   = attack->load();
    p.releaseMs  = release->load();
    p.ratio      = ratio->load();
    p.saturation = saturation->load() / 100.0f;
    p.parallel   = parallel->load() / 100.0f;
    p.softClip   = softClip->load() > 0.5f;
    p.keyOn      = keyOn->load() > 0.5f;
    p.keySource  = juce::roundToInt (keySource->load());
    p.keyFreq    = keyFreq->load();
    p.keyListen  = keyListen->load() > 0.5f;

    if (hasSidechainInput())
    {
        auto key = getBusBuffer (buffer, true, 1);
        engine.process (main, &key, p);
    }
    else
    {
        engine.process (main, nullptr, p);
    }

    if (unlicensed)
        applyEvaluationGaps (main, numSamples);
}

void ChromaProcessor::applyEvaluationGaps (juce::AudioBuffer<float>& main, int numSamples)
{
    if (gapPeriod <= 0)
        return;

    // The track passes unprocessed for gapLength samples out of every gapPeriod, starting 20 s in.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto phase = (evaluationClock + i) % gapPeriod;
        if (phase >= gapPeriod / 2 && phase < gapPeriod / 2 + gapLength)
            for (int ch = 0; ch < main.getNumChannels(); ++ch)
                main.setSample (ch, i, dryBuffer.getSample (ch, i));
    }
    evaluationClock += numSamples;
}

void ChromaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChromaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ChromaProcessor::createEditor()
{
    return new ChromaEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChromaProcessor();
}
