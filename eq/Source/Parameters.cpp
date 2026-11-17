#include "Parameters.h"

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    Range skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        Range r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::AudioParameterFloatAttributes unit (const char* label) { return juce::AudioParameterFloatAttributes().withLabel (label); }

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // The defaults leave the signal untouched: every band at 0 dB, both cuts off, output at 0 dB.
    layout.add (std::make_unique<APC> (juce::ParameterID { mode, 1 }, "Mode", modeNames(), 4));
    layout.add (std::make_unique<APF> (juce::ParameterID { drive, 1 }, "Drive", Range (0.0f, 100.0f, 0.1f), 0.0f, unit ("%")));
    layout.add (std::make_unique<APF> (juce::ParameterID { output, 1 }, "Output", Range (-18.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));

    for (int b = 0; b < Band::numBands; ++b)
    {
        const auto name = Band::names()[b];
        const bool edge = Band::isEdge (b);

        layout.add (std::make_unique<APB> (juce::ParameterID { Band::id (b, on), 1 }, name + " On", ! edge));
        layout.add (std::make_unique<APF> (juce::ParameterID { Band::id (b, freq), 1 }, name + " Freq",
                                           skewed (20.0f, 20000.0f, 1000.0f, 0.1f), Band::defaultFreq (b),
                                           unit ("Hz").withStringFromValueFunction ([] (float v, int) { return hzText (v); })));
        layout.add (std::make_unique<APF> (juce::ParameterID { Band::id (b, gain), 1 }, name + " Gain",
                                           Range (-18.0f, 18.0f, 0.1f), 0.0f, unit ("dB")));
        layout.add (std::make_unique<APF> (juce::ParameterID { Band::id (b, q), 1 }, name + " Q",
                                           skewed (0.1f, 18.0f, 1.0f, 0.01f), edge ? 0.71f : 1.0f, unit ("")));
        if (edge)
        {
            layout.add (std::make_unique<APC> (juce::ParameterID { Band::id (b, type), 1 }, name + " Type",
                                               juce::StringArray { b == Band::low ? "Low Cut" : "High Cut",
                                                                   b == Band::low ? "Low Shelf" : "High Shelf" }, 0));
            layout.add (std::make_unique<APC> (juce::ParameterID { Band::id (b, slope), 1 }, name + " Slope", Band::slopeNames(), 1));
        }
    }
    return layout;
}
