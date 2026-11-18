#include "UpdateBanner.h"

namespace rba
{
namespace
{
    constexpr float pad = 10.0f, dotSize = 6.0f, buttonWidth = 74.0f, closeWidth = 22.0f;
}

UpdateBanner::UpdateBanner (juce::String productName, juce::String productSlug, juce::String runningVersion, Style s)
    : name (std::move (productName)), slug (std::move (productSlug)), version (std::move (runningVersion)), style (s)
{
    setVisible (false);
    setRepaintsOnMouseActivity (true);
    auto* checker = UpdateChecker::getInstance();
    checker->addChangeListener (this);
    checker->start (slug, version);
    refresh();
}

UpdateBanner::~UpdateBanner()
{
    if (auto* checker = UpdateChecker::getInstanceWithoutCreating())
        checker->removeChangeListener (this);
}

void UpdateBanner::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void UpdateBanner::refresh()
{
    auto* checker = UpdateChecker::getInstance();
    const auto r = checker->getResult();
    const bool show = r.updateAvailable && ! checker->isDismissed();
    setTooltip (r.notes.isNotEmpty() ? "What's new: " + r.notes : juce::String ("Open the download page in your browser"));
    if (show != isVisible())
    {
        setVisible (show);
        if (onVisibilityChanged)
            onVisibilityChanged();
    }
    resized();
    repaint();
}

juce::String UpdateBanner::message() const
{
    return name + " " + UpdateChecker::getInstance()->getResult().latestVersion + " is available";
}

juce::Font UpdateBanner::font (float h, bool bold) const
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

int UpdateBanner::getIdealWidth() const
{
    juce::GlyphArrangement g;
    g.addLineOfText (font (12.0f), message(), 0.0f, 0.0f);
    return (int) std::ceil (pad + dotSize + 8.0f + g.getBoundingBox (0, -1, true).getWidth()
                            + 12.0f + buttonWidth + 4.0f + closeWidth + 6.0f);
}

void UpdateBanner::resized()
{
    auto r = getLocalBounds().toFloat();
    closeArea = r.removeFromRight (closeWidth + 6.0f).withTrimmedRight (6.0f);
    r.removeFromRight (4.0f);
    downloadArea = r.removeFromRight (buttonWidth).reduced (0.0f, 3.0f);
}

void UpdateBanner::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto radius = bounds.getHeight() * 0.5f;
    g.setColour (style.background);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (style.accent.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, radius, 1.0f);

    // Dot
    const auto dot = juce::Rectangle<float> (dotSize, dotSize).withCentre ({ bounds.getX() + pad + dotSize * 0.5f, bounds.getCentreY() });
    g.setColour (style.accent.withAlpha (0.25f));
    g.fillEllipse (dot.expanded (3.0f));
    g.setColour (style.accent);
    g.fillEllipse (dot);

    // Text
    auto textArea = bounds.withLeft (dot.getRight() + 8.0f).withRight (downloadArea.getX() - 8.0f);
    g.setColour (style.text);
    g.setFont (font (12.0f));
    g.drawFittedText (message(), textArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);

    // Download button
    const auto b = downloadArea;
    g.setColour (hover == Hover::download ? style.accent.brighter (0.25f) : style.accent);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (style.background.darker (0.4f));
    g.setFont (font (11.5f, true));
    g.drawText ("Download", b, juce::Justification::centred, false);

    // Close ×
    const auto c = closeArea.withSizeKeepingCentre (8.0f, 8.0f);
    g.setColour (hover == Hover::close ? style.text : style.muted);
    juce::Path x;
    x.startNewSubPath (c.getTopLeft());     x.lineTo (c.getBottomRight());
    x.startNewSubPath (c.getTopRight());    x.lineTo (c.getBottomLeft());
    g.strokePath (x, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void UpdateBanner::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const auto h = downloadArea.contains (p) ? Hover::download : closeArea.contains (p) ? Hover::close : Hover::none;
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h == Hover::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void UpdateBanner::mouseExit (const juce::MouseEvent&)
{
    hover = Hover::none;
    repaint();
}

void UpdateBanner::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;
    if (closeArea.contains (e.position))
        UpdateChecker::getInstance()->dismiss();
    else
        UpdateChecker::getInstance()->openDownloadPage();   // whole pill is clickable
}
} // namespace rba
