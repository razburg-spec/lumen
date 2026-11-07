#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Factory presets. Each preset starts from the plugin's defaults (fully dry, everything off)
// and changes only the settings listed. Choice parameters are given by their label, so presets
// keep working if a choice list grows.
struct PresetSetting
{
    const char* id;
    float value = 0.0f;
    const char* choice = nullptr;   // label for choice parameters (overrides value)
};

struct FactoryPreset
{
    const char* category;
    const char* name;
    std::vector<PresetSetting> settings;
};

const std::vector<FactoryPreset>& getFactoryPresets();
juce::StringArray getPresetCategories();

// Resets every parameter to its default, then applies the preset. Call on the message thread.
void applyFactoryPreset (juce::AudioProcessorValueTreeState&, const FactoryPreset&);
void resetToDefaults (juce::AudioProcessorValueTreeState&);
