#pragma once
#include "DspUtils.h"
#include "ReverbParams.h"

// The main algorithm: an 8-line feedback delay network with input diffusion, modulated lines,
// per-line damping and six modes (Hall, Plate, Room, Chamber, Bloom, Cloud). The Era control
// voices it after the studio reverbs of each decade:
//   60s  console send into tape and a physical space: band-limited, saturated, mid-forward, no movement.
//   70s  analog plates and the first digital units: dark, grainy, sparse diffusion, slow movement.
//   80s  classic digital halls: 12-bit, reduced-rate converters, bright but band-limited, strong chorus.
//   90s  later digital halls: random "wander" modulation, lush diffusion, long bass decay, open top.
//   Now  clean, full range, gentle movement.
// Delay lengths follow Size and Mode directly (no glide): ReverbEngine crossfades between two
// copies of the algorithm when they change, so the pitch never bends.
class FdnReverb
{
public:
    void prepare (float sampleRate);
    void reset (const ReverbParams&);
    void process (float* left, float* right, int numSamples, const ReverbParams&);

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;
    static constexpr int numBloom = 4;
    static constexpr int numEarly = 6;

    struct ModeShape
    {
        float minMs, maxMs;          // line lengths at size = 1
        float diffusionScale;        // multiplies the Diffusion control
        float modScale;              // multiplies the Mod Depth control
        float earlyLevel;            // early reflections amount
        float earlyMs;               // early reflection spread
        float bloom;                 // 0..1 slow-attack diffuser chain amount
        float damping;               // extra high damping factor
    };
    static ModeShape shapeFor (int mode);

    float sampleRate = 44100.0f;
    lumen::DelayLine preDelay, early;
    lumen::Allpass diffuser[2][numDiffusers];
    lumen::Allpass bloomChain[2][numBloom];
    lumen::DelayLine lines[numLines];
    lumen::OnePole damp[numLines], lowDamp[numLines], bassSplit[numLines];
    lumen::OnePole inHi[2], inLo[2], outTone[2], outTone2[2];
    lumen::OnePole sendHp[2], sendLp[2], presHp[2], presLp[2];
    float lfoPhase[numLines] {};
    float wander[numLines] {}, wanderTarget[numLines] {};
    int wanderCounter = 0;
    lumen::Rng rng;
    // 80s converter: sample-and-hold at a reduced rate.
    float holdPhase = 0.0f, held[2] {};
    lumen::OnePole imageA[2], imageB[2], imageC[2];   // output (anti-imaging) filter of the converter
    juce::SmoothedValue<float> level, widthSm;
    bool started = false;
};
