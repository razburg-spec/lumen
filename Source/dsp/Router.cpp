#include "Router.h"
#include "../Parameters.h"

namespace
{
    // Note lengths in quarter notes, matching the Delay Division choices.
    constexpr float divisionBeats[] = { 1.0f / 16, 1.0f / 8, 1.0f / 6, 1.0f / 4, 3.0f / 8, 1.0f / 3, 1.0f / 2, 3.0f / 4,
                                        2.0f / 3, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f };

    float dbToGain (float db) { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }
}

void Router::attach (juce::AudioProcessorValueTreeState& apvts)
{
    using namespace ParamID;
    auto raw = [&apvts] (const char* id) { return Raw { apvts.getRawParameterValue (id) }; };

    crossFeed = raw (ParamID::crossFeed);
    dTime = raw (delayTimeMs);   dSync = raw (delaySync);     dDiv = raw (delayDivision);
    dFb = raw (delayFeedback);   dMode = raw (delayMode);     dChar = raw (delayChar);
    dHc = raw (delayHighCut);    dLc = raw (delayLowCut);     dDrive = raw (delayDrive);
    dModRate = raw (delayModRate); dModDepth = raw (delayModDepth); dDuck = raw (delayDuck);
    dFreeze = raw (delayFreeze); dLevel = raw (delayLevel);

    sPitchA = raw (shimPitchA);  sPitchB = raw (shimPitchB);  sGrain = raw (shimGrainMs);
    sReverse = raw (shimReverse); sDensity = raw (shimDensity); sSpray = raw (shimSpray);
    sFb = raw (shimFeedback);    sLevel = raw (shimLevel);

    rMode = raw (revMode);       rColor = raw (revColor);     rPre = raw (revPreDelay);
    rDecay = raw (revDecay);     rSize = raw (revSize);       rDiff = raw (revDiffusion);
    rModRate = raw (revModRate); rModDepth = raw (revModDepth); rHc = raw (revHighCut);
    rLc = raw (revLowCut);       rWidth = raw (revWidth);     rLevel = raw (revLevel);
}

float Router::resolveDelayTimeMs() const
{
    if (dSync.get (1.0f) > 0.5f)
    {
        const int idx = juce::jlimit (0, (int) std::size (divisionBeats) - 1, (int) dDiv.get (7.0f));
        return (float) (divisionBeats[idx] * 60000.0 / tempoBpm);
    }
    return dTime.get (375.0f);
}

void Router::pullParameters()
{
    DelayEngine::Params d;
    d.timeMs = resolveDelayTimeMs();
    d.feedback = dFb.get (35.0f) / 100.0f;
    d.mode = (int) dMode.get (0.0f);
    d.character = (int) dChar.get (1.0f);
    d.highCutHz = dHc.get (8000.0f);
    d.lowCutHz = dLc.get (80.0f);
    d.drive = dDrive.get (10.0f) / 100.0f;
    d.modRateHz = dModRate.get (0.6f);
    d.modDepth = dModDepth.get (10.0f) / 100.0f;
    d.duck = dDuck.get (0.0f) / 100.0f;
    d.freeze = dFreeze.get (0.0f) > 0.5f;
    d.levelGain = dbToGain (dLevel.get (0.0f));
    delay.setParams (d);

    ShimmerEngine::Params s;
    s.pitchA = sPitchA.get (12.0f);
    s.pitchB = sPitchB.get (7.0f);
    s.grainMs = sGrain.get (120.0f);
    s.reverse = (int) sReverse.get (0.0f);
    s.density = sDensity.get (50.0f) / 100.0f;
    s.spray = sSpray.get (10.0f) / 100.0f;
    s.feedback = sFb.get (30.0f) / 100.0f;
    s.levelGain = dbToGain (sLevel.get (-6.0f));
    shimmer.setParams (s);

    ReverbEngine::Params r;
    r.mode = (int) rMode.get (0.0f);
    r.era = (int) rColor.get (4.0f);
    r.preDelayMs = rPre.get (20.0f);
    r.decaySeconds = rDecay.get (2.5f);
    r.size = rSize.get (60.0f) / 100.0f;
    r.diffusion = rDiff.get (75.0f) / 100.0f;
    r.modRateHz = rModRate.get (0.5f);
    r.modDepth = rModDepth.get (25.0f) / 100.0f;
    r.highCutHz = rHc.get (9000.0f);
    r.lowCutHz = rLc.get (100.0f);
    r.width = rWidth.get (100.0f) / 100.0f;
    r.levelGain = dbToGain (rLevel.get (0.0f));
    reverb.setParams (r);

    crossFeedSm.setTargetValue (crossFeed.get (0.0f) / 100.0f * 0.4f);
}

