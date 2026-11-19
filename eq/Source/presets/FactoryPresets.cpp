#include "FactoryPresets.h"
#include <functional>
#include <memory>
#include <string>

namespace
{
    PresetSetting v (const char* id, float value)        { return { id, value, nullptr }; }
    PresetSetting c (const char* id, const char* label)  { return { id, 0.0f, label }; }

    // Band helpers. Each returns the settings for one band.
    struct B { std::vector<PresetSetting> s; };
    std::string key (int band, const char* name) { return "b" + std::to_string (band) + "_" + name; }

    // The IDs must outlive the presets, so they are interned here.
    const char* intern (const std::string& s)
    {
        static std::vector<std::unique_ptr<std::string>> pool;
        for (auto& p : pool) if (*p == s) return p->c_str();
        pool.push_back (std::make_unique<std::string> (s));
        return pool.back()->c_str();
    }
    PresetSetting bp (int band, const char* name, float value) { return v (intern (key (band, name)), value); }
    PresetSetting bc (int band, const char* name, const char* label) { return c (intern (key (band, name)), label); }

    void bell (std::vector<PresetSetting>& out, int band, float hz, float db, float q)
    { out.push_back (bp (band, "freq", hz)); out.push_back (bp (band, "gain", db)); out.push_back (bp (band, "q", q)); }
    void lowCut (std::vector<PresetSetting>& out, float hz, const char* slope)
    { out.push_back (bp (0, "on", 1)); out.push_back (bc (0, "type", "Low Cut")); out.push_back (bp (0, "freq", hz)); out.push_back (bc (0, "slope", slope)); }
    void lowShelf (std::vector<PresetSetting>& out, float hz, float db)
    { out.push_back (bp (0, "on", 1)); out.push_back (bc (0, "type", "Low Shelf")); out.push_back (bp (0, "freq", hz)); out.push_back (bp (0, "gain", db)); }
    void highCut (std::vector<PresetSetting>& out, float hz, const char* slope)
    { out.push_back (bp (5, "on", 1)); out.push_back (bc (5, "type", "High Cut")); out.push_back (bp (5, "freq", hz)); out.push_back (bc (5, "slope", slope)); }
    void highShelf (std::vector<PresetSetting>& out, float hz, float db)
    { out.push_back (bp (5, "on", 1)); out.push_back (bc (5, "type", "High Shelf")); out.push_back (bp (5, "freq", hz)); out.push_back (bp (5, "gain", db)); }

    FactoryPreset make (const char* cat, const char* name, const char* mode, std::function<void (std::vector<PresetSetting>&)> fill)
    {
        std::vector<PresetSetting> s { c ("mode", mode) };
        fill (s);
        return { cat, name, s };
    }

