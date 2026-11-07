#include "ReverbEngine.h"

namespace
{
    // Mutually prime-ish ratios spread between 0 and 1 for line lengths.
    constexpr float lineRatios[8] = { 0.000f, 0.137f, 0.271f, 0.389f, 0.523f, 0.661f, 0.797f, 1.000f };
    constexpr float diffuserMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
    constexpr float bloomMs[4]    = { 41.3f, 67.9f, 89.1f, 113.7f };
    constexpr float earlyTaps[6]  = { 0.11f, 0.23f, 0.37f, 0.52f, 0.71f, 0.93f };
    constexpr float earlyGains[6] = { 0.85f, -0.70f, 0.62f, -0.52f, 0.41f, -0.30f };

    // In-place 8-point fast Hadamard transform, normalised (energy preserving).
    inline void hadamard8 (float* x)
    {
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        constexpr float s = 0.35355339f; // 1 / sqrt(8)
        for (int i = 0; i < 8; ++i) x[i] *= s;
    }
}

ReverbEngine::ModeShape ReverbEngine::shapeFor (int mode)
{
    switch (mode)
    {
        case 1:  return { 9.0f,  38.0f, 1.10f, 0.6f, 0.00f, 12.0f, 0.0f, 0.6f };  // Plate: dense, bright, no early
        case 2:  return { 6.0f,  29.0f, 0.80f, 0.5f, 0.55f, 18.0f, 0.0f, 1.3f };  // Room
        case 3:  return { 13.0f, 47.0f, 0.90f, 0.7f, 0.45f, 32.0f, 0.0f, 1.1f };  // Chamber
        case 4:  return { 31.0f, 97.0f, 1.00f, 1.0f, 0.10f, 40.0f, 1.0f, 1.0f };  // Bloom: slow swell
        case 5:  return { 53.0f, 151.0f, 1.20f, 2.2f, 0.00f, 40.0f, 0.7f, 0.9f }; // Cloud: huge, drifting
        default: return { 29.0f, 89.0f, 1.00f, 1.0f, 0.25f, 45.0f, 0.0f, 1.0f };  // Hall
    }
}

void ReverbEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    const auto ms = [this] (float m) { return (int) (m * 0.001f * sampleRate) + 64; };

    preDelay.setMaxSamples (ms (510.0f));
    early.setMaxSamples (ms (60.0f));
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int d = 0; d < numDiffusers; ++d) diffuser[ch][d].line.setMaxSamples (ms (diffuserMs[d] * 1.6f + 4.0f));
        for (int b = 0; b < numBloom; ++b)     bloomChain[ch][b].line.setMaxSamples (ms (bloomMs[b] * 1.2f + 4.0f));
    }
    for (auto& l : lines) l.setMaxSamples (ms (151.0f * 1.25f + 12.0f));

    level.reset (spec.sampleRate, 0.05);
    widthSm.reset (spec.sampleRate, 0.05);
    for (int i = 0; i < numLines; ++i)
        lfoPhase[i] = lumen::twoPi * (float) i / (float) numLines;
    reset();
}

void ReverbEngine::reset()
{
    preDelay.clear();
    early.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        for (auto& d : diffuser[ch]) d.line.clear();
        for (auto& b : bloomChain[ch]) b.line.clear();
        inHi[ch].reset(); inLo[ch].reset(); outTone[ch].reset();
        sendHp[ch].reset(); sendLp[ch].reset(); presHp[ch].reset(); presLp[ch].reset();
    }
    const auto shape = shapeFor (params.mode);
    const float scale = 0.25f + 0.75f * params.size;
    for (int i = 0; i < numLines; ++i)
    {
        lines[i].clear();
        damp[i].reset(); lowDamp[i].reset(); bassSplit[i].reset();
        wander[i] = wanderTarget[i] = 0.0f;
        lineState[i] = 0.0f;
        smoothedLen[i] = (shape.minMs + lineRatios[i] * (shape.maxMs - shape.minMs)) * scale * 0.001f * sampleRate;
    }
    level.setCurrentAndTargetValue (params.levelGain);
    widthSm.setCurrentAndTargetValue (params.width);
}

void ReverbEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const auto& p = params;
    auto shape = shapeFor (p.mode);

    // Era colors.
    //  80s: narrower band, chorus-like movement.
    //  70s: darker and grainier.
    //  60s: a physical echo chamber. The send is band-limited and lightly saturated (as if fed through
    //       a console send and a tape machine), the room is small, hard-walled and dense with strong
    //       early reflections, there is no modulation, and the return has a mid-forward speaker/mic colour.
    //  90s: a modern digital hall. Smooth random "wander" on every line instead of a regular LFO,
    //       extra diffusion, a longer bass decay than treble, and an open, extended top end.
    const bool sixties = p.color == 3, nineties = p.color == 4;
    float highCut = p.highCutHz;
    float colorMod = 1.0f, colorDrive = 0.0f, bassMult = 1.0f;
    if (p.color == 1) { highCut = juce::jmin (highCut, 11000.0f); colorMod = 1.4f; }
    if (p.color == 2) { highCut = juce::jmin (highCut, 6500.0f);  colorMod = 0.7f; colorDrive = 0.6f; }
    if (sixties)
    {
        // Pull every mode towards a tiled chamber: blend 70 % of the way to the chamber shape.
        const ModeShape room { 11.0f, 41.0f, 0.85f, 0.0f, 0.95f, 26.0f, 0.0f, 1.25f };
        auto mixf = [] (float a, float b) { return a + 0.7f * (b - a); };
        shape = { mixf (shape.minMs, room.minMs), mixf (shape.maxMs, room.maxMs),
                  mixf (shape.diffusionScale, room.diffusionScale), 0.0f,
                  mixf (shape.earlyLevel, room.earlyLevel), mixf (shape.earlyMs, room.earlyMs),
                  shape.bloom * 0.3f, mixf (shape.damping, room.damping) };
        highCut = juce::jmin (highCut, 10000.0f);
        colorDrive = 0.25f;
    }
    if (nineties)
    {
        highCut = juce::jmin (20000.0f, highCut * 1.25f);
        colorMod = 1.6f;
        bassMult = 1.35f;
        shape.diffusionScale *= 1.12f;
    }

    const float scale = 0.25f + 0.75f * p.size;
    float targetLen[numLines];
    for (int i = 0; i < numLines; ++i)
        targetLen[i] = (shape.minMs + lineRatios[i] * (shape.maxMs - shape.minMs)) * scale * 0.001f * sampleRate;

    // Per-line feedback gain for the requested RT60: g = 10^(-3 * len / (fs * T60)).
    const float t60 = juce::jlimit (0.2f, 20.0f, p.decaySeconds) * (p.mode == 5 ? 1.3f : 1.0f);
    float fbGain[numLines], fbGainLow[numLines];
    for (int i = 0; i < numLines; ++i)
    {
        fbGain[i] = std::pow (10.0f, -3.0f * targetLen[i] / (sampleRate * t60));
        fbGainLow[i] = std::pow (10.0f, -3.0f * targetLen[i] / (sampleRate * t60 * bassMult));
        bassSplit[i].setCutoff (280.0f, sampleRate);
    }

    // Damping: line high cut follows the control, scaled by the mode.
    const float lineHighCut = juce::jlimit (800.0f, 20000.0f, highCut / shape.damping);
    for (int i = 0; i < numLines; ++i)
    {
        damp[i].setCutoff (lineHighCut, sampleRate);
        lowDamp[i].setCutoff (juce::jmax (20.0f, p.lowCutHz * 0.5f), sampleRate);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        inHi[ch].setCutoff (highCut, sampleRate);
        inLo[ch].setCutoff (p.lowCutHz, sampleRate);
        outTone[ch].setCutoff (highCut, sampleRate);
        sendHp[ch].setCutoff (juce::jmax (p.lowCutHz, 320.0f), sampleRate);   // console send bass cut
        sendLp[ch].setCutoff (9000.0f, sampleRate);
        presHp[ch].setCutoff (700.0f, sampleRate);                             // speaker/mic presence band
        presLp[ch].setCutoff (3200.0f, sampleRate);
    }
    // 90s wander: a new random target per line every ~40 ms, glided smoothly.
    const int wanderPeriod = juce::jmax (1, (int) (0.04f * sampleRate / juce::jmax (0.2f, p.modRateHz)));
    const float wanderGlide = 1.0f - std::exp (-1.0f / (0.12f * sampleRate / juce::jmax (0.2f, p.modRateHz)));

    const float diff = juce::jlimit (0.0f, 0.78f, p.diffusion * 0.75f * shape.diffusionScale);
    const float bloomG = 0.62f * shape.bloom;
    const float modSamples = p.modDepth * shape.modScale * colorMod * 0.0012f * sampleRate;
    const float lfoInc = lumen::twoPi * p.modRateHz / sampleRate;
    const float preSamples = juce::jmax (1.0f, p.preDelayMs * 0.001f * sampleRate);
    const float earlySpread = shape.earlyMs * scale * 0.001f * sampleRate;
    const float lenGlide = 1.0f - std::exp (-1.0f / (0.2f * sampleRate));

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int d = 0; d < numDiffusers; ++d)
            diffuser[ch][d].delaySamples = diffuserMs[d] * (ch == 0 ? 1.0f : 1.13f) * (0.6f + 0.4f * scale) * 0.001f * sampleRate;
        for (int b = 0; b < numBloom; ++b)
            bloomChain[ch][b].delaySamples = bloomMs[b] * (ch == 0 ? 1.0f : 1.07f) * (0.5f + 0.5f * scale) * 0.001f * sampleRate;
    }

    level.setTargetValue (p.levelGain);
    widthSm.setTargetValue (p.width);

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int s = 0; s < n; ++s)
    {
        const float dryL = L[s];
        const float dryR = R ? R[s] : dryL;

        // Pre-delay on the mid signal keeps the image intact via the stereo diffusers below.
        preDelay.push (0.5f * (dryL + dryR));
        const float mid = preDelay.readInt ((int) preSamples);
        const float side = 0.5f * (dryL - dryR);

        float x[2] = { mid + side, mid - side };
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = inLo[ch].highpass (inHi[ch].lowpass (x[ch]));
            if (sixties)
                v = lumen::softClip (sendLp[ch].lowpass (sendHp[ch].highpass (v)) * 1.6f) * 0.75f;
            for (int d = 0; d < numDiffusers; ++d)
                v = diffuser[ch][d].process (v, diff);
            if (shape.bloom > 0.0f)
            {
                float b = v;
                for (int k = 0; k < numBloom; ++k)
                    b = bloomChain[ch][k].process (b, bloomG);
                v = v * (1.0f - shape.bloom) + b * shape.bloom;
            }
            x[ch] = v;
        }

        // Early reflections from a short multi-tap line.
        early.push (0.5f * (x[0] + x[1]));
        float erL = 0.0f, erR = 0.0f;
        if (shape.earlyLevel > 0.0f)
            for (int t = 0; t < numEarly; ++t)
            {
                const float tap = early.readLinear (1.0f + earlyTaps[t] * earlySpread) * earlyGains[t];
                if (t & 1) erR += tap; else erL += tap;
            }

        // Read the tank.
        if (nineties && ++wanderCounter >= wanderPeriod)
        {
            wanderCounter = 0;
            for (int i = 0; i < numLines; ++i)
                wanderTarget[i] = rng.nextBipolar();
        }
        float out[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            smoothedLen[i] += lenGlide * (targetLen[i] - smoothedLen[i]);
            float lfo = std::sin (lfoPhase[i]);
            if (nineties)
            {
                wander[i] += wanderGlide * (wanderTarget[i] - wander[i]);
                lfo = 0.35f * lfo + 0.9f * wander[i];
            }
            const float mod = lfo * modSamples * (0.6f + 0.4f * lineRatios[i]);
            lfoPhase[i] += lfoInc * (0.85f + 0.05f * (float) i);
            if (lfoPhase[i] > lumen::twoPi) lfoPhase[i] -= lumen::twoPi;
            out[i] = lines[i].readLinear (juce::jmax (4.0f, smoothedLen[i] + mod));
        }

        float mixv[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            float v = damp[i].lowpass (out[i]);
            v = lowDamp[i].highpass (v);
            if (colorDrive > 0.0f) v = lumen::softClip (v * (1.0f + colorDrive)) / (1.0f + colorDrive);
            if (bassMult > 1.0f)
            {
                const float lo = bassSplit[i].lowpass (v);
                mixv[i] = lo * fbGainLow[i] + (v - lo) * fbGain[i];
            }
            else
                mixv[i] = v * fbGain[i];
        }
        hadamard8 (mixv);

        for (int i = 0; i < numLines; ++i)
        {
            // Left feeds even lines, right feeds odd lines.
            const float inj = (i & 1) ? x[1] : x[0];
            float v = mixv[i] + inj * 0.35f;
            // Soft ceiling: linear below 1, saturates above, so extreme settings cannot run away.
            if (std::abs (v) > 1.0f) v = std::copysign (1.0f + lumen::softClip (std::abs (v) - 1.0f), v);
            lines[i].push (v);
        }

        float wetL = (out[0] + out[2] + out[4] + out[6]) * 0.5f + erL * shape.earlyLevel;
        float wetR = (out[1] + out[3] + out[5] + out[7]) * 0.5f + erR * shape.earlyLevel;
        wetL = outTone[0].lowpass (wetL);
        wetR = outTone[1].lowpass (wetR);
        if (sixties)
        {
            wetL += 0.45f * presLp[0].lowpass (presHp[0].highpass (wetL));
            wetR += 0.45f * presLp[1].lowpass (presHp[1].highpass (wetR));
        }

        // Width via mid/side.
        const float w = widthSm.getNextValue();
        const float m = 0.5f * (wetL + wetR), sd = 0.5f * (wetL - wetR) * w;
        const float g = level.getNextValue();
        L[s] = (m + sd) * g;
        if (R) R[s] = (m - sd) * g;
    }
}
