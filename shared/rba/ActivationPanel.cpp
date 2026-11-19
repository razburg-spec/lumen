#include "ActivationPanel.h"

namespace rba
{
namespace
{
    juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    const juce::String name (JucePlugin_Name);
}

//==============================================================================
void ActivationButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    const auto alpha = isEnabled() ? 1.0f : 0.45f;

    if (primary)
    {
        const auto top = style.accentTop.brighter (highlighted ? 0.25f : 0.1f);
        g.setGradientFill (juce::ColourGradient (top.withAlpha (alpha), 0.0f, r.getY(),
                                                 style.accentBottom.withAlpha (alpha), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.15f));
            g.fillRoundedRectangle (r, corner);
        }
        g.setColour (style.text.withAlpha (alpha));
    }
    else
    {
        g.setColour (style.raised.withAlpha (alpha));
        g.fillRoundedRectangle (r, corner);
        g.setColour ((highlighted ? style.faint : style.line).withAlpha (alpha));
        g.drawRoundedRectangle (r, corner, 1.0f);
        g.setColour (style.text.withAlpha (alpha * 0.9f));
    }

    g.setFont (font (14.0f, true));
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}

//==============================================================================
ActivationBadge::ActivationBadge (Activation& a, ActivationStyle s)
    : juce::Button ("activation"), activation (a), style (std::move (s))
{
    setTooltip ("Activation");
    activation.addChangeListener (this);
    refresh();
}

ActivationBadge::~ActivationBadge()
{
    activation.removeChangeListener (this);
}

void ActivationBadge::refresh()
{
    switch (activation.getStatus().state)
    {
        case Activation::State::activated:    text = "ACTIVATED"; colour = style.muted; break;
        case Activation::State::waiting:      text = "WAITING";   colour = style.warning; break;
        case Activation::State::verifyNeeded: text = "RECONNECT"; colour = style.warning; break;
        case Activation::State::notActivated: text = "ACTIVATE";  colour = style.accent; break;
    }
    repaint();
}

