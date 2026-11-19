#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Prism has six bands: Low (cut or shelf), four bells, High (cut or shelf).
// Every mode uses exactly the same parameters; a mode changes only the sound of the EQ.
namespace Band
{
    inline constexpr int numBands = 6;
    inline constexpr int low = 0, high = 5;

    inline bool isEdge (int band) { return band == low || band == high; }

    // Parameter ID for a band: "b<band>_<name>", for example "b0_freq".
    inline juce::String id (int band, const char* name) { return "b" + juce::String (band) + "_" + name; }

    inline const juce::StringArray& names()
    {
        static const juce::StringArray n { "Low", "Band 1", "Band 2", "Band 3", "Band 4", "High" };
        return n;
    }

    // Default frequency of each band.
    inline float defaultFreq (int band)
    {
        static const float f[] = { 80.0f, 200.0f, 800.0f, 2500.0f, 6000.0f, 12000.0f };
        return f[juce::jlimit (0, numBands - 1, band)];
    }

    // Edge band types and cut slopes.
    enum class EdgeType { cut = 0, shelf = 1 };
    inline const juce::StringArray& slopeNames()
    {
        static const juce::StringArray s { "12 dB", "24 dB", "36 dB", "48 dB" };
        return s;
    }
}

namespace ParamID
{
    inline constexpr auto mode   = "mode";     // Vintage / Retro / Chubby / Indie / Modern / Bus
    inline constexpr auto output = "output";   // output gain
    inline constexpr auto linear = "linear";   // Linear Phase: no phase shift, adds latency
    inline constexpr auto drive  = "drive";    // pushes the mode's analog stage (0 = clean, bit-exact)

    // Per-band names (see Band::id)
    inline constexpr auto on    = "on";
    inline constexpr auto freq  = "freq";
    inline constexpr auto gain  = "gain";
    inline constexpr auto q     = "q";
    inline constexpr auto type  = "type";      // edge bands only: Cut / Shelf
    inline constexpr auto slope = "slope";     // edge bands only: 12 / 24 / 36 / 48 dB per octave
}

// Display names of the six modes, in selector order (the same family as Chroma's characters).
inline const juce::StringArray& modeNames()
{
    static const juce::StringArray names { "Vintage", "Retro", "Chubby", "Indie", "Modern", "Bus" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
