#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DelayEngine.h"
#include "ShimmerEngine.h"
#include "ReverbEngine.h"

// Connects the three engines in the selected order and returns the wet signal.
//
// Serial          : Delay -> Shimmer -> Reverb. Each stage hears the dry input plus the
//                   stages before it, and the wet output is the sum of every stage.
// Reverb > Delay  : the same cascade in the order Reverb -> Shimmer -> Delay.
// Parallel        : every engine hears only the dry input; outputs are summed.
// Cross-Feed      : sends part of the reverb back into the delay input (Serial/Parallel)
//                   or part of the delay back into the reverb (Reverb > Delay).
class Router
{
public:
    enum class Mode { Serial = 0, Parallel, ReverbThenDelay };

    struct Settings
    {
        Mode mode = Mode::Serial;
        bool delayOn = true, shimmerOn = false, reverbOn = true;
    };

    // Call once from the processor constructor so the Router can read every parameter.
    void attach (juce::AudioProcessorValueTreeState& apvts);
    // Call each block before process() with the host tempo (for delay sync).
    void setTempo (double bpm) { if (bpm > 1.0) tempoBpm = bpm; }

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings& settings);

private:
    void pullParameters();
    float resolveDelayTimeMs() const;

    DelayEngine delay;
    ShimmerEngine shimmer;
    ReverbEngine reverb;
    juce::AudioBuffer<float> dry, stageIn, wetSum, feedBlock;
    double tempoBpm = 120.0;
    int numChannels = 2;
    juce::SmoothedValue<float> crossFeedSm;
    float limiterGain = 1.0f, limiterRelease = 0.0f;

    struct Raw
    {
        std::atomic<float>* p = nullptr;
        float get (float fallback) const { return p != nullptr ? p->load() : fallback; }
    };
    Raw crossFeed,
        dTime, dSync, dDiv, dFb, dMode, dChar, dHc, dLc, dDrive, dModRate, dModDepth, dDuck, dFreeze, dLevel,
        sPitchA, sPitchB, sGrain, sReverse, sDensity, sSpray, sFb, sLevel,
        rMode, rColor, rPre, rDecay, rSize, rDiff, rModRate, rModDepth, rHc, rLc, rWidth, rLevel;
};
