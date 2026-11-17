#include "PrismWidgets.h"
#include "../PluginProcessor.h"

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

juce::String modeBlurb (int index)
{
    static const char* blurbs[] = {
        "Passive tube warmth. Broad, gentle curves; low boosts scoop the mud just above them.",
        "70s console silk. Bells start wide and tighten as you push; sweet, forward mids.",
        "Class-A iron. Fat lows with a resonant shelf, broad musical bells, thick harmonics.",
        "Large-format console bite. Tight bells, punchy shelves with overshoot, a gritty edge.",
        "Clean and exact. Transparent, open top end, no added color.",
        "Mastering polish. Smooth wide curves, gentle air, subtle harmonic glue.",
    };
    return blurbs[juce::jlimit (0, 5, index)];
}

//==============================================================================
ModeSelector::ModeSelector (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const int i = juce::jlimit (0, 5, juce::roundToInt (v));
                      if (i != selected)
                      {
                          selected = i;
                          repaint();
                          if (onChange) onChange (selected);
                      }
                  })
{
    attachment.sendInitialUpdate();
    setTooltip ("Mode: the sound and response of the EQ. The controls stay the same in every mode.");
}

int ModeSelector::segmentAt (juce::Point<float> p) const
{
    if (! getLocalBounds().toFloat().contains (p))
        return -1;
    return juce::jlimit (0, 5, (int) (p.x / ((float) getWidth() / 6.0f)));
}

void ModeSelector::mouseDown (const juce::MouseEvent& e)
{
    const int i = segmentAt (e.position);
    if (i >= 0)
        attachment.setValueAsCompleteGesture ((float) i);
}

void ModeSelector::mouseMove (const juce::MouseEvent& e)
{
    const int i = segmentAt (e.position);
    if (i != hovered) { hovered = i; repaint(); }
}

void ModeSelector::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = r.getHeight() * 0.5f;
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (r, corner, 1.0f);

    const auto w = r.getWidth() / 6.0f;
    for (int i = 0; i < 6; ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        const auto col = Theme::modeColour (i);
        if (i == selected)
        {
            const auto pill = seg.reduced (3.0f);
            g.setColour (col.withAlpha (0.2f));
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
            g.setColour (col.withAlpha (0.85f));
            g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.2f);
        }
        auto text = seg;
        const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ seg.getX() + 15.0f, seg.getCentreY() });
        g.setColour (i == selected ? col : col.withAlpha (i == hovered ? 0.8f : 0.45f));
        g.fillEllipse (dot);
        text.removeFromLeft (14.0f);
        g.setColour (i == selected ? Theme::text : (i == hovered ? Theme::text.withAlpha (0.85f) : Theme::muted));
        g.setFont (Theme::caps (11.0f, 0.1f));
        g.drawText (modeNames()[i].toUpperCase(), text, juce::Justification::centred, false);
    }
}

//==============================================================================
void Spectrum::clear()
{
    curve.fill (-140.0f);
    history.fill (0.0f);
    newSamples = 0;
}

