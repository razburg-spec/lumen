#include "Sections.h"
#include "../Parameters.h"

namespace
{
    constexpr int padding = 14;
    constexpr int headerHeight = 28;
    constexpr int controlRowHeight = 26;
    constexpr int advancedRowHeight = 70;
    constexpr int advancedRows = 2;
    constexpr int advancedColumns = 3;

    void paintSectionBackground (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, bool on)
    {
        g.setColour (Theme::surface);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        // Accent hairline along the top edge identifies the engine.
        auto strip = r.reduced (18.0f, 0.0f).removeFromTop (2.0f);
        juce::ColourGradient grad (accent.withAlpha (0.0f), strip.getX(), 0.0f, accent.withAlpha (0.0f), strip.getRight(), 0.0f, false);
        grad.addColour (0.5, accent.withAlpha (on ? 0.9f : 0.25f));
        g.setGradientFill (grad);
        g.fillRect (strip);
    }
}

//==============================================================================
EngineSection::EngineSection (APVTS& s, const juce::String& titleText, const juce::String& subtitleText,
                              juce::Colour accentColour, const char* onParamID)
    : state (s), accent (accentColour), title (titleText), subtitle (subtitleText),
      onParam (s.getRawParameterValue (onParamID)),
      power (accentColour), powerAttachment (s, onParamID, power)
{
    addAndMakeVisible (power);
    power.setTooltip ("Engine on/off");
}

Knob& EngineSection::addMain (const char* paramID, const juce::String& name, bool bipolar)
{
    auto* knob = mainKnobs.add (new Knob (state, paramID, name, accent, Knob::Size::Large, bipolar));
    addAndMakeVisible (knob);
    return *knob;
}

Knob& EngineSection::addAdvanced (const char* paramID, const juce::String& name, bool bipolar)
{
    auto* knob = advancedKnobs.add (new Knob (state, paramID, name, accent, Knob::Size::Small, bipolar));
    addChildComponent (knob);
    return *knob;
}

void EngineSection::addCombo (const char* paramID)
{
    auto* box = combos.add (new juce::ComboBox());
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
        box->addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    comboAttachments.add (new APVTS::ComboBoxAttachment (state, paramID, *box));
}

void EngineSection::addPill (const char* paramID, const juce::String& text)
{
    auto* pill = pills.add (new PillToggle (text, accent));
    addAndMakeVisible (pill);
    pillAttachments.add (new APVTS::ButtonAttachment (state, paramID, *pill));
}

void EngineSection::setAdvanced (bool shouldShow)
{
    advanced = shouldShow;
    for (auto* k : advancedKnobs)
        k->setVisible (advanced);
    resized();
    repaint();
}

void EngineSection::poll()
{
    // Controls of a bypassed engine stay usable but recede.
    const bool on = onParam->load() > 0.5f;
    if (on == wasOn)
        return;
    wasOn = on;
    for (auto* k : mainKnobs)     k->setAlpha (on ? 1.0f : 0.4f);
    for (auto* k : advancedKnobs) k->setAlpha (on ? 1.0f : 0.4f);
    repaint();
}

void EngineSection::paint (juce::Graphics& g)
{
    const bool on = onParam->load() > 0.5f;
    paintSectionBackground (g, getLocalBounds().toFloat(), accent, on);

    auto t = titleArea.toFloat();
    g.setColour (on ? Theme::text : Theme::muted);
    g.setFont (Theme::caps (14.0f, 0.16f));
    const auto titleWidth = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title) + 4.0f;
    g.drawText (title, t.removeFromLeft (titleWidth), juce::Justification::centredLeft, false);
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (10.0f));
    g.drawText (subtitle, t.withTrimmedLeft (8.0f), juce::Justification::centredLeft, true);

    if (advanced && dividerY > 0)
    {
        const auto x0 = (float) padding, x1 = (float) (getWidth() - padding);
        g.setFont (Theme::caps (9.5f));
        const auto label = juce::String ("ADVANCED");
        const auto w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 16.0f;
        const auto mid = (float) getWidth() * 0.5f;
        g.setColour (Theme::line);
        g.drawHorizontalLine (dividerY, x0, mid - w * 0.5f);
        g.drawHorizontalLine (dividerY, mid + w * 0.5f, x1);
        g.setColour (Theme::faint);
        g.drawText (label, juce::Rectangle<float> (mid - w * 0.5f, (float) dividerY - 7.0f, w, 14.0f), juce::Justification::centred, false);
    }
}

void EngineSection::resized()
{
    auto r = getLocalBounds().reduced (padding);
    r.removeFromTop (2);

    auto header = r.removeFromTop (headerHeight);
    power.setBounds (header.removeFromRight (headerHeight).withSizeKeepingCentre (24, 24));
    header.removeFromRight (6);
    for (int i = pills.size(); --i >= 0;)
    {
        pills[i]->setBounds (header.removeFromRight (58).withSizeKeepingCentre (58, 22));
        header.removeFromRight (5);
    }
    titleArea = header;

    r.removeFromTop (8);
    auto row = r.removeFromTop (controlRowHeight);
    if (! combos.isEmpty())
    {
        const auto gap = 6;
        const auto w = (row.getWidth() - gap * (combos.size() - 1)) / combos.size();
        for (auto* box : combos)
        {
            box->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (gap);
        }
    }
    r.removeFromTop (10);

    dividerY = -1;
    if (advanced)
    {
        auto area = r.removeFromBottom (advancedRows * advancedRowHeight + 18);
        dividerY = area.getY() + 6;
        area.removeFromTop (18);
        const auto cellW = area.getWidth() / advancedColumns;
        for (int i = 0; i < advancedKnobs.size(); ++i)
            advancedKnobs[i]->setBounds (area.getX() + (i % advancedColumns) * cellW,
                                         area.getY() + (i / advancedColumns) * advancedRowHeight,
                                         cellW, advancedRowHeight);
        r.removeFromBottom (6);
    }

    const auto rows = juce::jmax (1, (mainKnobs.size() + 1) / 2);
    const auto cellW = r.getWidth() / 2;
    const auto cellH = r.getHeight() / rows;
    for (int i = 0; i < mainKnobs.size(); ++i)
        mainKnobs[i]->setBounds (juce::Rectangle<int> (r.getX() + (i % 2) * cellW, r.getY() + (i / 2) * cellH, cellW, cellH).reduced (6, 2));
}

