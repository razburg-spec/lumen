#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/RbaLookAndFeel.h"
#include "gui/PrismWidgets.h"
#include "gui/Toolbar.h"
#include <rba/UpdateBanner.h>
#include <rba/ActivationPanel.h>

// The whole interface, laid out at a fixed design size. The editor scales it as a
// unit, so everything stays vector-sharp at any window size.
class PrismView : public juce::Component,
                  private juce::ChangeListener
{
public:
    static constexpr int designWidth = 1120, designHeight = 790;

    explicit PrismView (PrismProcessor&);
    ~PrismView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void poll();
    int frame = 0;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showManual (bool show);
    void showActivation (bool show);
    void updateHistoryButtons();
    void applyModeColour (int mode);
    void selectBand (int band);

    PrismProcessor& processor;

    ModeSelector modes;
    EqDisplay display;
    juce::OwnedArray<BandStrip> strips;
    juce::Rectangle<int> outputCard;
    Knob drive, output;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    PillToggle linearPhase { "Linear Phase", Theme::modeColour (4) };
    APVTS::ButtonAttachment linearAttachment;

    PresetBar presets;
    ToolButton undoButton { "Undo", ToolButton::Glyph::undo }, redoButton { "Redo", ToolButton::Glyph::redo },
               resetButton { "Reset" }, helpButton { "User guide", ToolButton::Glyph::help };
    std::unique_ptr<ManualPanel> manual;

    rba::Activation& licence;
    rba::ActivationBadge licenceBadge;
    std::unique_ptr<rba::ActivationPanel> activation;

    // Small "new version available" pill in the footer (hidden unless there is one).
    rba::UpdateBanner update { "Prism", "prism", JucePlugin_VersionString,
                               { Theme::surface, Theme::line, Theme::text, Theme::muted, Theme::brandBlue } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismView)
};

class PrismEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit PrismEditor (PrismProcessor&);
    ~PrismEditor() override;

    void resized() override;

private:
    void timerCallback() override { view.poll(); }

    RbaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    PrismView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismEditor)
};
