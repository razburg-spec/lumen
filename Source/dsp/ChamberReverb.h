#pragma once
#include "DspUtils.h"
#include "ReverbParams.h"

// A 1960s studio echo chamber, modelled as its signal path:
//   mono send -> tape delay with feedback, wow and flutter -> band-limited speaker
//   -> a tiled room (early reflections from an image-source model of the room,
//      late field from a 16-line feedback delay network with band-dependent decay)
//   -> a spaced stereo microphone pair.
//
// The two variants are different rooms, not different tone settings:
//   0  Tile:   half-tiled, warm in the low mids, highs decay faster.
//   1  Mirror: smaller, hard glass walls, brighter with more flutter.
//
// Controls: Pre-Delay is the tape delay time, Mod Depth and Rate are wow and flutter,
// Diffusion moves the speaker and microphones from close (early reflections forward)
// to distant (diffuse), Size scales the room.
class ChamberReverb
{
public:
    void prepare (float newSampleRate);
    void reset();
    void process (float* left, float* right, int numSamples, const ReverbParams&, int variant);

private:
    static constexpr int numLines = 16;
    static constexpr int numDiffusers = 3;
    static constexpr int numTaps = 25;   // image sources up to second order, per microphone

    struct Tap { float ms = 0.0f, gain = 0.0f; };
    Tap taps[2][2][numTaps];             // [variant][microphone][image]

    float sampleRate = 44100.0f;

    lumen::DelayLine tape, early;
    lumen::OnePole tapeFbHigh, tapeFbLow;
    float tapeFeedbackState = 0.0f, wowPhase = 0.0f;
    lumen::Drift flutter;

    lumen::OnePole inHigh, inLow, speakerHigh, speakerLow;
    lumen::Allpass diffuser[numDiffusers];

    lumen::DelayLine lines[numLines];
    lumen::BandDecay decay[numLines];
    lumen::Drift drift[numLines];
    float smoothedLen[numLines] {};

    lumen::OnePole bodyHigh[2], bodyLow[2];
    juce::SmoothedValue<float> level, widthSm;
    bool started = false;
};
