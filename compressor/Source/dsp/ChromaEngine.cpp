#include "ChromaEngine.h"

namespace
{
    constexpr float softClipKnee = 0.6f;      // about -4.4 dBFS: below this Soft Clip does nothing
    constexpr float softClipCeiling = 0.98f;  // about -0.2 dBFS
    constexpr float freqKeyBoost = 2.0f;      // +6 dB: a narrow band carries less level than the full signal
    constexpr float freqKeyQ = 2.0f;
    constexpr float maxGainReductionDb = 60.0f;

    float timeCoeff (float ms, double fs) { return std::exp (-1.0f / (juce::jmax (0.001f, ms) * 0.001f * (float) fs)); }
    float dbToGain (float db) { return std::exp (db * 0.11512925465f); }   // exact 1 at 0 dB

    float softClipSample (float x)
    {
        const float a = std::abs (x);
        if (a <= softClipKnee)
            return x;
        const float span = softClipCeiling - softClipKnee;
        return std::copysign (softClipKnee + span * std::tanh ((a - softClipKnee) / span), x);
    }
}

void ChromaEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    maxBlock = juce::jmax (16, maxBlockSize);

    for (auto* b : { &dry, &wet, &detector, &residue, &shifted })
        b->setSize (2, maxBlock, false, true, true);

    for (auto* os : { &satOversampler, &dryOversampler, &clipOversampler })
    {
        *os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, 1,
                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        (*os)->initProcessing ((size_t) maxBlock);
    }

    inputGain.reset (sampleRate, 0.03);
    outputGain.reset (sampleRate, 0.03);
    parallelMix.reset (sampleRate, 0.03);
    satAmount.reset (sampleRate, 0.05);
    satPath.reset (sampleRate, 0.02);
    clipPath.reset (sampleRate, 0.01);
    reset();
}

void ChromaEngine::reset()
{
    meanSquare = env = envSlow = optoMemory = 0.0f;
    for (auto& f : keyFilter) f.reset();
    for (auto& f : toneLow) f.reset();
    for (auto& f : toneHigh) f.reset();
    for (auto* os : { satOversampler.get(), dryOversampler.get(), clipOversampler.get() })
        if (os != nullptr) os->reset();
    satPath.setCurrentAndTargetValue (0.0f);
    clipPath.setCurrentAndTargetValue (0.0f);
    lastCharacter = -1;
    lastKeyFreq = -1.0f;
    lastDrive = 1.0f;
    firstBlock = true;
}

float ChromaEngine::computeGainReduction (float levelDb, const Character& c, float ratio) const
{
    if (ratio <= 1.0f)
        return 0.0f;

    float slope = 1.0f - 1.0f / ratio;
    if (c.overCompress && ratio > 10.0f)
        slope = 0.9f + (ratio - 10.0f) / 10.0f * 0.5f;   // up to 1.4 at 20:1: the output level starts to fall

    const float over = levelDb - c.thresholdDb;
    const float w = c.kneeDb;
    float gr;
    if (w <= 0.0f)            gr = over > 0.0f ? slope * over : 0.0f;
    else if (2.0f * over < -w) gr = 0.0f;
    else if (2.0f * over > w)  gr = slope * over;
    else                       { const float t = over + 0.5f * w; gr = slope * t * t / (2.0f * w); }
    return juce::jmin (gr, maxGainReductionDb);
}

