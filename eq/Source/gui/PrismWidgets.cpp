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

    if (newSamples < size / 8)
    {
        // No fresh audio (transport stopped): let the curve fall away.
        bool changed = false;
        for (auto& c : curve)
            if (c > -140.0f) { c -= 0.8f; changed = true; }
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
    constexpr int numBins = size / 2 + 1;
    for (int i = 0; i < numBins; ++i)
        binDb[(size_t) i] = juce::Decibels::gainToDecibels (work[(size_t) i] * norm, -140.0f);

    // Smooth mapping onto the log-spaced points: below a few bins per point, a cubic curve through
    // the bins; above, a weighted average of power across 1/6 octave (no flat steps or corners).
    auto binAt = [&] (int i) { return binDb[(size_t) juce::jlimit (0, numBins - 1, i)]; };
    for (int p = 0; p < points; ++p)
    {
        const float f = frequencyAt (p);
        const float pos = f / binHz;
        const float span = pos * (1.059f - 0.944f);
        float db;
        if (span < 3.0f)
        {
            const int i1 = (int) pos;
            const float t = pos - (float) i1;
            const float y0 = binAt (i1 - 1), y1 = binAt (i1), y2 = binAt (i1 + 1), y3 = binAt (i1 + 2);
            // Catmull-Rom
            db = 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t * t
                         + (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t * t * t);
        }
        else
        {
            const float half = 0.5f * span * 1.5f;
            double sum = 0.0, wsum = 0.0;
            for (int i = juce::jmax (1, (int) (pos - half)); i <= juce::jmin (numBins - 1, (int) (pos + half) + 1); ++i)
            {
                const float w = juce::jmax (0.0f, 1.0f - std::abs ((float) i - pos) / half);
                const float m = work[(size_t) i] * norm;
                sum += w * m * m;
                wsum += w;
            }
            db = juce::Decibels::gainToDecibels ((float) std::sqrt (sum / juce::jmax (1.0e-9, wsum)), -140.0f);
        }
        raw[(size_t) p] = juce::jmax (-140.0f, db);
    }

    for (int p = 0; p < points; ++p)
    {
        const float db = 0.25f * raw[(size_t) juce::jmax (0, p - 1)] + 0.5f * raw[(size_t) p] + 0.25f * raw[(size_t) juce::jmin (points - 1, p + 1)];
        auto& c = curve[(size_t) p];
        c = db > c ? c + 0.45f * (db - c) : juce::jmax (db, c - 0.6f);
    }
    return true;
}

//==============================================================================
namespace
{
    constexpr float maxGainDb = 20.0f;      // the EQ scale shows +-20 dB (labels every 6 dB)
    constexpr float levelTop = 0.0f, levelBottom = -96.0f;
    constexpr float fineFactor = 0.15f;
    const juce::StringArray analyzerLabels { "PRE + POST", "POST", "OFF" };

    // A smooth path through the points (Catmull-Rom turned into cubic Beziers).
    juce::Path smoothPath (const std::vector<juce::Point<float>>& pts)
    {
        juce::Path p;
        if (pts.empty())
            return p;
        p.startNewSubPath (pts[0]);
        for (size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const auto p0 = pts[i == 0 ? 0 : i - 1], p1 = pts[i], p2 = pts[i + 1], p3 = pts[juce::jmin (pts.size() - 1, i + 2)];
            p.cubicTo (p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
        }
        return p;
    }

    // Bandwidth in octaves of a bell with quality factor q, and back.
    float octavesForQ (float q)  { return 2.0f / std::log (2.0f) * std::asinh (1.0f / (2.0f * q)); }
    float qForOctaves (float bw) { const float p = std::pow (2.0f, bw); return std::sqrt (p) / (p - 1.0f); }

    juce::String formatHz (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    // "1.5k", "1500", "1.5 kHz", "250 Hz"
    float parseHz (juce::String t)
    {
        t = t.trim().toLowerCase().removeCharacters (" ").replace ("hz", "");
        const bool k = t.endsWithChar ('k');
        const float v = t.getFloatValue();
        return k ? v * 1000.0f : v;
    }
}

// Inline text box for typing a value; Tab / Shift-Tab step through the fields.
class EqDisplay::ValueEntry : public juce::TextEditor
{
public:
    std::function<void (int)> onTab;
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == juce::KeyPress::tabKey)
        {
            if (onTab) onTab (k.getModifiers().isShiftDown() ? -1 : 1);
            return true;
        }
        return juce::TextEditor::keyPressed (k);
    }
};

EqDisplay::EqDisplay (PrismProcessor& p) : processor (p)
{
    pre.clear();
    post.clear();
    params = processor.currentParams();
    setRepaintsOnMouseActivity (false);
    setWantsKeyboardFocus (false);
}

EqDisplay::~EqDisplay()
{
    processor.setSolo (-1);
}

