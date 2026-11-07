#include "Widgets.h"
#include <cstring>
#include <iterator>

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramID, const juce::String& titleText, juce::Colour accentColour,
            Size knobSize, bool bipolar)
    : title (titleText.toUpperCase()), accent (accentColour), size (knobSize)
{
    param = state.getParameter (paramID);
    jassert (param != nullptr);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setMouseDragSensitivity (200);
    slider.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::ctrlAltCommandModifiers);
    addAndMakeVisible (slider);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.addMouseListener (this, false);
}

juce::String Knob::valueText() const
{
    const auto v = (float) slider.getValue();

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        return choice->choices[juce::jlimit (0, choice->choices.size() - 1, juce::roundToInt (v))];

    const auto unit = param->getLabel();
    const auto signedValue = [] (float x, int decimals) { return (x > 0.0f ? "+" : "") + juce::String (x, decimals); };

    if (unit == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
    if (unit == "s")  return juce::String (v, v < 10.0f ? 2 : 1) + " s";
    if (unit == "%")  return juce::String (juce::roundToInt (v)) + "%";
    if (unit == "dB") return v <= -59.9f ? juce::String ("-inf dB") : signedValue (v, 1) + " dB";
    if (unit == "st") return signedValue (v, 1) + " st";
    if (unit == "tone") return std::abs (v) < 0.5f ? juce::String ("Neutral")
                                                  : juce::String (v < 0.0f ? "Dark " : "Shine ") + juce::String (juce::roundToInt (std::abs (v)));
    return juce::String (v, 2);
}

void Knob::paint (juce::Graphics& g)
{
    const bool large = size == Size::Large;
    auto r = getLocalBounds().toFloat();

    g.setFont (Theme::caps (large ? 11.0f : 10.0f));
    g.setColour (Theme::muted);
    g.drawText (title, r.removeFromTop (large ? 16.0f : 14.0f), juce::Justification::centred, false);

    g.setFont (Theme::font (large ? 14.0f : 12.0f));
    g.setColour (slider.isMouseOverOrDragging() ? accent : Theme::text);
    g.drawText (valueText(), r.removeFromBottom (large ? 18.0f : 15.0f), juce::Justification::centred, false);
}

void Knob::resized()
{
    const bool large = size == Size::Large;
    auto r = getLocalBounds();
    r.removeFromTop (large ? 18 : 15);
    r.removeFromBottom (large ? 20 : 16);
    const auto side = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

//==============================================================================
void PowerButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const auto on = getToggleState();
    const auto colour = on ? accent : (highlighted ? Theme::muted : Theme::faint);

    g.setColour (on ? accent.withAlpha (0.16f) : Theme::raised);
    g.fillEllipse (r);
    g.setColour (colour.withAlpha (on ? 0.9f : 1.0f));
    g.drawEllipse (r, 1.2f);

    const auto c = r.getCentre();
    const auto rad = r.getWidth() * 0.24f;
    juce::Path glyph;
    glyph.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
    glyph.startNewSubPath (c.x, c.y - rad * 1.3f);
    glyph.lineTo (c.x, c.y - rad * 0.2f);
    g.setColour (colour);
    g.strokePath (glyph, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void PillToggle::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    const auto on = getToggleState();

    g.setColour (on ? accent.withAlpha (0.18f) : Theme::raised);
    g.fillRoundedRectangle (r, corner);
    g.setColour (on ? accent.withAlpha (0.8f) : (highlighted ? Theme::faint : Theme::line));
    g.drawRoundedRectangle (r, corner, 1.0f);

    g.setColour (on ? accent : Theme::muted);
    g.setFont (Theme::caps (10.5f));
    g.drawText (getButtonText().toUpperCase(), r, juce::Justification::centred, false);
}

//==============================================================================
Segmented::Segmented (juce::RangedAudioParameter& param, const juce::StringArray& segmentLabels, juce::Colour accentColour)
    : labels (segmentLabels), accent (accentColour),
      attachment (param, [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
}

int Segmented::segmentAt (juce::Point<float> p) const
{
    if (! getLocalBounds().toFloat().contains (p) || labels.isEmpty())
        return -1;
    return juce::jlimit (0, labels.size() - 1, (int) (p.x / (float) getWidth() * (float) labels.size()));
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    const auto index = segmentAt (e.position);
    if (index >= 0 && index != selected)
        attachment.setValueAsCompleteGesture ((float) index);
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    const auto index = segmentAt (e.position);
    if (index != hovered) { hovered = index; repaint(); }
}

void Segmented::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void Segmented::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (r, corner, 1.0f);

    const auto w = r.getWidth() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (i == selected)
        {
            const auto pill = seg.reduced (3.0f);
            g.setColour (accent.withAlpha (0.2f));
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
            g.setColour (accent.withAlpha (0.7f));
            g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
        }
        g.setColour (i == selected ? Theme::text : (i == hovered ? Theme::text.withAlpha (0.85f) : Theme::muted));
        g.setFont (Theme::font (12.5f, i == selected));
        g.drawText (labels[i], seg, juce::Justification::centred, false);
    }
}

//==============================================================================
void drawBrandMark (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Concept C "Three Arcs", drawn from the 512-unit master in rbac-mark.svg.
    const auto u = area.getWidth() / 512.0f;
    const auto corner = 118.0f * u;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a63e0), area.getCentreX(), area.getY(),
                                             Theme::brandBottom, area.getCentreX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, corner);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), area.getCentreX(), area.getY(),
                                             juce::Colours::white.withAlpha (0.0f), area.getCentreX(), area.getCentreY(), false));
    g.fillRoundedRectangle (area, corner);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (area.reduced (1.5f * u), corner, 3.0f * u);

    struct Arc { float radius, sweep; juce::Colour colour; };
    const Arc arcs[] = { { 178.0f, 839.0f / 178.0f, juce::Colours::white },
                         { 124.0f, 545.0f / 124.0f, Theme::brandCyan },
                         {  70.0f, 290.0f /  70.0f, Theme::brandBlue } };
    const auto c = area.getCentre();
    const auto start = juce::MathConstants<float>::pi * 1.25f;
    for (const auto& arc : arcs)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, arc.radius * u, arc.radius * u, 0.0f, start, start + arc.sweep, true);
        g.setColour (arc.colour);
        g.strokePath (p, juce::PathStrokeType (28.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (44.0f * u, 44.0f * u).withCentre (c));
}