//==============================================================================
DelaySection::DelaySection (APVTS& s)
    : EngineSection (s, "Delay", "ECHO", Theme::delay, ParamID::delayOn),
      time (addMain (ParamID::delayTimeMs, "Time")),
      division (s, ParamID::delayDivision, "Time", Theme::delay)
{
    addChildComponent (division);
    addMain (ParamID::delayFeedback, "Feedback");
    addMain (ParamID::delayHighCut, "Tone");
    addMain (ParamID::delayLevel, "Level");

    addAdvanced (ParamID::delayLowCut, "Low-Cut");
    addAdvanced (ParamID::delayDrive, "Drive");
    addAdvanced (ParamID::delayDuck, "Ducking");
    addAdvanced (ParamID::delayModRate, "Mod Rate");
    addAdvanced (ParamID::delayModDepth, "Mod Depth");

    addCombo (ParamID::delayMode);
    addCombo (ParamID::delayChar);
    addPill (ParamID::delaySync, "Sync");
    addPill (ParamID::delayFreeze, "Freeze");
    poll();
}

void DelaySection::poll()
{
    EngineSection::poll();
    const bool synced = state.getRawParameterValue (ParamID::delaySync)->load() > 0.5f;
    time.setVisible (! synced);
    division.setVisible (synced);
    division.setAlpha (time.getAlpha());
}

void DelaySection::resized()
{
    EngineSection::resized();
    division.setBounds (time.getBounds());
}

ShimmerSection::ShimmerSection (APVTS& s)
    : EngineSection (s, "Shimmer", "PITCH / GRAIN", Theme::shimmer, ParamID::shimOn)
{
    addMain (ParamID::shimPitchA, "Pitch A", true);
    addMain (ParamID::shimPitchB, "Pitch B", true);
    addMain (ParamID::shimFeedback, "Feedback");
    addMain (ParamID::shimLevel, "Level");

    addAdvanced (ParamID::shimGrainMs, "Grain");
    addAdvanced (ParamID::shimDensity, "Density");
    addAdvanced (ParamID::shimSpray, "Spray");

    addCombo (ParamID::shimReverse);
    poll();
}

ReverbSection::ReverbSection (APVTS& s)
    : EngineSection (s, "Reverb", "SPACE", Theme::reverb, ParamID::revOn)
{
    addMain (ParamID::revDecay, "Decay");
    addMain (ParamID::revSize, "Size");
    addMain (ParamID::revPreDelay, "Pre-Delay");
    addMain (ParamID::revLevel, "Level");

    addAdvanced (ParamID::revDiffusion, "Diffusion");
    addAdvanced (ParamID::revModRate, "Mod Rate");
    addAdvanced (ParamID::revModDepth, "Mod Depth");
    addAdvanced (ParamID::revLowCut, "Low-Cut");
    addAdvanced (ParamID::revHighCut, "High-Cut");
    addAdvanced (ParamID::revWidth, "Width");

    addCombo (ParamID::revMode);
    addCombo (ParamID::revColor);
    combos[0]->setTooltip ("Type of space");
    combos[1]->setTooltip ("Era: voices the reverb after the studio reverbs of each decade, on every mode");
    poll();
}

//==============================================================================
MasterSection::MasterSection (APVTS& s)
    : mix (s, ParamID::mix, "Mix", Theme::master),
      crossFeed (s, ParamID::crossFeed, "Cross-Feed", Theme::master, Knob::Size::Small),
      coloration (s, ParamID::coloration, "Coloration", Theme::master, Knob::Size::Small, true),
      output (s, ParamID::output, "Output", Theme::master)
{
    addAndMakeVisible (mix);
    addAndMakeVisible (crossFeed);
    addAndMakeVisible (coloration);
    coloration.slider.setTooltip ("Dark adds warm low-order harmonics and softens the top. Shine adds bright upper harmonics.");
    addAndMakeVisible (output);
}

void MasterSection::paint (juce::Graphics& g)
{
    paintSectionBackground (g, getLocalBounds().toFloat(), Theme::master, true);
    g.setColour (Theme::text);
    g.setFont (Theme::caps (14.0f, 0.16f));
    g.drawText ("OUTPUT", getLocalBounds().reduced (padding).withTrimmedTop (2).removeFromTop (headerHeight),
                juce::Justification::centredLeft, false);
}

void MasterSection::resized()
{
    auto r = getLocalBounds().reduced (padding);
    r.removeFromTop (2 + headerHeight + 8);
    const auto h = (float) r.getHeight();
    mix.setBounds (r.removeFromTop (juce::roundToInt (h * 0.32f)).reduced (2));
    crossFeed.setBounds (r.removeFromTop (juce::roundToInt (h * 0.20f)).reduced (14, 2));
    coloration.setBounds (r.removeFromTop (juce::roundToInt (h * 0.20f)).reduced (14, 2));
    output.setBounds (r.reduced (2));
}