bool Spectrum::update (AnalyzerTap& tap, double sampleRate)
{
    for (;;)
    {
        const int got = tap.pull (scratch.data(), (int) scratch.size());
        for (int i = 0; i < got; ++i)
        {
            history[(size_t) writePos] = scratch[(size_t) i];
            writePos = (writePos + 1) & (size - 1);
        }
        newSamples += got;
        if (got < (int) scratch.size())
            break;
    }

    if (newSamples < size / 4)
    {
        // No fresh audio (transport stopped): let the curve fall away.
        bool changed = false;
        for (auto& c : curve)
            if (c > -140.0f) { c -= 1.5f; changed = true; }
        return changed;
    }
    newSamples = 0;

    for (int i = 0; i < size; ++i)
        work[(size_t) i] = history[(size_t) ((writePos + i) & (size - 1))];
    std::fill (work.begin() + size, work.end(), 0.0f);
    window.multiplyWithWindowingTable (work.data(), (size_t) size);
    fft.performFrequencyOnlyForwardTransform (work.data(), true);

    const float norm = 4.0f / (float) size;   // a full-scale sine reads 0 dB
    const float binHz = (float) sampleRate / (float) size;
    for (int p = 0; p < points; ++p)
    {
        const float f = frequencyAt (p);
        const float lo = f * 0.944f / binHz, hi = f * 1.059f / binHz;   // 1/6 octave around the point
        float mag = 0.0f;
        if (hi - lo < 1.0f)
        {
            const float pos = f / binHz;
            const int i0 = juce::jlimit (0, size / 2 - 1, (int) pos);
            const float frac = pos - (float) i0;
            mag = work[(size_t) i0] * (1.0f - frac) + work[(size_t) juce::jmin (size / 2, i0 + 1)] * frac;
        }
        else
        {
            for (int i = juce::jmax (0, (int) lo); i <= juce::jmin (size / 2, (int) hi); ++i)
                mag = juce::jmax (mag, work[(size_t) i]);
        }
        const float db = juce::Decibels::gainToDecibels (mag * norm, -140.0f);
        auto& c = curve[(size_t) p];
        c = db > c ? c + 0.7f * (db - c) : juce::jmax (db, c - 1.2f);
    }
    return true;
}

//==============================================================================
namespace
{
    constexpr float maxGainDb = 20.0f;      // the EQ scale shows +-20 dB (labels every 6 dB)
    constexpr float levelTop = 0.0f, levelBottom = -96.0f;
    const juce::StringArray analyzerLabels { "PRE + POST", "POST", "OFF" };
}

EqDisplay::EqDisplay (PrismProcessor& p) : processor (p)
{
    pre.clear();
    post.clear();
    params = processor.currentParams();
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> EqDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (20.0f).withTrimmedRight (30.0f);
}

float EqDisplay::xForFreq (float hz) const
{
    const auto r = plot();
    return r.getX() + r.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f);
}

float EqDisplay::freqForX (float x) const
{
    const auto r = plot();
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}

float EqDisplay::yForGain (float db) const
{
    const auto r = plot().reduced (0.0f, 14.0f);
    return r.getCentreY() - db / maxGainDb * r.getHeight() * 0.5f;
}

float EqDisplay::gainForY (float y) const
{
    const auto r = plot().reduced (0.0f, 14.0f);
    return (r.getCentreY() - y) / (r.getHeight() * 0.5f) * maxGainDb;
}

float EqDisplay::yForLevel (float db, float hz) const
{
    const auto r = plot();
    const float tilted = db + 4.5f * std::log2 (hz / 1000.0f);   // pink-ish tilt so music looks level
    return r.getBottom() - (juce::jlimit (levelBottom, levelTop + 6.0f, tilted) - levelBottom) / (levelTop - levelBottom) * r.getHeight();
}

EqDisplay::AnalyzerMode EqDisplay::analyzerMode() const
{
    return (AnalyzerMode) juce::jlimit (0, 2, (int) processor.apvts.state.getProperty ("analyzer", 0));
}

juce::Rectangle<float> EqDisplay::analyzerButton (int i) const
{
    const auto r = plot();
    float x = r.getX() + 12.0f;
    for (int k = 0; k < i; ++k)
        x += (k == 0 ? 82.0f : 52.0f) + 4.0f;
    return { x, r.getY() + 10.0f, i == 0 ? 82.0f : 52.0f, 20.0f };
}

juce::Point<float> EqDisplay::nodePosition (int band) const
{
    const auto& b = params.bands[(size_t) band];
    const bool cut = Band::isEdge (band) && b.type == (int) Band::EdgeType::cut;
    return { xForFreq (b.freq), yForGain (cut ? 0.0f : b.gainDb) };
}

int EqDisplay::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int b = 0; b < Band::numBands; ++b)
    {
        const float d = nodePosition (b).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = b; }
    }
    return best;
}

void EqDisplay::setSelectedBand (int b)
{
    if (b != selected) { selected = b; repaint(); }
}

