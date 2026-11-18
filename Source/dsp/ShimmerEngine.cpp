#include "ShimmerEngine.h"

namespace
{
    // Tank line lengths at Size = 1 (ms): spread out and mutually prime-ish so the modes interleave.
    constexpr float lineMs[8] = { 47.3f, 56.9f, 64.1f, 73.7f, 82.3f, 93.1f, 104.9f, 117.7f };
    // Input diffusers (ms), different per side for a wide image.
    constexpr float diffMs[2][4] = { { 4.31f, 6.73f, 11.17f, 16.39f }, { 4.79f, 7.19f, 10.43f, 17.21f } };
    // Sign patterns spread one signal over four lines without the lines summing coherently later.
    constexpr float signs[8] = { 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f };

    float sizeScale (float size) { return 0.3f + 1.2f * juce::jlimit (0.0f, 1.0f, size); }
}

//==============================================================================
void ShimmerEngine::PitchShifter::prepare (float sr, uint32_t seed)
{
    sampleRate = sr;
    baseLength = 0.085f * sr;          // window (grain) duration around 85 ms
    minDelay = 0.0015f * sr;
    powerCoeff = 1.0f - std::exp (-1.0f / (0.08f * sr));
    // Longest sweep: +24 st forward (3 x window) or reverse (5 x window) with the 1.25 x random stretch.
    line.setMaxSamples ((int) (baseLength * 1.25f * 5.2f + minDelay) + 64);
    rng.state = seed;
    reset();
}

void ShimmerEngine::PitchShifter::reset()
{
    line.clear();
    inPower = outPower = 0.0f;
    for (int h = 0; h < numHeads; ++h)
    {
        newLength (heads[h]);
        heads[h].phase = (float) h / (float) numHeads;
    }
}

void ShimmerEngine::PitchShifter::newLength (Head& h)
{
    h.length = baseLength * (0.75f + 0.5f * rng.next01());
}

void ShimmerEngine::PitchShifter::setShift (float semitones, bool rev)
{
    ratio = std::pow (2.0f, semitones / 12.0f);
    reverse = rev;
    bypass = ! rev && std::abs (semitones) < 0.01f;
}

float ShimmerEngine::PitchShifter::process (float x)
{
    line.push (x);
    if (bypass)
        return x;

    // How fast the read delay changes per sample. Forward: 1 - ratio (shrinks when shifting up).
    // Reverse: the head walks backwards at the pitched speed while the writer moves forward.
    const float slope = reverse ? 1.0f + ratio : 1.0f - ratio;
    const float sweepScale = std::abs (slope);
    const bool shrinking = slope < 0.0f;

    float sum = 0.0f, wsum = 0.0f;
    for (auto& h : heads)
    {
        const float sweep = h.length * sweepScale;        // delay range covered in one window
        const float d = minDelay + sweep * (shrinking ? 1.0f - h.phase : h.phase);
        // Smooth bell window (a close, cheap stand-in for sin^2); the normalisation below absorbs the difference.
        const float b = 4.0f * h.phase * (1.0f - h.phase);
        const float w = b * b;
        sum += w * line.readCubic (d);
        wsum += w;

        h.phase += 1.0f / h.length;
        if (h.phase >= 1.0f)
        {
            h.phase -= 1.0f;
            newLength (h);
        }
    }
    const float y = sum / juce::jmax (0.5f, wsum);
    inPower += powerCoeff * (x * x - inPower);
    outPower += powerCoeff * (y * y - outPower);
    // Power gain held at unity: up to +6 dB of make-up, never a boost above the input power.
    const float makeup = juce::jlimit (1.0f, 2.0f, std::sqrt ((inPower + 1.0e-12f) / (outPower + 1.0e-12f)));
    return y * makeup;
}

