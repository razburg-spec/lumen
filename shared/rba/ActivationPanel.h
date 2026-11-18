#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Activation.h"

// The activation UI shared by every Raz Burg AudioCrafts plugin: a full-window overlay
// (ActivationPanel) and a small header badge (ActivationBadge). Give both the plugin's colours.
namespace rba
{
struct ActivationStyle
{
    juce::Colour background { 0xff11161d }, surface { 0xff19212b }, raised { 0xff202a36 }, line { 0xff2b3542 },
                 text { 0xfff2f5f9 }, muted { 0xff97a3b2 }, faint { 0xff5a6676 },
                 accent { 0xff6f9cff }, accentTop { 0xff1a4fc4 }, accentBottom { 0xff11287a },
                 highlight { 0xff8ddcff }, warning { 0xffffb45e };
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> drawLogo;   // optional brand mark
};

// Rounded action button: filled in the accent colour (primary) or outlined.
class ActivationButton : public juce::Button
{
public:
    ActivationButton (const ActivationStyle& s, bool isPrimary) : juce::Button ({}), style (s), primary (isPrimary) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    const ActivationStyle& style;
    bool primary;
};

class ActivationBadge : public juce::Button,
                        private juce::ChangeListener
{
public:
    ActivationBadge (Activation&, ActivationStyle);
    ~ActivationBadge() override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();

    Activation& activation;
    const ActivationStyle style;
    juce::String text;
    juce::Colour colour;
};

class ActivationPanel : public juce::Component,
                        private juce::ChangeListener
{
public:
    ActivationPanel (Activation&, ActivationStyle);
    ~ActivationPanel() override;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}   // swallow clicks on the backdrop

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void primaryClicked();
    void secondaryClicked();
    void showMessage (const juce::String&, bool isError);

    Activation& activation;
    const ActivationStyle style;
    Activation::Status status;
    juce::Rectangle<int> card;
    juce::String heading, body, message;
    bool messageIsError = false, busy = false;

    ActivationButton primary { style, true }, secondary { style, false };
    juce::HyperlinkButton accountLink;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationPanel)
};
} // namespace rba