void EqDisplay::setParam (const juce::String& id, float plain)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void EqDisplay::poll()
{
    const auto newParams = processor.currentParams();
    bool changed = std::memcmp (&newParams, &params, sizeof (params)) != 0;
    params = newParams;

    const auto mode = analyzerMode();
    const double sr = processor.getEngineSampleRate();
    if (mode != AnalyzerMode::off)
    {
        changed |= post.update (processor.postTap(), sr);
        if (mode == AnalyzerMode::prePost)
            changed |= pre.update (processor.preTap(), sr);
        else
            { float tmp[2048]; while (processor.preTap().pull (tmp, 2048) > 0) {} }
    }
    else
    {
        float tmp[2048];
        while (processor.preTap().pull (tmp, 2048) > 0) {}
        while (processor.postTap().pull (tmp, 2048) > 0) {}
    }
    if (changed)
        repaint();
}

void EqDisplay::resized() {}

void EqDisplay::drawSpectrum (juce::Graphics& g, const Spectrum& s, juce::Colour fill, juce::Colour line)
{
    const auto r = plot();
    juce::Path p;
    for (int i = 0; i < Spectrum::points; ++i)
    {
        const float f = Spectrum::frequencyAt (i);
        const juce::Point<float> pt (xForFreq (f), yForLevel (s.levelAt (i), f));
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    juce::Path area (p);
    area.lineTo (r.getRight(), r.getBottom());
    area.lineTo (r.getX(), r.getBottom());
    area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (fill, 0.0f, r.getY() + r.getHeight() * 0.3f, fill.withAlpha (0.0f), 0.0f, r.getBottom(), false));
    g.fillPath (area);
    g.setColour (line);
    g.strokePath (p, juce::PathStrokeType (1.1f));
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto r = plot();
    const int mode = params.mode;
    const auto accent = Theme::modeColour (mode);
    const double sr = processor.getEngineSampleRate();

    g.setColour (Theme::display);
    g.fillRoundedRectangle (bounds, 12.0f);

    // Grid
    g.setFont (Theme::font (10.0f));
    for (float f : { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f, 700.0f, 800.0f, 900.0f,
                     1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 10000.0f, 20000.0f })
    {
        const bool major = f == 20.0f || f == 50.0f || f == 100.0f || f == 200.0f || f == 500.0f || f == 1000.0f
                           || f == 2000.0f || f == 5000.0f || f == 10000.0f || f == 20000.0f;
        const float x = xForFreq (f);
        g.setColour (Theme::line.withAlpha (major ? 0.55f : 0.22f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        if (major)
        {
            g.setColour (Theme::faint);
            const auto label = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
            g.drawText (label, juce::Rectangle<float> (x - 20.0f, r.getBottom() + 3.0f, 40.0f, 14.0f),
                        f == 20.0f ? juce::Justification::centredLeft : (f == 20000.0f ? juce::Justification::centredRight : juce::Justification::centred), false);
        }
    }
    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
    {
        const float y = yForGain (db);
        g.setColour (db == 0.0f ? Theme::line.brighter (0.25f) : Theme::line.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (Theme::faint);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (r.getRight() + 4.0f, y - 7.0f, 26.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }

    // Spectrum: the source before the EQ underneath, the result on top.
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (r.toNearestInt());
        const auto am = analyzerMode();
        if (am == AnalyzerMode::prePost)
            drawSpectrum (g, pre, Theme::muted.withAlpha (0.20f), Theme::muted.withAlpha (0.45f));
        if (am != AnalyzerMode::off)
            drawSpectrum (g, post, accent.withAlpha (0.22f), accent.withAlpha (0.75f));

        // EQ curves: each band faintly in its color, the sum bold.
        constexpr int numPoints = 360;
        std::array<PrismFilters::Design, Band::numBands> designs;
        std::array<bool, Band::numBands> active {};
        for (int b = 0; b < Band::numBands; ++b)
        {
            active[(size_t) b] = params.bands[(size_t) b].on;
            if (active[(size_t) b])
                designs[(size_t) b] = PrismEngine::design (b, params.bands[(size_t) b], mode, sr);
        }

        std::array<float, numPoints> total {};
        const float zeroY = yForGain (0.0f);
        for (int b = 0; b < Band::numBands; ++b)
        {
            if (! active[(size_t) b]) continue;
            juce::Path band;
            band.startNewSubPath (r.getX(), zeroY);
            for (int i = 0; i < numPoints; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) (numPoints - 1);
                const double w = 2.0 * PrismFilters::pi * freqForX (x) / sr;
                const float db = w < PrismFilters::pi ? (float) juce::Decibels::gainToDecibels (designs[(size_t) b].magnitude (w), -60.0) : total[(size_t) juce::jmax (0, i - 1)];
                total[(size_t) i] += db;
                band.lineTo (x, yForGain (juce::jlimit (-40.0f, 40.0f, db)));
            }
            band.lineTo (r.getRight(), zeroY);
            band.closeSubPath();
            const auto col = Theme::bandColour (b);
            g.setColour (col.withAlpha (b == selected ? 0.20f : 0.09f));
            g.fillPath (band);
        }

        juce::Path curve;
        for (int i = 0; i < numPoints; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (numPoints - 1);
            const float y = yForGain (juce::jlimit (-40.0f, 40.0f, total[(size_t) i]));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Theme::text);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Nodes
    for (int b = 0; b < Band::numBands; ++b)
    {
        const auto pos = nodePosition (b);
        const auto col = Theme::bandColour (b);
        const bool on = params.bands[(size_t) b].on;
        const bool hot = b == selected || b == hovered || b == dragging;
        const float rad = hot ? 10.0f : 8.5f;
        const auto circle = juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos);
        if (hot)
        {
            g.setColour (col.withAlpha (0.18f));
            g.fillEllipse (circle.expanded (6.0f));
        }
        g.setColour (on ? col : Theme::display);
        g.fillEllipse (circle);
        g.setColour (on ? col.brighter (0.3f) : col.withAlpha (0.6f));
        g.drawEllipse (circle, 1.5f);
        g.setColour (on ? Theme::display : col.withAlpha (0.8f));
        g.setFont (Theme::font (10.5f, true));
        const auto label = b == Band::low ? juce::String ("L") : (b == Band::high ? juce::String ("H") : juce::String (b));
        g.drawText (label, circle, juce::Justification::centred, false);
    }

    const int bubble = dragging >= 0 ? dragging : hovered;
    if (bubble >= 0)
        drawNodeBubble (g, bubble);

    // Analyzer switch
    const auto am = (int) analyzerMode();
    for (int i = 0; i < 3; ++i)
    {
        const auto b = analyzerButton (i);
        g.setColour (i == am ? accent.withAlpha (0.18f) : Theme::surface.withAlpha (0.85f));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (i == am ? accent.withAlpha (0.8f) : Theme::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (i == am ? Theme::text : Theme::muted);
        g.setFont (Theme::caps (9.0f, 0.1f));
        g.drawText (analyzerLabels[i], b, juce::Justification::centred, false);
    }
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.0f, 0.12f));
    g.drawText ("ANALYZER", juce::Rectangle<float> (analyzerButton (2).getRight() + 10.0f, analyzerButton (2).getY(), 80.0f, 20.0f),
                juce::Justification::centredLeft, false);

    g.setColour (Theme::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);
}

void EqDisplay::drawNodeBubble (juce::Graphics& g, int band)
{
    const auto& b = params.bands[(size_t) band];
    const bool edge = Band::isEdge (band);
    const bool cut = edge && b.type == (int) Band::EdgeType::cut;
    const auto hz = b.freq >= 1000.0f ? juce::String (b.freq / 1000.0f, b.freq >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (b.freq)) + " Hz";

    juce::String line2 = cut ? juce::String (12 * (b.slope + 1)) + " dB/oct" : ((b.gainDb > 0 ? "+" : "") + juce::String (b.gainDb, 1) + " dB");
    line2 << "   Q " << juce::String (b.q, 2);
    const auto title = Band::names()[band].toUpperCase() + (edge ? (cut ? "  CUT" : "  SHELF") : "") + (b.on ? "" : "  (OFF)");

    const auto pos = nodePosition (band);
    auto box = juce::Rectangle<float> (130.0f, 50.0f).withCentre ({ pos.x, pos.y - 44.0f });
    if (box.getY() < plot().getY() + 4.0f)
        box.setY (pos.y + 18.0f);
    box = box.constrainedWithin (plot().reduced (4.0f));

    g.setColour (Theme::raised.withAlpha (0.95f));
    g.fillRoundedRectangle (box, 8.0f);
    g.setColour (Theme::bandColour (band).withAlpha (0.8f));
    g.drawRoundedRectangle (box, 8.0f, 1.0f);
    auto t = box.reduced (10.0f, 6.0f);
    g.setFont (Theme::caps (9.5f, 0.12f));
    g.setColour (Theme::bandColour (band));
    g.drawText (title, t.removeFromTop (13.0f), juce::Justification::centredLeft, false);
    g.setFont (Theme::font (12.0f, true));
    g.setColour (Theme::text);
    g.drawText (hz, t.removeFromTop (14.0f), juce::Justification::centredLeft, false);
    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::muted);
    g.drawText (line2, t, juce::Justification::centredLeft, false);
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n != hovered) { hovered = n; repaint(); }
    setMouseCursor (n >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqDisplay::mouseExit (const juce::MouseEvent&)
{
    if (hovered >= 0) { hovered = -1; repaint(); }
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
        if (analyzerButton (i).contains (e.position))
        {
            processor.apvts.state.setProperty ("analyzer", i, nullptr);
            pre.clear();
            post.clear();
            repaint();
            return;
        }

    dragging = nodeAt (e.position);
    if (dragging < 0)
        return;

    setSelectedBand (dragging);
    if (onSelect) onSelect (dragging);

    for (auto* name : { ParamID::freq, ParamID::gain, ParamID::on })
        if (auto* p = processor.apvts.getParameter (Band::id (dragging, name)))
            p->beginChangeGesture();
    dragTurnedOn = false;
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    if (! params.bands[(size_t) dragging].on && e.getDistanceFromDragStart() > 2 && ! dragTurnedOn)
    {
        setParam (Band::id (dragging, ParamID::on), 1.0f);   // dragging a band that is off switches it on
        dragTurnedOn = true;
    }

    const bool fine = e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isAltDown();
    auto pos = e.position;
    if (fine)
        pos = e.mouseDownPosition + (e.position - e.mouseDownPosition) * 0.2f;
    if (e.mods.isShiftDown())   // shift locks to frequency only
        pos.y = e.mouseDownPosition.y;

    setParam (Band::id (dragging, ParamID::freq), freqForX (pos.x));
    const auto& b = params.bands[(size_t) dragging];
    const bool cut = Band::isEdge (dragging) && b.type == (int) Band::EdgeType::cut;
    if (! cut && ! e.mods.isShiftDown())
        setParam (Band::id (dragging, ParamID::gain), juce::jlimit (-18.0f, 18.0f, gainForY (pos.y)));
    params = processor.currentParams();
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0)
        return;
    for (auto* name : { ParamID::freq, ParamID::gain, ParamID::on })
        if (auto* p = processor.apvts.getParameter (Band::id (dragging, name)))
            p->endChangeGesture();
    dragging = -1;
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n < 0)
        return;
    if (auto* p = processor.apvts.getParameter (Band::id (n, ParamID::on)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (params.bands[(size_t) n].on ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int n = nodeAt (e.position);
    if (n < 0) n = selected;
    if (n < 0)
        return;
    auto* p = processor.apvts.getParameter (Band::id (n, ParamID::q));
    const float q = params.bands[(size_t) n].q * std::pow (1.25f, (wheel.deltaY + wheel.deltaX) * 4.0f);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.1f, 18.0f, q)));
    p->endChangeGesture();
    params = processor.currentParams();
    repaint();
}