//==============================================================================
void ShimmerEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    const float maxScale = sizeScale (1.0f);
    const float maxMod = 0.004f * sampleRate;

    for (int k = 0; k < numLines; ++k)
    {
        lines[k].setMaxSamples ((int) (lineMs[k] * 0.001f * sampleRate * maxScale + maxMod) + 16);
        drift[k].seed (0x9E3779B9u * (uint32_t) (k + 1));
        shiftA[k].prepare (sampleRate, 0x1234567u + (uint32_t) k * 7919u);
        shiftB[k].prepare (sampleRate, 0x7654321u + (uint32_t) k * 104729u);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < numDiffusers; ++i)
            diffuser[ch][i].line.setMaxSamples ((int) (diffMs[ch][i] * 0.001f * sampleRate * maxScale) + 16);
    }
    classicEngine.prepare (spec);
    amountSm.reset (spec.sampleRate, 0.08);
    levelSm.reset (spec.sampleRate, 0.05);
    reset();
}

void ShimmerEngine::reset()
{
    classicEngine.setParams (params.classic);
    classicEngine.reset();
    classicActive = params.voice == 2;
    for (int ch = 0; ch < 2; ++ch)
    {
        inHp[ch].reset(); inLp[ch].reset();
        for (auto& d : diffuser[ch]) d.line.clear();
    }
    for (int k = 0; k < numLines; ++k)
    {
        lines[k].clear();
        damping[k].reset();
        dcBlock[k].reset();
        shiftLp[k].reset(); shiftHp[k].reset();
        shiftA[k].reset(); shiftB[k].reset();
    }
    updateCoefficients();
    for (int k = 0; k < numLines; ++k)
        lineSamples[k] = lineMs[k] * 0.001f * sampleRate * sizeScale (params.size);
    amountSm.setCurrentAndTargetValue (params.amount);
    levelSm.setCurrentAndTargetValue (params.levelGain);
}

void ShimmerEngine::updateCoefficients()
{
    const auto& p = params;
    const float scale = sizeScale (p.size);
    for (int ch = 0; ch < 2; ++ch)
    {
        inHp[ch].setCutoff (p.lowCutHz, sampleRate);
        inLp[ch].setCutoff (juce::jmin (16000.0f, p.toneHz * 1.6f), sampleRate);
        for (int i = 0; i < numDiffusers; ++i)
            diffuser[ch][i].delaySamples = diffMs[ch][i] * 0.001f * sampleRate * scale;
    }
    for (int k = 0; k < numLines; ++k)
    {
        const float target = lineMs[k] * 0.001f * sampleRate * scale;
        // Decay is set for the line's own length; highs fade faster above the Tone corner.
        damping[k].set (target, juce::jmax (0.2f, p.decaySeconds), 1.0f, 0.45f, 400.0f, p.toneHz, sampleRate);
        dcBlock[k].setCutoff (12.0f, sampleRate);
        // Each pitched pass is filtered: the Tone keeps the climbing octaves from turning shrill,
        // the Low-Cut keeps downward shifts from piling up mud.
        shiftLp[k].setCutoff (p.toneHz, sampleRate);
        shiftHp[k].setCutoff (p.lowCutHz, sampleRate);
        shiftA[k].setShift (p.pitchA, p.voice == 1);
        shiftB[k].setShift (p.pitchB, p.voice == 1);
    }
}

void ShimmerEngine::process (juce::AudioBuffer<float>& buffer)
{
    // Classic voice: the original grain engine, exactly as in 0.3.x. Switching voices starts the
    // newly chosen engine from silence.
    const bool wantClassic = params.voice == 2;
    if (wantClassic != classicActive)
    {
        classicActive = wantClassic;
        if (classicActive) { classicEngine.setParams (params.classic); classicEngine.reset(); }
        else               { auto keep = params; reset(); params = keep; }
    }
    if (classicActive)
    {
        classicEngine.setParams (params.classic);
        classicEngine.process (buffer);
        return;
    }
    processReverb (buffer);
}

