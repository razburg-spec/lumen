#include "ShimmerEngine.h"

void ShimmerEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    // Room for the longest grain read at +24 st (4x speed) plus spray.
    const int cap = (int) (maxGrainSeconds * sampleRate * 5.0f) + 1024;
    for (auto& c : capture) c.setMaxSamples (cap);

    window.resize (2048);
    for (size_t i = 0; i < window.size(); ++i)
        window[i] = 0.5f - 0.5f * std::cos (lumen::twoPi * (float) i / (float) (window.size() - 1));

    level.reset (spec.sampleRate, 0.05);
    reset();
}

void ShimmerEngine::reset()
{
    for (auto& c : capture) c.clear();
    for (auto& g : grains) g.active = false;
    spawnCounter[0] = spawnCounter[1] = 0.0f;
    fbState[0] = fbState[1] = 0.0f;
    for (int ch = 0; ch < 2; ++ch) { fbHp[ch].reset(); fbLp[ch].reset(); }
    level.setCurrentAndTargetValue (params.levelGain);
}

void ShimmerEngine::spawnGrain (int layer)
{
    const auto& p = params;
    int slot = -1;
    for (int i = 0; i < maxGrains; ++i)
        if (! grains[(size_t) i].active) { slot = i; break; }
    if (slot < 0) return;

    const float semis = layer == 0 ? p.pitchA : p.pitchB;
    const double rate = std::pow (2.0, (double) semis / 12.0);
    const int length = juce::jmax (64, (int) (p.grainMs * 0.001f * sampleRate));

    bool rev = p.reverse == 1 || (p.reverse == 2 && rng.next01() < 0.5f);

    const double write = (double) capture[0].getWritePos();
    const double sprayOffset = (double) (p.spray * rng.next01() * (float) length);
    const double span = rate * (double) length;           // input samples a grain covers
    double start;
    double r;
    if (! rev)
    {
        // Forward: the read head must never overtake the write head.
        const double lag = juce::jmax (0.0, span - (double) length) + 8.0 + sprayOffset;
        start = write - lag;
        r = rate;
    }
    else
    {
        // Reverse: start at the newest material and walk backwards.
        start = write - 8.0 - sprayOffset;
        r = -rate;
    }

    auto& g = grains[(size_t) slot];
    g.active = true;
    g.readPos = start;
    g.rate = r;
    g.length = length;
    g.age = 0;

    // Equal-power pan, scattered by spray; layers lean opposite ways for width.
    const float lean = layer == 0 ? -0.15f : 0.15f;
    const float pan = juce::jlimit (-1.0f, 1.0f, lean + p.spray * rng.nextBipolar());
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g.gainL = std::cos (angle) * 1.41421356f;
    g.gainR = std::sin (angle) * 1.41421356f;
    grainChannel[(size_t) slot] = layer;
}

void ShimmerEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const auto& p = params;
    const int length = juce::jmax (64, (int) (p.grainMs * 0.001f * sampleRate));
    // Overlap 2..8 grains per layer; Hann windows at overlap k sum to k/2.
    const float overlap = 2.0f + p.density * 6.0f;
    const float interval = (float) length / overlap;
    // Overlapping pitch-shifted grains add up mostly incoherently, so normalise by power:
    // Hann windows at overlap k have sum(w^2) = 3k/8. The two layers share the output (1/sqrt 2).
    const float norm = 0.70710678f / std::sqrt (0.375f * overlap);
    // With power normalisation the loop is close to unity, so 100 % gives a long cascade.
    const float fb = juce::jlimit (0.0f, 1.0f, p.feedback) * 1.05f;

    for (int ch = 0; ch < 2; ++ch)
    {
        fbHp[ch].setCutoff (150.0f, sampleRate);
        fbLp[ch].setCutoff (12000.0f, sampleRate);
    }
    level.setTargetValue (p.levelGain);

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float winScale = (float) (window.size() - 1);

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R ? R[i] : inL;

        // Capture input plus filtered feedback of our own output.
        capture[0].push (inL + fbState[0]);
        capture[1].push (inR + fbState[1]);

        for (int layer = 0; layer < 2; ++layer)
        {
            spawnCounter[layer] -= 1.0f;
            if (spawnCounter[layer] <= 0.0f)
            {
                spawnGrain (layer);
                // Small jitter avoids a buzzy, periodic grain rate.
                spawnCounter[layer] += interval * (1.0f + 0.2f * p.spray * rng.nextBipolar());
            }
        }

        float outL = 0.0f, outR = 0.0f;
        for (int gi = 0; gi < maxGrains; ++gi)
        {
            auto& g = grains[(size_t) gi];
            if (! g.active) continue;
            const float w = window[(size_t) ((float) g.age / (float) g.length * winScale)];
            const float sL = capture[0].readAbsolute (g.readPos);
            const float sR = capture[1].readAbsolute (g.readPos);
            const float mono = 0.5f * (sL + sR);
            // Keep some of the source's own stereo image, add the grain's pan.
            outL += w * (0.5f * sL + 0.5f * mono * g.gainL);
            outR += w * (0.5f * sR + 0.5f * mono * g.gainR);
            g.readPos += g.rate;
            if (++g.age >= g.length) g.active = false;
        }
        outL *= norm;
        outR *= norm;

        fbState[0] = lumen::softClip (fbLp[0].lowpass (fbHp[0].highpass (outL)) * fb);
        fbState[1] = lumen::softClip (fbLp[1].lowpass (fbHp[1].highpass (outR)) * fb);

        const float g = level.getNextValue();
        L[i] = outL * g;
        if (R) R[i] = outR * g;
    }
}
