#include "Display.h"
#include <cstring>
#include <iterator>
#include "../Parameters.h"

namespace
{
    constexpr float minDb = -60.0f, maxDb = 6.0f;

    // Beats per delay division, in the order of the delayDivision choices.
    constexpr float divisionBeats[] = { 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 6.0f, 1.0f / 4.0f, 3.0f / 8.0f, 1.0f / 3.0f,
                                        1.0f / 2.0f, 3.0f / 4.0f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f };
    constexpr float displayBpm = 120.0f;

    juce::String timeLabel (float seconds)
    {
        if (seconds < 1.0f)
            return juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";
        const bool whole = std::abs (seconds - std::round (seconds)) < 0.001f;
        return juce::String (seconds, whole ? 0 : 1) + " s";
    }
}

Display::Display (APVTS& s) : state (s)
{
    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            watched.push_back (state.getRawParameterValue (ranged->paramID));
    lastValues.assign (watched.size(), std::numeric_limits<float>::quiet_NaN());
}

void Display::poll()
{
    bool changed = false;
    for (size_t i = 0; i < watched.size(); ++i)
    {
        const auto v = watched[i]->load();
        if (! juce::exactlyEqual (v, lastValues[i])) { lastValues[i] = v; changed = true; }
    }
    if (changed)
        repaint();
}

float Display::delaySeconds() const
{
    using namespace ParamID;
    if (get (delaySync) > 0.5f)
    {
        const auto index = juce::jlimit (0, (int) std::size (divisionBeats) - 1, juce::roundToInt (get (delayDivision)));
        return divisionBeats[index] * 60.0f / displayBpm;
    }
    return get (delayTimeMs) / 1000.0f;
}

juce::String Display::delayReadout() const
{
    using namespace ParamID;
    if (get (delaySync) > 0.5f)
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (delayDivision)))
            return choice->getCurrentChoiceName();
    return timeLabel (delaySeconds());
}

float Display::reverbLevelAt (float t) const
{
    using namespace ParamID;
    const auto preDelay = get (revPreDelay) / 1000.0f;
    const auto decay = juce::jmax (0.05f, get (revDecay));
    if (t < preDelay)
        return -200.0f;

    const auto tt = t - preDelay;
    const auto mode = juce::roundToInt (get (revMode));   // Hall, Plate, Room, Chamber, Bloom, Cloud, 60s Chamber, 60s Mirror, 90s Hall, 90s Concert
    static constexpr float attackFraction[] = { 0.012f, 0.004f, 0.004f, 0.006f, 0.20f, 0.09f, 0.006f, 0.004f, 0.012f, 0.04f };
    const auto attack = juce::jmax (0.002f, decay * attackFraction[juce::jlimit (0, 9, mode)]);
    const auto attackGain = 1.0f - std::exp (-tt / attack);
    return -60.0f * tt / decay + juce::Decibels::gainToDecibels (attackGain, -200.0f);
}

