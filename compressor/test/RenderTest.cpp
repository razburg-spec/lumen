// Offline render test for the Chroma signal path. Exits non-zero if any check fails.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/Parameters.h"
#include "../Source/dsp/ChromaEngine.h"
#include <cstdio>
#include <functional>

namespace
{
    constexpr double sr = 48000.0;
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    using Signal = std::function<float (int t, int ch)>;

    float sine (int t, float hz, float amp) { return amp * std::sin (juce::MathConstants<float>::twoPi * hz * (float) t / (float) sr); }

    struct Result { juce::AudioBuffer<float> in, out; float maxGr = 0; bool finite = true; };

    Result render (const ChromaEngine::Params& p, Signal sig, float seconds = 1.0f, int channels = 2,
                   Signal key = {}, int block = 512)
    {
        ChromaEngine e;
        e.prepare (sr, block, channels);
        const int total = (int) (seconds * sr);
        Result r;
        r.in.setSize (channels, total);
        for (int ch = 0; ch < channels; ++ch)
            for (int t = 0; t < total; ++t)
                r.in.setSample (ch, t, sig (t, ch));
        r.out.makeCopyOf (r.in);
        juce::AudioBuffer<float> keyBuf;
        if (key)
        {
            keyBuf.setSize (1, total);
            for (int t = 0; t < total; ++t) keyBuf.setSample (0, t, key (t, 0));
        }
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> view (r.out.getArrayOfWritePointers(), channels, pos, n);
            if (key)
            {
                juce::AudioBuffer<float> kv (keyBuf.getArrayOfWritePointers(), 1, pos, n);
                e.process (view, &kv, p);
            }
            else
            {
                e.process (view, nullptr, p);
            }
            r.maxGr = juce::jmax (r.maxGr, e.meters.gainReductionDb.exchange (0.0f));
        }
        for (int ch = 0; ch < channels; ++ch)
            for (int t = 0; t < total; ++t)
                if (! std::isfinite (r.out.getSample (ch, t))) r.finite = false;
        return r;
    }

    float rmsDb (const juce::AudioBuffer<float>& b, float fromSec = 0.5f)
    {
        const int start = (int) (fromSec * sr);
        return juce::Decibels::gainToDecibels (b.getRMSLevel (0, start, b.getNumSamples() - start), -120.0f);
    }

    bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int t = 0; t < a.getNumSamples(); ++t)
                if (a.getSample (ch, t) != b.getSample (ch, t)) return false;
        return true;
    }

    // Energy of the 2nd..5th harmonics relative to the fundamental (crude THD by correlation).
    float thdDb (const juce::AudioBuffer<float>& b, float f0)
    {
        const int start = (int) (0.5 * sr), n = b.getNumSamples() - start;
        auto mag = [&] (float f)
        {
            double re = 0, im = 0;
            for (int t = 0; t < n; ++t)
            {
                const double w = juce::MathConstants<double>::twoPi * f * (start + t) / sr;
                re += b.getSample (0, start + t) * std::cos (w);
                im += b.getSample (0, start + t) * std::sin (w);
            }
            return std::sqrt (re * re + im * im);
        };
        const double fund = mag (f0);
        double harm = 0;
        for (int k = 2; k <= 5; ++k) harm += std::pow (mag (f0 * (float) k), 2.0);
        return (float) (10.0 * std::log10 (harm / (fund * fund) + 1e-20));
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::Random rnd (7);
    const Signal music = [&] (int t, int ch) { return sine (t, 110.0f, 0.25f) + sine (t, 1234.0f + 3.0f * (float) ch, 0.15f)
                                                      + 0.05f * (rnd.nextFloat() * 2.0f - 1.0f); };

    // 1. Defaults are an exact bypass, for every character, stereo and mono.
    for (int c = 0; c < 6; ++c)
    {
        ChromaEngine::Params p;
        p.character = c;
        auto r = render (p, music);
        check (identical (r.in, r.out) && r.maxGr == 0.0f, characterNames()[c] + ": defaults pass the signal bit-for-bit");
    }
    {
        ChromaEngine::Params p;
        auto r = render (p, music, 0.5f, 1);
        check (identical (r.in, r.out), "Mono: defaults pass the signal bit-for-bit");
    }

    // 2. Every character compresses, stays finite, and reports its gain reduction.
    std::printf ("\n  character   GR @+6  GR @+12  GR @+18 (1 kHz at -18 dBFS, ratio 4:1)\n");
    for (int c = 0; c < 6; ++c)
    {
        float gr[3];
        bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            ChromaEngine::Params p;
            p.character = c; p.ratio = 4.0f; p.inputDb = 6.0f * (float) (k + 1);
            p.attackMs = 5.0f; p.releaseMs = 100.0f;
            auto r = render (p, [] (int t, int) { return sine (t, 1000.0f, 0.126f); });
            const float inLevel = -18.0f - 3.01f + p.inputDb;
            gr[k] = inLevel - rmsDb (r.out);
            finite &= r.finite;
        }
        std::printf ("  %-10s %6.1f  %7.1f  %7.1f\n", characterNames()[c].toRawUTF8(), gr[0], gr[1], gr[2]);
        check (finite && gr[2] > 4.0f && gr[2] > gr[0], characterNames()[c] + ": compresses more as Input rises");
    }

    // 3. Indie past 10:1 over-compresses: more input gives less output.
    {
        ChromaEngine::Params p;
        p.character = 3; p.ratio = 20.0f; p.attackMs = 1.0f; p.releaseMs = 50.0f;
        p.inputDb = 12.0f; const float a = rmsDb (render (p, [] (int t, int) { return sine (t, 1000.0f, 0.126f); }).out);
        p.inputDb = 24.0f; const float b = rmsDb (render (p, [] (int t, int) { return sine (t, 1000.0f, 0.126f); }).out);
        check (b < a, "Indie 20:1: louder input comes out quieter (" + juce::String (a, 1) + " -> " + juce::String (b, 1) + " dB)");
    }

    // 4. Saturation adds harmonics in every character and stays bounded.
    for (int c = 0; c < 6; ++c)
    {
        ChromaEngine::Params p;
        p.character = c;
        const Signal tone = [] (int t, int) { return sine (t, 500.0f, 0.5f); };
        const float clean = thdDb (render (p, tone).out, 500.0f);
        p.saturation = 1.0f;
        auto r = render (p, tone);
        const float sat = thdDb (r.out, 500.0f);
        check (r.finite && sat > clean + 20.0f && r.out.getMagnitude (0, r.out.getNumSamples()) < 2.0f,
               characterNames()[c] + ": Saturation 100% THD " + juce::String (sat, 1) + " dB");
    }

    // 4b. Heavy saturation compresses high frequencies too, it never makes them louder.
    for (int c = 0; c < 6; ++c)
    {
        ChromaEngine::Params p;
        p.character = c; p.saturation = 1.0f;
        auto r = render (p, [] (int t, int) { return sine (t, 8000.0f, 0.5f); });
        const float change = rmsDb (r.out) - rmsDb (r.in);
        check (change < 0.5f, characterNames()[c] + ": Saturation on 8 kHz changes the level by " + juce::String (change, 2) + " dB");
    }

    // 5. Soft Clip tames peaks and leaves quiet material alone.
    {
        ChromaEngine::Params p;
        p.softClip = true;
        auto quiet = render (p, [] (int t, int) { return sine (t, 200.0f, 0.3f); });
        const float levelChange = rmsDb (quiet.out) - rmsDb (quiet.in);
        check (std::abs (levelChange) < 0.05f && thdDb (quiet.out, 200.0f) < -80.0f, "Soft Clip: below the knee the level and tone stay the same");
        p.outputDb = 12.0f;
        auto hot = render (p, [] (int t, int) { return sine (t, 200.0f, 0.5f); });
        const float peak = hot.out.getMagnitude (0, (int) (0.2 * sr), hot.out.getNumSamples() - (int) (0.2 * sr));
        check (peak < 1.05f, "Soft Clip: a +6 dBFS peak comes out at " + juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dBFS");
    }

    // 6. Parallel 0% returns the source exactly, whatever the compressor is doing.
    {
        ChromaEngine::Params p;
        p.ratio = 10.0f; p.inputDb = 20.0f; p.parallel = 0.0f;
        auto r = render (p, music);
        check (identical (r.in, r.out), "Parallel 0%: output is the untouched source");
        p.saturation = 0.7f;
        auto s = render (p, music);
        check (std::abs (rmsDb (s.out) - rmsDb (s.in)) < 0.05f, "Parallel 0% with Saturation: the source passes at the same level");
        p.parallel = 0.5f;
        auto half = render (p, music);
        check (half.finite && ! identical (half.in, half.out), "Parallel 50%: blend of source and processed");
    }

    // 7. Freq Key: a key at 5 kHz reacts to a 5 kHz tone far more than to a 100 Hz tone of the same level.
    {
        ChromaEngine::Params p;
        p.ratio = 6.0f; p.inputDb = 12.0f; p.keyOn = true; p.keySource = 1; p.keyFreq = 5000.0f;
        const float grHigh = render (p, [] (int t, int) { return sine (t, 5000.0f, 0.126f); }).maxGr;
        const float grLow  = render (p, [] (int t, int) { return sine (t, 100.0f, 0.126f); }).maxGr;
        check (grHigh > 6.0f && grLow < 1.0f, "Freq Key 5 kHz: GR " + juce::String (grHigh, 1) + " dB on 5 kHz, " + juce::String (grLow, 1) + " dB on 100 Hz");
    }

    // 8. External key: the sidechain, not the track, drives the compressor.
    {
        ChromaEngine::Params p;
        p.ratio = 8.0f; p.inputDb = 6.0f; p.keyOn = true; p.keySource = 0;
        const Signal track = [] (int t, int) { return sine (t, 300.0f, 0.5f); };
        const float grSilentKey = render (p, track, 1.0f, 2, [] (int, int) { return 0.0f; }).maxGr;
        const float grLoudKey = render (p, [] (int t, int) { return sine (t, 300.0f, 0.02f); }, 1.0f, 2,
                                        [] (int t, int) { return sine (t, 60.0f, 0.8f); }).maxGr;
        check (grSilentKey == 0.0f && grLoudKey > 10.0f, "External key: silent key gives no GR, loud key gives " + juce::String (grLoudKey, 1) + " dB on a quiet track");
        // No sidechain connected: falls back to the track itself.
        const float grFallback = render (p, track).maxGr;
        check (grFallback > 3.0f, "External key without a sidechain falls back to the track");
    }

    // 9. Key Listen outputs the detector signal.
    {
        ChromaEngine::Params p;
        p.keyOn = true; p.keySource = 1; p.keyFreq = 5000.0f; p.keyListen = true;
        auto r = render (p, [] (int t, int) { return sine (t, 100.0f, 0.5f); });
        check (rmsDb (r.out) < -30.0f, "Key Listen at 5 kHz removes a 100 Hz tone (" + juce::String (rmsDb (r.out), 1) + " dB)");
    }

    // 10. Stress: extreme settings, odd block sizes, every character.
    for (int c = 0; c < 6; ++c)
    {
        ChromaEngine::Params p;
        p.character = c; p.ratio = 20.0f; p.inputDb = 30.0f; p.attackMs = 0.05f; p.releaseMs = 20.0f;
        p.saturation = 1.0f; p.softClip = true; p.outputDb = 24.0f;
        auto r = render (p, music, 1.0f, 2, {}, 333);
        check (r.finite && r.out.getMagnitude (0, r.out.getNumSamples()) < 4.0f, characterNames()[c] + ": extreme settings stay finite and bounded");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
