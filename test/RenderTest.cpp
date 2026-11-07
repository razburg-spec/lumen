// Offline render test: drives the Router with real parameters and checks the output.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Parameters.h"
#include "../Source/dsp/Router.h"
#include "../Source/dsp/Coloration.h"
#include <cstdio>
#include <functional>

struct Host : juce::AudioProcessor
{
    Host() : AudioProcessor (BusesProperties().withInput ("in", juce::AudioChannelSet::stereo()).withOutput ("out", juce::AudioChannelSet::stereo())) {}
    const juce::String getName() const override { return "t"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "S", createParameterLayout() };
};

static void set (Host& h, const char* id, float plain)
{
    auto* p = h.apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

struct Stats { float peak = 0, rmsEarly = 0, rmsLate = 0; bool finite = true; };

static Stats render (Host& h, Router::Settings st, const char* wavName, float seconds = 6.0f, int block = 256,
                     std::function<void (double)> atTime = {})
{
    const double sr = 48000.0;
    Router r;
    r.attach (h.apvts);
    r.setTempo (120.0);
    r.prepare ({ sr, (juce::uint32) block, 2 });
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> b (2, block);
    juce::Random rnd (1);
    Stats s;
    double eE = 0, eL = 0; int cE = 0, cL = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        if (atTime) atTime (pos / sr);
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, n);
        for (int i = 0; i < n; ++i)
        {
            const int t = pos + i;
            // 0.5 s of plucked chord, then silence: shows the tail.
            float x = 0;
            if (t < sr * 0.5)
            {
                const float env = std::exp (-(float) t / (float) (sr * 0.15));
                x = env * 0.3f * (std::sin (juce::MathConstants<float>::twoPi * 220.0f * t / sr)
                                + std::sin (juce::MathConstants<float>::twoPi * 277.2f * t / sr)
                                + std::sin (juce::MathConstants<float>::twoPi * 329.6f * t / sr));
            }
            v.setSample (0, i, x);
            v.setSample (1, i, x);
        }
        r.process (v, st);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float y = v.getSample (ch, i);
                if (! std::isfinite (y)) s.finite = false;
                s.peak = juce::jmax (s.peak, std::abs (y));
                out.setSample (ch, pos + i, y);
                if (pos + i < sr * 1.0) { eE += y * y; ++cE; }
                else if (pos + i > sr * 2.0) { eL += y * y; ++cL; }
            }
    }
    s.rmsEarly = (float) std::sqrt (eE / juce::jmax (1, cE));
    s.rmsLate = (float) std::sqrt (eL / juce::jmax (1, cL));

    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (wavName);
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (out, 0, total);
        }
    return s;
}

// Writes stereo impulse responses and CPU figures for analysis (test/analyse_ir.py).
// Prints the wet loudness (pink-ish noise burst, 100 % wet) of every mode x era, for level matching.
static int loudness()
{
    constexpr double sr = 48000.0; constexpr int block = 256;
    for (int m = 0; m < 6; ++m)
    {
        std::printf ("mode %d:", m);
        for (int e = 0; e < 5; ++e)
        {
            Host h; set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) e);
            Router r; r.attach (h.apvts); r.setTempo (120.0); r.prepare ({ sr, (juce::uint32) block, 2 });
            juce::AudioBuffer<float> b (2, block); juce::Random rnd (7); float lp = 0.0f;
            double energy = 0, hiEnergy = 0; lumen::OnePole hp; hp.setCutoff (4000.0f, (float) sr);
            for (int pos = 0; pos < (int) (4.0 * sr); pos += block)
            {
                for (int k = 0; k < block; ++k)
                {
                    lp += 0.1f * ((rnd.nextFloat() - 0.5f) - lp);
                    const float x = pos < (int) (1.0 * sr) ? lp * 2.0f : 0.0f;
                    b.setSample (0, k, x); b.setSample (1, k, x);
                }
                r.process (b, { Router::Mode::Serial, false, false, true });
                for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < block; ++k) energy += (double) b.getSample (ch, k) * b.getSample (ch, k);
                for (int k = 0; k < block; ++k) { const float y = hp.highpass (b.getSample (0, k)); hiEnergy += 2.0 * y * y; }
            }
            std::printf ("  %+6.2f (%5.1f)", 10.0 * std::log10 (energy + 1e-20), 10.0 * std::log10 ((hiEnergy + 1e-20) / (energy + 1e-20)));
        }
        std::printf ("\n");
    }
    return 0;
}

