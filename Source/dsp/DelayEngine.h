#pragma once
#include "DspUtils.h"

// Stereo delay: time or tempo sync, feedback up to 110 %, filters in the loop,
// four characters (Clean / Analog / Tape / Lo-Fi), modulation, ducking and freeze.
// Contract shared by all engines: process() replaces the buffer contents with this engine's wet signal.
class DelayEngine
{
public:
    struct Params
    {
        float timeMs = 375.0f;      // already resolved from sync/division by the Router
        float feedback = 0.35f;     // 0..1.1
        int mode = 0;               // 0 Stereo, 1 Ping-Pong, 2 Dual, 3 Wide
        int character = 1;          // 0 Clean, 1 Analog, 2 Tape, 3 Lo-Fi
        float highCutHz = 8000.0f;
        float lowCutHz = 80.0f;
        float drive = 0.1f;         // 0..1
        float modRateHz = 0.6f;
        float modDepth = 0.1f;      // 0..1
        float duck = 0.0f;          // 0..1
        bool freeze = false;
        float levelGain = 1.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (const Params& p) { params = p; }
    void process (juce::AudioBuffer<float>& buffer);

private:
    static constexpr float maxDelaySeconds = 8.0f;

    Params params;
    float sampleRate = 44100.0f;

    lumen::DelayLine line[2];
    lumen::OnePole hiCut[2], loCut[2], tapeTone[2];
    float smoothedDelay[2] { 0.0f, 0.0f };
    float lfoPhase = 0.0f, flutterPhase = 0.0f;
    float envelope = 0.0f;
    float freezeMix = 0.0f;         // 0 = normal, 1 = frozen (crossfaded)
    float crushHold[2] { 0.0f, 0.0f };
    float crushCounter = 0.0f;
    juce::SmoothedValue<float> level;
    lumen::Rng rng;
};
