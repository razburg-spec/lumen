#include "FactoryPresets.h"

namespace
{
    PresetSetting on (const char* id)                    { return { id, 1.0f, nullptr }; }
    PresetSetting v (const char* id, float value)        { return { id, value, nullptr }; }
    PresetSetting c (const char* id, const char* label)  { return { id, 0.0f, label }; }

    PresetSetting chr (const char* name) { return c ("character", name); }
    PresetSetting in (float db)          { return v ("input", db); }
    PresetSetting out (float db)         { return v ("output", db); }
    PresetSetting atk (float ms)         { return v ("attack", ms); }
    PresetSetting rel (float ms)         { return v ("release", ms); }
    PresetSetting ratio (float r)        { return v ("ratio", r); }
    PresetSetting sat (float pct)        { return v ("saturation", pct); }
    PresetSetting par (float pct)        { return v ("parallel", pct); }
    PresetSetting freqKey (float hz)     { return v ("keyFreq", hz); }
    const PresetSetting CLIP = on ("softClip"), KEY = on ("keyOn");

    std::vector<FactoryPreset> build()
    {
        return {
            // ---------------------------------------------------------------- Vocals
            { "Vocals", "Smooth Vocal Leveler", { chr ("Vintage"), in (10), ratio (3), atk (10), rel (400), sat (15), out (-4) } },
            { "Vocals", "Upfront Pop Vocal",    { chr ("Retro"), in (12), ratio (4), atk (2), rel (120), sat (20), out (-6) } },
            { "Vocals", "Warm Ballad Vocal",    { chr ("Chubby"), in (9), ratio (2.5f), atk (20), rel (600), sat (25), out (-4) } },
            { "Vocals", "Aggressive Rap Vocal", { chr ("Retro"), in (16), ratio (8), atk (0.5f), rel (80), sat (35), out (-8), CLIP } },
            { "Vocals", "Transparent Vocal",    { chr ("Modern"), in (8), ratio (3), atk (5), rel (150), out (-3) } },
            { "Vocals", "Vocal De-Esser",       { chr ("Modern"), in (6), ratio (6), atk (0.3f), rel (60), KEY, c ("keySource", "Freq Key"), freqKey (6500) } },

            // ---------------------------------------------------------------- Drums
            { "Drums", "Punchy Kick",          { chr ("Retro"), in (8), ratio (4), atk (25), rel (90), sat (15), out (-3) } },
            { "Drums", "Snare Crack",          { chr ("Indie"), in (10), ratio (6), atk (8), rel (70), sat (30), out (-4), CLIP } },
            { "Drums", "Smashed Room",         { chr ("Indie"), in (22), ratio (20), atk (0.1f), rel (60), sat (40), out (-10), par (45), CLIP } },
            { "Drums", "Parallel Drum Bus",    { chr ("Retro"), in (18), ratio (12), atk (0.8f), rel (100), sat (25), out (-6), par (35) } },
            { "Drums", "Glue Drum Group",      { chr ("Bus"), in (6), ratio (4), atk (10), rel (100), sat (10) } },
            { "Drums", "Overheads Tame",       { chr ("Modern"), in (6), ratio (2.5f), atk (15), rel (200), out (-2) } },

            // ---------------------------------------------------------------- Bass
            { "Bass", "Steady Bass",           { chr ("Vintage"), in (10), ratio (4), atk (12), rel (300), sat (20), out (-4) } },
            { "Bass", "Fat Bass DI",           { chr ("Chubby"), in (12), ratio (4), atk (15), rel (250), sat (45), out (-6) } },
            { "Bass", "Growl Bass",            { chr ("Retro"), in (14), ratio (8), atk (3), rel (90), sat (55), out (-7), CLIP } },
            { "Bass", "Kick Duck (Key)",       { chr ("Modern"), in (6), ratio (6), atk (1), rel (120), KEY, c ("keySource", "External") } },

            // ---------------------------------------------------------------- Guitars & Keys
            { "Guitars & Keys", "Acoustic Sparkle", { chr ("Vintage"), in (8), ratio (3), atk (15), rel (350), sat (10), out (-3) } },
            { "Guitars & Keys", "Electric Sustain", { chr ("Retro"), in (14), ratio (8), atk (5), rel (400), sat (25), out (-7) } },
            { "Guitars & Keys", "Piano Leveler",    { chr ("Modern"), in (6), ratio (2), atk (20), rel (300), out (-2) } },
            { "Guitars & Keys", "Tame Harsh Guitar", { chr ("Modern"), in (6), ratio (4), atk (2), rel (100), KEY, c ("keySource", "Freq Key"), freqKey (3200) } },

            // ---------------------------------------------------------------- Mix Bus & Mastering
            { "Mix Bus", "Gentle Mix Glue",    { chr ("Bus"), in (4), ratio (2), atk (30), rel (300), sat (5) } },
            { "Mix Bus", "Punchy Mix Bus",     { chr ("Bus"), in (7), ratio (4), atk (10), rel (100), sat (12), out (-1.5f) } },
            { "Mix Bus", "Warm Master",        { chr ("Chubby"), in (5), ratio (1.8f), atk (30), rel (800), sat (15), out (-1.5f) } },
            { "Mix Bus", "Clean Master Level", { chr ("Modern"), in (5), ratio (1.5f), atk (30), rel (500), out (-1), CLIP } },
            { "Mix Bus", "Loud and Proud",     { chr ("Bus"), in (12), ratio (4), atk (3), rel (80), sat (20), out (-4), CLIP } },

            // ---------------------------------------------------------------- Creative
            { "Creative", "Breathing Pump",     { chr ("Bus"), in (16), ratio (10), atk (5), rel (350), out (-6) } },
            { "Creative", "Reverse Squash",     { chr ("Indie"), in (18), ratio (20), atk (0.5f), rel (200), out (-2) } },
            { "Creative", "Lo-Fi Crush",        { chr ("Indie"), in (20), ratio (12), atk (0.1f), rel (40), sat (100), out (-12), CLIP } },
            { "Creative", "Tube Thickener",     { chr ("Chubby"), in (3), ratio (1.5f), sat (70), out (-3) } },
        };
    }
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const auto presets = build();
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
