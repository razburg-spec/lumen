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
    dryBuffer.setSize (juce::jmax (1, getMainBusNumOutputChannels()), samplesPerBlock);
    gapPeriod = juce::roundToInt (sampleRate * 40.0);
    gapLength = juce::roundToInt (sampleRate * 1.5);
    evaluationClock = 0;
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

    const int numSamples = buffer.getNumSamples();
    const bool unlicensed = licence->grant() != expectedGrant;
    if (unlicensed)
    {
        dryBuffer.setSize (buffer.getNumChannels(), numSamples, false, false, true);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    const auto params = currentParams();
    if (params.linear != reportedLinear.load())
        triggerAsyncUpdate();
    engine.process (buffer, params);

    if (unlicensed)
        applyEvaluationGaps (buffer, numSamples);
}

const std::vector<rba::state::Migration>& PrismProcessor::stateMigrations()
{
    // One step per stateVersion bump, oldest first. Never edit a released step.
    //   { 2, [] (juce::ValueTree& s) { rba::state::renameParam (s, "old", "new"); } },
    static const std::vector<rba::state::Migration> steps {};
    return steps;
}

void PrismProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    rba::state::save (apvts, stateVersion, destData);
}

void PrismProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Sessions from every older version (including the 0.x test builds) open unchanged.
    rba::state::load (apvts, data, sizeInBytes, stateVersion, stateMigrations());
}

void PrismProcessor::applyEvaluationGaps (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (gapPeriod <= 0)
        return;

    // The track passes unprocessed for gapLength samples out of every gapPeriod, starting 20 s in.
    // In Linear Phase the dry signal is behind by the latency; the gap is short enough not to matter.
    const int numCh = juce::jmin (buffer.getNumChannels(), dryBuffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i)
    {
        const auto phase = (evaluationClock + i) % gapPeriod;
        if (phase >= gapPeriod / 2 && phase < gapPeriod / 2 + gapLength)
            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, i, dryBuffer.getSample (ch, i));
    }
    evaluationClock += numSamples;
}

juce::AudioProcessorEditor* PrismProcessor::createEditor()
{
    return new PrismEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismProcessor();
}