void Router::prepare (const juce::dsp::ProcessSpec& spec)
{
    numChannels = (int) spec.numChannels;
    pullParameters();
    delay.prepare (spec);
    shimmer.prepare (spec);
    reverb.prepare (spec);
    for (auto* b : { &dry, &stageIn, &wetSum, &feedBlock })
        b->setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
    feedBlock.clear();
    crossFeedSm.reset (spec.sampleRate, 0.05);
    crossFeedSm.setCurrentAndTargetValue (crossFeed.get (0.0f) / 100.0f * 0.4f);
    limiterGain = 1.0f;
    limiterRelease = 1.0f - std::exp (-1.0f / (0.15f * (float) spec.sampleRate));
}

void Router::reset()
{
    delay.reset();
    shimmer.reset();
    reverb.reset();
    feedBlock.clear();
}

void Router::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    pullParameters();

    const int numCh = juce::jmin (buffer.getNumChannels(), dry.getNumChannels());
    const int n = buffer.getNumSamples();
    if (n > dry.getNumSamples() || numCh == 0)
        return; // host broke the block-size promise; leave audio untouched

    auto view = [numCh, n] (juce::AudioBuffer<float>& b)
    {
        return juce::AudioBuffer<float> (b.getArrayOfWritePointers(), numCh, n);
    };
    auto dryV = view (dry), inV = view (stageIn), wetV = view (wetSum), fbV = view (feedBlock);

    for (int ch = 0; ch < numCh; ++ch)
        dryV.copyFrom (ch, 0, buffer, ch, 0, n);
    wetV.clear();

    // Cross-feed from the previous block (a one-block delay keeps the loop stable and cheap).
    const float cf0 = crossFeedSm.getCurrentValue();
    crossFeedSm.skip (n);
    const float cf1 = crossFeedSm.getCurrentValue();

    auto runStage = [&] (auto& engine, bool cascade, bool addFeed)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            inV.copyFrom (ch, 0, dryV, ch, 0, n);
            if (cascade) inV.addFrom (ch, 0, wetV, ch, 0, n);
            if (addFeed) inV.addFromWithRamp (ch, 0, fbV.getReadPointer (ch), n, cf0, cf1);
        }
        engine.process (inV);
        for (int ch = 0; ch < numCh; ++ch)
            wetV.addFrom (ch, 0, inV, ch, 0, n);
    };

    auto captureFeed = [&]
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* f = fbV.getWritePointer (ch);
            const auto* src = inV.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                f[i] = lumen::softClip (src[i]);   // bounded send keeps the loop stable
        }
    };

    const bool useFeed = cf0 > 0.0f || cf1 > 0.0f;
    bool fed = false;

    switch (s.mode)
    {
        case Mode::Serial:
        case Mode::Parallel:
        {
            const bool cascade = s.mode == Mode::Serial;
            if (s.delayOn)   runStage (delay, cascade, useFeed);
            if (s.shimmerOn) runStage (shimmer, cascade, false);
            if (s.reverbOn)  { runStage (reverb, cascade, false); captureFeed(); fed = true; }
            break;
        }
        case Mode::ReverbThenDelay:
        {
            if (s.reverbOn)  runStage (reverb, true, useFeed);
            if (s.shimmerOn) runStage (shimmer, true, false);
            if (s.delayOn)   { runStage (delay, true, false); captureFeed(); fed = true; }
            break;
        }
    }
    if (! fed) fbV.clear();

    // Safety limiter on the wet bus: instant attack, 150 ms release, ceiling 0 dBFS.
    // Normal settings never reach it; it only catches self-oscillating extremes.
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (wetV.getSample (ch, i)));
        const float needed = peak > 1.0f ? 1.0f / peak : 1.0f;
        limiterGain = needed < limiterGain ? needed : limiterGain + limiterRelease * (needed - limiterGain);
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, wetV.getSample (ch, i) * limiterGain);
    }
}
