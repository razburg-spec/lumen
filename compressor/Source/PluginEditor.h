#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/RbaLookAndFeel.h"
#include "gui/ChromaWidgets.h"
#include "gui/ActivationPanel.h"
#include "gui/Toolbar.h"

// The whole interface, laid out at a fixed design size. The editor scales it as a
// unit, so everything stays vector-sharp at any window size.
class ChromaView : public juce::Component,
                   private juce::ChangeListener
{
public:
    static constexpr int designWidth = 1040, designHeight = 700;

    explicit ChromaView (ChromaProcessor&);
    ~ChromaView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void poll();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showActivation (bool show);
    void showManual (bool show);
    void updateHistoryButtons();
    void applyCharacterColour (int character);
    PillToggle& addPill (const char* paramID, const juce::String& text, juce::Colour accent);

    ChromaProcessor& processor;
    APVTS& state;

    // Cards (painted by the view)
    juce::Rectangle<int> inputCard, centreCard, outputCard, dynamicsCard, colourCard, keyCard;

    CharacterDial dial;
    GrMeter grMeter;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };

    Knob input, output, attack, release, ratio, saturation, parallel, keyFreq;
    PowerButton keyPower { Theme::key };
    APVTS::ButtonAttachment keyPowerAttachment;
    Segmented keySource;
    juce::OwnedArray<PillToggle> pills;
    juce::OwnedArray<APVTS::ButtonAttachment> pillAttachments;
    PillToggle* softClip = nullptr;
    PillToggle* listen = nullptr;
    int lastKeyState = -1;

    LicenseManager& licence;
    LicenceBadge licenceBadge;
    std::unique_ptr<ActivationPanel> activation;

    PresetBar presets;
    ToolButton undoButton { "Undo", ToolButton::Glyph::undo }, redoButton { "Redo", ToolButton::Glyph::redo },
               resetButton { "Reset" }, helpButton { "User guide", ToolButton::Glyph::help };
    std::unique_ptr<ManualPanel> manual;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaView)
};

class ChromaEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit ChromaEditor (ChromaProcessor&);
    ~ChromaEditor() override;

    void resized() override;

private:
    void timerCallback() override { view.poll(); }

    RbaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    ChromaView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaEditor)
};
