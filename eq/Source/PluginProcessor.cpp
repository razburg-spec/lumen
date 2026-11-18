#include "PluginProcessor.h"
#include "PluginEditor.h"

PrismProcessor::PrismProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    mode = apvts.getRawParameterValue (ParamID::mode);
    output = apvts.getRawParameterValue (ParamID::output);
    drive = apvts.getRawParameterValue (ParamID::drive);
    linear = apvts.getRawParameterValue (ParamID::linear);
    for (int b = 0; b < Band::numBands; ++b)
    {
        auto& r = bandRaw[(size_t) b];
        r.on   = apvts.getRawParameterValue (Band::id (b, ParamID::on));
        r.freq = apvts.getRawParameterValue (Band::id (b, ParamID::freq));
        r.gain = apvts.getRawParameterValue (Band::id (b, ParamID::gain));
        r.q    = apvts.getRawParameterValue (Band::id (b, ParamID::q));
        if (Band::isEdge (b))
        {
            r.type  = apvts.getRawParameterValue (Band::id (b, ParamID::type));
            r.slope = apvts.getRawParameterValue (Band::id (b, ParamID::slope));
        }
    }
}

PrismProcessor::~PrismProcessor()
{
    cancelPendingUpdate();
    irBuilder.stopThread (2000);
}

void PrismProcessor::IrBuilder::run()
{
    PrismEngine::Params last;
    double lastRate = 0.0;
    bool haveLast = false;
    while (! threadShouldExit())
    {
        wait (25);
        if (! owner.isLinearPhase())
        {
            haveLast = false;   // rebuild as soon as Linear Phase comes back on
            continue;
        }
        const auto p = owner.currentParams();
        const double rate = owner.getEngineSampleRate();
        auto comparable = p;
        comparable.outputDb = 0.0f;   // output and drive are not part of the filter
        comparable.drive = 0.0f;
        if (haveLast && ! dirty.load() && juce::exactlyEqual (rate, lastRate) && comparable == last)
            continue;
        dirty.store (false);
        owner.engine.loadLinearIr (PrismEngine::buildLinearIr (p, rate));
        last = comparable;
        lastRate = rate;
        haveLast = true;
    }
}

void PrismProcessor::updateLatency()
{
    const bool lin = isLinearPhase();
    reportedLinear.store (lin);
    setLatencySamples (lin ? PrismEngine::linearLatency (getEngineSampleRate()) : 0);
}

void PrismProcessor::handleAsyncUpdate()
{
    updateLatency();
}

bool PrismProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PrismProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getMainBusNumOutputChannels());
    sampleRateForDisplay.store (sampleRate);
    if (isLinearPhase())
        engine.loadLinearIr (PrismEngine::buildLinearIr (currentParams(), sampleRate));
    updateLatency();
    irBuilder.invalidate();
    if (! irBuilder.isThreadRunning())
        irBuilder.startThread (juce::Thread::Priority::low);
}

PrismEngine::Params PrismProcessor::currentParams() const
{
    PrismEngine::Params p;
    p.mode = juce::roundToInt (mode->load());
    p.outputDb = output->load();
    p.drive = drive->load() / 100.0f;
    p.linear = linear->load() > 0.5f;
    for (int b = 0; b < Band::numBands; ++b)
    {
        const auto& r = bandRaw[(size_t) b];
        auto& band = p.bands[(size_t) b];
        band.on = r.on->load() > 0.5f;
        band.freq = r.freq->load();
        band.gainDb = r.gain->load();
        band.q = r.q->load();
        if (r.type != nullptr)
        {
            band.type = juce::roundToInt (r.type->load());
            band.slope = juce::roundToInt (r.slope->load());
        }
    }
    return p;
}

void PrismProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto params = currentParams();
    if (params.linear != reportedLinear.load())
        triggerAsyncUpdate();
    engine.process (buffer, params);
}

void PrismProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PrismProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PrismProcessor::createEditor()
{
    return new PrismEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismProcessor();
}
