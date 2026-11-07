#pragma once
#include "DspUtils.h"

// Global tone stage after the whole chain: one bipolar control from Dark (-100) to Shine (+100).
//
// Shine adds even and odd harmonics generated from the upper band (above ~2.5 kHz) plus a gentle
// presence lift, so the result sounds brighter and more open without simply turning up the treble.
// Dark adds low-order harmonics generated from the low-mids (below ~500 Hz) for warmth and tilts the
// top end down, so the result sounds thicker and darker.
// At 0 the stage is an exact bypass.
class Coloration
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = (float) sampleRate;
        channels = juce::jmin (2, numChannels);
        amount.reset (sampleRate, 0.05);
        for (int ch = 0; ch < 2; ++ch)
        {
            hpA[ch].setCutoff (2500.0f, fs); hpB[ch].setCutoff (2500.0f, fs); hpOut[ch].setCutoff (3000.0f, fs);
            lowBand[ch].setCutoff (500.0f, fs); warmOut[ch].setCutoff (60.0f, fs);
            tilt[ch].setCutoff (2200.0f, fs);
            dc[ch].setCutoff (20.0f, fs);
        }
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            for (auto* f : { &hpA[ch], &hpB[ch], &hpOut[ch], &lowBand[ch], &warmOut[ch], &tilt[ch], &dc[ch] })
                f->reset();
    }

    // value: -100 (Dark) .. +100 (Shine)
    void setValue (float value) { amount.setTargetValue (juce::jlimit (-1.0f, 1.0f, value / 100.0f)); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (channels, buffer.getNumChannels());
        if (! amount.isSmoothing() && std::abs (amount.getTargetValue()) < 1.0e-4f)
            return; // neutral: untouched

        for (int i = 0; i < n; ++i)
        {
            const float a = amount.getNextValue();
            const float shine = juce::jmax (0.0f, a), dark = juce::jmax (0.0f, -a);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float x = d[i];
                float y = x;

                // Run the filters every sample so switching direction never clicks.
                const float h = hpB[ch].highpass (hpA[ch].highpass (x));
                const float even = std::abs (h) - dc[ch].lowpass (std::abs (h));   // rectified: 2nd, 4th ...
                const float odd = std::tanh (2.5f * h) * 0.4f - h;                   // soft clip residue: 3rd, 5th ...
                const float harmonicsHi = hpOut[ch].highpass (2.2f * even - 1.6f * odd);

                const float lo = lowBand[ch].lowpass (x);
                const float warm = warmOut[ch].highpass (std::tanh (1.8f * lo + 0.6f * lo * lo) / 1.8f - lo);
                const float top = x - tilt[ch].lowpass (x);

                if (shine > 0.0f)
                    y += shine * (harmonicsHi * 1.2f + 0.3f * h);
                if (dark > 0.0f)
                    y += dark * (warm * 2.0f - 0.7f * top);
                d[i] = y;
            }
        }
    }

private:
    float fs = 44100.0f;
    int channels = 2;
    juce::SmoothedValue<float> amount;
    lumen::OnePole hpA[2], hpB[2], hpOut[2], lowBand[2], warmOut[2], tilt[2], dc[2];
};
