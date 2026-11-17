#pragma once
#include "DspUtils.h"
#include <array>

// Shimmer reverb: a dense, modulated reverb tank with pitch shifters inside its feedback loop.
// Every time the tail goes round the loop, part of it is shifted by Interval A (and B), so the
// sound climbs octave by octave while the tank smears each step into a smooth, rising wash.
//
//   input -> low/high cut -> input diffusers -> 8-line feedback network -> output taps
//                                 ^                         |
//                                 |   decay + tone damping, orthogonal mix
//                                 +-- plain share   <-------+
//                                 +-- pitched share <-------+   (per line: shifter, Tone, Low-Cut)
//
// The Shimmer amount sets the pitched share of each pass (kept small: the plain part has to carry
// the whole decay while the pitched part builds up an interval at a time on top of it).
// The pitch shifters are delay-line shifters with three overlapping read heads. Each head picks a
// new, slightly random window length every cycle, so the shifter never settles into a periodic
// flutter; inside the diffuse tank the remaining artifacts become a soft, choir-like sheen.
// Voice "Reverse" plays each window backwards (smoother, organ-like).
// Contract shared by all engines: process() replaces the buffer contents with this engine's wet signal.
class ShimmerEngine
{
public:
    struct Params
    {
        float pitchA = 12.0f;        // semitones
        float pitchB = 0.0f;         // semitones, 0 = off
        float amount = 0.5f;         // 0..1, share of every pass that is pitch shifted
        float decaySeconds = 6.0f;
        float size = 0.7f;           // 0..1
        float diffusion = 0.8f;      // 0..1
        float modulation = 0.3f;     // 0..1
        float toneHz = 7000.0f;      // low-pass inside the loop
        float lowCutHz = 200.0f;     // high-pass inside the loop
        int voice = 0;               // 0 Smooth, 1 Reverse
        float levelGain = 0.5f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (const Params& p) { params = p; }
    void process (juce::AudioBuffer<float>& buffer);

    // Exposed for the offline tests.
    class PitchShifter
    {
    public:
        void prepare (float sampleRate, uint32_t seed);
        void reset();
        void setShift (float semitones, bool reverse);
        float process (float x);
        void push (float x) { line.push (x); }   // keeps the history fresh while unused

    private:
        static constexpr int numHeads = 3;
        struct Head { float phase = 0.0f, length = 4000.0f; };
        void newLength (Head&);

        lumen::DelayLine line;
        Head heads[numHeads];
        float sampleRate = 44100.0f, baseLength = 4000.0f, minDelay = 64.0f;
        float ratio = 1.0f;
        bool reverse = false, bypass = true;
        // Level keeper: the crossfades of overlapping heads lose power on diffuse material, which would
        // make every pitched pass quieter. Slow power tracking restores unity power gain (never more).
        float inPower = 0.0f, outPower = 0.0f, powerCoeff = 0.001f;
        lumen::Rng rng;
    };

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;

    void updateCoefficients();

    Params params;
    float sampleRate = 44100.0f;

    lumen::OnePole inHp[2], inLp[2];
    lumen::Allpass diffuser[2][numDiffusers];

    lumen::DelayLine lines[numLines];
    float lineSamples[numLines] {};
    lumen::Drift drift[numLines];
    lumen::BandDecay damping[numLines];
    lumen::OnePole dcBlock[numLines];

    // One shifter pair per tank line: every line is shifted on its own, so the loop stays orthogonal
    // (no energy is gained or lost by the routing) and the eight shifted copies are decorrelated.
    PitchShifter shiftA[numLines], shiftB[numLines];
    lumen::OnePole shiftLp[numLines], shiftHp[numLines];

    juce::SmoothedValue<float> amountSm, levelSm;
};