static int measure()
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    // (mode, era): Hall, Plate and Chamber clean, plus the 60s chamber/room and 90s hall/cloud algorithms.
    const std::pair<int, int> combos[] = { { 0, 4 }, { 1, 4 }, { 3, 4 }, { 3, 0 }, { 2, 0 }, { 0, 3 }, { 5, 3 } };
    for (auto [m, e] : combos)
        for (float decay : { 1.0f, 2.0f, 5.0f })
        {
            Host h;
            set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) e); set (h, ParamID::revDecay, decay);
            set (h, ParamID::revPreDelay, 0.0f); set (h, ParamID::revLowCut, 20.0f);
            Router r; r.attach (h.apvts); r.setTempo (120.0); r.prepare ({ sr, (juce::uint32) block, 2 });
            const int total = (int) ((decay * 2.0f + 1.0f) * sr);
            juce::AudioBuffer<float> out (2, total), b (2, block);
            for (int pos = 0; pos < total; pos += block)
            {
                const int n = juce::jmin (block, total - pos);
                juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, n);
                v.clear();
                if (pos == 0) { v.setSample (0, 0, 1.0f); v.setSample (1, 0, 1.0f); }
                r.process (v, { Router::Mode::Serial, false, false, true });
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, v, ch, 0, n);
            }
            char wn[64]; std::snprintf (wn, sizeof wn, "ir_m%d_e%d_t%d.wav", m, e, (int) decay);
            juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (wn);
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 32, {}, 0)))
                {
                    os.release();
                    w->writeFromAudioSampleBuffer (out, 0, total);
                }
        }
    for (auto [m, e] : combos)
    {
        Host h; set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) e);
        Router r; r.attach (h.apvts); r.setTempo (120.0); r.prepare ({ sr, (juce::uint32) block, 2 });
        juce::AudioBuffer<float> b (2, block);
        juce::Random rnd (2);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const int blocks = (int) (10.0 * sr / block);
        for (int i = 0; i < blocks; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < block; ++k)
                    b.setSample (ch, k, (rnd.nextFloat() - 0.5f) * 0.3f);
            r.process (b, { Router::Mode::Serial, false, false, true });
        }
        std::printf ("cpu mode %d era %d: %.2f%% of one core\n", m, e, (juce::Time::getMillisecondCounterHiRes() - start) / 10000.0 * 100.0);
    }
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "measure")
        return measure();
    if (argc > 1 && juce::String (argv[1]) == "loudness")
        return loudness();
    int failures = 0;
    auto check = [&] (const char* name, Stats s, bool expectTail)
    {
        const bool ok = s.finite && s.peak > 0.01f && s.peak < 4.0f && (! expectTail || s.rmsLate > 1e-6f);
        std::printf ("%-28s peak %.3f  rms(0-1s) %.4f  rms(>2s) %.5f  %s\n", name, s.peak, s.rmsEarly, s.rmsLate, ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    };

    using M = Router::Mode;
    {
        Host h; check ("delay only", render (h, { M::Serial, true, false, false }, "delay.wav"), true);
    }
    for (int c = 0; c < 4; ++c)
    {
        Host h; set (h, ParamID::delayChar, (float) c); set (h, ParamID::delayFeedback, 110.0f);
        char nm[64]; std::snprintf (nm, sizeof nm, "delay char %d fb 110%%", c);
        char wn[64]; std::snprintf (wn, sizeof wn, "delay_char%d_fb110.wav", c);
        check (nm, render (h, { M::Serial, true, false, false }, wn, 12.0f), true);
    }
    for (int m = 0; m < 4; ++m)
    {
        Host h; set (h, ParamID::delayMode, (float) m);
        char nm[64]; std::snprintf (nm, sizeof nm, "delay mode %d", m);
        check (nm, render (h, { M::Serial, true, false, false }, "tmp.wav"), true);
    }
    {
        Host h; set (h, ParamID::delayFeedback, 0.0f);
        check ("delay freeze at 0.3s", render (h, { M::Serial, true, false, false }, "delay_freeze.wav", 6.0f, 256,
               [&h] (double t) { if (t > 0.3) set (h, ParamID::delayFreeze, 1.0f); }), true);
    }
    {
        Host h; check ("shimmer only", render (h, { M::Serial, false, true, false }, "shimmer.wav"), false);
    }
    {
        Host h; set (h, ParamID::shimReverse, 1.0f); set (h, ParamID::shimGrainMs, 400.0f);
        check ("shimmer reverse", render (h, { M::Serial, false, true, false }, "tmp.wav"), true);
    }
    {
        Host h; set (h, ParamID::shimFeedback, 100.0f); set (h, ParamID::shimDensity, 100.0f);
        check ("shimmer fb 100%", render (h, { M::Serial, false, true, false }, "shimfb.wav", 12.0f), false);
    }
    static const char* eraNames[] = { "60s", "70s", "80s", "90s", "Now" };
    for (int m = 0; m < 6; ++m)
        for (int e = 0; e < 5; ++e)
        {
            Host h; set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) e);
            char nm[64]; std::snprintf (nm, sizeof nm, "reverb mode %d era %s", m, eraNames[e]);
            char wn[64]; std::snprintf (wn, sizeof wn, "reverb_m%d_e%d.wav", m, e);
            check (nm, render (h, { M::Serial, false, false, true }, wn), true);
        }
    // Extremes of every control, on every algorithm: FDN, 60s chamber/room, 90s hall/cloud.
    for (auto [m, e] : { std::pair<int, int> { 0, 4 }, { 4, 1 }, { 1, 2 }, { 3, 0 }, { 2, 0 }, { 0, 3 }, { 5, 3 } })
    {
        Host h; set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) e);
        char nm[64];
        set (h, ParamID::revDecay, 20.0f); set (h, ParamID::revSize, 100.0f); set (h, ParamID::revDiffusion, 100.0f);
        set (h, ParamID::revModDepth, 100.0f); set (h, ParamID::revModRate, 5.0f); set (h, ParamID::revPreDelay, 500.0f);
        std::snprintf (nm, sizeof nm, "mode %d era %s all max", m, eraNames[e]);
        check (nm, render (h, { M::Serial, false, false, true }, "tmp.wav", 15.0f), true);
        set (h, ParamID::revDecay, 0.2f); set (h, ParamID::revSize, 0.0f); set (h, ParamID::revDiffusion, 0.0f);
        set (h, ParamID::revModDepth, 0.0f); set (h, ParamID::revPreDelay, 0.0f); set (h, ParamID::revHighCut, 1000.0f);
        set (h, ParamID::revLowCut, 2000.0f);
        std::snprintf (nm, sizeof nm, "mode %d era %s all min", m, eraNames[e]);
        check (nm, render (h, { M::Serial, false, false, true }, "tmp.wav"), false);
    }
    {
        // Walk through every mode and era while audio plays, including across the algorithm crossfades.
        Host h;
        check ("mode/era changes while playing", render (h, { M::Serial, false, false, true }, "mode_walk.wav", 6.0f, 256,
               [&h] (double t) { const int k = (int) (t * 8.0); set (h, ParamID::revMode, (float) (k % 6)); set (h, ParamID::revColor, (float) ((k / 2) % 5)); }), false);
        Host h2;
        check ("size/pre-delay sweeps while playing", render (h2, { M::Serial, false, false, true }, "tmp.wav", 6.0f, 256,
               [&h2] (double t) { set (h2, ParamID::revSize, (float) (50.0 + 50.0 * std::sin (t * 3.0))); set (h2, ParamID::revPreDelay, (float) (100.0 + 90.0 * std::sin (t * 2.0))); }), false);
    }
    {
        Host h; set (h, ParamID::revMode, 3.0f); set (h, ParamID::revColor, 0.0f); set (h, ParamID::crossFeed, 100.0f); set (h, ParamID::revDecay, 20.0f);
        set (h, ParamID::delayFeedback, 110.0f);
        check ("60s worst case cross-feed", render (h, { M::Serial, true, true, true }, "tmp.wav", 15.0f, 64), true);
        set (h, ParamID::revMode, 0.0f); set (h, ParamID::revColor, 3.0f);
        check ("90s worst case cross-feed", render (h, { M::Serial, true, true, true }, "tmp.wav", 15.0f, 64), true);
    }
    {
        // Size must not bend the pitch: a steady 1 kHz sine, Size swept from 0 to 100 % in 1.5 s.
        // Nearly all of the output's energy has to stay within a semitone of 1 kHz (a gliding delay smears it far away).
        const double sr = 48000.0; constexpr int order = 15; const int fftN = 1 << order; const int block = 256;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) fftN, juce::dsp::WindowingFunction<float>::hann);
        float worst = 1.0f; int worstM = 0, worstE = 0;
        for (auto [m, e] : { std::pair<int, int> { 0, 4 }, { 5, 4 }, { 1, 2 }, { 3, 0 }, { 0, 3 }, { 5, 3 } })
        {
            Host h; set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) e); set (h, ParamID::revDecay, 4.0f);
            set (h, ParamID::revModDepth, 0.0f); set (h, ParamID::revSize, 0.0f);
            Router r; r.attach (h.apvts); r.setTempo (120.0); r.prepare ({ sr, (juce::uint32) block, 2 });
            const int total = (int) (2.0 * sr) + fftN;
            std::vector<float> out ((size_t) total);
            juce::AudioBuffer<float> b (2, block);
            for (int pos = 0; pos + block <= total; pos += block)
            {
                const double t = (double) pos / sr;
                if (t > 2.0 && ! std::getenv ("LUMEN_NOSWEEP")) set (h, ParamID::revSize, (float) juce::jlimit (0.0, 100.0, (t - 2.0) / 1.5 * 100.0));
                for (int k = 0; k < block; ++k)
                {
                    const float x = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (pos + k) / sr);
                    b.setSample (0, k, x); b.setSample (1, k, x);
                }
                r.process (b, { M::Serial, false, false, true });
                for (int k = 0; k < block; ++k) out[(size_t) (pos + k)] = b.getSample (0, k);
            }
            if (std::getenv ("LUMEN_SWEEPRAW"))
            {
                char fn[64]; std::snprintf (fn, sizeof fn, "sweep_m%d_e%d.raw", m, e);
                juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (fn)).replaceWithData (out.data(), out.size() * sizeof (float));
            }
            std::vector<float> data ((size_t) fftN * 2, 0.0f);
            for (int i = 0; i < fftN; ++i) data[(size_t) i] = out[(size_t) (total - fftN + i)];
            window.multiplyWithWindowingTable (data.data(), (size_t) fftN);
            fft.performFrequencyOnlyForwardTransform (data.data());
            const float binHz = (float) sr / (float) fftN;
            double inBand = 0, all = 0;
            // Only look around 1 kHz (250 Hz to 4 kHz): a bent pitch lands there, while the 80s converter's
            // intended images sit far above.
            for (int k = (int) (250.0f / binHz); k < (int) (4000.0f / binHz); ++k)
            {
                const double e2 = (double) data[(size_t) k] * data[(size_t) k];
                all += e2;
                if (std::abs ((float) k * binHz - 1000.0f) < 60.0f) inBand += e2;   // within a semitone
            }
            const float frac = (float) (inBand / juce::jmax (1.0e-20, all));
            if (std::getenv ("LUMEN_VERBOSE")) std::printf ("   size sweep mode %d era %d: %.2f%%\n", m, e, frac * 100.0f);
            if (frac < worst) { worst = frac; worstM = m; worstE = e; }
        }
        const bool ok = worst > 0.97f;
        std::printf ("%-28s worst %.1f%% of energy at 1 kHz (mode %d era %d)  %s\n", "size sweep pitch", worst * 100.0f, worstM, worstE, ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }
    {
        Host h; set (h, ParamID::revDecay, 20.0f); set (h, ParamID::revSize, 100.0f); set (h, ParamID::revMode, 5.0f);
        check ("reverb cloud 20s", render (h, { M::Serial, false, false, true }, "tmp.wav", 15.0f), true);
    }
    {
        Host h; set (h, ParamID::shimOn, 1.0f);
        check ("all serial", render (h, { M::Serial, true, true, true }, "all_serial.wav"), true);
        check ("all parallel", render (h, { M::Parallel, true, true, true }, "all_parallel.wav"), true);
        check ("all reverb>delay", render (h, { M::ReverbThenDelay, true, true, true }, "all_revdelay.wav"), true);
    }
    {
        Host h; set (h, ParamID::crossFeed, 100.0f); set (h, ParamID::delayFeedback, 110.0f); set (h, ParamID::revDecay, 20.0f);
        set (h, ParamID::shimFeedback, 100.0f);
        check ("worst case cross-feed", render (h, { M::Serial, true, true, true }, "tmp.wav", 20.0f, 64), true);
    }
    {
        Host h; check ("block size 1", render (h, { M::Serial, true, true, true }, "tmp.wav", 2.0f, 1), false);
    }
    {
        // Coloration: Shine should raise high-frequency energy, Dark should lower it, 0 is a bypass.
        auto brightness = [] (float value, float& rmsOut)
        {
            const double sr = 48000.0; const int n = 48000;
            Coloration c; c.prepare (sr, 2); c.setValue (value);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                // guitar-like test tone: 220 Hz with a few harmonics
                const float t = (float) i / (float) sr;
                const float x = 0.25f * (std::sin (juce::MathConstants<float>::twoPi * 220.0f * t)
                                       + 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * t)
                                       + 0.3f * std::sin (juce::MathConstants<float>::twoPi * 1320.0f * t)
                                       + 0.2f * std::sin (juce::MathConstants<float>::twoPi * 3300.0f * t));
                b.setSample (0, i, x); b.setSample (1, i, x);
            }
            // two passes so the smoothing has settled; measure the second
            c.process (b);
            for (int i = 0; i < n; ++i) { const float t = (float) i / (float) sr;
                const float x = 0.25f * (std::sin (juce::MathConstants<float>::twoPi * 220.0f * t)
                                       + 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * t)
                                       + 0.3f * std::sin (juce::MathConstants<float>::twoPi * 1320.0f * t)
                                       + 0.2f * std::sin (juce::MathConstants<float>::twoPi * 3300.0f * t));
                b.setSample (0, i, x); b.setSample (1, i, x); }
            c.process (b);
            double e = 0, d = 0; float prev = 0;
            for (int i = 1000; i < n; ++i) { const float y = b.getSample (0, i); e += y * y; d += (y - prev) * (y - prev); prev = y; }
            rmsOut = (float) std::sqrt (e / (n - 1000));
            return (float) std::sqrt (d / e);   // high-frequency weighting
        };
        float r0, rs, rd;
        const float b0 = brightness (0.0f, r0), bs = brightness (100.0f, rs), bd = brightness (-100.0f, rd);
        const bool ok = bs > b0 * 1.05f && bd < b0 * 0.95f && std::abs (juce::Decibels::gainToDecibels (rs / r0)) < 3.0f
                        && std::abs (juce::Decibels::gainToDecibels (rd / r0)) < 3.0f;
        std::printf ("%-28s brightness neutral %.3f  shine %.3f  dark %.3f  level shine %+.1f dB dark %+.1f dB  %s\n",
                     "coloration", b0, bs, bd, juce::Decibels::gainToDecibels (rs / r0), juce::Decibels::gainToDecibels (rd / r0), ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }
    {
        // Coloration aliasing: Shine +100 on pure sines must not create anything that is not a harmonic.
        const double sr = 44100.0; constexpr int order = 15; const int fftN = 1 << order; const int n = fftN * 2;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) fftN, juce::dsp::WindowingFunction<float>::blackmanHarris);
        float worst = -200.0f; float worstF = 0.0f, worstA = 0.0f;
        for (float freq : { 1000.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f, 16000.0f })
            for (float amp : { 0.3f, 0.03f })
            {
                Coloration c; c.prepare (sr, 2, 512); c.setValue (100.0f);
                juce::AudioBuffer<float> b (2, n);
                for (int i = 0; i < n; ++i)
                {
                    const float x = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
                    b.setSample (0, i, x); b.setSample (1, i, x);
                }
                c.process (b);
                std::vector<float> data ((size_t) fftN * 2, 0.0f);
                for (int i = 0; i < fftN; ++i) data[(size_t) i] = b.getSample (0, n - fftN + i);
                window.multiplyWithWindowingTable (data.data(), (size_t) fftN);
                fft.performFrequencyOnlyForwardTransform (data.data());
                const float binHz = (float) sr / (float) fftN;
                float peak = 0.0f, stray = 0.0f, strayF = 0.0f;
                for (int k = 1; k < fftN / 2; ++k) peak = juce::jmax (peak, data[(size_t) k]);
                for (int k = 8; k < fftN / 2; ++k)
                {
                    const float f = (float) k * binHz;
                    const float nearest = std::round (f / freq) * freq;
                    if (std::abs (f - nearest) < 8.0f * binHz) continue;   // a harmonic (or the fundamental)
                    if (data[(size_t) k] > stray) { stray = data[(size_t) k]; strayF = f; }
                }
                if (std::getenv ("LUMEN_VERBOSE")) std::printf ("   f %.0f amp %.2f stray %.1f dB at %.0f Hz\n", freq, amp, juce::Decibels::gainToDecibels (stray / peak, -200.0f), strayF);
                const float db = juce::Decibels::gainToDecibels (stray / peak, -200.0f);
                if (db > worst) { worst = db; worstF = freq; worstA = amp; }
            }
        const bool ok = worst < -55.0f;
        std::printf ("%-28s worst non-harmonic %.1f dB (at %.0f Hz, amp %.2f)  %s\n", "coloration aliasing", worst, worstF, worstA, ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }
    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