void ShimmerEngine::processReverb (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const auto& p = params;
    updateCoefficients();
    amountSm.setTargetValue (juce::jlimit (0.0f, 1.0f, p.amount));
    levelSm.setTargetValue (p.levelGain);

    const float scale = sizeScale (p.size);
    float targetLen[numLines];
    for (int k = 0; k < numLines; ++k)
        targetLen[k] = lineMs[k] * 0.001f * sampleRate * scale;
    // Size glides over about half a second rather than jumping.
    const float glide = 1.0f - std::exp (-1.0f / (0.15f * sampleRate));

    const float g = 0.45f + 0.3f * juce::jlimit (0.0f, 1.0f, p.diffusion);        // diffuser coefficient
    const float modSamples = juce::jlimit (0.0f, 1.0f, p.modulation) * 0.0016f * sampleRate;
    const float modRate = 0.25f + 0.9f * p.modulation;
    // An interval of 0 st means "no shift" for that voice. An unshifted copy would add up in phase with the
    // plain path and build up, so a voice at 0 st simply drops out (both at 0 = a plain reverb).
    const bool useA = std::abs (p.pitchA) >= 0.5f || p.voice == 1;
    const bool useB = std::abs (p.pitchB) >= 0.5f;
    // With two intervals, A leads and B sits a little under it (equal-power split).
    const float wA = useA ? (useB ? 0.8f : 1.0f) : 0.0f;
    const float wB = useB ? (useA ? 0.6f : 1.0f) : 0.0f;
    const float shiftScale = (useA || useB) ? 1.0f : 0.0f;
    const float climb = juce::jmin (1.0f, juce::jmax (useA ? std::abs (p.pitchA) : 0.0f, useB ? std::abs (p.pitchB) : 0.0f) / 12.0f);
    // The shifter's crossfades lose a little power on diffuse material; this restores it (measured).
    constexpr float maxShiftPower = 0.25f;

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float s = amountSm.getNextValue();
        // Share of the energy that is shifted on each trip round the tank. It stays small: the plain part
        // has to last the whole decay, while the pitched part builds up an octave at a time on top of it.
        const float shiftPower = maxShiftPower * s * s * shiftScale;
        const float plainGain = std::sqrt (1.0f - shiftPower);
        const float shiftGain = std::sqrt (shiftPower);
        // The octaves that climb past the Tone filter take energy out of the loop; a little extra gain on
        // the pitched path gives it back, so high Shimmer settings keep ringing instead of thinning out.
        // Small intervals climb slowly and lose little, so they get proportionally less.
        const float shiftMakeup = 1.0f + 0.2f * s * climb;

        // Input: band-limit, then diffuse.
        float in[2] = { L[i], R ? R[i] : L[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = inLp[ch].lowpass (inHp[ch].highpass (in[ch]));
            for (auto& d : diffuser[ch])
                x = d.process (x, g);
            in[ch] = x;
        }

        // Read the tank, apply decay and tone.
        float f[numLines];
        float sum = 0.0f;
        for (int k = 0; k < numLines; ++k)
        {
            lineSamples[k] += glide * (targetLen[k] - lineSamples[k]);
            const float mod = modSamples * drift[k].next (modRate * (0.8f + 0.06f * (float) k), sampleRate);
            f[k] = dcBlock[k].highpass (damping[k].process (lines[k].readCubic (lineSamples[k] + mod + modSamples)));
            sum += f[k];
        }

        // Householder mix (orthogonal), then each line is split between the plain and the pitched path.
        const float hh = sum * (2.0f / (float) numLines);
        for (int k = 0; k < numLines; ++k)
        {
            const float y = f[k] - hh;
            float pitched = 0.0f;
            if (useA) pitched += wA * shiftA[k].process (y);
            else      shiftA[k].push (y);
            if (useB) pitched += wB * shiftB[k].process (y);
            else      shiftB[k].push (y);
            pitched = shiftLp[k].lowpass (shiftHp[k].highpass (pitched)) * shiftMakeup;
            const float x = 0.5f * signs[(k + 3) & 7] * in[k & 1] + plainGain * y + shiftGain * pitched;
            // Soft safety in the loop: transparent at normal levels.
            lines[k].push (std::abs (x) < 1.0f ? x : lumen::softClip (x));
        }

        const float outL = 0.5f * (f[0] + f[2] - f[4] + f[6]);
        const float outR = 0.5f * (f[1] - f[3] + f[5] + f[7]);
        const float lv = levelSm.getNextValue();
        L[i] = outL * lv;
        if (R) R[i] = outR * lv;
    }
}
