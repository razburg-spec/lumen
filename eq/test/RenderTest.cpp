// DSP checks for Prism. Exits non-zero if anything fails.
#include "../Source/dsp/PrismEngine.h"
#include <cstdio>
#include <random>

static int failures = 0;

static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("%s  %-58s got %9.3f  want %9.3f\n", ok ? "PASS" : "FAIL", what, got, want);
    if (! ok) ++failures;
}

static void near (const char* what, double got, double want, double tol) { check (std::abs (got - want) <= tol, what, got, want); }

static double designDb (int band, PrismEngine::BandParams p, double sr, double hz)
{
    p.on = true;
    const auto d = PrismEngine::design (band, p, 4, sr);
    return 20.0 * std::log10 (d.magnitude (2.0 * PrismFilters::pi * hz / sr));
}

// Plays a sine through the engine and returns the gain in dB (after the settle time).
static double sineDb (PrismEngine::Params params, double sr, double hz, float amp = 0.25f)
{
    PrismEngine e;
    const int block = 256, total = (int) sr;   // 1 s
    e.prepare (sr, block, 2);
    juce::AudioBuffer<float> buf (2, block);
    double inSq = 0, outSq = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const float x = amp * (float) std::sin (2.0 * PrismFilters::pi * hz * (pos + i) / sr);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        juce::AudioBuffer<float> in;
        in.makeCopyOf (buf);
        e.process (buf, params);
        if (pos > total / 2)
            for (int i = 0; i < block; ++i)
            {
                inSq += (double) in.getSample (0, i) * in.getSample (0, i);
                outSq += (double) buf.getSample (0, i) * buf.getSample (0, i);
            }
    }
    return 10.0 * std::log10 (outSq / inSq);
}

static PrismEngine::Params defaults()
{
    PrismEngine::Params p;
    for (int b = 0; b < Band::numBands; ++b)
    {
        auto& band = p.bands[(size_t) b];
        band.on = ! Band::isEdge (b);
        band.freq = Band::defaultFreq (b);
        band.q = Band::isEdge (b) ? 0.71f : 1.0f;
    }
    return p;
}