void ChromaEngine::process (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain, const Params& p)
{
    const int total = main.getNumSamples();
    const int numCh = juce::jmin (channels, main.getNumChannels());
    if (numCh <= 0 || total <= 0)
        return;

    if (firstBlock)
    {
        inputGain.setCurrentAndTargetValue (dbToGain (p.inputDb));
        outputGain.setCurrentAndTargetValue (dbToGain (p.outputDb));
        parallelMix.setCurrentAndTargetValue (p.parallel);
        satAmount.setCurrentAndTargetValue (p.saturation);
        satPath.setCurrentAndTargetValue (p.saturation > 0.0f ? 1.0f : 0.0f);
        clipPath.setCurrentAndTargetValue (p.softClip ? 1.0f : 0.0f);
        firstBlock = false;
    }

    // Hosts may send blocks larger than announced: work through them in prepared-size chunks.
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        juce::AudioBuffer<float> chunk (main.getArrayOfWritePointers(), numCh, start, n);
        if (sidechain != nullptr && sidechain->getNumChannels() > 0 && sidechain->getNumSamples() >= start + n)
        {
            juce::AudioBuffer<float> key (const_cast<float* const*> (sidechain->getArrayOfReadPointers()),
                                          juce::jmin (2, sidechain->getNumChannels()), start, n);
            processChunk (chunk, &key, p);
        }
        else
        {
            processChunk (chunk, nullptr, p);
        }
    }
}

