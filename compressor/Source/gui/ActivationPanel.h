#pragma once
#include "Widgets.h"
#include "../licensing/LicenseManager.h"

// Rounded action button: filled in the accent colour (primary) or outlined.
class ActionButton : public juce::Button
{
public:
    ActionButton (const juce::String& text, bool isPrimary) : juce::Button (text), primary (isPrimary) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool primary;
};

// Header badge that shows the licence state and opens the activation panel.
class LicenceBadge : public juce::Button
{
public:
    LicenceBadge() : juce::Button ("licence") {}
    void setStatus (const LicenseManager::Status&);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String text;
    juce::Colour colour = Theme::brandBlue;
};

// Full-window overlay: activate a key, start or continue the trial, or manage
// an activated computer. The editor shows it by itself whenever the plugin is not
// free to run.
class ActivationPanel : public juce::Component,
                        private juce::ChangeListener
{
public:
    explicit ActivationPanel (LicenseManager&);
    ~ActivationPanel() override;

    std::function<void()> onClose;
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}   // swallow clicks on the backdrop

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void setBusy (bool, const juce::String& message = {});
    void showMessage (const juce::String&, bool isError);
    void primaryClicked();
    void secondaryClicked();

    LicenseManager& licence;
    LicenseManager::Status status;
    juce::Rectangle<int> card;
    juce::String heading, body, message;
    bool messageIsError = false, busy = false;

    juce::TextEditor keyField;
    ActionButton primary { "Activate", true }, secondary { "Start 14-day trial", false };
    juce::HyperlinkButton buyLink;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationPanel)
};