void EqDisplay::previewHover (int band)
{
    params = processor.currentParams();
    hovered = bubbleBand = band;
    mousePos = listenButton (band).getCentre();
    repaint();
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

bool EqDisplay::isCut (int band) const
{
    return Band::isEdge (band) && params.bands[(size_t) band].type == (int) Band::EdgeType::cut;
}

juce::Point<float> EqDisplay::nodePosition (int band) const
{
    const auto& b = params.bands[(size_t) band];
    return { xForFreq (b.freq), yForGain (isCut (band) ? 0.0f : b.gainDb) };
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
    const auto newSelection = b >= 0 ? (isSelected (b) ? selection : (1u << b)) : 0u;
    if (b != selected || newSelection != selection) { selected = b; selection = newSelection; repaint(); }
}

void EqDisplay::selectOnly (int b)
{
    selection = b >= 0 ? (1u << b) : 0u;
    setPrimary (b);
}

void EqDisplay::setPrimary (int b)
{
    selected = b;
    if (onSelect) onSelect (b);
    repaint();
}

void EqDisplay::selectRange (int from, int to)
{
    // A consecutive range of bands, in frequency order.
    if (from < 0) { selectOnly (to); return; }
    const float lo = juce::jmin (params.bands[(size_t) from].freq, params.bands[(size_t) to].freq);
    const float hi = juce::jmax (params.bands[(size_t) from].freq, params.bands[(size_t) to].freq);
    selection = 0;
    for (int b = 0; b < Band::numBands; ++b)
        if (params.bands[(size_t) b].freq >= lo && params.bands[(size_t) b].freq <= hi)
            selection |= 1u << b;
    selection |= (1u << from) | (1u << to);
    setPrimary (to);
}

void EqDisplay::setParam (const juce::String& id, float plain)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void EqDisplay::poll()
{
    const auto newParams = processor.currentParams();
    bool changed = ! (newParams == params);
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

void EqDisplay::resized()
{
    if (entry != nullptr)
        closeEntry (false);
}

void EqDisplay::drawSpectrum (juce::Graphics& g, const Spectrum& s, juce::Colour fill, juce::Colour line)
{
    const auto r = plot();
    std::vector<juce::Point<float>> pts;
    pts.reserve ((size_t) Spectrum::points);
    for (int i = 0; i < Spectrum::points; ++i)
    {
        const float f = Spectrum::frequencyAt (i);
        pts.emplace_back (xForFreq (f), yForLevel (s.levelAt (i), f));
    }
    const auto p = smoothPath (pts);
    juce::Path area (p);
    area.lineTo (r.getRight(), r.getBottom());
    area.lineTo (r.getX(), r.getBottom());
    area.closeSubPath();
    g.setGradientFill (juce::ColourGradient (fill, 0.0f, r.getY() + r.getHeight() * 0.3f, fill.withAlpha (0.0f), 0.0f, r.getBottom(), false));
    g.fillPath (area);
    g.setColour (line);
    g.strokePath (p, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EqDisplay::drawSoloRegion (juce::Graphics& g, int band)
{
    // The part of the spectrum you hear while listening to this band.
    const auto r = plot();
    const double sr = processor.getEngineSampleRate();
    const auto d = PrismEngine::soloDesign (band, params.bands[(size_t) band], params.mode, sr);
    std::vector<juce::Point<float>> pts;
    constexpr int n = 200;
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const double w = juce::jmin (2.0 * PrismFilters::pi * freqForX (x) / sr, PrismFilters::pi * 0.999);
        const float m = (float) juce::jlimit (0.0, 1.0, d.magnitude (w));
        pts.emplace_back (x, r.getBottom() - m * r.getHeight());
    }
    auto edge = smoothPath (pts);
    juce::Path area (edge);
    area.lineTo (r.getRight(), r.getBottom());
    area.lineTo (r.getX(), r.getBottom());
    area.closeSubPath();
    const auto col = Theme::bandColour (band);
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.22f), 0.0f, r.getY(), col.withAlpha (0.04f), 0.0f, r.getBottom(), false));
    g.fillPath (area);
    g.setColour (col.withAlpha (0.55f));
    g.strokePath (edge, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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

    const bool soloing = listening >= 0;
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (r.toNearestInt());

        // Spectrum: the source before the EQ underneath, the result on top.
        const auto am = analyzerMode();
        if (am == AnalyzerMode::prePost)
            drawSpectrum (g, pre, Theme::muted.withAlpha (0.20f), Theme::muted.withAlpha (0.45f));
        if (am != AnalyzerMode::off)
            drawSpectrum (g, post, accent.withAlpha (0.22f), accent.withAlpha (0.75f));

        if (soloing)
            drawSoloRegion (g, listening);

        // EQ curves: each band faintly in its color, the sum bold.
        constexpr int numPoints = 400;
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
        std::vector<juce::Point<float>> pts ((size_t) numPoints);
        for (int b = 0; b < Band::numBands; ++b)
        {
            if (! active[(size_t) b]) continue;
            for (int i = 0; i < numPoints; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) (numPoints - 1);
                const double w = 2.0 * PrismFilters::pi * freqForX (x) / sr;
                const float db = w < PrismFilters::pi ? (float) juce::Decibels::gainToDecibels (designs[(size_t) b].magnitude (w), -60.0) : total[(size_t) juce::jmax (0, i - 1)];
                total[(size_t) i] += db;
                pts[(size_t) i] = { x, yForGain (juce::jlimit (-40.0f, 40.0f, db)) };
            }
            auto band = smoothPath (pts);
            band.lineTo (r.getRight(), zeroY);
            band.lineTo (r.getX(), zeroY);
            band.closeSubPath();
            const auto col = Theme::bandColour (b);
            const float alpha = soloing ? (b == listening ? 0.22f : 0.04f) : (isSelected (b) ? 0.20f : 0.09f);
            g.setColour (col.withAlpha (alpha));
            g.fillPath (band);
        }

        for (int i = 0; i < numPoints; ++i)
            pts[(size_t) i] = { r.getX() + r.getWidth() * (float) i / (float) (numPoints - 1), yForGain (juce::jlimit (-40.0f, 40.0f, total[(size_t) i])) };
        const auto curve = smoothPath (pts);
        g.setColour (accent.withAlpha (soloing ? 0.05f : 0.13f));
        g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Theme::text.withAlpha (soloing ? 0.35f : 0.95f));
        g.strokePath (curve, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Nodes
    for (int b = 0; b < Band::numBands; ++b)
    {
        const auto pos = nodePosition (b);
        const auto col = Theme::bandColour (b);
        const bool on = params.bands[(size_t) b].on;
        const bool hot = isSelected (b) || b == hovered || b == dragging;
        const float rad = hot ? 10.0f : 8.5f;
        const auto circle = juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos);
        juce::Graphics::ScopedSaveState fade (g);
        if (soloing && b != listening)
            g.setOpacity (0.35f);
        if (hot)
        {
            g.setColour (col.withAlpha (0.18f));
            g.fillEllipse (circle.expanded (6.0f));
        }
        if (isSelected (b) && selection != (1u << b))
        {
            g.setColour (col.withAlpha (0.9f));
            g.drawEllipse (circle.expanded (3.5f), 1.2f);   // part of a multi-selection
        }
        g.setColour (on ? col : Theme::display);
        g.fillEllipse (circle);
        g.setColour (on ? col.brighter (0.3f) : col.withAlpha (0.6f));
        g.drawEllipse (circle, 1.2f);
        g.setColour (on ? Theme::display : col.withAlpha (0.8f));
        g.setFont (Theme::font (10.5f, true));
        const auto label = b == Band::low ? juce::String ("L") : (b == Band::high ? juce::String ("H") : juce::String (b));
        g.drawText (label, circle, juce::Justification::centred, false);
    }

    // Width handles of the selected (or hovered) bell.
    if (! soloing)
        for (int b : { selected, hovered })
            if (hasHandles (b))
                for (int side : { -1, 1 })
                {
                    const auto hp = handlePosition (b, side);
                    const bool hot = (b == hovered && side == hoveredSide) || (b == dragging && side == dragSide);
                    const auto col = Theme::bandColour (b);
                    g.setColour (col.withAlpha (0.35f));
                    g.drawLine (juce::Line<float> (nodePosition (b), hp), 1.0f);
                    g.setColour (hot ? col : Theme::display);
                    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (hp));
                    g.setColour (col);
                    g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (hp), 1.4f);
                }

    // Click-to-add preview when nothing is selected.
    if (showGhost && dragMode == DragMode::none && r.contains (mousePos))
    {
        const auto c = juce::Rectangle<float> (18.0f, 18.0f).withCentre (mousePos);
        g.setColour (Theme::text.withAlpha (0.4f));
        juce::Path ring;
        ring.addEllipse (c);
        const float dashes[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (ring, ring, dashes, 2);
        g.fillPath (ring);
        g.drawLine (mousePos.x - 4.0f, mousePos.y, mousePos.x + 4.0f, mousePos.y, 1.2f);
        g.drawLine (mousePos.x, mousePos.y - 4.0f, mousePos.x, mousePos.y + 4.0f, 1.2f);
    }

    // Marquee selection
    if (dragMode == DragMode::marquee && ! marquee.isEmpty())
    {
        g.setColour (Theme::text.withAlpha (0.06f));
        g.fillRoundedRectangle (marquee, 4.0f);
        g.setColour (Theme::text.withAlpha (0.4f));
        g.drawRoundedRectangle (marquee, 4.0f, 1.0f);
    }

    const int bubble = listening >= 0 ? listening : (dragging >= 0 ? dragging : bubbleBand);
    if (bubble >= 0 && entry == nullptr)
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

juce::Rectangle<float> EqDisplay::bubbleBox (int band) const
{
    const auto pos = nodePosition (band);
    auto box = juce::Rectangle<float> (164.0f, 52.0f).withCentre ({ pos.x, pos.y - 46.0f });
    if (box.getY() < plot().getY() + 4.0f)
        box.setY (pos.y + 20.0f);
    return box.constrainedWithin (plot().reduced (4.0f));
}

juce::Rectangle<float> EqDisplay::listenButton (int band) const
{
    const auto box = bubbleBox (band);
    return juce::Rectangle<float> (28.0f, 28.0f).withCentre ({ box.getRight() - 22.0f, box.getCentreY() });
}

void EqDisplay::drawNodeBubble (juce::Graphics& g, int band)
{
    const auto& b = params.bands[(size_t) band];
    const bool edge = Band::isEdge (band);
    const bool cut = isCut (band);
    const bool soloHere = listening == band;

    juce::String line2;
    if (soloHere)
        line2 << "Listen " << (listenLevelDb > 0 ? "+" : "") << juce::String (listenLevelDb, 1) << " dB";
    else
        line2 << (cut ? juce::String (12 * (b.slope + 1)) + " dB/oct" : ((b.gainDb > 0 ? "+" : "") + juce::String (b.gainDb, 1) + " dB"))
              << "   Q " << juce::String (b.q, 2);
    const auto title = Band::names()[band].toUpperCase() + (edge ? (cut ? "  CUT" : "  SHELF") : "") + (b.on ? "" : "  (OFF)");

    const auto box = bubbleBox (band);
    const auto col = Theme::bandColour (band);
    g.setColour (Theme::raised.withAlpha (0.96f));
    g.fillRoundedRectangle (box, 10.0f);
    g.setColour (col.withAlpha (0.8f));
    g.drawRoundedRectangle (box, 10.0f, 1.0f);

    auto t = box.reduced (11.0f, 6.0f).withTrimmedRight (32.0f);
    g.setFont (Theme::caps (9.5f, 0.12f));
    g.setColour (col);
    g.drawText (title, t.removeFromTop (13.0f), juce::Justification::centredLeft, false);
    g.setFont (Theme::font (12.0f, true));
    g.setColour (Theme::text);
    g.drawText (formatHz (b.freq), t.removeFromTop (15.0f), juce::Justification::centredLeft, false);
    g.setFont (Theme::font (11.0f));
    g.setColour (soloHere ? col : Theme::muted);
    g.drawText (line2, t, juce::Justification::centredLeft, false);

    // Listen button: a small speaker. Hold it to hear only this band.
    const auto btn = listenButton (band);
    const bool hot = soloHere || (hovered == band && btn.contains (mousePos));
    g.setColour (soloHere ? col : (hot ? col.withAlpha (0.25f) : Theme::surface));
    g.fillEllipse (btn);
    g.setColour (soloHere ? col : col.withAlpha (hot ? 0.9f : 0.55f));
    g.drawEllipse (btn.reduced (0.5f), 1.0f);

    const auto ic = btn.reduced (7.5f);
    juce::Path spk;
    spk.addRoundedRectangle (ic.getX(), ic.getCentreY() - ic.getHeight() * 0.18f, ic.getWidth() * 0.22f, ic.getHeight() * 0.36f, 1.0f);
    spk.startNewSubPath (ic.getX() + ic.getWidth() * 0.22f, ic.getCentreY() - ic.getHeight() * 0.18f);
    spk.lineTo (ic.getX() + ic.getWidth() * 0.52f, ic.getY() + ic.getHeight() * 0.04f);
    spk.lineTo (ic.getX() + ic.getWidth() * 0.52f, ic.getBottom() - ic.getHeight() * 0.04f);
    spk.lineTo (ic.getX() + ic.getWidth() * 0.22f, ic.getCentreY() + ic.getHeight() * 0.18f);
    spk.closeSubPath();
    const auto iconCol = soloHere ? Theme::display : (hot ? Theme::text : Theme::muted);
    g.setColour (iconCol);
    g.fillPath (spk);
    juce::Path waves;
    const auto c = juce::Point<float> (ic.getX() + ic.getWidth() * 0.5f, ic.getCentreY());
    for (float rad : { ic.getWidth() * 0.26f, ic.getWidth() * 0.46f })
        waves.addCentredArc (c.x, c.y, rad, rad, 0.0f, juce::MathConstants<float>::pi * 0.22f, juce::MathConstants<float>::pi * 0.78f, true);
    g.strokePath (waves, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---- Bandwidth handles: two small dots on the curve at the band's width edges (bells only) ----
bool EqDisplay::hasHandles (int band) const
{
    return band >= 0 && ! Band::isEdge (band) && params.bands[(size_t) band].on;
}

juce::Point<float> EqDisplay::handlePosition (int band, int side) const
{
    const auto& b = params.bands[(size_t) band];
    const float half = 0.5f * octavesForQ (b.q);
    const float hz = b.freq * std::pow (2.0f, side < 0 ? -half : half);
    return { xForFreq (hz), yForGain (b.gainDb * 0.5f) };
}

EqDisplay::PointHit EqDisplay::pointAt (juce::Point<float> p) const
{
    const int n = nodeAt (p);
    if (n >= 0)
        return { n, 0 };
    for (int b : { selected, hovered })
        if (hasHandles (b))
            for (int side : { -1, 1 })
                if (handlePosition (b, side).getDistanceFrom (p) < 8.0f)
                    return { b, side };
    return {};
}

void EqDisplay::beginGestures()
{
    gestureBands = selection;
    for (int b = 0; b < Band::numBands; ++b)
        if (gestureBands >> b & 1u)
            for (auto* name : { ParamID::freq, ParamID::gain, ParamID::q, ParamID::on })
                if (auto* prm = processor.apvts.getParameter (Band::id (b, name)))
                    prm->beginChangeGesture();
}

void EqDisplay::endGestures()
{
    for (int b = 0; b < Band::numBands; ++b)
        if (gestureBands >> b & 1u)
            for (auto* name : { ParamID::freq, ParamID::gain, ParamID::q, ParamID::on })
                if (auto* prm = processor.apvts.getParameter (Band::id (b, name)))
                    prm->endChangeGesture();
    gestureBands = 0;
}

void EqDisplay::setAsStep (const juce::String& id, float plain)
{
    if (auto* prm = processor.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void EqDisplay::resetBand (int band)
{
    processor.history.performAsOneStep ([&]
    {
        for (auto* name : { ParamID::freq, ParamID::gain, ParamID::q, ParamID::type, ParamID::slope })
            if (auto* prm = processor.apvts.getParameter (Band::id (band, name)))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getDefaultValue());
                prm->endChangeGesture();
            }
    });
}

void EqDisplay::createBandAt (juce::Point<float> pos)
{
    // Prism has six fixed bands: "adding" one brings the nearest band that is off (or else the
    // nearest bell) to that spot, at that frequency and gain.
    const float hz = freqForX (pos.x);
    int best = -1;
    float bestDist = 1.0e9f;
    for (int pass = 0; pass < 2 && best < 0; ++pass)
        for (int b = 0; b < Band::numBands; ++b)
        {
            const auto& band = params.bands[(size_t) b];
            if (pass == 0 && band.on) continue;
            if (isCut (b)) continue;
            const float d = std::abs (std::log2 (band.freq / hz));
            if (d < bestDist) { bestDist = d; best = b; }
        }
    if (best < 0)
        return;
    processor.history.performAsOneStep ([&]
    {
        setAsStep (Band::id (best, ParamID::freq), hz);
        setAsStep (Band::id (best, ParamID::gain), juce::jlimit (-18.0f, 18.0f, gainForY (pos.y)));
        setAsStep (Band::id (best, ParamID::on), 1.0f);
    });
    params = processor.currentParams();
    createdBand = best;
    createdAt = juce::Time::getMillisecondCounter();
    selectOnly (best);
}

void EqDisplay::rebaseline (juce::Point<float> origin)
{
    // Called when a modifier changes mid-drag, so the point continues from where it is.
    dragOrigin = origin;
    startBands = params.bands;
    axisLock = 0;
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;

    // Keep the value bubble while the mouse is on it (so its listen button can be reached).
    if (bubbleBand >= 0 && bubbleBox (bubbleBand).expanded (0.0f, 14.0f).contains (e.position) && nodeAt (e.position) < 0)
    {
        hovered = bubbleBand;
        hoveredSide = 0;
        showGhost = false;
        setMouseCursor (listenButton (bubbleBand).contains (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
        return;
    }

    const auto hit = pointAt (e.position);
    const bool ghost = selection == 0 && hit.band < 0 && plot().contains (e.position);
    if (hit.band != hovered || hit.side != hoveredSide || ghost || ghost != showGhost)
    {
        hovered = hit.band;
        hoveredSide = hit.side;
        bubbleBand = hit.band;
        showGhost = ghost;
        repaint();
    }
    setMouseCursor (hit.side != 0 ? juce::MouseCursor::LeftRightResizeCursor
                                  : (hit.band >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
}

void EqDisplay::mouseExit (const juce::MouseEvent&)
{
    if (dragMode != DragMode::none)
        return;
    hovered = -1;
    hoveredSide = 0;
    bubbleBand = -1;
    showGhost = false;
    repaint();
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (entry != nullptr)
        closeEntry (true);

    for (int i = 0; i < 3; ++i)
        if (analyzerButton (i).contains (e.position))
        {
            processor.apvts.state.setProperty ("analyzer", i, nullptr);
            pre.clear();
            post.clear();
            repaint();
            return;
        }

    moved = false;
    dragOrigin = e.position;
    const auto& mods = e.mods;
    const bool cmd = mods.isCommandDown() || (mods.isCtrlDown() && ! mods.isPopupMenu());
    const bool alt = mods.isAltDown();
    const bool shift = mods.isShiftDown();

    // Listen button in the value bubble: hold to hear this band alone.
    if (bubbleBand >= 0 && ! mods.isPopupMenu())
    {
        if (listenButton (bubbleBand).contains (e.position))
        {
            listening = bubbleBand;
            listenStartLevel = listenLevelDb;
            listenStartFreq = params.bands[(size_t) listening].freq;
            dragMode = DragMode::listen;
            if (auto* prm = processor.apvts.getParameter (Band::id (listening, ParamID::freq)))
                prm->beginChangeGesture();
            processor.setSolo (listening, listenLevelDb);
            repaint();
            return;
        }
        if (bubbleBox (bubbleBand).contains (e.position) && nodeAt (e.position) < 0)
            return;   // clicks on the bubble itself do nothing
    }

    const auto hit = pointAt (e.position);
    if (hit.band >= 0)
    {
        const int b = hit.band;
        if (mods.isPopupMenu())
        {
            if (! isSelected (b)) selectOnly (b); else setPrimary (b);
            showBandMenu (b);
            return;
        }
        if (alt && cmd)
        {
            // Change the band's shape (the Low and High bands: Cut or Shelf).
            selectOnly (b);
            if (Band::isEdge (b))
                setAsStep (Band::id (b, ParamID::type), isCut (b) ? 1.0f : 0.0f);
            return;
        }
        if (alt && shift)
        {
            // Change the slope (cuts: 12 > 24 > 36 > 48 dB/oct).
            selectOnly (b);
            if (isCut (b))
                setAsStep (Band::id (b, ParamID::slope), (float) ((params.bands[(size_t) b].slope + 1) % 4));
            return;
        }
        if (alt && hit.side == 0)
        {
            // Bypass the band.
            setAsStep (Band::id (b, ParamID::on), params.bands[(size_t) b].on ? 0.0f : 1.0f);
            if (! isSelected (b)) selectOnly (b); else setPrimary (b);
            return;
        }

        wasSelected = isSelected (b);
        if (cmd)
        {
            selection |= 1u << b;          // Cmd/Ctrl-click adds a band to the selection
            setPrimary (b);
        }
        else if (shift && ! wasSelected)
        {
            selectRange (selected, b);     // Shift-click selects a range
        }
        else if (! wasSelected || hit.side != 0)
        {
            selectOnly (b);
        }
        else
        {
            setPrimary (b);
        }

        if (hit.side != 0)
            selection = 1u << b;           // width handles act on their own band only

        dragging = b;
        dragSide = hit.side;
        dragMode = hit.side != 0 ? DragMode::width : DragMode::move;
        dragQ = cmd;
        dragFine = shift;
        axisLock = 0;
        startBands = params.bands;
        dragTurnedOn = false;
        beginGestures();
        return;
    }

    if (! plot().contains (e.position))
        return;

    if (mods.isPopupMenu())
    {
        showBackgroundMenu();
        return;
    }
    if (cmd && selection != 0)
    {
        createBandAt (e.position);         // Cmd/Ctrl-click on the background adds a band
        return;
    }
    hadSelection = selection != 0;
    dragMode = DragMode::marquee;
    marqueeStart = e.position;
    marquee = {};
    showGhost = false;
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (e.getDistanceFromDragStart() > 2)
        moved = true;

    if (dragMode == DragMode::listen)
    {
        // Sideways: the band's frequency. Up and down: the listening level.
        const auto delta = e.position - e.mouseDownPosition;
        if (std::abs (delta.x) > 3.0f)
            setParam (Band::id (listening, ParamID::freq), freqForX (xForFreq (listenStartFreq) + delta.x));
        listenLevelDb = juce::jlimit (-24.0f, 12.0f, std::round ((listenStartLevel - delta.y / 6.0f) * 2.0f) * 0.5f);
        processor.setSolo (listening, listenLevelDb);
        params = processor.currentParams();
        repaint();
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        if (! moved)
            return;
        marquee = juce::Rectangle<float> (marqueeStart, e.position).getIntersection (plot());
        juce::uint32 sel = 0;
        int first = -1;
        for (int b = 0; b < Band::numBands; ++b)
            if (marquee.contains (nodePosition (b)))
            {
                sel |= 1u << b;
                if (first < 0) first = b;
            }
        if (sel != selection)
        {
            selection = sel;
            selected = first;
            if (onSelect) onSelect (first);
        }
        repaint();
        return;
    }

    if (dragging < 0 || ! moved)
        return;

    // Switching Cmd/Ctrl (Q) or Shift (fine) mid-drag continues smoothly from where the point is.
    const bool cmd = e.mods.isCommandDown() || e.mods.isCtrlDown();
    const bool fine = e.mods.isShiftDown();
    if (cmd != dragQ || fine != dragFine)
    {
        dragQ = cmd;
        dragFine = fine;
        rebaseline (e.position);
    }

    // Dragging a band that is off switches it on.
    if (! dragTurnedOn)
    {
        for (int b = 0; b < Band::numBands; ++b)
            if (isSelected (b) && ! params.bands[(size_t) b].on && (b == dragging || dragMode != DragMode::width))
                setParam (Band::id (b, ParamID::on), 1.0f);
        dragTurnedOn = true;
    }

    auto delta = e.position - dragOrigin;
    if (fine)
        delta *= fineFactor;

    // Option/Alt keeps the move on one axis: whichever way you go first.
    if (e.mods.isAltDown())
    {
        if (axisLock == 0 && delta.getDistanceFromOrigin() > 4.0f * (fine ? fineFactor : 1.0f))
            axisLock = std::abs (delta.x) >= std::abs (delta.y) ? 1 : 2;
        if (axisLock == 1) delta.y = 0.0f;
        if (axisLock == 2) delta.x = 0.0f;
    }
    else
    {
        axisLock = 0;
    }

    if (dragMode == DragMode::width)
    {
        // Drag a width handle away from the centre to widen, towards it to narrow.
        const auto& s = startBands[(size_t) dragging];
        const float x = xForFreq (s.freq * std::pow (2.0f, dragSide * 0.5f * octavesForQ (s.q))) + delta.x;
        const float octaves = std::abs (std::log2 (freqForX (x) / s.freq)) * 2.0f;
        setParam (Band::id (dragging, ParamID::q), juce::jlimit (0.1f, 18.0f, qForOctaves (juce::jmax (0.02f, octaves))));
    }
    else
    {
        const auto& lead = startBands[(size_t) dragging];
        const float ratio = freqForX (xForFreq (lead.freq) + delta.x) / juce::jlimit (20.0f, 20000.0f, lead.freq);
        const float y0 = yForGain (isCut (dragging) ? 0.0f : lead.gainDb);
        const float gainDelta = gainForY (y0 + delta.y) - gainForY (y0);
        const float qFactor = std::pow (2.0f, -delta.y / 60.0f);   // up = narrower / more resonant

        for (int b = 0; b < Band::numBands; ++b)
        {
            if (! isSelected (b)) continue;
            const auto& s = startBands[(size_t) b];
            if (dragQ)
            {
                // Cmd/Ctrl + vertical drag: Q of every selected band.
                setParam (Band::id (b, ParamID::q), juce::jlimit (0.1f, 18.0f, s.q * qFactor));
                continue;
            }
            setParam (Band::id (b, ParamID::freq), juce::jlimit (20.0f, 20000.0f, s.freq * ratio));
            if (isCut (b))
                setParam (Band::id (b, ParamID::q), juce::jlimit (0.1f, 18.0f, s.q * qFactor));   // a cut has no gain: up/down = resonance
            else
                setParam (Band::id (b, ParamID::gain), juce::jlimit (-18.0f, 18.0f, s.gainDb + gainDelta));
        }
    }
    params = processor.currentParams();
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::listen)
    {
        processor.setSolo (-1);
        if (auto* prm = processor.apvts.getParameter (Band::id (listening, ParamID::freq)))
            prm->endChangeGesture();
        listening = -1;
        dragMode = DragMode::none;
        repaint();
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        dragMode = DragMode::none;
        marquee = {};
        if (! moved)
        {
            if (hadSelection)
                selectOnly (-1);           // click on the background: deselect
            else
                createBandAt (e.position); // nothing selected: a click adds a band
        }
        repaint();
        return;
    }

    if (dragging >= 0)
    {
        endGestures();
        if (! moved && dragMode == DragMode::move)
        {
            const bool cmd = e.mods.isCommandDown() || e.mods.isCtrlDown();
            if (cmd && wasSelected && selection != (1u << dragging))
            {
                selection &= ~(1u << dragging);   // Cmd/Ctrl-click on a selected band removes it
                int next = -1;
                for (int b = 0; b < Band::numBands && next < 0; ++b)
                    if (isSelected (b)) next = b;
                setPrimary (next);
            }
            else if (! cmd && ! e.mods.isShiftDown() && wasSelected)
            {
                selectOnly (dragging);            // plain click inside a multi-selection
            }
        }
        dragging = -1;
    }
    dragMode = DragMode::none;
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    int n = nodeAt (e.position);
    if (n < 0 && bubbleBand >= 0 && bubbleBox (bubbleBand).contains (e.position) && ! listenButton (bubbleBand).contains (e.position))
        n = bubbleBand;
    if (n >= 0)
    {
        // The second click of a click that just added this band: not a request to type.
        if (n == createdBand && juce::Time::getMillisecondCounter() - createdAt < 600)
            return;
        openEntry (n, 0);   // type values: Frequency, then Tab to Gain and Q
        return;
    }
    if (plot().contains (e.position) && ! analyzerButton (0).getUnion (analyzerButton (2)).contains (e.position)
        && juce::Time::getMillisecondCounter() - createdAt > 600)
        createBandAt (e.position);
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int n = nodeAt (e.position);
    if (n < 0 && bubbleBand >= 0 && bubbleBox (bubbleBand).contains (e.position))
        n = bubbleBand;
    if (n < 0)
        return;

    const float step = (wheel.deltaY + wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    const bool fine = e.mods.isShiftDown();
    const bool cmd = e.mods.isCommandDown() || e.mods.isCtrlDown();
    if (e.mods.isAltDown())
        return;   // reserved

    // Acts on the band under the mouse, or on the whole selection if that band is part of it.
    const juce::uint32 targets = isSelected (n) ? selection : (1u << n);

    if (! cmd && isCut (n))
    {
        // Scroll over a cut: slope, in steps.
        wheelSlope += step * (fine ? 2.0f : 6.0f);
        if (std::abs (wheelSlope) >= 1.0f)
        {
            const int dir = wheelSlope > 0.0f ? 1 : -1;
            wheelSlope = 0.0f;
            setAsStep (Band::id (n, ParamID::slope), (float) juce::jlimit (0, 3, params.bands[(size_t) n].slope + dir));
        }
    }
    else
    {
        processor.history.performAsOneStep ([&]
        {
            for (int b = 0; b < Band::numBands; ++b)
            {
                if ((targets >> b & 1u) == 0) continue;
                const auto& bp = params.bands[(size_t) b];
                if (cmd)        // Cmd/Ctrl + scroll: gain
                {
                    if (! isCut (b))
                        setAsStep (Band::id (b, ParamID::gain), juce::jlimit (-18.0f, 18.0f, bp.gainDb + step * (fine ? 2.0f : 12.0f)));
                }
                else if (! isCut (b))   // scroll: Q
                {
                    setAsStep (Band::id (b, ParamID::q), juce::jlimit (0.1f, 18.0f, bp.q * std::pow (1.25f, step * (fine ? 1.0f : 4.0f))));
                }
            }
        });
    }
    params = processor.currentParams();
    repaint();
}

void EqDisplay::openEntry (int band, int field)
{
    closeEntry (true);
    const auto fields = isCut (band) ? std::vector<int> { 0, 2 } : std::vector<int> { 0, 1, 2 };
    if (std::find (fields.begin(), fields.end(), field) == fields.end())
        field = 0;
    entryBand = band;
    entryField = field;
    selectOnly (band);

    const auto& b = params.bands[(size_t) band];
    juce::String text = field == 0 ? formatHz (b.freq) : (field == 1 ? juce::String (b.gainDb, 1) : juce::String (b.q, 2));

    entry = std::make_unique<ValueEntry>();
    auto& ed = *entry;
    const auto col = Theme::bandColour (band);
    ed.setFont (Theme::font (13.0f, true));
    ed.setJustification (juce::Justification::centred);
    ed.setColour (juce::TextEditor::backgroundColourId, Theme::raised);
    ed.setColour (juce::TextEditor::textColourId, Theme::text);
    ed.setColour (juce::TextEditor::outlineColourId, col);
    ed.setColour (juce::TextEditor::focusedOutlineColourId, col);
    ed.setColour (juce::TextEditor::highlightColourId, col.withAlpha (0.35f));
    ed.setTextToShowWhenEmpty (field == 0 ? "Frequency" : (field == 1 ? "Gain dB" : "Q"), Theme::faint);
    ed.setText (text, false);
    ed.selectAll();

    const auto box = bubbleBox (band);
    addAndMakeVisible (ed);
    ed.setBounds (box.withSizeKeepingCentre (130.0f, 26.0f).toNearestInt());

    ed.onReturnKey = [this] { closeEntry (true); };
    ed.onEscapeKey = [this] { closeEntry (false); };
    ed.onFocusLost = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<EqDisplay> (this)] { if (safe != nullptr) safe->closeEntry (true); }); };
    ed.onTab = [this, fields] (int dir)
    {
        const int b = entryBand;
        auto it = std::find (fields.begin(), fields.end(), entryField);
        int idx = (int) std::distance (fields.begin(), it) + dir;
        idx = (idx + (int) fields.size()) % (int) fields.size();
        const int next = fields[(size_t) idx];
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<EqDisplay> (this), b, next]
        {
            if (safe != nullptr) safe->openEntry (b, next);
        });
        closeEntry (true);
    };
    ed.grabKeyboardFocus();
    repaint();
}

void EqDisplay::closeEntry (bool commit)
{
    if (entry == nullptr)
        return;
    auto ed = std::move (entry);   // entry is null from here on, so re-entrant calls do nothing
    if (commit && entryBand >= 0)
    {
        const auto t = ed->getText();
        if (t.trim().isNotEmpty())
        {
            if (entryField == 0)
                setAsStep (Band::id (entryBand, ParamID::freq), juce::jlimit (20.0f, 20000.0f, parseHz (t)));
            else if (entryField == 1)
                setAsStep (Band::id (entryBand, ParamID::gain), juce::jlimit (-18.0f, 18.0f, t.getFloatValue()));
            else
                setAsStep (Band::id (entryBand, ParamID::q), juce::jlimit (0.1f, 18.0f, t.getFloatValue()));
        }
    }
    ed->onFocusLost = nullptr;
    removeChildComponent (ed.get());
    params = processor.currentParams();
    juce::MessageManager::callAsync ([e = std::shared_ptr<ValueEntry> (std::move (ed))] {});   // delete after the current callback
    repaint();
}

void EqDisplay::showBandMenu (int band)
{
    const auto& b = params.bands[(size_t) band];
    juce::PopupMenu m;
    m.addSectionHeader (Band::names()[band]);
    m.addItem (1, "Band on", true, b.on);
    if (Band::isEdge (band))
    {
        m.addSeparator();
        m.addItem (10, band == Band::low ? "Low Cut" : "High Cut", true, b.type == 0);
        m.addItem (11, band == Band::low ? "Low Shelf" : "High Shelf", true, b.type == 1);
        juce::PopupMenu slopes;
        for (int i = 0; i < 4; ++i)
            slopes.addItem (20 + i, Band::slopeNames()[i] + "/oct", true, b.slope == i);
        m.addSubMenu ("Cut slope", slopes, b.type == 0);
    }
    m.addSeparator();
    m.addItem (3, "Type values...");
    m.addItem (2, "Reset band");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe = juce::Component::SafePointer<EqDisplay> (this), band] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& d = *safe;
                         if (r == 1)       d.setAsStep (Band::id (band, ParamID::on), d.params.bands[(size_t) band].on ? 0.0f : 1.0f);
                         else if (r == 2)  d.resetBand (band);
                         else if (r == 3)  d.openEntry (band, 0);
                         else if (r >= 10 && r <= 11) d.setAsStep (Band::id (band, ParamID::type), (float) (r - 10));
                         else if (r >= 20) d.setAsStep (Band::id (band, ParamID::slope), (float) (r - 20));
                     });
}

