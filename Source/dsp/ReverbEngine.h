#pragma once
#include "DspUtils.h"
#include <array>

// Algorithmic reverb: an 8-line feedback delay network with input diffusion,
// modulated lines, per-line damping and six modes (Hall, Plate, Room, Chamber, Bloom, Cloud)
// plus three era colors (Now, 80s, 70s).
// Contract shared by all engines: process() replaces the buffer contents with this engine's wet signal.
class ReverbEngine
{
public:
    struct Params
    {
        int mode = 0;            // 0 Hall, 1 Plate, 2 Room, 3 Chamber, 4 Bloom, 5 Cloud
        int color = 1;           // 0 Now, 1 80s, 2 70s
        float preDelayMs = 20.0f;
        float decaySeconds = 2.5f;
        float size = 0.6f;       // 0..1
        float diffusion = 0.75f; // 0..1
        float modRateHz = 0.5f;
        float modDepth = 0.25f;  // 0..1
        float highCutHz = 9000.0f;
        float lowCutHz = 100.0f;
        float width = 1.0f;      // 0..1
        float levelGain = 1.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (const Params& p) { params = p; }
    void process (juce::AudioBuffer<float>& buffer);

private:
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
    juce::SmoothedValue<float> level, widthSm;
};
