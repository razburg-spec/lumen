#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/LumenLookAndFeel.h"
#include "gui/Display.h"
#include "gui/Sections.h"
#include "gui/ActivationPanel.h"

// The whole interface, laid out at a fixed design size. The editor scales it as a
// unit, so everything stays vector-sharp at any window size.
class LumenView : public juce::Component,
                  private juce::ChangeListener
{
public:
    static constexpr int designWidth = 1100, designHeight = 720;

    explicit LumenView (LumenProcessor&);
    ~LumenView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void poll();

private:
    void setAdvanced (bool);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showActivation (bool show);

    LumenProcessor& processor;
    Display display;
    DelaySection delay;
    ShimmerSection shimmer;
    ReverbSection reverb;
    MasterSection master;
    Segmented routing;
    PillToggle advancedToggle { "Advanced", Theme::brandBlue };
    bool advanced = false;

    LicenseManager& licence;
    LicenceBadge licenceBadge;
    std::unique_ptr<ActivationPanel> activation;

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