void ActivationBadge::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    g.setColour (colour.withAlpha (highlighted ? 0.22f : 0.12f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (colour.withAlpha (0.6f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (colour);
    g.setFont (font (10.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (text, r, juce::Justification::centred, false);
}

//==============================================================================
ActivationPanel::ActivationPanel (Activation& a, ActivationStyle s) : activation (a), style (std::move (s))
{
    primary.onClick = [this] { primaryClicked(); };
    secondary.onClick = [this] { secondaryClicked(); };
    addAndMakeVisible (primary);
    addAndMakeVisible (secondary);

    accountLink.setButtonText ("Manage your computers in My plugins at audiocrafts.razburg.com");
    accountLink.onClick = [] { Activation::openAccountPage(); };
    accountLink.setFont (font (13.0f), false, juce::Justification::centred);
    accountLink.setColour (juce::HyperlinkButton::textColourId, style.highlight);
    addAndMakeVisible (accountLink);

    activation.addChangeListener (this);
    refresh();
}

ActivationPanel::~ActivationPanel()
{
    activation.removeChangeListener (this);
}

void ActivationPanel::refresh()
{
    using State = Activation::State;
    const auto previous = status.state;
    status = activation.getStatus();

    switch (status.state)
    {
        case State::notActivated:
            heading = "Activate " + name;
            body = name + " is free. Activate it once with your free AudioCrafts account: your browser opens, "
                   "you sign in and approve this computer. Up to " + juce::String (Activation::maxComputers) + " computers per account.";
            primary.setButtonText ("Activate in browser");
            secondary.setButtonText ("Later");
            break;
        case State::waiting:
            heading = "Finish in your browser";
            body = "Sign in at audiocrafts.razburg.com and click Activate there. " + name
                 + " unlocks by itself a few seconds later. You can keep this window open.";
            primary.setButtonText ("Open browser again");
            secondary.setButtonText ("Cancel");
            break;
        case State::activated:
            heading = name + " is activated";
            body = "Activated on this computer" + (status.email.isNotEmpty() ? " for " + status.email : juce::String())
                 + ". Thanks for using Raz Burg AudioCrafts.";
            primary.setButtonText ("Deactivate this computer");
            secondary.setButtonText ("Close");
            break;
        case State::verifyNeeded:
            heading = "Please reconnect";
            body = name + " checks its activation online every week and has not reached audiocrafts.razburg.com for "
                   + juce::String (Activation::offlineGraceDays) + " days. Connect to the internet and check now.";
            primary.setButtonText ("Check now");
            secondary.setButtonText ("Later");
            break;
    }

    if (! busy)
    {
        if (status.problem.isNotEmpty())
            showMessage (status.problem, true);
        else if (status.state == State::waiting)
            showMessage ("Waiting for your approval in the browser...", false);
        else if (status.state == State::notActivated || status.state == State::verifyNeeded)
            showMessage ("Until then the effect drops out for a moment every 40 seconds.", false);
        else
            showMessage ({}, false);
    }

    accountLink.setVisible (status.state != State::waiting);
    resized();
    repaint();

    // Close by itself once the browser approval comes through.
    if (previous == State::waiting && status.state == State::activated && onClose)
        onClose();
}

void ActivationPanel::showMessage (const juce::String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    repaint();
}

void ActivationPanel::primaryClicked()
{
    using State = Activation::State;
    if (busy)
        return;

    const auto done = [safe = juce::Component::SafePointer<ActivationPanel> (this)] (bool ok, const juce::String& text)
    {
        if (safe == nullptr)
            return;
        safe->busy = false;
        safe->primary.setEnabled (true);
        safe->secondary.setEnabled (true);
        safe->refresh();
        if (! ok && text.isNotEmpty())
            safe->showMessage (text, true);
    };

    switch (status.state)
    {
        case State::notActivated: activation.startBrowserActivation(); break;
        case State::waiting:      activation.reopenBrowser(); break;
        case State::activated:
        case State::verifyNeeded:
            busy = true;
            primary.setEnabled (false);
            secondary.setEnabled (false);
            showMessage (status.state == State::activated ? "Deactivating..." : "Checking...", false);
            if (status.state == State::activated)
                activation.deactivate (done);
            else
                activation.verifyNow (done);
            break;
    }
}

void ActivationPanel::secondaryClicked()
{
    if (status.state == Activation::State::waiting)
    {
        activation.cancelWaiting();
        return;
    }
    if (onClose)
        onClose();
}

void ActivationPanel::paint (juce::Graphics& g)
{
    g.fillAll (style.background.withAlpha (0.84f));

    const auto c = card.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (c.translated (0.0f, 8.0f).expanded (4.0f), 20.0f);
    g.setColour (style.surface);
    g.fillRoundedRectangle (c, 16.0f);
    g.setGradientFill (juce::ColourGradient (style.accentTop.withAlpha (0.22f), c.getCentreX(), c.getY(),
                                             style.surface.withAlpha (0.0f), c.getCentreX(), c.getY() + 160.0f, false));
    g.fillRoundedRectangle (c, 16.0f);
    g.setColour (style.line);
    g.drawRoundedRectangle (c.reduced (0.5f), 16.0f, 1.0f);

    auto r = card.reduced (36, 30).toFloat();
    auto brand = r.removeFromTop (40.0f);
    if (style.drawLogo)
    {
        style.drawLogo (g, brand.removeFromLeft (40.0f));
        brand.removeFromLeft (12.0f);
    }
    g.setColour (style.text);
    g.setFont (font (18.0f).withExtraKerningFactor (0.3f));
    g.drawText (name.toUpperCase(), brand.removeFromTop (22.0f), juce::Justification::centredLeft, false);
    g.setColour (style.highlight.withAlpha (0.85f));
    g.setFont (font (9.0f, true).withExtraKerningFactor (0.22f));
    g.drawText ("RAZ BURG AUDIOCRAFTS", brand, juce::Justification::centredLeft, false);

    r.removeFromTop (22.0f);
    g.setColour (style.text);
    g.setFont (font (23.0f, true));
    g.drawText (heading, r.removeFromTop (30.0f), juce::Justification::centredLeft, false);
    r.removeFromTop (8.0f);
    g.setColour (style.muted);
    g.setFont (font (14.5f));
    g.drawFittedText (body, r.removeFromTop (66.0f).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);

    if (message.isNotEmpty())
    {
        g.setColour (messageIsError ? juce::Colour (0xffff8a8a) : style.muted);
        g.setFont (font (13.0f));
        g.drawFittedText (message, juce::Rectangle<int> (card.getX() + 24, primary.getBottom() + 12, card.getWidth() - 48, 34),
                          juce::Justification::centredTop, 2, 1.0f);
    }
}

void ActivationPanel::resized()
{
    card = juce::Rectangle<int> (560, 380).withCentre (getLocalBounds().getCentre());
    auto r = card.reduced (36, 30);
    r.removeFromTop (40 + 22 + 30 + 8 + 66 + 16);

    auto buttons = r.removeFromTop (44);
    const auto w = (buttons.getWidth() - 12) * 3 / 5;
    primary.setBounds (buttons.removeFromLeft (w));
    buttons.removeFromLeft (12);
    secondary.setBounds (buttons);

    accountLink.setBounds (card.getX(), card.getBottom() - 44, card.getWidth(), 24);
}
} // namespace rba