    std::vector<FactoryPreset> build()
    {
        using S = std::vector<PresetSetting>;
        return {
            make ("Vocals", "Vocal Presence", "Modern", [] (S& s) { lowCut (s, 90, "24 dB"); bell (s, 2, 300, -2.5f, 1.2f); bell (s, 3, 3500, 2.5f, 0.9f); highShelf (s, 10000, 3); }),
            make ("Vocals", "Warm Vintage Vocal", "Vintage", [] (S& s) { lowCut (s, 80, "12 dB"); bell (s, 1, 220, 1.5f, 0.8f); bell (s, 3, 5000, 3, 0.7f); highShelf (s, 12000, 2.5f); s.push_back (v ("drive", 25)); }),
            make ("Vocals", "Thick Ballad", "Chubby", [] (S& s) { lowCut (s, 70, "12 dB"); bell (s, 1, 180, 2, 0.8f); bell (s, 3, 3200, 2, 0.9f); s.push_back (v ("drive", 30)); }),
            make ("Vocals", "Airy Pop Vocal", "Bus", [] (S& s) { lowCut (s, 100, "24 dB"); bell (s, 2, 450, -2, 1.5f); highShelf (s, 14000, 4); }),

            make ("Drums", "Kick Punch", "Indie", [] (S& s) { lowShelf (s, 60, 3); bell (s, 2, 350, -4, 1.4f); bell (s, 3, 3500, 3, 1.2f); s.push_back (v ("drive", 20)); }),
            make ("Drums", "Snare Crack", "Indie", [] (S& s) { lowCut (s, 80, "24 dB"); bell (s, 1, 200, 2.5f, 1.2f); bell (s, 3, 5000, 3.5f, 1.0f); s.push_back (v ("drive", 30)); }),
            make ("Drums", "Overheads Shine", "Modern", [] (S& s) { lowCut (s, 150, "24 dB"); bell (s, 2, 600, -2, 1.0f); highShelf (s, 9000, 3); }),
            make ("Drums", "Retro Drum Bus", "Retro", [] (S& s) { lowShelf (s, 80, 2); bell (s, 2, 500, -1.5f, 0.9f); bell (s, 4, 7000, 2, 0.8f); s.push_back (v ("drive", 35)); }),

            make ("Bass", "Round Bass", "Chubby", [] (S& s) { lowCut (s, 35, "24 dB"); lowShelf (s, 80, 0); bell (s, 1, 100, 3, 0.9f); bell (s, 2, 700, 2, 1.2f); s.push_back (v ("drive", 35)); }),
            make ("Bass", "Low Trick", "Vintage", [] (S& s) { lowShelf (s, 60, 5); bell (s, 3, 2200, 1.5f, 0.8f); s.push_back (v ("drive", 20)); }),
            make ("Bass", "Tight Bass", "Indie", [] (S& s) { lowCut (s, 30, "36 dB"); bell (s, 1, 250, -3, 1.4f); bell (s, 3, 1200, 2.5f, 1.2f); }),

            make ("Guitars & Keys", "Acoustic Sparkle", "Modern", [] (S& s) { lowCut (s, 90, "24 dB"); bell (s, 2, 250, -2.5f, 1.2f); highShelf (s, 10000, 3); }),
            make ("Guitars & Keys", "Electric Bite", "Retro", [] (S& s) { lowCut (s, 100, "24 dB"); bell (s, 2, 800, 2, 1.0f); bell (s, 3, 2800, 3, 1.0f); highCut (s, 11000, "12 dB"); }),
            make ("Guitars & Keys", "Piano Clarity", "Bus", [] (S& s) { lowCut (s, 40, "12 dB"); bell (s, 1, 300, -2, 0.9f); highShelf (s, 8000, 2); }),
            make ("Guitars & Keys", "Synth Pad Air", "Modern", [] (S& s) { lowCut (s, 150, "24 dB"); highShelf (s, 12000, 4); }),

            make ("Mix Bus", "Gentle Smile", "Bus", [] (S& s) { lowShelf (s, 80, 1.5f); highShelf (s, 12000, 1.5f); }),
            make ("Mix Bus", "Warm Master", "Vintage", [] (S& s) { lowShelf (s, 60, 2); bell (s, 3, 3000, -0.8f, 0.8f); highShelf (s, 14000, 1.5f); s.push_back (v ("drive", 15)); }),
            make ("Mix Bus", "Clean Master Lift", "Modern", [] (S& s) { lowCut (s, 25, "24 dB"); highShelf (s, 16000, 2); }),
            make ("Mix Bus", "Analog Glue", "Bus", [] (S& s) { lowCut (s, 25, "12 dB"); bell (s, 2, 500, -1, 0.7f); highShelf (s, 10000, 1); s.push_back (v ("drive", 30)); }),

            make ("Creative", "Telephone", "Indie", [] (S& s) { lowCut (s, 400, "48 dB"); highCut (s, 3200, "48 dB"); bell (s, 2, 1500, 6, 1.2f); s.push_back (v ("drive", 60)); }),
            make ("Creative", "Lo-Fi Radio", "Chubby", [] (S& s) { lowCut (s, 250, "24 dB"); highCut (s, 5000, "24 dB"); bell (s, 2, 1000, 4, 0.8f); s.push_back (v ("drive", 70)); }),
            make ("Creative", "Dark Room", "Vintage", [] (S& s) { highCut (s, 4000, "12 dB"); lowShelf (s, 120, 3); s.push_back (v ("drive", 40)); }),
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
