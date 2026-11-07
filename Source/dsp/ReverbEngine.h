#pragma once
#include "DspUtils.h"
#include "ReverbParams.h"
#include "ChamberReverb.h"
#include "RingHall.h"
#include <array>

// Algorithmic reverb: an 8-line feedback delay network with input diffusion,
// modulated lines, per-line damping and six modes (Hall, Plate, Room, Chamber, Bloom, Cloud)
// plus five era colors (Now, 80s, 70s, 60s echo chamber, 90s modern digital). Modes 6 and 7 hand over
// to ChamberReverb (60s rooms), modes 8 and 9 to RingHall (90s halls); changing algorithm crossfades.
// Contract shared by all engines: process() replaces the buffer contents with this engine's wet signal.
class ReverbEngine
{
public:
    using Params = ReverbParams;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (const Params& p) { params = p; }
    void process (juce::AudioBuffer<float>& buffer);

private:
    enum class Kind { Fdn, Chamber, Ring };
    static Kind kindFor (int mode) { return mode >= 8 ? Kind::Ring : (mode >= 6 ? Kind::Chamber : Kind::Fdn); }
    void processKind (Kind, juce::AudioBuffer<float>&);
    void resetKind (Kind);
    void processFdn (juce::AudioBuffer<float>&);
    void resetFdn();

    ChamberReverb chamber;
    RingHall ring;
    Kind activeKind = Kind::Fdn, fadingKind = Kind::Fdn;
    int fadeLeft = 0, fadeLength = 1;
    int chamberVariant = 0, ringVariant = 0;
    juce::AudioBuffer<float> fadeBuffer;

    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;
    static constexpr int numBloom = 4;
    static constexpr int numEarly = 6;

    struct ModeShape
    {
        float minMs, maxMs;          // FDN line lengths at size = 1
        float diffusionScale;        // multiplies the Diffusion control
        float modScale;              // multiplies the Mod Depth control
        float earlyLevel;            // early reflections amount
        float earlyMs;               // early reflection spread
        float bloom;                 // 0..1 slow-attack diffuser chain amount
        float damping;               // extra high damping factor
    };
    static ModeShape shapeFor (int mode);

    Params params;
    float sampleRate = 44100.0f;

    lumen::DelayLine preDelay;
    lumen::DelayLine early;
    lumen::Allpass diffuser[2][numDiffusers];
    lumen::Allpass bloomChain[2][numBloom];
    lumen::DelayLine lines[numLines];
    lumen::OnePole damp[numLines], lowDamp[numLines];
    lumen::OnePole inHi[2], inLo[2], outTone[2];
    float lfoPhase[numLines] {};
    float lineState[numLines] {};
    float smoothedLen[numLines] {};
    // 60s: send EQ, tape-style send saturation and a mid-forward speaker/mic colour.
    lumen::OnePole sendHp[2], sendLp[2], presHp[2], presLp[2];
    // 90s: smoothed random "wander" on each line, and a longer low-frequency decay.
    lumen::OnePole bassSplit[numLines];
    float wander[numLines] {}, wanderTarget[numLines] {};
    int wanderCounter = 0;
    lumen::Rng rng;
    juce::SmoothedValue<float> level, widthSm;
};
