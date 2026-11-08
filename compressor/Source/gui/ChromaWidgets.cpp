#include "ChromaWidgets.h"
#include "../Parameters.h"

//==============================================================================
void paintCard (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, bool lit)
{
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    auto strip = r.reduced (18.0f, 0.0f).removeFromTop (2.0f);
    juce::ColourGradient grad (accent.withAlpha (0.0f), strip.getX(), 0.0f, accent.withAlpha (0.0f), strip.getRight(), 0.0f, false);
    grad.addColour (0.5, accent.withAlpha (lit ? 0.9f : 0.25f));
    g.setGradientFill (grad);
    g.fillRect (strip);
}

void paintCardTitle (juce::Graphics& g, juce::Rectangle<float> t, const juce::String& title, const juce::String& subtitle, bool lit)
{
    g.setColour (lit ? Theme::text : Theme::muted);
    g.setFont (Theme::caps (13.0f, 0.16f));
    const auto w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title) + 4.0f;
    g.drawText (title, t.removeFromLeft (w), juce::Justification::centredLeft, false);
    if (subtitle.isNotEmpty())
    {
        g.setColour (Theme::faint);
        g.setFont (Theme::caps (9.5f));
        g.drawText (subtitle, t.withTrimmedLeft (8.0f), juce::Justification::centredLeft, true);
    }
}

juce::String characterBlurb (int index)
{
    static const char* blurbs[] = {
        "Optical-tube leveling. Smooth, slow to let go, program-dependent release.",
        "Fast transistor punch. Grabs transients, bright and forward.",
        "Variable-tube thickness. Gentle at first, firmer the harder you push.",
        "Raw and gritty. Past 10:1 it over-compresses: louder in, quieter out.",
        "Clean and precise. Controls dynamics without adding color.",
        "Mix-bus glue. Dual release keeps the mix moving and together.",
    };
    return blurbs[juce::jlimit (0, 5, index)];
}

//==============================================================================
CharacterDial::CharacterDial (juce::RangedAudioParameter& p)
    : param (p),
      attachment (p, [this] (float v)
      {
          index = juce::jlimit (0, 5, juce::roundToInt (v));
          shownPosition = (float) index;
          repaint();
          if (onChange) onChange (index);
      })
{
    attachment.sendInitialUpdate();
    setTooltip ("Character: drag, scroll or click a name");
}

juce::Point<float> CharacterDial::centre() const { return { (float) getWidth() * 0.5f, (float) getHeight() * 0.55f }; }
float CharacterDial::radius() const { return juce::jmin ((float) getWidth() * 0.24f, (float) getHeight() * 0.30f); }

float CharacterDial::angleFor (float position) const
{
    const float start = -juce::MathConstants<float>::pi * 2.0f / 3.0f;   // -120 degrees
    return start + position / 5.0f * juce::MathConstants<float>::pi * 4.0f / 3.0f;
}

juce::Rectangle<float> CharacterDial::labelBounds (int i) const
{
    const auto c = centre();
    const auto a = angleFor ((float) i);
    const auto r = radius() * 1.52f;
    const auto p = juce::Point<float> (c.x + std::sin (a) * r, c.y - std::cos (a) * r);
    return juce::Rectangle<float> (78.0f, 20.0f).withCentre (p);
}

int CharacterDial::labelAt (juce::Point<float> p) const
{
    for (int i = 0; i < 6; ++i)
        if (labelBounds (i).expanded (4.0f, 3.0f).contains (p))
            return i;
    return -1;
}

void CharacterDial::select (int i)
{
    i = juce::jlimit (0, 5, i);
    if (i != index)
        attachment.setValueAsCompleteGesture ((float) i);
}

void CharacterDial::mouseDown (const juce::MouseEvent& e)
{
    const auto label = labelAt (e.position);
    clickedLabel = label >= 0;
    if (clickedLabel)
    {
        select (label);
        return;
    }
    dragging = true;
    dragStartIndex = index;
    attachment.beginGesture();
}

void CharacterDial::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    const auto delta = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    const auto target = juce::jlimit (0, 5, dragStartIndex + juce::roundToInt (delta / 36.0f));
    if (target != index)
        attachment.setValueAsPartOfGesture ((float) target);
}

void CharacterDial::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attachment.endGesture();
    dragging = false;
}

void CharacterDial::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (labelAt (e.position) < 0)
        select (juce::roundToInt (param.convertFrom0to1 (param.getDefaultValue())));
}

void CharacterDial::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto d = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (std::abs (d) > 0.0f)
        select (index + (d > 0.0f ? 1 : -1) * (wheel.isReversed ? -1 : 1));
}

void CharacterDial::mouseMove (const juce::MouseEvent& e)
{
    const auto h = labelAt (e.position);
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::UpDownResizeCursor);
}