int main()
{
    using P = PrismEngine::BandParams;

    // 1. Defaults are bit-exact.
    {
        PrismEngine e;
        e.prepare (48000.0, 512, 2);
        juce::AudioBuffer<float> buf (2, 512), ref (2, 512);
        std::mt19937 rng (1);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        bool exact = true;
        for (int blk = 0; blk < 50; ++blk)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (ch, i, dist (rng));
            ref.makeCopyOf (buf);
            e.process (buf, defaults());
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    exact = exact && juce::exactlyEqual (buf.getSample (ch, i), ref.getSample (ch, i));
        }
        check (exact, "defaults pass audio bit-exact", exact ? 1 : 0, 1);
    }

    // 2. Bells: exact gain at the centre, including near the top of the spectrum at 44.1 kHz.
    near ("bell +12 dB @1 kHz, 48k: gain at 1 kHz", designDb (2, P { true, 1000, 12, 1 }, 48000, 1000), 12, 0.05);
    near ("bell +12 dB @1 kHz, 48k: gain at 100 Hz", designDb (2, P { true, 1000, 12, 1 }, 48000, 100), 0.12, 0.15);
    near ("bell -9 dB @15 kHz Q2, 44.1k: gain at 15 kHz", designDb (3, P { true, 15000, -9, 2 }, 44100, 15000), -9, 0.05);
    {
        // No cramping: the bell is symmetric on a log axis (analog shape), half an octave either side.
        const double lo = designDb (3, P { true, 12000, 12, 1 }, 44100, 12000 / std::sqrt (2.0));
        const double hi = designDb (3, P { true, 12000, 12, 1 }, 44100, 12000 * std::sqrt (2.0));
        near ("bell +12 @12 kHz, 44.1k: near-symmetric +-half octave", hi - lo, 0, 1.5);
    }

    // 3. Shelves.
    near ("low shelf +6 dB @200 Hz: gain at 20 Hz", designDb (0, P { true, 200, 6, 0.71f, 1, 0 }, 48000, 20), 6, 0.1);
    near ("low shelf +6 dB @200 Hz: gain at 200 Hz (half)", designDb (0, P { true, 200, 6, 0.71f, 1, 0 }, 48000, 200), 3, 0.1);
    near ("low shelf +6 dB @200 Hz: gain at 5 kHz", designDb (0, P { true, 200, 6, 0.71f, 1, 0 }, 48000, 5000), 0, 0.05);
    near ("high shelf -8 dB @10 kHz, 44.1k: gain at 21 kHz", designDb (5, P { true, 10000, -8, 0.71f, 1, 0 }, 44100, 21000), -8, 0.6);
    near ("high shelf -8 dB @10 kHz, 44.1k: gain at 10 kHz (half)", designDb (5, P { true, 10000, -8, 0.71f, 1, 0 }, 44100, 10000), -4, 0.1);
    near ("high shelf +12 dB @16 kHz, 44.1k: gain at 1 kHz", designDb (5, P { true, 16000, 12, 0.71f, 1, 0 }, 44100, 1000), 0, 0.1);

    // 4. Cuts.
    for (int slope = 0; slope < 4; ++slope)
    {
        const double dbPerOct = 12.0 * (slope + 1);
        char label[96];
        std::snprintf (label, sizeof (label), "low cut %d dB/oct @100 Hz: -3 dB at 100 Hz", (int) dbPerOct);
        near (label, designDb (0, P { true, 100, 0, 0.7071f, 0, slope }, 48000, 100), -3.01, 0.05);
        std::snprintf (label, sizeof (label), "low cut %d dB/oct @100 Hz: at 25 Hz (2 octaves)", (int) dbPerOct);
        near (label, designDb (0, P { true, 100, 0, 0.7071f, 0, slope }, 48000, 25), -2.0 * dbPerOct, 1.0);
        std::snprintf (label, sizeof (label), "high cut %d dB/oct @5 kHz: -3 dB at 5 kHz", (int) dbPerOct);
        near (label, designDb (5, P { true, 5000, 0, 0.7071f, 0, slope }, 48000, 5000), -3.01, 0.05);
    }
    near ("low cut 24 @20 Hz, 192k: -3 dB at 20 Hz", designDb (0, P { true, 20, 0, 0.7071f, 0, 1 }, 192000, 20), -3.01, 0.05);
    near ("high cut 12 @18 kHz, 44.1k: -3 dB at 18 kHz", designDb (5, P { true, 18000, 0, 0.7071f, 0, 0 }, 44100, 18000), -3.01, 0.3);

    // 5. Real processing matches the design.
    {
        auto p = defaults();
        p.bands[2].gainDb = 6.0f;
        p.bands[2].freq = 1000.0f;
        near ("engine: bell +6 @1 kHz, sine at 1 kHz", sineDb (p, 48000, 1000), 6, 0.05);
        p = defaults();
        p.bands[0].on = true;   // low cut 24 dB @80 Hz
        near ("engine: low cut 24 @80 Hz, sine at 40 Hz", sineDb (p, 48000, 40), designDb (0, P { true, 80, 0, 0.71f, 0, 1 }, 48000, 40), 0.1);
        p = defaults();
        p.outputDb = -6.0f;
        near ("engine: output -6 dB", sineDb (p, 48000, 1000), -6, 0.01);
    }

    // 6. Modes: every mode keeps a flat EQ bit-exact; Drive adds harmonics but keeps quiet levels.
    for (int m = 0; m < 6; ++m)
    {
        auto p = defaults();
        p.mode = m;
        p.drive = 1.0f;
        char label[96];
        std::snprintf (label, sizeof (label), "drive 100%% in %s: level of a -30 dBFS 1 kHz sine", modeVoicing (m).name);
        near (label, sineDb (p, 48000, 1000, 0.03f), 0, 0.3);
    }

    // 7. Linear Phase: same magnitude as the biquads, delayed by exactly half the filter length.
    {
        auto p = defaults();
        p.linear = true;
        p.bands[2].gainDb = 6.0f;
        p.bands[2].freq = 1000.0f;
        p.bands[0].on = true;            // low cut 24 dB @ 80 Hz
        const double sr = 48000;
        PrismEngine e;
        const int block = 256;
        e.prepare (sr, block, 2);
        e.loadLinearIr (PrismEngine::buildLinearIr (p, sr));
        juce::Thread::sleep (200);       // the convolution loads its filter in the background

        auto run = [&] (double hz)
        {
            double inSq = 0, outSq = 0;
            juce::AudioBuffer<float> buf (2, block);
            for (int pos = 0; pos < (int) sr * 2; pos += block)
            {
                for (int i = 0; i < block; ++i)
                {
                    const float x = 0.25f * (float) std::sin (2.0 * PrismFilters::pi * hz * (pos + i) / sr);
                    buf.setSample (0, i, x); buf.setSample (1, i, x);
                }
                e.process (buf, p);
                if (pos > (int) sr)
                    for (int i = 0; i < block; ++i)
                    {
                        const double x = 0.25 * std::sin (2.0 * PrismFilters::pi * hz * (pos + i) / sr);
                        inSq += x * x;
                        outSq += (double) buf.getSample (0, i) * buf.getSample (0, i);
                    }
            }
            return 10.0 * std::log10 (outSq / inSq);
        };
        near ("linear phase: bell +6 @1 kHz, sine at 1 kHz", run (1000), 6, 0.1);
        near ("linear phase: low cut 24 @80 Hz, sine at 40 Hz", run (40), designDb (0, P { true, 80, 0, 0.71f, 0, 1 }, sr, 40), 1.0);

        // Impulse: the peak of a flat filter sits exactly at the reported latency, at unity.
        auto flat = defaults();
        flat.linear = true;
        PrismEngine f;
        f.prepare (sr, block, 1);
        f.loadLinearIr (PrismEngine::buildLinearIr (flat, sr));
        juce::Thread::sleep (200);
        const int lat = PrismEngine::linearLatency (sr);
        juce::AudioBuffer<float> imp (1, block);
        int peakAt = -1; float peak = 0.0f;
        for (int pos = 0; pos < lat + 2 * block; pos += block)
        {
            imp.clear();
            if (pos == 0) imp.setSample (0, 0, 1.0f);
            f.process (imp, flat);
            for (int i = 0; i < block; ++i)
                if (std::abs (imp.getSample (0, i)) > peak) { peak = std::abs (imp.getSample (0, i)); peakAt = pos + i; }
        }
        near ("linear phase: flat impulse peak position (samples)", peakAt, lat, 0);
        near ("linear phase: flat impulse peak level", peak, 1.0, 0.01);
    }

    // 8. Torture: random settings changed every block, including band switches. Must stay finite and bounded.
    {
        PrismEngine e;
        const double sr = 44100;
        e.prepare (sr, 64, 2);
        juce::AudioBuffer<float> buf (2, 64);
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        auto p = defaults();
        float maxAbs = 0.0f;
        bool finite = true;
        for (int blk = 0; blk < 20000; ++blk)
        {
            if (blk % 3 == 0)
            {
                auto& band = p.bands[(size_t) (rng() % 6)];
                band.on = u (rng) > 0.2f;
                band.freq = 20.0f * std::pow (1000.0f, u (rng));
                band.gainDb = -18.0f + 36.0f * u (rng);
                band.q = 0.1f * std::pow (180.0f, u (rng));
                band.type = (int) (rng() % 2);
                band.slope = (int) (rng() % 4);
                p.mode = (int) (rng() % 6);
                p.drive = u (rng);
            }
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 64; ++i)
                    buf.setSample (ch, i, 0.5f * (u (rng) * 2.0f - 1.0f));
            e.process (buf, p);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 64; ++i)
                {
                    const float s = buf.getSample (ch, i);
                    finite = finite && std::isfinite (s);
                    maxAbs = std::max (maxAbs, std::abs (s));
                }
        }
        check (finite, "torture: output stays finite", finite ? 1 : 0, 1);
        check (maxAbs < 200.0f, "torture: output bounded (peak)", maxAbs, 200.0);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
