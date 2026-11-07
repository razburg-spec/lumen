#pragma once
#include "Widgets.h"

// One engine column: power switch and title, mode selectors, four main knobs,
// and an Advanced block that only appears when Advanced is switched on.
class EngineSection : public juce::Component
{
public:
    EngineSection (APVTS&, const juce::String& title, const juce::String& subtitle, juce::Colour accent, const char* onParamID);

    void setAdvanced (bool);
    virtual void poll();

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    Knob& addMain (const char* paramID, const juce::String& name, bool bipolar = false);
    Knob& addAdvanced (const char* paramID, const juce::String& name, bool bipolar = false);
    void addCombo (const char* paramID);
    void addPill (const char* paramID, const juce::String& text);

    APVTS& state;
    juce::Colour accent;
    juce::OwnedArray<Knob> mainKnobs, advancedKnobs;
    juce::OwnedArray<juce::ComboBox> combos;

private:
    juce::String title, subtitle;
    bool advanced = false;
    int wasOn = -1;
    std::atomic<float>* onParam = nullptr;
    juce::Rectangle<int> titleArea;
    int dividerY = -1;

    PowerButton power;
    APVTS::ButtonAttachment powerAttachment;
    juce::OwnedArray<APVTS::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<PillToggle> pills;
    juce::OwnedArray<APVTS::ButtonAttachment> pillAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineSection)
};

class DelaySection : public EngineSection
{
public:
    explicit DelaySection (APVTS&);
    void poll() override;
    void resized() override;

private:
    Knob& time;
    Knob division;
};

class ShimmerSection : public EngineSection
{
public:
    explicit ShimmerSection (APVTS&);
};

class ReverbSection : public EngineSection
{
public:
    explicit ReverbSection (APVTS&);
};

// Global output column: Mix, Cross-Feed and Output.
class MasterSection : public juce::Component
{
public:
    explicit MasterSection (APVTS&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Knob mix, crossFeed, coloration, output;
};
