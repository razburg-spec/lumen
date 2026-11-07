#pragma once
#include "DspUtils.h"
#include "ReverbParams.h"

// A 1990s studio hall: one long recirculating ring of allpass sections and delays,
// where every delay keeps drifting. The moving delays stop any single resonance from
// outliving the rest, which is what makes the tail smooth instead of metallic. There is
// no fixed early-reflection pattern: the sound builds out of the ring itself, and it
// has separate low, mid and high decay times.
//
// Variants:
//   0  Hall:    random drift, dense and quick to build.
//   1  Concert: slower, chorus-like movement, less dense, a slower build and more bass.
//
// Controls: Mod Rate is how fast the delays move, Mod Depth is how far they move.
class RingHall
{
public:
    void prepare (float newSampleRate);
    void reset();
    void process (float* left, float* right, int numSamples, const ReverbParams&, int variant);

private:
    static constexpr int numSections = 8;
    static constexpr int numDiffusers = 4;
    static constexpr int tapsPerSection = 3;

    float sampleRate = 44100.0f;

    lumen::DelayLine preDelay[2];
    lumen::OnePole inHigh[2], inLow[2];
    lumen::Allpass diffuser[2][numDiffusers];

    lumen::DelayLine lines[numSections];
    lumen::Allpass allpass[numSections], inner[numSections];
    lumen::BandDecay decay[numSections];
    lumen::Drift drift[numSections];
    float smoothedLen[numSections] {}, smoothedAp[numSections] {}, smoothedInner[numSections] {}, lfoPhase[numSections] {};
    float lineOut[numSections] {};

    juce::SmoothedValue<float> level, widthSm;
    bool started = false;
};
