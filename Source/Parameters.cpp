#include "Parameters.h"

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    Range skewed (float lo, float hi, float centre)
    {
        Range r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<APF> pct (const char* id, const char* name, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, Range (0.0f, 100.0f, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("%"));
    }

    std::unique_ptr<APF> hz (const char* id, const char* name, float lo, float hi, float centre, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, skewed (lo, hi, centre), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("Hz"));
    }

    std::unique_ptr<APF> db (const char* id, const char* name, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, Range (-60.0f, 12.0f, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("dB"));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Global
    layout.add (std::make_unique<APC> (juce::ParameterID { routing, 1 }, "Routing",
                                       juce::StringArray { "Serial", "Parallel", "Reverb > Delay" }, 0));
    layout.add (pct (mix, "Mix", 35.0f));
    layout.add (db (output, "Output", 0.0f));
    layout.add (pct (crossFeed, "Cross-Feed", 0.0f));
    layout.add (std::make_unique<APF> (juce::ParameterID { coloration, 1 }, "Coloration", Range (-100.0f, 100.0f, 0.1f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("tone")
                                           .withStringFromValueFunction ([] (float v, int)
                                           {
                                               if (std::abs (v) < 0.5f) return juce::String ("Neutral");
                                               return juce::String (v < 0.0f ? "Dark " : "Shine ") + juce::String (juce::roundToInt (std::abs (v)));
                                           })));

    // Delay
    layout.add (std::make_unique<APB> (juce::ParameterID { delayOn, 1 }, "Delay On", false));
    layout.add (std::make_unique<APF> (juce::ParameterID { delayTimeMs, 1 }, "Delay Time", skewed (1.0f, 4000.0f, 400.0f), 375.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<APB> (juce::ParameterID { delaySync, 1 }, "Delay Sync", true));
    layout.add (std::make_unique<APC> (juce::ParameterID { delayDivision, 1 }, "Delay Division",
                                       juce::StringArray { "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                                           "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1/1", "2/1", "4/1" }, 7));
    layout.add (std::make_unique<APF> (juce::ParameterID { delayFeedback, 1 }, "Delay Feedback", Range (0.0f, 110.0f, 0.1f), 35.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<APC> (juce::ParameterID { delayMode, 1 }, "Delay Mode",
                                       juce::StringArray { "Stereo", "Ping-Pong", "Dual", "Wide" }, 0));
    layout.add (std::make_unique<APC> (juce::ParameterID { delayChar, 1 }, "Delay Character",
                                       juce::StringArray { "Clean", "Analog", "Tape", "Lo-Fi" }, 1));
    layout.add (hz (delayHighCut, "Delay High-Cut", 1000.0f, 20000.0f, 6000.0f, 8000.0f));
    layout.add (hz (delayLowCut, "Delay Low-Cut", 20.0f, 2000.0f, 200.0f, 80.0f));
    layout.add (pct (delayDrive, "Delay Drive", 10.0f));
    layout.add (hz (delayModRate, "Delay Mod Rate", 0.05f, 10.0f, 1.0f, 0.6f));
    layout.add (pct (delayModDepth, "Delay Mod Depth", 10.0f));
    layout.add (pct (delayDuck, "Delay Ducking", 0.0f));
    layout.add (std::make_unique<APB> (juce::ParameterID { delayFreeze, 1 }, "Delay Freeze", false));
    layout.add (db (delayLevel, "Delay Level", 0.0f));

    // Shimmer
    layout.add (std::make_unique<APB> (juce::ParameterID { shimOn, 1 }, "Shimmer On", false));
    const auto intervalText = [] (float v, int)
    {
        const int st = juce::roundToInt (v);
        juce::String name;
        switch (std::abs (st))
        {
            case 3: name = "min 3rd"; break;   case 4: name = "3rd"; break;     case 5: name = "4th"; break;
            case 7: name = "5th"; break;       case 12: name = "Oct"; break;    case 19: name = "Oct+5th"; break;
            case 24: name = "2 Oct"; break;    default: break;
        }
        if (std::abs (v - (float) st) > 0.005f)    // older sessions may hold fractional values
            return (v > 0 ? "+" : "") + juce::String (v, 2);
        const auto num = (st > 0 ? "+" : "") + juce::String (st);
        return name.isEmpty() ? num : num + " " + name;
    };
    layout.add (std::make_unique<APF> (juce::ParameterID { shimPitchA, 1 }, "Interval A", Range (-24.0f, 24.0f, 0.01f), 12.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (intervalText)));
    layout.add (std::make_unique<APF> (juce::ParameterID { shimPitchB, 1 }, "Interval B", Range (-24.0f, 24.0f, 0.01f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("st")
                                           .withStringFromValueFunction ([intervalText] (float v, int len)
                                           {
                                               return juce::roundToInt (v) == 0 ? juce::String ("Off") : intervalText (v, len);
                                           })));
    layout.add (pct (shimFeedback, "Shimmer", 50.0f));
    layout.add (std::make_unique<APF> (juce::ParameterID { shimDecay, 1 }, "Shimmer Decay", skewed (0.5f, 30.0f, 6.0f), 6.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (pct (shimSize, "Shimmer Size", 70.0f));
    layout.add (pct (shimDiffusion, "Shimmer Diffusion", 80.0f));
    layout.add (pct (shimMod, "Shimmer Modulation", 30.0f));
    layout.add (hz (shimTone, "Shimmer Tone", 1000.0f, 16000.0f, 5000.0f, 7000.0f));
    layout.add (hz (shimLowCut, "Shimmer Low-Cut", 20.0f, 1000.0f, 200.0f, 200.0f));
    layout.add (std::make_unique<APC> (juce::ParameterID { shimVoice, 1 }, "Shimmer Voice",
                                       juce::StringArray { "Smooth", "Reverse", "Classic" }, 0));
    // Classic voice (same IDs, ranges and defaults as 0.3.x, so older sessions restore them exactly).
    layout.add (std::make_unique<APF> (juce::ParameterID { shimGrainMs, 1 }, "Grain Size", skewed (10.0f, 2000.0f, 150.0f), 120.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<APC> (juce::ParameterID { shimReverse, 1 }, "Reverse",
                                       juce::StringArray { "Off", "On", "Random" }, 0));
    layout.add (pct (shimDensity, "Density", 50.0f));
    layout.add (pct (shimSpray, "Spray", 10.0f));
    layout.add (db (shimLevel, "Shimmer Level", -6.0f));

    // Reverb
    layout.add (std::make_unique<APB> (juce::ParameterID { revOn, 1 }, "Reverb On", false));
    layout.add (std::make_unique<APC> (juce::ParameterID { revMode, 1 }, "Reverb Mode",
                                       juce::StringArray { "Hall", "Plate", "Room", "Chamber", "Bloom", "Cloud" }, 0));
    layout.add (std::make_unique<APC> (juce::ParameterID { revColor, 1 }, "Reverb Era",
                                       juce::StringArray { "60s", "70s", "80s", "90s", "Now" }, 4));
    layout.add (std::make_unique<APF> (juce::ParameterID { revPreDelay, 1 }, "Pre-Delay", Range (0.0f, 500.0f, 0.1f), 20.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<APF> (juce::ParameterID { revDecay, 1 }, "Decay", skewed (0.2f, 20.0f, 2.5f), 2.5f,
                                       juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (pct (revSize, "Size", 60.0f));
    layout.add (pct (revDiffusion, "Diffusion", 75.0f));
    layout.add (hz (revModRate, "Reverb Mod Rate", 0.05f, 5.0f, 0.8f, 0.5f));
    layout.add (pct (revModDepth, "Reverb Mod Depth", 25.0f));
    layout.add (hz (revHighCut, "Reverb High-Cut", 1000.0f, 20000.0f, 6000.0f, 9000.0f));
    layout.add (hz (revLowCut, "Reverb Low-Cut", 20.0f, 2000.0f, 200.0f, 100.0f));
    layout.add (pct (revWidth, "Reverb Width", 100.0f));
    layout.add (db (revLevel, "Reverb Level", 0.0f));

    return layout;
}
