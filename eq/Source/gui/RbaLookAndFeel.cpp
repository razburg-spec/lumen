#include "RbaLookAndFeel.h"

RbaLookAndFeel::RbaLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, Theme::raised);
    setColour (juce::ComboBox::outlineColourId, Theme::line);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::muted);
    setColour (juce::PopupMenu::backgroundColourId, Theme::raised);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::line);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::text);
    setColour (juce::Label::textColourId, Theme::text);
}

void RbaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = area.getCentre();
    const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    if (radius < 6.0f)
        return;

    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto trackWidth = juce::jmax (2.5f, radius * 0.085f);
    const auto arcRadius = radius - trackWidth * 0.5f;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool bipolar = slider.getProperties()["bipolar"];
    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const juce::PathStrokeType stroke (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::line);
    g.strokePath (track, stroke);

    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (value, juce::PathStrokeType (trackWidth * 2.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, stroke);
    }

    // Knob body: soft top-lit disc.
    const auto bodyRadius = arcRadius - trackWidth * 1.9f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, bodyRadius * 0.08f).expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d3846), centre.x, body.getY(),
                                             juce::Colour (0xff18202a), centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff3a4656));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    const auto inner = centre.getPointOnCircumference (bodyRadius * 0.38f, angle);
    const auto outer = centre.getPointOnCircumference (bodyRadius * 0.80f, angle);
    g.setColour (Theme::text);
    g.drawLine ({ inner, outer }, juce::jmax (1.8f, radius * 0.055f));
}

void RbaLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                     juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, corner);
    g.setColour (box.isMouseOver (true) ? Theme::faint : Theme::line);
    g.drawRoundedRectangle (r, corner, 1.0f);

    const auto cx = r.getRight() - corner - 2.0f;
    const auto cy = r.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.8f);
    chevron.lineTo (cx, cy + 1.8f);
    chevron.lineTo (cx + 3.5f, cy - 1.8f);
    g.setColour (Theme::muted);
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void RbaLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().withTrimmedLeft (box.getHeight() / 2).withTrimmedRight (box.getHeight()));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font RbaLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Theme::font (13.0f); }
juce::Font RbaLookAndFeel::getPopupMenuFont() { return Theme::font (14.0f); }

void RbaLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int, int)
{
    g.fillAll (Theme::raised);
}