//==============================================================================
BandStrip::BandStrip (APVTS& s, int b)
    : state (s), band (b),
      power (Theme::bandColour (b)),
      powerAttachment (s, Band::id (b, ParamID::on), power),
      freq (s, Band::id (b, ParamID::freq), "Freq", Theme::bandColour (b), Knob::Size::Large),
      gain (s, Band::id (b, ParamID::gain), "Gain", Theme::bandColour (b), Knob::Size::Small, true),
      q    (s, Band::id (b, ParamID::q),    "Q",    Theme::bandColour (b), Knob::Size::Small)
{
    for (auto* c : std::initializer_list<juce::Component*> { &power, &freq, &gain, &q })
        addAndMakeVisible (c);

    if (Band::isEdge (b))
    {
        type = std::make_unique<Segmented> (*s.getParameter (Band::id (b, ParamID::type)), juce::StringArray { "Cut", "Shelf" }, Theme::bandColour (b));
        slope = std::make_unique<Segmented> (*s.getParameter (Band::id (b, ParamID::slope)), juce::StringArray { "12", "24", "36", "48" }, Theme::bandColour (b));
        addAndMakeVisible (*type);
        addAndMakeVisible (*slope);
        q.slider.setTooltip (b == Band::low ? "Q: shelf shape, or resonance at the corner of the cut" : "Q: shelf shape, or resonance at the corner of the cut");
        gain.slider.setTooltip ("Shelf gain (the cut has no gain)");
        freq.slider.setTooltip (b == Band::low ? "Corner frequency of the low cut or shelf" : "Corner frequency of the high cut or shelf");
    }
    else
    {
        freq.slider.setTooltip ("Center frequency");
        gain.slider.setTooltip ("Boost or cut");
        q.slider.setTooltip ("Bandwidth: low Q = wide, high Q = narrow");
    }
    power.setTooltip ("Switch the band on or off");
    refresh();
}