void EqDisplay::showBackgroundMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Select all bands");
    m.addItem (2, "Deselect", selection != 0);
    m.addSeparator();
    m.addItem (3, "Switch all bands off");
    m.addItem (4, "Reset all bands");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe = juce::Component::SafePointer<EqDisplay> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& d = *safe;
                         if (r == 1)      { d.selection = (1u << Band::numBands) - 1u; d.setPrimary (d.selected >= 0 ? d.selected : 1); }
                         else if (r == 2) d.selectOnly (-1);
                         else if (r == 3)
                             d.processor.history.performAsOneStep ([&] { for (int b = 0; b < Band::numBands; ++b) d.setAsStep (Band::id (b, ParamID::on), 0.0f); });
                         else if (r == 4)
                             d.processor.history.performAsOneStep ([&]
                             {
                                 for (int b = 0; b < Band::numBands; ++b)
                                     for (auto* name : { ParamID::on, ParamID::freq, ParamID::gain, ParamID::q, ParamID::type, ParamID::slope })
                                         if (auto* prm = d.processor.apvts.getParameter (Band::id (b, name)))
                                         {
                                             prm->beginChangeGesture();
                                             prm->setValueNotifyingHost (prm->getDefaultValue());
                                             prm->endChangeGesture();
                                         }
                             });
                     });
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
