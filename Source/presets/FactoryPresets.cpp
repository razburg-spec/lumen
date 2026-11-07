#include "FactoryPresets.h"

namespace
{
    PresetSetting on (const char* id)                    { return { id, 1.0f, nullptr }; }
    PresetSetting off (const char* id)                   { return { id, 0.0f, nullptr }; }
    PresetSetting v (const char* id, float value)        { return { id, value, nullptr }; }
    PresetSetting c (const char* id, const char* label)  { return { id, 0.0f, label }; }

    // Shorthands for the engines' power switches.
    const PresetSetting REV = on ("revOn"), DLY = on ("delayOn"), SHM = on ("shimOn");

    std::vector<FactoryPreset> build()
    {
        return {
            // ---------------------------------------------------------------- Vocals
            { "Vocals", "Vocal Plate", { REV, c ("revMode", "Plate"), c ("revColor", "Now"), v ("revPreDelay", 40), v ("revDecay", 1.8f),
                                          v ("revSize", 55), v ("revDiffusion", 80), v ("revHighCut", 9000), v ("revLowCut", 250), v ("mix", 22) } },
            { "Vocals", "Pop Vocal Hall", { REV, c ("revMode", "Hall"), c ("revColor", "Now"), v ("revPreDelay", 60), v ("revDecay", 2.6f),
                                            v ("revSize", 70), v ("revHighCut", 8000), v ("revLowCut", 300), v ("revModDepth", 30), v ("mix", 20) } },
            { "Vocals", "Ballad Vocal", { REV, DLY, c ("revMode", "Hall"), v ("revPreDelay", 80), v ("revDecay", 3.4f), v ("revSize", 75),
                                          v ("revLowCut", 280), c ("delayDivision", "1/4"), v ("delayFeedback", 25), v ("delayDuck", 60),
                                          v ("delayLevel", -8), v ("mix", 24) } },
            { "Vocals", "Slapback Vocal", { DLY, on ("delaySync"), c ("delayDivision", "1/16"), v ("delayFeedback", 8), c ("delayChar", "Tape"),
                                            v ("delayHighCut", 5000), v ("delayLowCut", 200), v ("mix", 20) } },
            { "Vocals", "Dotted Eighth Throw", { DLY, REV, c ("delayDivision", "1/8D"), v ("delayFeedback", 38), c ("delayMode", "Ping-Pong"),
                                                 v ("delayDuck", 70), v ("delayHighCut", 6000), v ("delayLowCut", 250), c ("revMode", "Plate"),
                                                 v ("revDecay", 1.6f), v ("revLevel", -6), v ("mix", 25) } },
            { "Vocals", "Rap Vocal Room", { REV, c ("revMode", "Room"), c ("revColor", "Now"), v ("revPreDelay", 10), v ("revDecay", 0.7f),
                                            v ("revSize", 35), v ("revLowCut", 350), v ("mix", 14) } },
            { "Vocals", "Ethereal Vocal", { REV, SHM, c ("revMode", "Cloud"), v ("revDecay", 6.0f), v ("revPreDelay", 50), v ("shimPitchA", 12),
                                            v ("shimPitchB", 0), v ("shimFeedback", 35), v ("shimLevel", -12), v ("mix", 30) } },
            { "Vocals", "60s Chamber Vocal", { REV, c ("revMode", "60s Chamber"), c ("revColor", "60s"), v ("revPreDelay", 90), v ("revDecay", 2.2f),
                                               v ("revLowCut", 300), v ("mix", 22) } },

            // ---------------------------------------------------------------- Drums & Percussion
            { "Drums", "Snare Plate", { REV, c ("revMode", "Plate"), c ("revColor", "80s"), v ("revPreDelay", 15), v ("revDecay", 1.4f),
                                        v ("revSize", 50), v ("revLowCut", 200), v ("revHighCut", 10000), v ("mix", 25) } },
            { "Drums", "Tight Drum Room", { REV, c ("revMode", "Room"), v ("revPreDelay", 0), v ("revDecay", 0.6f), v ("revSize", 30),
                                            v ("revDiffusion", 85), v ("mix", 20) } },
            { "Drums", "Big 80s Snare", { REV, c ("revMode", "Hall"), c ("revColor", "80s"), v ("revPreDelay", 20), v ("revDecay", 1.8f),
                                          v ("revSize", 80), v ("revDiffusion", 90), v ("revLowCut", 250), v ("mix", 35) } },
            { "Drums", "Drum Chamber", { REV, c ("revMode", "Chamber"), v ("revPreDelay", 10), v ("revDecay", 1.2f), v ("revSize", 45), v ("mix", 18) } },
            { "Drums", "Percussion Space", { REV, c ("revMode", "Chamber"), c ("revColor", "Now"), v ("revPreDelay", 25), v ("revDecay", 1.6f),
                                             v ("revWidth", 100), v ("revLowCut", 300), v ("mix", 22) } },
            { "Drums", "Dub Snare Echo", { DLY, REV, c ("delayDivision", "1/4D"), v ("delayFeedback", 55), c ("delayChar", "Tape"), v ("delayHighCut", 3500),
                                           v ("delayLowCut", 400), v ("delayModDepth", 25), c ("revMode", "Plate"), v ("revDecay", 2.0f), v ("revLevel", -4),
                                           c ("routing", "Serial"), v ("mix", 35) } },

            // ---------------------------------------------------------------- Guitars
            { "Guitars", "Spring-ish Twang", { REV, DLY, c ("revMode", "Plate"), c ("revColor", "70s"), v ("revDecay", 1.5f), v ("revSize", 35),
                                               c ("delayDivision", "1/16"), v ("delayFeedback", 15), c ("delayChar", "Analog"), v ("delayLevel", -8), v ("mix", 25) } },
            { "Guitars", "Lead Guitar Echo", { DLY, REV, c ("delayDivision", "1/8D"), v ("delayFeedback", 35), c ("delayChar", "Analog"), v ("delayDuck", 40),
                                               c ("revMode", "Hall"), v ("revDecay", 2.2f), v ("revLevel", -6), v ("mix", 28) } },
            { "Guitars", "Ambient Swells", { REV, SHM, DLY, c ("revMode", "Bloom"), v ("revDecay", 7.0f), v ("revSize", 85), v ("shimPitchA", 12),
                                             v ("shimPitchB", 7), v ("shimFeedback", 40), c ("delayDivision", "1/4"), v ("delayFeedback", 45),
                                             c ("delayMode", "Wide"), v ("mix", 45) } },
            { "Guitars", "Acoustic Room", { REV, c ("revMode", "Chamber"), c ("revColor", "Now"), v ("revPreDelay", 20), v ("revDecay", 1.3f),
                                            v ("revSize", 45), v ("revLowCut", 180), v ("mix", 16) } },
            { "Guitars", "Wide Stereo Doubler", { DLY, c ("delayMode", "Dual"), off ("delaySync"), v ("delayTimeMs", 24), v ("delayFeedback", 0),
                                                  v ("delayModDepth", 30), v ("delayModRate", 0.4f), c ("delayChar", "Clean"), v ("mix", 35) } },
            { "Guitars", "Tape Echo Rockabilly", { DLY, c ("delayDivision", "1/16D"), v ("delayFeedback", 20), c ("delayChar", "Tape"), v ("delayDrive", 30),
                                                   v ("delayHighCut", 4500), v ("mix", 30) } },

            // ---------------------------------------------------------------- Keys & Piano
            { "Keys & Piano", "Grand Piano Hall", { REV, c ("revMode", "Hall"), c ("revColor", "Now"), v ("revPreDelay", 30), v ("revDecay", 2.4f),
                                                    v ("revSize", 70), v ("revLowCut", 120), v ("revHighCut", 11000), v ("mix", 22) } },
            { "Keys & Piano", "Intimate Piano Room", { REV, c ("revMode", "Room"), v ("revPreDelay", 12), v ("revDecay", 0.9f), v ("revSize", 40), v ("mix", 18) } },
            { "Keys & Piano", "Rhodes Chorus Space", { REV, DLY, c ("revMode", "Plate"), c ("revColor", "80s"), v ("revDecay", 2.0f), v ("revModDepth", 60),
                                                       c ("delayMode", "Dual"), off ("delaySync"), v ("delayTimeMs", 18), v ("delayFeedback", 0),
                                                       v ("delayModDepth", 35), v ("delayLevel", -6), v ("mix", 30) } },
            { "Keys & Piano", "Organ Cathedral", { REV, c ("revMode", "Hall"), v ("revPreDelay", 40), v ("revDecay", 6.5f), v ("revSize", 95),
                                                   v ("revHighCut", 7000), v ("mix", 30) } },
            { "Keys & Piano", "Felt Piano Haze", { REV, c ("revMode", "Cloud"), c ("revColor", "70s"), v ("revDecay", 4.5f), v ("revHighCut", 6000),
                                                   v ("coloration", -40), v ("mix", 35) } },

            // ---------------------------------------------------------------- Synths & Pads
            { "Synths & Pads", "Shimmer Pad", { REV, SHM, c ("revMode", "Hall"), v ("revDecay", 8.0f), v ("revSize", 90), v ("shimPitchA", 12),
                                                v ("shimPitchB", 24), v ("shimFeedback", 50), v ("shimLevel", -6), v ("coloration", 25), v ("mix", 45) } },
            { "Synths & Pads", "Lush Synth Hall", { REV, c ("revMode", "Hall"), c ("revColor", "90s"), v ("revDecay", 4.0f), v ("revSize", 80),
                                                    v ("revModDepth", 45), v ("mix", 30) } },
            { "Synths & Pads", "Synth Lead Echo", { DLY, REV, c ("delayDivision", "1/8"), c ("delayMode", "Ping-Pong"), v ("delayFeedback", 40),
                                                    c ("revMode", "Plate"), v ("revDecay", 1.8f), v ("revLevel", -6), v ("mix", 28) } },
            { "Synths & Pads", "Arp Pulse", { DLY, c ("delayDivision", "1/16"), c ("delayMode", "Ping-Pong"), v ("delayFeedback", 50), c ("delayChar", "Clean"),
                                              v ("delayHighCut", 7000), v ("mix", 30) } },
            { "Synths & Pads", "Octave Cloud", { REV, SHM, c ("revMode", "Cloud"), v ("revDecay", 12.0f), v ("shimPitchA", 12), v ("shimPitchB", -12),
                                                 v ("shimFeedback", 45), v ("shimGrainMs", 220), v ("mix", 50) } },

            // ---------------------------------------------------------------- Strings & Orchestral
            { "Strings & Orchestral", "Scoring Stage", { REV, c ("revMode", "Hall"), c ("revColor", "Now"), v ("revPreDelay", 35), v ("revDecay", 2.8f),
                                                         v ("revSize", 80), v ("revLowCut", 80), v ("revHighCut", 9500), v ("mix", 25) } },
            { "Strings & Orchestral", "Concert Hall", { REV, c ("revMode", "90s Concert"), c ("revColor", "90s"), v ("revPreDelay", 30), v ("revDecay", 3.2f),
                                                        v ("revSize", 85), v ("mix", 28) } },
            { "Strings & Orchestral", "Chamber Strings", { REV, c ("revMode", "Chamber"), v ("revPreDelay", 20), v ("revDecay", 1.7f), v ("revSize", 55), v ("mix", 22) } },

            // ---------------------------------------------------------------- Ambient & Sound Design
            { "Ambient & FX", "Infinite Freeze", { DLY, REV, on ("delayFreeze"), c ("revMode", "Cloud"), v ("revDecay", 15.0f), v ("mix", 60) } },
            { "Ambient & FX", "Reverse Grains", { SHM, REV, c ("shimReverse", "On"), v ("shimGrainMs", 400), v ("shimPitchA", 0), v ("shimPitchB", 12),
                                                  v ("shimFeedback", 30), c ("revMode", "Bloom"), v ("revDecay", 5.0f), v ("mix", 50) } },
            { "Ambient & FX", "Lo-Fi Memory", { DLY, REV, c ("delayChar", "Lo-Fi"), c ("delayDivision", "1/4"), v ("delayFeedback", 45), v ("delayModDepth", 40),
                                                c ("revMode", "Room"), c ("revColor", "70s"), v ("coloration", -60), v ("mix", 40) } },
            { "Ambient & FX", "Rising Shimmer", { SHM, REV, DLY, v ("shimPitchA", 12), v ("shimPitchB", 19), v ("shimFeedback", 70), c ("revMode", "Cloud"),
                                                  v ("revDecay", 10.0f), v ("crossFeed", 30), c ("delayDivision", "1/2"), v ("delayFeedback", 40), v ("mix", 55) } },
            { "Ambient & FX", "Runaway Dub", { DLY, REV, c ("delayChar", "Tape"), c ("delayDivision", "1/4D"), v ("delayFeedback", 100), v ("delayDrive", 40),
                                               v ("delayHighCut", 3000), c ("revMode", "Plate"), v ("revLevel", -8), v ("mix", 40) } },

            // ---------------------------------------------------------------- Vintage
            { "Vintage", "60s Echo Chamber", { REV, c ("revMode", "60s Chamber"), c ("revColor", "60s"), v ("revPreDelay", 80), v ("revDecay", 2.0f), v ("mix", 25) } },
            { "Vintage", "60s Mirror Room", { REV, c ("revMode", "60s Mirror"), v ("revDecay", 1.6f), v ("mix", 22) } },
            { "Vintage", "70s Studio Plate", { REV, c ("revMode", "Plate"), c ("revColor", "70s"), v ("revDecay", 2.4f), v ("revPreDelay", 30), v ("mix", 25) } },
            { "Vintage", "80s Digital Hall", { REV, c ("revMode", "Hall"), c ("revColor", "80s"), v ("revDecay", 3.0f), v ("revModDepth", 50), v ("mix", 28) } },
            { "Vintage", "90s Random Hall", { REV, c ("revMode", "90s Hall"), c ("revColor", "90s"), v ("revDecay", 2.8f), v ("revSize", 75), v ("mix", 26) } },

            // ---------------------------------------------------------------- Send (100 % wet, for aux tracks)
            { "Send (100% Wet)", "Send: Vocal Plate", { REV, c ("revMode", "Plate"), v ("revPreDelay", 40), v ("revDecay", 1.8f), v ("revLowCut", 250), v ("mix", 100) } },
            { "Send (100% Wet)", "Send: Medium Hall", { REV, c ("revMode", "Hall"), v ("revPreDelay", 30), v ("revDecay", 2.2f), v ("revLowCut", 200), v ("mix", 100) } },
            { "Send (100% Wet)", "Send: Drum Room", { REV, c ("revMode", "Room"), v ("revDecay", 0.8f), v ("revLowCut", 150), v ("mix", 100) } },
            { "Send (100% Wet)", "Send: Quarter Echo", { DLY, c ("delayDivision", "1/4"), v ("delayFeedback", 30), v ("delayLowCut", 250), v ("delayHighCut", 6000), v ("mix", 100) } },
        };
    }
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = build();
    return presets;
}

juce::StringArray getPresetCategories()
{
    juce::StringArray cats;
    for (const auto& p : getFactoryPresets())
        cats.addIfNotAlreadyThere (p.category);
    return cats;
}

void resetToDefaults (juce::AudioProcessorValueTreeState& state)
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& state, const FactoryPreset& preset)
{
    resetToDefaults (state);
    for (const auto& s : preset.settings)
    {
        auto* p = state.getParameter (s.id);
        if (p == nullptr)
            continue;

        float plain = s.value;
        if (s.choice != nullptr)
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p);
            if (choice == nullptr)
                continue;
            const int index = choice->choices.indexOf (s.choice);
            if (index < 0)
                continue;   // label not available in this build: keep the default
            plain = (float) index;
        }
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}