void ChromaEngine::processChunk (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* key, const Params& p)
{
    const int n = main.getNumSamples();
    const int numCh = main.getNumChannels();
    const int characterIndex = juce::jlimit (0, 5, p.character);
    const auto& c = characterAt (characterIndex);

    if (characterIndex != lastCharacter)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            toneLow[ch].setCutoff (c.toneLowHz, (float) fs);
            toneHigh[ch].setCutoff (c.toneHighHz, (float) fs);
        }
        lastCharacter = characterIndex;
    }

    inputGain.setTargetValue (dbToGain (p.inputDb));
    outputGain.setTargetValue (dbToGain (p.outputDb));
    parallelMix.setTargetValue (juce::jlimit (0.0f, 1.0f, p.parallel));
    satAmount.setTargetValue (juce::jlimit (0.0f, 1.0f, p.saturation));

    for (int ch = 0; ch < numCh; ++ch)
        dry.copyFrom (ch, 0, main, ch, 0, n);

    // Where the detector listens.
    const bool wantsExternal = p.keyOn && p.keySource == 0;
    const bool useExternal = wantsExternal && key != nullptr && key->getNumChannels() > 0;
    const bool freqKey = p.keyOn && p.keySource == 1;
    meters.externalKeyMissing.store (wantsExternal && ! useExternal, std::memory_order_relaxed);
    if (freqKey && std::abs (p.keyFreq - lastKeyFreq) > 0.01f)
    {
        for (auto& f : keyFilter)
            f.set (p.keyFreq, freqKeyQ, (float) fs);
        lastKeyFreq = p.keyFreq;
    }

    // Time constants for this block.
    const float aAtt = timeCoeff (juce::jmax (p.attackMs, c.minAttackMs), fs);
    const float releaseMs = p.releaseMs * c.releaseScale;
    const float aRel = timeCoeff (releaseMs, fs);
    const float aSlowAtt = timeCoeff (300.0f, fs), aSlowRel = timeCoeff (releaseMs * 4.0f, fs);
    const float rmsCoeff = c.rmsMs > 0.0f ? 1.0f - timeCoeff (c.rmsMs, fs) : 1.0f;
    const float memUp = 1.0f - timeCoeff (1500.0f, fs), memDown = 1.0f - timeCoeff (4000.0f, fs);

    const float* inL = main.getReadPointer (0);
    const float* inR = main.getReadPointer (numCh > 1 ? 1 : 0);
    const float* keyL = useExternal ? key->getReadPointer (0) : nullptr;
    const float* keyR = useExternal ? key->getReadPointer (key->getNumChannels() > 1 ? 1 : 0) : nullptr;
    float* wetL = wet.getWritePointer (0);
    float* wetR = wet.getWritePointer (1);
    float* det = detector.getWritePointer (0);

    float peakIn = 0.0f, maxGr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float ig = inputGain.getNextValue();
        const float xl = inL[i] * ig, xr = inR[i] * ig;

        float dl = xl, dr = xr;
        if (useExternal)  { dl = keyL[i] * ig; dr = keyR[i] * ig; }
        else if (freqKey) { dl = keyFilter[0].process (xl) * freqKeyBoost; dr = keyFilter[1].process (xr) * freqKeyBoost; }
        det[i] = 0.5f * (dl + dr);
        peakIn = juce::jmax (peakIn, std::abs (xl), std::abs (xr));

        // Stereo-linked level: the louder side drives both.
        const float sq = juce::jmax (dl * dl, dr * dr);
        float level;
        if (c.rmsMs > 0.0f)
        {
            meanSquare += rmsCoeff * (sq - meanSquare);
            level = std::sqrt (2.0f * meanSquare);   // a sine reads its peak level, like the peak detector
        }
        else
        {
            level = std::sqrt (sq);
        }
        const float target = computeGainReduction (juce::Decibels::gainToDecibels (level, -120.0f), c, p.ratio);

        // Ballistics in the dB domain.
        float rel = aRel;
        if (c.release == Character::Release::opto)
        {
            // The longer and harder it has been working, the slower it lets go.
            const float memTarget = juce::jlimit (0.0f, 1.0f, env / 8.0f);
            optoMemory += (memTarget > optoMemory ? memUp : memDown) * (memTarget - optoMemory);
            rel = timeCoeff (releaseMs * (1.0f + 3.0f * optoMemory), fs);
        }
        env = target > env ? aAtt * env + (1.0f - aAtt) * target
                           : rel * env + (1.0f - rel) * target;

        float gr = env;
        if (c.release == Character::Release::dual)
        {
            // Sustained squeeze builds a slow envelope that holds the release back; short peaks recover fast.
            envSlow = target > envSlow ? aSlowAtt * envSlow + (1.0f - aSlowAtt) * target
                                       : aSlowRel * envSlow + (1.0f - aSlowRel) * target;
            gr = juce::jmax (env, envSlow);
        }

        const float g = dbToGain (-gr);
        wetL[i] = xl * g;
        wetR[i] = xr * g;
        maxGr = juce::jmax (maxGr, gr);
    }

    // Saturation on the compressed signal.
    if (satAmount.isSmoothing() || satAmount.getTargetValue() > 0.0f || satPath.getCurrentValue() > 0.0f)
        saturate (wet, dry, n, c, numCh, maxGr);
    else
        satAmount.skip (n);

    // Make-up, then the parallel blend with the untouched source.
    for (int i = 0; i < n; ++i)
    {
        const float og = outputGain.getNextValue();
        const float mix = parallelMix.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            main.setSample (ch, i, dry.getSample (ch, i) * (1.0f - mix) + wet.getSample (ch, i) * og * mix);
    }

    softClipBlock (main, n, p.softClip);

    // Key Listen: hear exactly what the detector hears.
    if (p.keyOn && p.keyListen)
        for (int ch = 0; ch < numCh; ++ch)
            main.copyFrom (ch, 0, detector, 0, 0, n);

    meters.inputPeak.store (juce::jmax (peakIn, meters.inputPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    float peakOut = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        peakOut = juce::jmax (peakOut, main.getMagnitude (ch, 0, n));
    meters.outputPeak.store (juce::jmax (peakOut, meters.outputPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    meters.gainReductionDb.store (juce::jmax (maxGr, meters.gainReductionDb.load (std::memory_order_relaxed)), std::memory_order_relaxed);
}

void ChromaEngine::saturate (juce::AudioBuffer<float>& signal, juce::AudioBuffer<float>& source, int n,
                             const Character& c, int numCh, float grDb)
{
    // The saturator runs at 2x so the new harmonics don't fold back. The 2x filters shift the phase a
    // little, so the untouched source for the Parallel blend goes through an identical (unshaped) pass
    // and both stay aligned. Switching the stage in and out cross-fades between the two paths.
    const float pathStart = satPath.getCurrentValue();
    satPath.setTargetValue (satAmount.getTargetValue() > 0.0f ? 1.0f : 0.0f);
    if (pathStart <= 0.0f)
    {
        satOversampler->reset();
        dryOversampler->reset();
        for (auto& f : toneLow) f.reset();
        for (auto& f : toneHigh) f.reset();
    }

    const float amountStart = satAmount.getCurrentValue();
    satAmount.skip (n);
    const float amountEnd = satAmount.getCurrentValue();
    const float driveEnd = 1.0f + (c.drive - 1.0f) * amountEnd + c.grDrive * grDb * amountEnd;

    for (int ch = 0; ch < channels; ++ch)
    {
        residue.copyFrom (ch, 0, signal, juce::jmin (ch, numCh - 1), 0, n);
        shifted.copyFrom (ch, 0, source, juce::jmin (ch, numCh - 1), 0, n);
    }

    auto block = juce::dsp::AudioBlock<float> (residue).getSubsetChannelBlock (0, (size_t) channels).getSubBlock (0, (size_t) n);
    auto up = satOversampler->processSamplesUp (block);
    const auto upN = (int) up.getNumSamples();
    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        auto* d = up.getChannelPointer (ch);
        for (int j = 0; j < upN; ++j)
        {
            const float frac = (float) j / (float) upN;
            const float drive = lastDrive + (driveEnd - lastDrive) * frac;
            const float weight = juce::jmin (1.0f, (amountStart + (amountEnd - amountStart) * frac) * 4.0f);   // fades in over the first quarter
            const float x = d[j];
            const float harmonics = shapeSample (x * drive, c.shape, c.bias) / drive - x;
            d[j] = x + weight * toneHigh[ch].lowpass (toneLow[ch].highpass (harmonics));
        }
    }
    satOversampler->processSamplesDown (block);
    lastDrive = driveEnd;

    auto dryBlock = juce::dsp::AudioBlock<float> (shifted).getSubsetChannelBlock (0, (size_t) channels).getSubBlock (0, (size_t) n);
    dryOversampler->processSamplesUp (dryBlock);
    dryOversampler->processSamplesDown (dryBlock);

    for (int i = 0; i < n; ++i)
    {
        const float m = satPath.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int src = juce::jmin (ch, channels - 1);
            signal.setSample (ch, i, signal.getSample (ch, i) * (1.0f - m) + residue.getSample (src, i) * m);
            source.setSample (ch, i, source.getSample (ch, i) * (1.0f - m) + shifted.getSample (src, i) * m);
        }
    }
}

void ChromaEngine::softClipBlock (juce::AudioBuffer<float>& buffer, int n, bool on)
{
    const float pathStart = clipPath.getCurrentValue();
    clipPath.setTargetValue (on ? 1.0f : 0.0f);
    if (pathStart <= 0.0f && ! on)
        return;
    if (pathStart <= 0.0f)
        clipOversampler->reset();

    const int numCh = juce::jmin (buffer.getNumChannels(), channels);
    for (int ch = 0; ch < channels; ++ch)
        residue.copyFrom (ch, 0, buffer, juce::jmin (ch, numCh - 1), 0, n);

    auto block = juce::dsp::AudioBlock<float> (residue).getSubsetChannelBlock (0, (size_t) channels).getSubBlock (0, (size_t) n);
    auto up = clipOversampler->processSamplesUp (block);
    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        auto* d = up.getChannelPointer (ch);
        for (size_t j = 0; j < up.getNumSamples(); ++j)
            d[j] = softClipSample (d[j]);
    }
    clipOversampler->processSamplesDown (block);

    for (int i = 0; i < n; ++i)
    {
        const float m = clipPath.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * (1.0f - m) + juce::jlimit (-1.0f, 1.0f, residue.getSample (ch, i)) * m);
    }
}
