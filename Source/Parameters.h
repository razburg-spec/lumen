#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Every parameter ID lives here so the DSP, GUI and presets agree on names.
namespace ParamID
{
    // Global
    inline constexpr auto routing      = "routing";       // Serial / Parallel / Reverb->Delay
    inline constexpr auto mix          = "mix";
    inline constexpr auto output       = "output";
    inline constexpr auto crossFeed    = "crossFeed";
    inline constexpr auto coloration   = "coloration";    // -100 Dark .. +100 Shine

    // Delay
    inline constexpr auto delayOn       = "delayOn";
    inline constexpr auto delayTimeMs   = "delayTimeMs";
    inline constexpr auto delaySync     = "delaySync";
    inline constexpr auto delayDivision = "delayDivision";
    inline constexpr auto delayFeedback = "delayFeedback";
    inline constexpr auto delayMode     = "delayMode";     // Stereo / Ping-Pong / Dual / Wide
    inline constexpr auto delayChar     = "delayChar";     // Clean / Analog / Tape / Lo-Fi
    inline constexpr auto delayHighCut  = "delayHighCut";
    inline constexpr auto delayLowCut   = "delayLowCut";
    inline constexpr auto delayDrive    = "delayDrive";
    inline constexpr auto delayModRate  = "delayModRate";
    inline constexpr auto delayModDepth = "delayModDepth";
    inline constexpr auto delayDuck     = "delayDuck";
    inline constexpr auto delayFreeze   = "delayFreeze";
    inline constexpr auto delayLevel    = "delayLevel";

    // Shimmer
    inline constexpr auto shimOn       = "shimOn";
    inline constexpr auto shimPitchA   = "shimPitchA";
    inline constexpr auto shimPitchB   = "shimPitchB";
    inline constexpr auto shimGrainMs  = "shimGrainMs";
    inline constexpr auto shimReverse  = "shimReverse";    // Off / On / Random
    inline constexpr auto shimDensity  = "shimDensity";
    inline constexpr auto shimSpray    = "shimSpray";
    inline constexpr auto shimFeedback = "shimFeedback";
    inline constexpr auto shimLevel    = "shimLevel";

    // Reverb
    inline constexpr auto revOn        = "revOn";
    inline constexpr auto revMode      = "revMode";        // Hall, Plate, Room, Chamber, Bloom, Cloud, 60s Chamber, 60s Mirror, 90s Hall, 90s Concert
    inline constexpr auto revColor     = "revColor";       // Now / 80s / 70s / 60s / 90s (modes 0-5 only)
    inline constexpr auto revPreDelay  = "revPreDelay";
    inline constexpr auto revDecay     = "revDecay";
    inline constexpr auto revSize      = "revSize";
    inline constexpr auto revDiffusion = "revDiffusion";
    inline constexpr auto revModRate   = "revModRate";
    inline constexpr auto revModDepth  = "revModDepth";
    inline constexpr auto revHighCut   = "revHighCut";
    inline constexpr auto revLowCut    = "revLowCut";
    inline constexpr auto revWidth     = "revWidth";
    inline constexpr auto revLevel     = "revLevel";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
