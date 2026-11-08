#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Every parameter ID lives here so the DSP, GUI and presets agree on names.
namespace ParamID
{
    inline constexpr auto character  = "character";   // Vintage / Retro / Chubby / Indie / Modern / Bus
    inline constexpr auto input      = "input";       // drive into the (fixed) threshold
    inline constexpr auto output     = "output";      // make-up on the processed signal
    inline constexpr auto attack     = "attack";
    inline constexpr auto release    = "release";
    inline constexpr auto ratio      = "ratio";
    inline constexpr auto saturation = "saturation";
    inline constexpr auto softClip   = "softClip";
    inline constexpr auto parallel   = "parallel";    // 0 = source only, 100 = processed only

    inline constexpr auto keyOn      = "keyOn";
    inline constexpr auto keySource  = "keySource";   // External / Freq Key
    inline constexpr auto keyFreq    = "keyFreq";     // 50 Hz .. 16 kHz
    inline constexpr auto keyListen  = "keyListen";
}

// Display names of the six characters, in knob order.
inline const juce::StringArray& characterNames()
{
    static const juce::StringArray names { "Vintage", "Retro", "Chubby", "Indie", "Modern", "Bus" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
