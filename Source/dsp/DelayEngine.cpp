#include "DelayEngine.h"

void DelayEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    for (auto& l : line)
        l.setMaxSamples ((int) (maxDelaySeconds * sampleRate) + 64);
    level.reset (spec.sampleRate, 0.05);
    reset();
}

void DelayEngine::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        line[ch].clear();
        hiCut[ch].reset(); loCut[ch].reset(); tapeTone[ch].reset();
        smoothedDelay[ch] = params.timeMs * 0.001f * sampleRate;
        crushHold[ch] = 0.0f;
    }
    envelope = 0.0f;
    freezeMix = params.freeze ? 1.0f : 0.0f;
    level.setCurrentAndTargetValue (params.levelGain);
}

void DelayEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const auto& p = params;
    const int character = p.character;

    // Loop filters. Characters narrow the band a little more each step.
    float hc = p.highCutHz, lc = p.lowCutHz;
    if (character == 2) hc = juce::jmin (hc, 9000.0f);
    if (character == 3) { hc = juce::jmin (hc, 3800.0f); lc = juce::jmax (lc, 250.0f); }
    for (int ch = 0; ch < 2; ++ch)
    {
        hiCut[ch].setCutoff (hc, sampleRate);
        loCut[ch].setCutoff (lc, sampleRate);
        tapeTone[ch].setCutoff (character == 2 ? 6500.0f : 18000.0f, sampleRate);
    }

    const float baseDelay = juce::jlimit (1.0f, maxDelaySeconds * 1000.0f, p.timeMs) * 0.001f * sampleRate;
    float targetDelay[2] { baseDelay, baseDelay };
    if (p.mode == 2) targetDelay[1] = baseDelay * 0.75f;                 // Dual: R at 3/4 of L
    if (p.mode == 3) targetDelay[1] = baseDelay + 0.013f * sampleRate;   // Wide: Haas offset on R

    // Modulation depth in samples (up to ~4 ms), Tape adds wow and flutter.
    const float modSamples = p.modDepth * 0.004f * sampleRate;
    const float wowSamples = character == 2 ? 0.0012f * sampleRate : 0.0f;
    const float lfoInc = lumen::twoPi * p.modRateHz / sampleRate;
    const float flutterInc = lumen::twoPi * 6.3f / sampleRate;

    // Glide time: tape-like pitch bend when the time changes.
    const float glide = 1.0f - std::exp (-1.0f / (0.08f * sampleRate));
    const float freezeStep = 1.0f / (0.03f * sampleRate);
    const float duckAttack = 1.0f - std::exp (-1.0f / (0.005f * sampleRate));
    const float duckRelease = 1.0f - std::exp (-1.0f / (0.25f * sampleRate));

    const float drive = 1.0f + p.drive * 6.0f;
    const float fb = juce::jlimit (0.0f, 1.1f, p.feedback);

    // Lo-Fi sample-rate reduction to ~11 kHz with 10-bit steps.
    const float crushRatio = sampleRate / 11025.0f;
    const float crushSteps = 512.0f;

    level.setTargetValue (p.levelGain);

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R ? R[i] : inL;

        freezeMix += p.freeze ? freezeStep : -freezeStep;
        freezeMix = juce::jlimit (0.0f, 1.0f, freezeMix);

        // Envelope of the input, used for ducking.
        const float a = std::abs (0.5f * (inL + inR));
        envelope += (a > envelope ? duckAttack : duckRelease) * (a - envelope);

        const float lfo = std::sin (lfoPhase);
        const float lfo2 = std::cos (lfoPhase);
        const float flutter = std::sin (flutterPhase) * 0.25f + rng.nextBipolar() * 0.02f;
        lfoPhase += lfoInc; if (lfoPhase > lumen::twoPi) lfoPhase -= lumen::twoPi;
        flutterPhase += flutterInc; if (flutterPhase > lumen::twoPi) flutterPhase -= lumen::twoPi;

        float wet[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            smoothedDelay[ch] += glide * (targetDelay[ch] - smoothedDelay[ch]);
            const float m = (ch == 0 ? lfo : (p.mode == 3 ? -lfo : lfo2));
            const float d = smoothedDelay[ch] + m * modSamples + (lfo * 0.6f + flutter) * wowSamples;
            wet[ch] = line[ch].readCubic (juce::jmax (2.0f, d));
        }

        // Feedback path: filters + character, bypassed while frozen.
        float loop[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = wet[ch];
            x = loCut[ch].highpass (hiCut[ch].lowpass (x));
            switch (character)
            {
                case 0: x = x > 1.0f || x < -1.0f ? lumen::softClip (x) : x; break;                 // Clean: only guards
                case 1: x = lumen::softClip (x * drive) / drive * (1.0f + 0.15f * p.drive); break;  // Analog
                case 2: x = tapeTone[ch].lowpass (lumen::softClip (x * drive + 0.1f * x * x) / drive * (1.0f + 0.2f * p.drive)); break;
                default:
                    x = lumen::softClip (x * drive) / drive;
                    break;
            }
            loop[ch] = x;
        }

        if (character == 3)
        {
            crushCounter += 1.0f;
            if (crushCounter >= crushRatio)
            {
                crushCounter -= crushRatio;
                for (int ch = 0; ch < 2; ++ch)
                    crushHold[ch] = std::round (loop[ch] * crushSteps) / crushSteps;
            }
            loop[0] = crushHold[0];
            loop[1] = crushHold[1];
        }

        const float fbNow = fb * (1.0f - freezeMix) + 1.0f * freezeMix;
        const float inGain = 1.0f - freezeMix;
        float fbL = (loop[0] * (1.0f - freezeMix) + wet[0] * freezeMix) * fbNow;
        float fbR = (loop[1] * (1.0f - freezeMix) + wet[1] * freezeMix) * fbNow;

        float writeL, writeR;
        if (p.mode == 1)
        {
            // Ping-pong: mono input enters on the left, repeats bounce across.
            writeL = 0.5f * (inL + inR) * inGain + fbR;
            writeR = fbL;
        }
        else
        {
            writeL = inL * inGain + fbL;
            writeR = inR * inGain + fbR;
        }
        // Hard safety net against runaway (soft clip keeps 110 % musical).
        line[0].push (juce::jlimit (-4.0f, 4.0f, writeL));
        line[1].push (juce::jlimit (-4.0f, 4.0f, writeR));

        const float duckGain = 1.0f - p.duck * juce::jlimit (0.0f, 1.0f, envelope * 4.0f);
        const float g = level.getNextValue() * duckGain;
        L[i] = wet[0] * g;
        if (R) R[i] = wet[1] * g;
    }
}
