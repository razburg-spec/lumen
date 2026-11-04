#pragma once
#include "DspUtils.h"
#include <array>

// Granular pitch engine: two pitch layers (A and B), grain size, reverse (Off / On / Random),
// density (grain overlap), spray (position and stereo scatter) and pitch feedback,
// which re-pitches its own output for rising "shimmer" cascades.
// Contract shared by all engines: process() replaces the buffer contents with this engine's wet signal.
class ShimmerEngine
{
public:
    struct Params
    {
        float pitchA = 12.0f;     // semitones
        float pitchB = 7.0f;      // semitones
        float grainMs = 120.0f;
        int reverse = 0;          // 0 Off, 1 On, 2 Random
        float density = 0.5f;     // 0..1
        float spray = 0.1f;       // 0..1
        float feedback = 0.3f;    // 0..1
        float levelGain = 0.5f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (const Params& p) { params = p; }
    void process (juce::AudioBuffer<float>& buffer);

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0.0;   // absolute position in the capture buffer
        double rate = 1.0;      // playback rate, negative for reverse
        int length = 0, age = 0;
        float gainL = 0.0f, gainR = 0.0f;
    };

    void spawnGrain (int layer);

    static constexpr int maxGrains = 48;
    static constexpr float maxGrainSeconds = 2.0f;

    Params params;
    float sampleRate = 44100.0f;
    lumen::DelayLine capture[2];         // input + feedback, per channel
    std::array<Grain, maxGrains> grains {};
    std::array<int, maxGrains> grainChannel {};
    float spawnCounter[2] { 0.0f, 0.0f };
    float fbState[2] { 0.0f, 0.0f };
    lumen::OnePole fbHp[2], fbLp[2];
    std::vector<float> window;           // Hann table
    juce::SmoothedValue<float> level;
    lumen::Rng rng;
};
