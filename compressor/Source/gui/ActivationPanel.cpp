#include "ActivationPanel.h"
#include "../licensing/LicenseConfig.h"

//==============================================================================
void ActionButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    const auto alpha = isEnabled() ? 1.0f : 0.45f;

    if (primary)
    {
        auto top = Theme::brandBlue.brighter (highlighted ? 0.15f : 0.0f);
        g.setGradientFill (juce::ColourGradient (top.withAlpha (alpha), 0.0f, r.getY(),
                                                 Theme::brandTop.withAlpha (alpha), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.15f));
            g.fillRoundedRectangle (r, corner);
        }
        g.setColour (Theme::text.withAlpha (alpha));
    }
    else
    {
        g.setColour (Theme::raised.withAlpha (alpha));
        g.fillRoundedRectangle (r, corner);
        g.setColour ((highlighted ? Theme::faint : Theme::line).withAlpha (alpha));
        g.drawRoundedRectangle (r, corner, 1.0f);
        g.setColour (Theme::text.withAlpha (alpha * 0.9f));
    }

    g.setFont (Theme::font (14.0f, true));
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}

//==============================================================================
void LicenceBadge::setStatus (const LicenseManager::Status& s)
{
    using State = LicenseManager::State;
    switch (s.state)
    {
        case State::licensed:     text = "LICENSED"; colour = Theme::muted; break;
        case State::trial:        text = juce::String::fromUTF8 ("TRIAL \xc2\xb7 ") + juce::String (s.trialDaysLeft) + (s.trialDaysLeft == 1 ? " DAY" : " DAYS"); colour = Theme::characterColour (0); break;
        case State::verifyNeeded: text = "RECONNECT"; colour = Theme::characterColour (0); break;
        case State::trialOver:
        case State::none:         text = "ACTIVATE"; colour = Theme::brandBlue; break;
    }
    repaint();
}