void CharacterDial::paint (juce::Graphics& g)
{
    const auto c = centre();
    const auto r = radius();
    const auto accent = Theme::characterColour (index);

    // Detent ticks and the lit arc up to the current position.
    juce::Path track;
    track.addCentredArc (c.x, c.y, r * 1.18f, r * 1.18f, 0.0f, angleFor (0.0f), angleFor (5.0f), true);
    g.setColour (Theme::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int i = 0; i < 6; ++i)
    {
        const auto a = angleFor ((float) i);
        const auto col = Theme::characterColour (i);
        const auto p = juce::Point<float> (c.x + std::sin (a) * r * 1.18f, c.y - std::cos (a) * r * 1.18f);
        g.setColour (i == index ? col : col.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (i == index ? 9.0f : 6.0f, i == index ? 9.0f : 6.0f).withCentre (p));

        const auto lb = labelBounds (i);
        if (i == index)
        {
            g.setColour (col.withAlpha (0.16f));
            g.fillRoundedRectangle (lb, lb.getHeight() * 0.5f);
            g.setColour (col.withAlpha (0.7f));
            g.drawRoundedRectangle (lb.reduced (0.5f), lb.getHeight() * 0.5f, 1.0f);
        }
        g.setColour (i == index ? col : (i == hovered ? Theme::text : Theme::muted));
        g.setFont (Theme::caps (10.5f, 0.14f));
        g.drawText (characterNames()[i].toUpperCase(), lb, juce::Justification::centred, false);
    }

    // Glow in the current colour, then the knob body.
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.22f), c.x, c.y, accent.withAlpha (0.0f), c.x + r * 1.3f, c.y, true));
    g.fillEllipse (juce::Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c));

    const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (body.translated (0.0f, r * 0.07f).expanded (2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff313d4c), c.x, body.getY(), juce::Colour (0xff161d26), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff3d4a5b));
    g.drawEllipse (body.reduced (0.5f), 1.2f);

    // Fine knurl ring.
    for (int k = 0; k < 48; ++k)
    {
        const auto a = (float) k / 48.0f * juce::MathConstants<float>::twoPi;
        const auto p1 = juce::Point<float> (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f);
        const auto p2 = juce::Point<float> (c.x + std::sin (a) * r * 0.95f, c.y - std::cos (a) * r * 0.95f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine ({ p1, p2 }, 1.0f);
    }

    const auto a = angleFor (shownPosition);
    const auto inner = juce::Point<float> (c.x + std::sin (a) * r * 0.30f, c.y - std::cos (a) * r * 0.30f);
    const auto outer = juce::Point<float> (c.x + std::sin (a) * r * 0.80f, c.y - std::cos (a) * r * 0.80f);
    g.setColour (accent);
    g.drawLine ({ inner, outer }, juce::jmax (3.0f, r * 0.07f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.12f, r * 0.12f).withCentre (outer));
}

//==============================================================================
namespace
{
    // Gain-reduction scale: 0 dB on the right, 20 dB on the left, expanded at the low end.
    float grPosition (float db) { return 1.0f - std::log1p (juce::jlimit (0.0f, 20.0f, db)) / std::log1p (20.0f); }
}

void GrMeter::setState (float grDb, int newCharacter, const juce::String& status, bool warning)
{
    needleDb += (grDb - needleDb) * (grDb > needleDb ? 0.45f : 0.18f);
    if (grDb >= peakDb) { peakDb = grDb; peakHold = 45; }
    else if (--peakHold < 0) peakDb = juce::jmax (0.0f, peakDb - 0.4f);

    character = newCharacter;
    keyStatus = status;
    keyWarning = warning;
    repaint();
}

void GrMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto accent = Theme::characterColour (character);
    g.setColour (Theme::display);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    // Needle meter in the upper part.
    auto meterArea = bounds.reduced (18.0f, 14.0f).removeFromTop (bounds.getHeight() * 0.56f);
    const auto pivot = juce::Point<float> (meterArea.getCentreX(), meterArea.getBottom() + meterArea.getHeight() * 0.35f);
    const auto radius = meterArea.getHeight() * 1.18f;
    const auto sweep = juce::degreesToRadians (46.0f);
    auto angleOf = [&] (float db) { return -sweep + grPosition (db) * 2.0f * sweep; };
    auto pointAt = [&] (float angle, float rad) { return juce::Point<float> (pivot.x + std::sin (angle) * rad, pivot.y - std::cos (angle) * rad); };

    juce::Path scale;
    scale.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, -sweep, sweep, true);
    g.setColour (Theme::line);
    g.strokePath (scale, juce::PathStrokeType (2.0f));

    // Lit arc showing the current reduction.
    if (needleDb > 0.05f)
    {
        juce::Path lit;
        lit.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleOf (needleDb), sweep, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (lit, juce::PathStrokeType (7.0f));
        g.setColour (accent);
        g.strokePath (lit, juce::PathStrokeType (2.0f));
    }

    for (float db : { 0.0f, 1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 15.0f, 20.0f })
    {
        const auto a = angleOf (db);
        g.setColour (Theme::muted);
        g.drawLine ({ pointAt (a, radius + 3.0f), pointAt (a, radius + 9.0f) }, 1.2f);
        g.setFont (Theme::font (10.5f));
        g.drawText (db < 0.5f ? juce::String ("0") : juce::String ((int) db),
                    juce::Rectangle<float> (26.0f, 14.0f).withCentre (pointAt (a, radius + 19.0f)), juce::Justification::centred, false);
    }
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.0f, 0.18f));
    g.drawText ("GAIN REDUCTION  dB", juce::Rectangle<float> (meterArea.getX(), pivot.y - radius * 0.62f, meterArea.getWidth(), 14.0f),
                juce::Justification::centred, false);

    // Peak marker and needle.
    g.setColour (accent.withAlpha (0.6f));
    g.drawLine ({ pointAt (angleOf (peakDb), radius - 8.0f), pointAt (angleOf (peakDb), radius + 2.0f) }, 2.0f);

    g.saveState();
    g.reduceClipRegion (bounds.reduced (2.0f).withBottom (meterArea.getBottom() + 4.0f).toNearestInt());
    const auto na = angleOf (needleDb);
    g.setColour (Theme::text);
    g.drawLine ({ pointAt (na, radius * 0.48f), pointAt (na, radius + 4.0f) }, 2.0f);
    g.restoreState();

    // Character name, blurb, readout and key status.
    auto info = bounds.reduced (20.0f, 10.0f).withTrimmedTop (meterArea.getBottom() - bounds.getY() + 2.0f);
    auto top = info.removeFromTop (30.0f);
    g.setColour (accent);
    g.setFont (Theme::font (24.0f).withExtraKerningFactor (0.26f));
    g.drawText (characterNames()[character].toUpperCase(), top, juce::Justification::centredLeft, false);
    g.setColour (Theme::text);
    g.setFont (Theme::font (20.0f));
    g.drawText ((needleDb > 0.05f ? "-" : "") + juce::String (needleDb, 1) + " dB", top, juce::Justification::centredRight, false);

    g.setColour (Theme::muted);
    g.setFont (Theme::font (12.5f));
    g.drawFittedText (characterBlurb (character), info.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);

    if (keyStatus.isNotEmpty())
    {
        g.setColour (keyWarning ? juce::Colour (0xffff7a6b) : Theme::key);
        g.setFont (Theme::caps (9.5f, 0.16f));
        g.drawText (keyStatus, info.removeFromTop (16.0f), juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void LevelMeter::setLevel (float peak)
{
    const auto db = juce::Decibels::gainToDecibels (peak, -100.0f);
    levelDb = db > levelDb ? db : juce::jmax (db, levelDb - 1.2f);
    if (db >= holdDb) { holdDb = db; holdFrames = 40; }
    else if (--holdFrames < 0) holdDb = juce::jmax (-100.0f, holdDb - 1.0f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromBottom (14.0f);
    r.removeFromBottom (4.0f);
    const auto bar = r.withX (r.getX() + 4.0f).withWidth (8.0f);
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (8.5f));
    g.drawText (label, labelArea.withX (bar.getCentreX() - 15.0f).withWidth (30.0f), juce::Justification::centred, false);
    g.setColour (Theme::display);
    g.fillRoundedRectangle (bar, 4.0f);

    auto yFor = [&] (float db) { return bar.getBottom() - (juce::jlimit (-48.0f, 6.0f, db) + 48.0f) / 54.0f * bar.getHeight(); };
    if (levelDb > -48.0f)
    {
        const auto top = yFor (levelDb);
        const auto zero = yFor (0.0f);
        g.setColour (accent.withAlpha (0.85f));
        g.fillRoundedRectangle (bar.withTop (juce::jmax (top, zero)), 3.0f);
        if (top < zero)
        {
            g.setColour (juce::Colour (0xffff7a6b));
            g.fillRect (bar.withTop (top).withBottom (zero));
        }
    }
    if (holdDb > -48.0f)
    {
        g.setColour (holdDb > 0.0f ? juce::Colour (0xffff7a6b) : Theme::text);
        g.fillRect (bar.withTop (yFor (holdDb) - 1.0f).withHeight (2.0f));
    }

    // Scale marks on the left.
    g.setFont (Theme::font (9.0f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
    {
        const auto y = yFor (db);
        g.setColour (Theme::line);
        g.drawHorizontalLine ((int) y, bar.getRight() + 2.0f, bar.getRight() + 6.0f);
        g.setColour (Theme::faint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (bar.getRight() + 7.0f, y - 6.0f, 24.0f, 12.0f), juce::Justification::centredLeft, false);
    }
}
