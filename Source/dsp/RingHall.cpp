#include "RingHall.h"

namespace
{
    struct Voice
    {
        float lengthScale, allpassGain, diffusionScale;
        float bassRatio, midCornerHz, highRatio;
        float firstTapGain;      // how quickly the sound arrives
        float wanderMs;          // delay movement at full Mod Depth
        float chorus;            // 0 = pure random movement, 1 = pure sine movement
        float outputGain;
    };

    constexpr Voice voices[2] = {
        { 0.90f, 0.66f, 1.00f, 1.25f, 300.0f, 0.55f, 1.00f, 10.0f, 0.0f, 0.90f },
        { 1.20f, 0.52f, 0.90f, 1.60f, 340.0f, 0.45f, 0.50f,  7.0f, 0.7f, 1.40f },
    };

    constexpr float lineMs[8]     = { 71.3f, 53.9f, 83.7f, 61.1f, 97.3f, 47.9f, 89.1f, 67.3f };
    constexpr float allpassMs[8]  = { 11.3f, 23.7f, 17.9f, 31.1f, 13.7f, 27.3f, 19.1f, 37.9f };
    constexpr float innerMs[8]    = { 3.1f, 5.3f, 4.1f, 6.7f, 3.7f, 7.9f, 4.7f, 5.9f };
    constexpr float diffuserMs[4] = { 3.3f, 4.9f, 11.3f, 8.1f };

    // Where the channels listen along a section, as a fraction of its length.
    constexpr float tapFrac[3] = { 0.13f, 0.41f, 0.77f };
    constexpr float tapPolarity[8][3] = { {  1, -1,  1 }, { -1,  1,  1 }, {  1,  1, -1 }, {  1, -1, -1 },
                                          { -1,  1, -1 }, { -1, -1,  1 }, {  1, -1,  1 }, { -1,  1,  1 } };
}

void RingHall::prepare (float newSampleRate)
{
    sampleRate = newSampleRate;
    const auto ms = [this] (float m) { return (int) (m * 0.001f * sampleRate) + 64; };

    for (int ch = 0; ch < 2; ++ch)
    {
        preDelay[ch].setMaxSamples (ms (510.0f));
        for (int d = 0; d < numDiffusers; ++d)
            diffuser[ch][d].line.setMaxSamples (ms (diffuserMs[d] * 1.5f + 2.0f));
    }
    for (int i = 0; i < numSections; ++i)
    {
        lines[i].setMaxSamples (ms (97.3f * 1.2f * 1.5f + 30.0f));
        allpass[i].line.setMaxSamples (ms (37.9f * 1.2f * 1.5f + 4.0f));
        inner[i].line.setMaxSamples (ms (7.9f * 1.2f * 1.5f + 4.0f));
        drift[i].seed (0x85EBCA6Bu * (uint32_t) (i + 3));
        lfoPhase[i] = lumen::twoPi * (float) i / (float) numSections;
    }
    level.reset ((double) sampleRate, 0.05);
    widthSm.reset ((double) sampleRate, 0.05);
    reset();
}

void RingHall::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        preDelay[ch].clear();
        inHigh[ch].reset(); inLow[ch].reset();
        for (auto& d : diffuser[ch]) d.line.clear();
    }
    for (int i = 0; i < numSections; ++i)
    {
        lines[i].clear();
        allpass[i].line.clear();
        inner[i].line.clear();
        decay[i].reset();
        lineOut[i] = 0.0f;
    }
    started = false;
}

