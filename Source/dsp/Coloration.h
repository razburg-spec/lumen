#pragma once
#include <juce_dsp/juce_dsp.h>
#include "DspUtils.h"

// Global tone stage after the whole chain: one bipolar control from Dark (-100) to Shine (+100).
//
// Shine adds 2nd (even) and 3rd (odd) harmonics generated from the upper band (above ~2.5 kHz)
// plus a gentle presence lift, so the result sounds brighter and more open without simply turning
// up the treble. The harmonics come from a cubic polynomial (so nothing above the 3rd is created)
// evaluated at 2x oversampling and normalised by a smooth level envelope, which keeps them level-independent
// and free of aliasing across the whole audio band. Only this parallel harmonic path is oversampled:
// the dry signal is untouched and gains no latency.
//
// Dark adds low-order harmonics generated from the low-mids (below ~500 Hz) for warmth and tilts the
// top end down, so the result sounds thicker and darker.
// At 0 the stage is an exact bypass.
class Coloration
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlockSize = 4096)
    {
        fs = (float) sampleRate;
        channels = juce::jlimit (1, 2, numChannels);
        maxBlock = juce::jmax (1, maxBlockSize);
        amount.reset (sampleRate, 0.05);

        for (int ch = 0; ch < 2; ++ch)
        {
            hpA[ch].setCutoff (2500.0f, fs); hpB[ch].setCutoff (2500.0f, fs);
            hpOut[ch].setCutoff (3000.0f, fs); hpOut2[ch].setCutoff (3000.0f, fs);
            lowBand[ch].setCutoff (500.0f, fs); warmOut[ch].setCutoff (60.0f, fs);
            tilt[ch].setCutoff (2200.0f, fs);
        }

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, 1,
                          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlock);
        band.setSize (channels, maxBlock);

        // Steep low-pass on the harmonics at the oversampled rate. A harmonic that lands between the base-rate
        // Nyquist and the decimator's stop band would otherwise fold back down; this removes it first.
        const auto osRate = (double) fs * 2.0;
        const auto coeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderEllipticMethod (
                                juce::jmin (19500.0f, fs * 0.44f), osRate, 0.02f, -0.1f, -90.0f);
        for (int ch = 0; ch < 2; ++ch)
        {
            guard[ch].clear();
            for (auto* c : coeffs)
                guard[ch].push_back (juce::dsp::IIR::Filter<float> (c));
        }

        // Smooth level envelope at the oversampled rate (a peak follower would jitter with the sample phase).
        envCoeff = 1.0f - std::exp (-1.0f / (0.005f * fs * 2.0f));
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto* f : { &hpA[ch], &hpB[ch], &hpOut[ch], &hpOut2[ch], &lowBand[ch], &warmOut[ch], &tilt[ch] })
                f->reset();
            env[ch] = 0.0f;
            for (auto& f : guard[ch])
                f.reset();
        }
        if (oversampler != nullptr)
            oversampler->reset();
        band.clear();
    }

    int getHarmonicPathLatency() const { return oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0; }

    // value: -100 (Dark) .. +100 (Shine)
    void setValue (float value) { amount.setTargetValue (juce::jlimit (-1.0f, 1.0f, value / 100.0f)); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (oversampler == nullptr)
            return;
        if (! amount.isSmoothing() && std::abs (amount.getTargetValue()) < 1.0e-4f)
        {
            idle = true;
            return; // neutral: untouched
        }
        if (idle)
        {
            // Coming out of bypass: start the filters and oversampler from silence.
            reset();
            idle = false;
        }

        const int numCh = juce::jmin (channels, buffer.getNumChannels());
        for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
            processChunk (buffer, start, juce::jmin (maxBlock, buffer.getNumSamples() - start), numCh);
    }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n, int numCh)
    {
        // 1. Upper band of the input, at the base rate.
        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto* x = buffer.getReadPointer (ch, start);
            auto* h = band.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                h[i] = hpB[ch].highpass (hpA[ch].highpass (x[i]));
        }
        for (int ch = numCh; ch < band.getNumChannels(); ++ch)
            band.clear (ch, 0, n);

        // 2. Harmonics at 2x: a cubic of the envelope-normalised band, so only H2 and H3 exist.
        //    For a sine u = sin: u^2 -> 0.5 - 0.5 cos 2w (DC removed below), u^3 - 0.75 u -> -0.25 sin 3w.
        juce::dsp::AudioBlock<float> block (band.getArrayOfWritePointers(), (size_t) band.getNumChannels(), (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* s = up.getChannelPointer (ch);
            float e = env[ch];
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float v = s[i];
                e += envCoeff * (v * v - e);                           // mean square, ~5 ms
                const float peak = std::sqrt (2.0f * e) + 1.0e-6f;     // sine-equivalent peak
                // Steady tones give |u| <= 1 and a pure cubic; only sudden transients reach the clamp.
                const float u = juce::jlimit (-1.5f, 1.5f, v / peak);
                float y = peak * (evenGain * u * u + oddGain * (0.75f * u - u * u * u));
                for (auto& f : guard[ch])
                    y = f.processSample (y);
                s[i] = y;
            }
            env[ch] = e;
        }
        oversampler->processSamplesDown (block);

        // 3. Mix, sample by sample, with the smoothed amount.
        for (int i = 0; i < n; ++i)
        {
            const float a = amount.getNextValue();
            const float shine = juce::jmax (0.0f, a), dark = juce::jmax (0.0f, -a);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch, start);
                const float x = d[i];

                // Harmonics: remove the DC and envelope ripple of the even term (H2 of a >2.5 kHz band sits above 5 kHz).
                const float harm = hpOut2[ch].highpass (hpOut[ch].highpass (band.getSample (ch, i)));
                // Presence lift uses the band at the base rate, recomputed from the input so it stays phase-aligned.
                const float top = x - tilt[ch].lowpass (x);

                const float lo = lowBand[ch].lowpass (x);
                const float warm = warmOut[ch].highpass (std::tanh (1.8f * lo + 0.6f * lo * lo) / 1.8f - lo);

                float y = x;
                if (shine > 0.0f)
                    y += shine * (harm + presence * top);
                if (dark > 0.0f)
                    y += dark * (warm * 2.0f - 0.7f * top);
                d[i] = y;
            }
        }
    }

    static constexpr float evenGain = 0.5f;   // H2 at about -12 dB of the upper band at Shine 100
    static constexpr float oddGain  = 0.9f;   // H3 at about -13 dB of the upper band at Shine 100
    static constexpr float presence = 0.18f;  // about +1.5 dB above ~2 kHz at Shine 100

    float fs = 44100.0f;
    int channels = 2, maxBlock = 4096;
    bool idle = true;
    juce::SmoothedValue<float> amount;
    lumen::OnePole hpA[2], hpB[2], hpOut[2], hpOut2[2], lowBand[2], warmOut[2], tilt[2];
    float env[2] {}, envCoeff = 0.01f;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<juce::dsp::IIR::Filter<float>> guard[2];
    juce::AudioBuffer<float> band;
};
