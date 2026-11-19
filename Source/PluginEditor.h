#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/LumenLookAndFeel.h"
#include "gui/Display.h"
#include "gui/Sections.h"
#include <rba/ActivationPanel.h>
#include "gui/Toolbar.h"
#include <rba/UpdateBanner.h>

// The whole interface, laid out at a fixed design size. The editor scales it as a
// unit, so everything stays vector-sharp at any window size.
class LumenView : public juce::Component,
                  private juce::ChangeListener
{
public:
    static constexpr int designWidth = 1100, designHeight = 764;

    explicit LumenView (LumenProcessor&);
    ~LumenView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void poll();

private:
    void setAdvanced (bool);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showActivation (bool show);
    void showManual (bool show);
    void updateHistoryButtons();

    LumenProcessor& processor;
    Display display;
    DelaySection delay;
    ShimmerSection shimmer;
    ReverbSection reverb;
    MasterSection master;
    Segmented routing;
    PillToggle advancedToggle { "Advanced", Theme::brandBlue };
    bool advanced = false;

    rba::Activation& licence;
    rba::ActivationBadge licenceBadge;
    std::unique_ptr<rba::ActivationPanel> activation;

    PresetBar presets;
    ToolButton undoButton { "Undo", ToolButton::Glyph::undo }, redoButton { "Redo", ToolButton::Glyph::redo },
               resetButton { "Reset" }, helpButton { "User guide", ToolButton::Glyph::help };
    std::unique_ptr<ManualPanel> manual;

    // Small "new version available" pill in the footer (hidden unless there is one).
    rba::UpdateBanner update { "Lumen", "lumen", JucePlugin_VersionString,
                               { Theme::surface, Theme::line, Theme::text, Theme::muted, Theme::brandBlue } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenView)
};

class LumenEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit LumenEditor (LumenProcessor&);
    ~LumenEditor() override;

    void resized() override;

private:
    void timerCallback() override { view.poll(); }

    LumenLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    LumenView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenEditor)
};
