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
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // The defaults leave the signal untouched: ratio 1:1, no gain, no saturation, no clipping.
    layout.add (std::make_unique<APC> (juce::ParameterID { character, 1 }, "Character", characterNames(), 0));
    layout.add (std::make_unique<APF> (juce::ParameterID { input, 1 }, "Input", Range (-20.0f, 30.0f, 0.1f), 0.0f, unit ("dB")));
    layout.add (std::make_unique<APF> (juce::ParameterID { output, 1 }, "Output", Range (-24.0f, 24.0f, 0.1f), 0.0f, unit ("dB")));
    layout.add (std::make_unique<APF> (juce::ParameterID { attack, 1 }, "Attack", skewed (0.05f, 200.0f, 8.0f, 0.01f), 10.0f, unit ("ms")));
    layout.add (std::make_unique<APF> (juce::ParameterID { release, 1 }, "Release", skewed (20.0f, 3000.0f, 300.0f, 1.0f), 250.0f, unit ("ms")));
    layout.add (std::make_unique<APF> (juce::ParameterID { ratio, 1 }, "Ratio", skewed (1.0f, 20.0f, 4.0f, 0.01f), 1.0f,
                                       unit ("ratio").withStringFromValueFunction ([] (float v, int)
                                       {
                                           return (v < 9.95f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + ":1";
                                       })));
    layout.add (std::make_unique<APF> (juce::ParameterID { saturation, 1 }, "Saturation", Range (0.0f, 100.0f, 0.1f), 0.0f, unit ("%")));
    layout.add (std::make_unique<APB> (juce::ParameterID { softClip, 1 }, "Soft Clip", false));
    layout.add (std::make_unique<APF> (juce::ParameterID { parallel, 1 }, "Parallel", Range (0.0f, 100.0f, 0.1f), 100.0f, unit ("%")));

    layout.add (std::make_unique<APB> (juce::ParameterID { keyOn, 1 }, "Key", false));
    layout.add (std::make_unique<APC> (juce::ParameterID { keySource, 1 }, "Key Source", juce::StringArray { "External", "Freq Key" }, 0));
    layout.add (std::make_unique<APF> (juce::ParameterID { keyFreq, 1 }, "Key Freq", skewed (50.0f, 16000.0f, 1000.0f, 1.0f), 1000.0f, unit ("Hz")));
    layout.add (std::make_unique<APB> (juce::ParameterID { keyListen, 1 }, "Key Listen", false));

    return layout;
}
