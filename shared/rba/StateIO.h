#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>

// Backward-compatible save/load of plugin state, shared by every Raz Burg AudioCrafts plugin.
//
// Rule (Raz, 2026-11-18): a session saved with ANY older version must open exactly the
// same in every newer version. So:
//   - Parameter IDs are forever. Never rename or reuse one; retire it instead.
//   - Every save stamps the state with a stateVersion (an integer per plugin, bumped
//     whenever a release changes what a parameter means, its range or its ID).
//   - On load, migration steps run in order from the saved version up to the current one,
//     translating old values into new parameters.
//   - Parameters the saved state does not know about are set to their defaults, explicitly
//     (not left to the JUCE version in use).
//   - Unknown parameters (from a newer version, or retired) are ignored, so loading never fails.
//   - A golden state from every released version lives in test/state-fixtures and the
//     StateCompatTest checks it still loads correctly (see shared/rba/README.md).
namespace rba::state
{
inline const juce::Identifier versionProperty { "rbaStateVersion" };
inline const juce::Identifier pluginVersionProperty { "rbaPluginVersion" };
inline const juce::Identifier paramTag { "PARAM" }, idAttr { "id" }, valueAttr { "value" };

// A step that turns a state of version (toVersion - 1) into toVersion. Work on plain
// (real-unit) values with the helpers below.
struct Migration
{
    int toVersion;
    std::function<void (juce::ValueTree& state)> apply;
};

//------------------------------------------------------------------------------
// Helpers for migrations

inline juce::ValueTree findParam (const juce::ValueTree& state, const juce::String& id)
{
    return state.getChildWithProperty (idAttr, id);
}

inline bool hasParam (const juce::ValueTree& state, const juce::String& id)
{
    return findParam (state, id).isValid();
}

inline float getParam (const juce::ValueTree& state, const juce::String& id, float fallback)
{
    const auto p = findParam (state, id);
    return p.isValid() ? (float) p.getProperty (valueAttr, fallback) : fallback;
}

inline void setParam (juce::ValueTree& state, const juce::String& id, float value)
{
    auto p = findParam (state, id);
    if (! p.isValid())
    {
        p = juce::ValueTree (paramTag);
        p.setProperty (idAttr, id, nullptr);
        state.appendChild (p, nullptr);
    }
    p.setProperty (valueAttr, value, nullptr);
}

inline void removeParam (juce::ValueTree& state, const juce::String& id)
{
    const auto p = findParam (state, id);
    if (p.isValid())
        state.removeChild (p, nullptr);
}

inline void renameParam (juce::ValueTree& state, const juce::String& oldId, const juce::String& newId)
{
    auto p = findParam (state, oldId);
    if (p.isValid() && ! hasParam (state, newId))
        p.setProperty (idAttr, newId, nullptr);
}

//------------------------------------------------------------------------------

// The state as saved by a version without StateIO counts as version 1.
inline int versionOf (const juce::ValueTree& state)
{
    return (int) state.getProperty (versionProperty, 1);
}

// Runs the migrations and fills in parameters the saved state lacks. Exposed for tests.
inline juce::ValueTree upgrade (juce::ValueTree state, juce::AudioProcessorValueTreeState& apvts,
                                int currentVersion, const std::vector<Migration>& migrations)
{
    state = state.createCopy();
    const auto saved = versionOf (state);

    for (int v = saved + 1; v <= currentVersion; ++v)
        for (auto& m : migrations)
            if (m.toVersion == v && m.apply)
                m.apply (state);

    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! hasParam (state, ranged->getParameterID()))
                setParam (state, ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getDefaultValue()));

    state.setProperty (versionProperty, juce::jmax (saved, currentVersion), nullptr);
    return state;
}

inline void save (juce::AudioProcessorValueTreeState& apvts, int currentVersion, juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty (versionProperty, currentVersion, nullptr);
    state.setProperty (pluginVersionProperty, JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

inline bool load (juce::AudioProcessorValueTreeState& apvts, const void* data, int size,
                  int currentVersion, const std::vector<Migration>& migrations)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    apvts.replaceState (upgrade (juce::ValueTree::fromXml (*xml), apvts, currentVersion, migrations));
    return true;
}
} // namespace rba::state