void Display::paint (juce::Graphics& g)
{
    using namespace ParamID;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::display);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);

    const bool isDelayOn = get (ParamID::delayOn) > 0.5f;
    const bool isShimOn = get (ParamID::shimOn) > 0.5f;
    const bool isRevOn = get (ParamID::revOn) > 0.5f;

    // Header line: legend on the left, readouts on the right.
    auto content = bounds.reduced (18.0f, 12.0f);
    auto header = content.removeFromTop (18.0f);
    {
        auto legend = header;
        const std::pair<const char*, std::pair<juce::Colour, bool>> items[] = {
            { "DELAY", { Theme::delay, isDelayOn } }, { "SHIMMER", { Theme::shimmer, isShimOn } }, { "REVERB", { Theme::reverb, isRevOn } }
        };
        g.setFont (Theme::caps (10.0f));
        for (auto& [name, info] : items)
        {
            const auto colour = info.second ? info.first : Theme::faint;
            g.setColour (colour);
            g.fillEllipse (legend.removeFromLeft (7.0f).withSizeKeepingCentre (7.0f, 7.0f));
            legend.removeFromLeft (6.0f);
            const auto w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 4.0f;
            g.setColour (info.second ? Theme::muted : Theme::faint);
            g.drawText (name, legend.removeFromLeft (w), juce::Justification::centredLeft, false);
            legend.removeFromLeft (16.0f);
        }

        auto readouts = header;
        const auto pitch = [this] (const char* id) { auto v = get (id); return (v > 0 ? "+" : "") + juce::String (v, 1); };
        const std::pair<juce::String, juce::Colour> values[] = {
            { "RT60 " + timeLabel (get (revDecay)), isRevOn ? Theme::reverb : Theme::faint },
            { "PITCH " + pitch (shimPitchA) + " / " + pitch (shimPitchB), isShimOn ? Theme::shimmer : Theme::faint },
            { "ECHO " + delayReadout(), isDelayOn ? Theme::delay : Theme::faint },
        };
        g.setFont (Theme::caps (10.5f, 0.08f));
        for (auto& [text, colour] : values)
        {
            const auto w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 4.0f;
            g.setColour (colour);
            g.drawText (text, readouts.removeFromRight (w), juce::Justification::centredRight, false);
            readouts.removeFromRight (18.0f);
        }
    }

    content.removeFromTop (6.0f);
    const auto axis = content.removeFromBottom (16.0f);
    const auto plot = content;

    // Time window that fits the longest active tail.
    const auto echo = delaySeconds();
    const auto feedback = get (delayFreeze) > 0.5f ? 1.0f : juce::jlimit (0.0f, 1.0f, get (delayFeedback) / 100.0f);
    float window = 1.0f;
    if (isRevOn)   window = juce::jmax (window, get (revPreDelay) / 1000.0f + get (revDecay) * 1.05f);
    if (isDelayOn) window = juce::jmax (window, echo * 7.5f);
    window = juce::jmin (window, 24.0f);

    const auto xFor = [&] (float t) { return plot.getX() + t / window * plot.getWidth(); };
    const auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, plot.getBottom(), plot.getY()); };

    // Grid.
    {
        static constexpr float steps[] = { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f };
        float step = 5.0f;
        for (auto s : steps)
            if (window / s <= 8.0f) { step = s; break; }

        g.setFont (Theme::font (10.5f));
        for (float t = 0.0f; t <= window + 0.0001f; t += step)
        {
            const auto x = xFor (t);
            g.setColour (Theme::line.withAlpha (t == 0.0f ? 0.9f : 0.45f));
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            g.setColour (Theme::faint);
            g.drawText (timeLabel (t), juce::Rectangle<float> (x + 4.0f, axis.getY(), 60.0f, axis.getHeight()),
                        juce::Justification::centredLeft, false);
        }
        for (float db : { 0.0f, -20.0f, -40.0f })
        {
            g.setColour (Theme::line.withAlpha (0.4f));
            g.drawHorizontalLine (juce::roundToInt (yFor (db)), plot.getX(), plot.getRight());
        }
    }

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (plot.expanded (0.0f, 6.0f).toNearestInt());

    // Reverb envelope.
    if (isRevOn)
    {
        const auto level = get (revLevel);
        juce::Path envelope;
        envelope.startNewSubPath (plot.getX(), plot.getBottom());
        constexpr int numPoints = 240;
        for (int i = 0; i <= numPoints; ++i)
        {
            const auto t = window * (float) i / (float) numPoints;
            envelope.lineTo (xFor (t), yFor (reverbLevelAt (t) + level - 3.0f));
        }
        juce::Path fill (envelope);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Theme::reverb.withAlpha (0.32f), 0.0f, plot.getY(),
                                                 Theme::reverb.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (Theme::reverb.withAlpha (0.9f));
        g.strokePath (envelope, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Early reflections for the rooms that have a distinct first bounce.
        const auto mode = juce::roundToInt (get (revMode));
        if (mode == 0 || mode == 2 || mode == 3 || mode == 6 || mode == 7)
        {
            juce::Random rng (11 + mode);
            const auto spread = 0.02f + 0.08f * get (revSize) / 100.0f;
            for (int i = 0; i < 9; ++i)
            {
                const auto t = get (revPreDelay) / 1000.0f + rng.nextFloat() * spread;
                const auto x = xFor (t);
                g.setColour (Theme::reverb.withAlpha (0.55f));
                g.drawLine (x, plot.getBottom(), x, yFor (level - 4.0f - rng.nextFloat() * 14.0f), 1.2f);
            }
        }
    }

    // Dry hit.
    g.setColour (Theme::text.withAlpha (0.55f));
    g.drawLine (xFor (0.0f) + 1.0f, plot.getBottom(), xFor (0.0f) + 1.0f, yFor (0.0f), 2.0f);

    // Delay taps.
    if (isDelayOn && echo > 0.0f)
    {
        const auto level = get (delayLevel);
        const bool pingPong = juce::roundToInt (get (delayMode)) == 1;
        for (int n = 1; n < 200; ++n)
        {
            const auto t = echo * (float) n;
            const auto db = level - 2.0f + (feedback > 0.0f ? juce::Decibels::gainToDecibels (std::pow (feedback, (float) (n - 1))) : (n > 1 ? -200.0f : 0.0f));
            if (t > window || db < minDb)
                break;

            const auto x = xFor (t);
            const auto y = yFor (db);
            const auto colour = pingPong && n % 2 == 0 ? Theme::delay.interpolatedWith (Theme::text, 0.35f) : Theme::delay;
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.85f), x, y, colour.withAlpha (0.05f), x, plot.getBottom(), false));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, y, 2.0f, plot.getBottom() - y));
            g.setColour (colour.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ x, y }));
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, y }));
        }
    }

    // Shimmer grains, riding above the wet signal and climbing with pitch feedback.
    if (isShimOn)
    {
        juce::Random rng (7);
        const auto pitchFeedback = get (shimFeedback) / 100.0f;
        const auto spray = get (shimSpray) / 100.0f;
        const auto reverse = juce::roundToInt (get (shimReverse));
        const auto numGrains = 8 + juce::roundToInt (get (shimDensity) * 0.4f);
        const auto grainWidth = juce::jlimit (4.0f, 70.0f, get (shimGrainMs) / 1000.0f / window * plot.getWidth());
        const auto pxPerSemitone = plot.getHeight() / 110.0f;

        for (int i = 0; i < numGrains; ++i)
        {
            int generation = 1;
            if (rng.nextFloat() < pitchFeedback) ++generation;
            if (generation == 2 && rng.nextFloat() < pitchFeedback) ++generation;

            const auto t = ((float) i + 0.5f) / (float) numGrains * window * 0.9f
                         + (rng.nextFloat() - 0.5f) * spray * window * 0.2f;
            if (t < 0.0f)
                continue;

            const auto base = isRevOn ? juce::jmax (-42.0f, reverbLevelAt (t) + get (revLevel) - 3.0f) : -14.0f;
            const auto db = base + (get (shimLevel) + 6.0f) * 0.5f - (float) (generation - 1) * (1.0f - pitchFeedback) * 14.0f;
            const auto pitch = get (i % 2 == 0 ? shimPitchA : shimPitchB) * (float) generation;
            const auto y = yFor (db) - juce::jlimit (-36.0f, 36.0f, pitch) * pxPerSemitone
                         + (rng.nextFloat() - 0.5f) * spray * plot.getHeight() * 0.25f;
            const auto x = xFor (t);
            const bool reversed = reverse == 1 || (reverse == 2 && rng.nextBool());
            const auto alpha = 1.0f - 0.28f * (float) (generation - 1);

            const auto grain = juce::Rectangle<float> (x, y - 1.75f, grainWidth, 3.5f);
            const auto bright = Theme::shimmer.withAlpha (alpha);
            const auto dim = Theme::shimmer.withAlpha (alpha * 0.08f);
            g.setGradientFill (juce::ColourGradient (reversed ? dim : bright, grain.getX(), y,
                                                     reversed ? bright : dim, grain.getRight(), y, false));
            g.fillRoundedRectangle (grain, 1.75f);
        }
    }
}