void LicenceBadge::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    g.setColour (colour.withAlpha (highlighted ? 0.22f : 0.12f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (colour.withAlpha (0.6f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (colour);
    g.setFont (Theme::caps (10.5f));
    g.drawText (text, r, juce::Justification::centred, false);
}

//==============================================================================
ActivationPanel::ActivationPanel (LicenseManager& manager) : licence (manager)
{
    keyField.setTextToShowWhenEmpty ("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX", Theme::faint);
    keyField.setFont (Theme::font (15.0f));
    keyField.setJustification (juce::Justification::centred);
    keyField.setColour (juce::TextEditor::backgroundColourId, Theme::bg);
    keyField.setColour (juce::TextEditor::outlineColourId, Theme::line);
    keyField.setColour (juce::TextEditor::focusedOutlineColourId, Theme::brandBlue);
    keyField.setColour (juce::TextEditor::textColourId, Theme::text);
    keyField.setColour (juce::TextEditor::highlightColourId, Theme::brandBlue.withAlpha (0.35f));
    keyField.setColour (juce::CaretComponent::caretColourId, Theme::brandCyan);
    keyField.setIndents (12, 11);
    keyField.onReturnKey = [this] { primaryClicked(); };
    addAndMakeVisible (keyField);

    primary.onClick = [this] { primaryClicked(); };
    secondary.onClick = [this] { secondaryClicked(); };
    addAndMakeVisible (primary);
    addAndMakeVisible (secondary);

    buyLink.setButtonText ("Buy " JucePlugin_Name " at audiocrafts.razburg.com");
    buyLink.setURL (juce::URL (LicenseConfig::buyUrl));
    buyLink.setFont (Theme::font (13.0f), false, juce::Justification::centred);
    buyLink.setColour (juce::HyperlinkButton::textColourId, Theme::brandCyan);
    addAndMakeVisible (buyLink);

    licence.addChangeListener (this);
    refresh();
}

ActivationPanel::~ActivationPanel()
{
    licence.removeChangeListener (this);
}

void ActivationPanel::refresh()
{
    using State = LicenseManager::State;
    status = licence.getStatus();
    const auto days = juce::String (status.trialDaysLeft);

    switch (status.state)
    {
        case State::none:
            heading = "Welcome to " JucePlugin_Name;
            body = LicenseConfig::trialDays > 0
                       ? "Enter the licence key from your purchase email, or try every feature free for "
                             + juce::String (LicenseConfig::trialDays) + " days."
                       : juce::String ("Enter the licence key from your purchase email to activate " JucePlugin_Name " on this computer.");
            primary.setButtonText ("Activate");
            secondary.setButtonText ("Start " + juce::String (LicenseConfig::trialDays) + "-day trial");
            break;
        case State::trial:
            heading = "Trial: " + days + (status.trialDaysLeft == 1 ? " day left" : " days left");
            body = "Everything works during the trial. Enter your licence key to keep " JucePlugin_Name " after it ends.";
            primary.setButtonText ("Activate");
            secondary.setButtonText ("Continue trial");
            break;
        case State::trialOver:
            heading = "Your trial has ended";
            body = "Activate " JucePlugin_Name " to keep using it. Until then the effect drops out every 40 seconds.";
            primary.setButtonText ("Activate");
            break;
        case State::verifyNeeded:
            heading = "Please reconnect";
            body = JucePlugin_Name " checks your licence online every few weeks. Connect to the internet and check now.";
            primary.setButtonText ("Check now");
            break;
        case State::licensed:
            heading = JucePlugin_Name " is activated";
            body = (status.customerName.isNotEmpty() ? status.customerName + "\n" : juce::String())
                 + (status.customerEmail.isNotEmpty() ? status.customerEmail + "\n" : juce::String())
                 + "Key " + status.maskedKey + juce::String::fromUTF8 ("  \xc2\xb7  one computer per licence");
            primary.setButtonText ("Deactivate this computer");
            secondary.setButtonText ("Close");
            break;
    }

    const bool wantsKey = status.state == State::none || status.state == State::trial || status.state == State::trialOver;
    keyField.setVisible (wantsKey);
    secondary.setVisible ((status.state == State::none && LicenseConfig::trialDays > 0)
                          || status.state == State::trial || status.state == State::licensed);
    buyLink.setVisible (status.state != State::licensed);
    resized();
    repaint();
}

void ActivationPanel::setBusy (bool isBusy, const juce::String& text)
{
    busy = isBusy;
    primary.setEnabled (! busy);
    secondary.setEnabled (! busy);
    keyField.setEnabled (! busy);
    showMessage (text, false);
}

void ActivationPanel::showMessage (const juce::String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    repaint();
}

void ActivationPanel::primaryClicked()
{
    using State = LicenseManager::State;
    if (busy)
        return;

    const auto done = [safe = juce::Component::SafePointer<ActivationPanel> (this)] (bool ok, const juce::String& text)
    {
        if (safe == nullptr)
            return;
        safe->setBusy (false);
        safe->refresh();
        if (! ok)
            safe->showMessage (text.isNotEmpty() ? text : juce::String ("Something went wrong. Please try again."), true);
        else if (safe->status.state == LicenseManager::State::licensed && safe->onClose)
            safe->onClose();
    };

    if (status.state == State::licensed)
    {
        setBusy (true, "Deactivating...");
        licence.deactivate (done);
    }
    else if (status.state == State::verifyNeeded)
    {
        setBusy (true, "Checking...");
        licence.verifyNow (done);
    }
    else
    {
        if (keyField.getText().trim().isEmpty())
        {
            showMessage ("Paste your licence key first.", true);
            keyField.grabKeyboardFocus();
            return;
        }
        setBusy (true, "Activating...");
        licence.activate (keyField.getText(), done);
    }
}

void ActivationPanel::secondaryClicked()
{
    if (status.state == LicenseManager::State::none)
        licence.startTrial();
    if (onClose)
        onClose();
}

void ActivationPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg.withAlpha (0.82f));

    const auto c = card.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (c.translated (0.0f, 8.0f).expanded (4.0f), 20.0f);
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (c, 16.0f);
    g.setGradientFill (juce::ColourGradient (Theme::brandTop.withAlpha (0.22f), c.getCentreX(), c.getY(),
                                             Theme::surface.withAlpha (0.0f), c.getCentreX(), c.getY() + 160.0f, false));
    g.fillRoundedRectangle (c, 16.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (c.reduced (0.5f), 16.0f, 1.0f);

    auto r = card.reduced (36, 30).toFloat();
    auto brand = r.removeFromTop (40.0f);
    drawBrandMark (g, brand.removeFromLeft (40.0f));
    brand.removeFromLeft (12.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (18.0f).withExtraKerningFactor (0.3f));
    g.drawText ("LUMEN", brand.removeFromTop (22.0f), juce::Justification::centredLeft, false);
    g.setColour (Theme::brandCyan.withAlpha (0.85f));
    g.setFont (Theme::caps (9.0f, 0.22f));
    g.drawText ("RAZ BURG AUDIOCRAFTS", brand, juce::Justification::centredLeft, false);

    r.removeFromTop (22.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (23.0f, true));
    g.drawText (heading, r.removeFromTop (30.0f), juce::Justification::centredLeft, false);
    r.removeFromTop (6.0f);
    g.setColour (Theme::muted);
    g.setFont (Theme::font (14.5f));
    g.drawFittedText (body, r.removeFromTop (62.0f).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);

    if (message.isNotEmpty())
    {
        g.setColour (messageIsError ? juce::Colour (0xffff8a8a) : Theme::muted);
        g.setFont (Theme::font (13.0f));
        g.drawText (message, juce::Rectangle<float> ((float) card.getX(), (float) primary.getBottom() + 10.0f,
                                                     (float) card.getWidth(), 18.0f),
                    juce::Justification::centred, false);
    }
}

void ActivationPanel::resized()
{
    card = juce::Rectangle<int> (540, 400).withCentre (getLocalBounds().getCentre());
    auto r = card.reduced (36, 30);
    r.removeFromTop (40 + 22 + 30 + 6 + 62 + 10);

    if (keyField.isVisible())
    {
        keyField.setBounds (r.removeFromTop (42));
        r.removeFromTop (14);
    }

    auto buttons = r.removeFromTop (44);
    if (secondary.isVisible())
    {
        const auto w = (buttons.getWidth() - 12) / 2;
        primary.setBounds (buttons.removeFromLeft (w));
        buttons.removeFromLeft (12);
        secondary.setBounds (buttons);
    }
    else
    {
        primary.setBounds (buttons);
    }

    buyLink.setBounds (card.getX(), card.getBottom() - 46, card.getWidth(), 24);
}