void BandStrip::refresh()
{
    const bool on = state.getRawParameterValue (Band::id (band, ParamID::on))->load() > 0.5f;
    const bool cut = Band::isEdge (band) && juce::roundToInt (state.getRawParameterValue (Band::id (band, ParamID::type))->load()) == 0;
    const int s = (on ? 1 : 0) | (cut ? 2 : 0);
    if (s == lastState)
        return;
    lastState = s;

    const float base = on ? 1.0f : 0.5f;
    freq.setAlpha (base);
    q.setAlpha (base);
    gain.setAlpha (cut ? 0.3f : base);
    if (type != nullptr) type->setAlpha (base);
    if (slope != nullptr) slope->setAlpha (cut ? base : 0.3f);
    repaint();
}

void BandStrip::paint (juce::Graphics& g)
{
    const bool on = (lastState & 1) != 0;
    const auto col = Theme::bandColour (band);
    paintCard (g, getLocalBounds().toFloat(), col, on);
    if (selected)
    {
        g.setColour (col.withAlpha (0.55f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.75f), 12.0f, 1.5f);
    }
    paintCardTitle (g, getLocalBounds().reduced (14, 0).removeFromTop (40).toFloat().withTrimmedTop (4.0f),
                    Band::names()[band].toUpperCase(), {}, on);
}

void BandStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    power.setBounds (getWidth() - 12 - 26, 10, 26, 26);
    r.removeFromTop (42);
    r.removeFromBottom (12);
    if (type != nullptr)
    {
        type->setBounds (r.removeFromTop (26));
        r.removeFromTop (6);
        slope->setBounds (r.removeFromBottom (24));
        r.removeFromBottom (6);
    }
    auto smallRow = r.removeFromBottom (juce::jmin (84, r.getHeight() / 2));
    gain.setBounds (smallRow.removeFromLeft (smallRow.getWidth() / 2));
    q.setBounds (smallRow);
    freq.setBounds (r);
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
    const auto bar = r.withX (r.getX() + 12.0f).withWidth (8.0f);
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