void RingHall::process (float* left, float* right, int numSamples, const ReverbParams& p, int variant)
{
    const auto& voice = voices[juce::jlimit (0, 1, variant)];

    const float scale = (0.5f + 0.8333f * p.size) * voice.lengthScale;   // Size 60% = 1 x the voice's length
    const float t60 = juce::jlimit (0.2f, 20.0f, p.decaySeconds);

    float targetLen[numSections], targetAp[numSections], targetInner[numSections];
    for (int i = 0; i < numSections; ++i)
    {
        targetLen[i] = lineMs[i] * scale * 0.001f * sampleRate;
        targetAp[i] = std::round (allpassMs[i] * scale * 0.001f * sampleRate);
        targetInner[i] = std::round (innerMs[i] * scale * 0.001f * sampleRate);
    }

    if (! started)
    {
        for (int i = 0; i < numSections; ++i)
        {
            smoothedLen[i] = targetLen[i];
            smoothedAp[i] = targetAp[i];
            smoothedInner[i] = targetInner[i];
        }
        level.setCurrentAndTargetValue (p.levelGain);
        widthSm.setCurrentAndTargetValue (p.width);
        started = true;
    }

    const float highCorner = juce::jlimit (voice.midCornerHz * 2.0f, 18000.0f, p.highCutHz);
    for (int i = 0; i < numSections; ++i)
        decay[i].set (targetLen[i] + targetAp[i] + targetInner[i], t60, voice.bassRatio, voice.highRatio, voice.midCornerHz, highCorner, sampleRate);

    for (int ch = 0; ch < 2; ++ch)
    {
        inHigh[ch].setCutoff (p.highCutHz, sampleRate);
        inLow[ch].setCutoff (p.lowCutHz, sampleRate);
        for (int d = 0; d < numDiffusers; ++d)
            diffuser[ch][d].delaySamples = std::round (diffuserMs[d] * (ch == 0 ? 1.0f : 1.11f) * (0.6f + 0.4f * scale) * 0.001f * sampleRate);
    }

    const float diff = juce::jlimit (0.0f, 0.75f, p.diffusion * 0.75f * voice.diffusionScale);

    // Delay movement. The range is limited so that fast rates stay a shimmer, not a vibrato:
    // pitch deviation is kept under about 1.5%.
    const float rate = juce::jmax (0.01f, p.modRateHz);
    const float maxRangeSec = 0.015f / (2.0f * rate);
    const float wander = juce::jmin (p.modDepth * voice.wanderMs * 0.001f, maxRangeSec) * sampleRate;
    const float lfoInc = lumen::twoPi * rate * 0.6f / sampleRate;

    const float preSamples = juce::jmax (1.0f, p.preDelayMs * 0.001f * sampleRate);
    const float apGain = voice.allpassGain;
    constexpr float tapNorm = 0.31f;

    level.setTargetValue (p.levelGain);
    widthSm.setTargetValue (p.width);

    for (int s = 0; s < numSamples; ++s)
    {
        float x[2] = { left[s], right != nullptr ? right[s] : left[s] };
        for (int ch = 0; ch < 2; ++ch)
        {
            preDelay[ch].push (x[ch]);
            float v = preDelay[ch].readInt ((int) preSamples);
            v = inLow[ch].highpass (inHigh[ch].lowpass (v));
            for (auto& d : diffuser[ch])
                v = d.process (v, diff);
            x[ch] = v;
        }

        for (int i = 0; i < numSections; ++i)
        {
            // Size changes are crossfaded by ReverbEngine, never glided (a gliding delay bends the pitch).
            smoothedLen[i] = targetLen[i];
            smoothedAp[i] = targetAp[i];
            smoothedInner[i] = targetInner[i];
            lfoPhase[i] += lfoInc * (0.91f + 0.04f * (float) i);
            if (lfoPhase[i] > lumen::twoPi) lfoPhase[i] -= lumen::twoPi;
            const float random = drift[i].next (rate * (0.85f + 0.06f * (float) i), sampleRate);
            const float movement = voice.chorus * std::sin (lfoPhase[i]) + (1.0f - voice.chorus) * random;
            lineOut[i] = lines[i].readCubic (juce::jmax (8.0f, smoothedLen[i] + movement * wander));
        }

        // Output taps, read before the ring advances.
        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < numSections; ++i)
        {
            // The first section after each input decides how fast the sound arrives.
            const float weight = (i == 0 || i == 4) ? voice.firstTapGain : 1.0f;
            for (int t = 0; t < tapsPerSection; ++t)
            {
                const float tap = lines[i].readInt ((int) (tapFrac[t] * smoothedLen[i])) * weight;
                // The right channel flips every other tap of the left channel's pattern, so the
                // two share half their taps in phase and half out: uncorrelated, and safe in mono.
                wetL += tap * tapPolarity[i][t];
                wetR += tap * tapPolarity[i][t] * (((i * tapsPerSection + t) & 1) != 0 ? -1.0f : 1.0f);
            }
        }

        // Advance the ring; the left input enters at section 0, the right at the opposite side.
        for (int i = 0; i < numSections; ++i)
        {
            const int previous = (i + numSections - 1) % numSections;
            float v = decay[i].process (lineOut[previous]);
            if (i == 0) v += x[0] * 0.5f;
            if (i == 4) v += x[1] * 0.5f;
            inner[i].delaySamples = smoothedInner[i];
            allpass[i].delaySamples = smoothedAp[i];
            v = allpass[i].process (inner[i].process (v, apGain), apGain);
            if (std::abs (v) > 1.0f) v = std::copysign (1.0f + lumen::softClip (std::abs (v) - 1.0f), v);
            lines[i].push (v);
        }

        wetL *= tapNorm * voice.outputGain;
        wetR *= tapNorm * voice.outputGain;

        const float w = widthSm.getNextValue();
        const float m = 0.5f * (wetL + wetR), sd = 0.5f * (wetL - wetR) * w;
        const float g = level.getNextValue();
        left[s] = (m + sd) * g;
        if (right != nullptr) right[s] = (m - sd) * g;
    }
}
