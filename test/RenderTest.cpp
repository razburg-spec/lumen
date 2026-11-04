// Offline render test: drives the Router with real parameters and checks the output.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Parameters.h"
#include "../Source/dsp/Router.h"
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

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
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
    for (int m = 0; m < 6; ++m)
        for (int c = 0; c < 3; ++c)
        {
            Host h; set (h, ParamID::revMode, (float) m); set (h, ParamID::revColor, (float) c);
            char nm[64]; std::snprintf (nm, sizeof nm, "reverb mode %d color %d", m, c);
            char wn[64]; std::snprintf (wn, sizeof wn, "reverb_m%d_c%d.wav", m, c);
            check (nm, render (h, { M::Serial, false, false, true }, c == 0 ? wn : "tmp.wav"), true);
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
    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
