#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Characters.h"

// The whole Chroma signal path, shared by the plugin and the offline render test.
//
//   source ─┬─ Input ─ compressor (character) ─ Saturation ─ Output ─┐
//           │              ▲ detector: input, external key or         ├─ Parallel mix ─ Soft Clip ─ out
//           │              │ band-passed input (Freq Key)             │
//           └──────────────────────────── source (dry) ───────────────┘
//
// No stage adds latency. Saturation and Soft Clip run at 2x oversampling only while they are switched
// in (the source for the Parallel blend takes a matching pass), and are skipped entirely when off.
// With the defaults (ratio 1:1, 0 dB, no saturation, no clipping, Parallel 100%) the output is
// bit-identical to the input.
class ChromaEngine
{
public:
    struct Params
    {
        int character = 0;
        float inputDb = 0.0f, outputDb = 0.0f;
        float attackMs = 10.0f, releaseMs = 250.0f, ratio = 1.0f;
        float saturation = 0.0f;   // 0..1
        float parallel = 1.0f;     // 0 = source only, 1 = processed only
        bool softClip = false;
        bool keyOn = false;
        int keySource = 0;         // 0 External, 1 Freq Key
        float keyFreq = 1000.0f;
        bool keyListen = false;
    };

    struct Meters
    {
        std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionDb { 0.0f };
        std::atomic<bool> externalKeyMissing { false };
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // main: the track (1 or 2 channels, processed in place). sidechain: the host's key input, or nullptr.
    void process (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain, const Params&);

    Meters meters;

private:
    struct OnePole
    {
        float g = 0.0f, s = 0.0f;
        void setCutoff (float hz, float fs)
        {
            hz = juce::jlimit (5.0f, 0.45f * fs, hz);
            const float wc = std::tan (juce::MathConstants<float>::pi * hz / fs);
            g = wc / (1.0f + wc);
        }
        float lowpass (float x) { const float v = (x - s) * g; const float y = v + s; s = y + v; return y; }
        float highpass (float x) { return x - lowpass (x); }
        void reset() { s = 0.0f; }
    };

    // Topology-preserving state-variable band-pass for the Freq Key detector.
    struct BandPass
    {
        float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
        void set (float hz, float q, float fs)
        {
            g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, 0.45f * fs, hz) / fs);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        float process (float x)
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return k * v1;   // unity gain at the centre frequency
        }
        void reset() { ic1 = ic2 = 0.0f; }
    };

    void processChunk (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* key, const Params&);
    float computeGainReduction (float levelDb, const Character&, float ratio) const;
    void saturate (juce::AudioBuffer<float>& signal, juce::AudioBuffer<float>& source, int numSamples,
                   const Character&, int numChannels, float grDb);
    void softClipBlock (juce::AudioBuffer<float>& buffer, int numSamples, bool on);

    double fs = 48000.0;
    int channels = 2, maxBlock = 512;

    juce::AudioBuffer<float> dry, wet, detector, residue, shifted;
    juce::SmoothedValue<float> inputGain, outputGain, parallelMix, satAmount, satPath, clipPath;

    // Detector state
    float meanSquare = 0.0f, env = 0.0f, envSlow = 0.0f, optoMemory = 0.0f;
    BandPass keyFilter[2];
    float lastKeyFreq = -1.0f;

    // Saturation state
    std::unique_ptr<juce::dsp::Oversampling<float>> satOversampler, dryOversampler, clipOversampler;
    OnePole toneLow[2], toneHigh[2];
    int lastCharacter = -1;
    float lastDrive = 1.0f;
    bool firstBlock = true;
};
