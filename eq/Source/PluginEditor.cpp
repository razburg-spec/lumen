#include "PluginEditor.h"

namespace
{
    constexpr int margin = 20;
    constexpr int headerHeight = 76;
    constexpr int toolbarHeight = 44;
    constexpr int footerHeight = 30;
    constexpr int gap = 12;
    constexpr int displayHeight = 330;

    rba::ActivationStyle activationStyle (juce::Colour warning)
    {
        rba::ActivationStyle s { Theme::bg, Theme::surface, Theme::raised, Theme::line, Theme::text, Theme::muted, Theme::faint,
                                 Theme::brandBlue, Theme::brandTop, Theme::brandBottom, Theme::brandCyan, warning, {} };
        s.drawLogo = [] (juce::Graphics& g, juce::Rectangle<float> area) { drawBrandMark (g, area); };
        return s;
    }
}

//==============================================================================
PrismView::PrismView (PrismProcessor& p)
    : processor (p),
      modes (*p.apvts.getParameter (ParamID::mode)),
      display (p),
      drive  (p.apvts, ParamID::drive,  "Drive",  Theme::modeColour (4), Knob::Size::Large),
      output (p.apvts, ParamID::output, "Output", Theme::modeColour (4), Knob::Size::Large, true),
      linearAttachment (p.apvts, ParamID::linear, linearPhase),
      presets (p.apvts, p.history),
      licence (*p.licence),
      licenceBadge (*p.licence, activationStyle (Theme::modeColour (0)))
{
    addAndMakeVisible (modes);
    addAndMakeVisible (display);
    addAndMakeVisible (linearPhase);
    linearPhase.setTooltip ("Linear Phase: no phase shift at all, for mastering and parallel tracks. Adds latency (the host compensates).");
    for (int b = 0; b < Band::numBands; ++b)
    {
        auto* s = strips.add (new BandStrip (p.apvts, b));
        s->onSelect = [this] (int band) { selectBand (band); };
        addAndMakeVisible (s);
    }
    display.onSelect = [this] (int band) { selectBand (band); };

    for (auto* c : std::initializer_list<juce::Component*> { &drive, &output, &inMeter, &outMeter })
        addAndMakeVisible (c);
    drive.slider.setTooltip ("Pushes the mode's analog stage: harmonics, weight and softness in the voice of the selected mode");
    output.slider.setTooltip ("Output level");

    modes.onChange = [this] (int m) { applyModeColour (m); };
    applyModeColour (modes.getIndex());

    // Toolbar
    for (auto* c : std::initializer_list<juce::Component*> { &presets, &undoButton, &redoButton, &resetButton, &helpButton })
        addAndMakeVisible (c);
    undoButton.setTooltip ("Undo (up to 100 steps)");
    redoButton.setTooltip ("Redo");
    resetButton.setTooltip ("Reset everything to the initial state: flat, no processing");
    helpButton.setTooltip ("User guide");
    undoButton.onClick = [this] { processor.history.undo(); };
    redoButton.onClick = [this] { processor.history.redo(); };
    resetButton.onClick = [this] { presets.resetAll(); };
    helpButton.onClick = [this] { showManual (manual == nullptr); };
    processor.history.addChangeListener (this);
    updateHistoryButtons();

    addChildComponent (update);
    update.onVisibilityChanged = [this] { resized(); repaint(); };

    addAndMakeVisible (licenceBadge);
    licenceBadge.onClick = [this] { showActivation (true); };

    // Ask straight away whenever Prism is not activated on this computer.
    if (! licence.isActivated())
        showActivation (true);
}

PrismView::~PrismView()
{
    processor.history.removeChangeListener (this);
}

void PrismView::selectBand (int band)
{
    display.setSelectedBand (band);
    for (int b = 0; b < strips.size(); ++b)
        strips[b]->setSelected (b == band);
}

void PrismView::applyModeColour (int mode)
{
    const auto c = Theme::modeColour (mode);
    drive.setAccent (c);
    output.setAccent (c);
    linearPhase.setAccent (c);
    inMeter.setAccent (c);
    outMeter.setAccent (c);
    display.repaint();
    repaint();
}

void PrismView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateHistoryButtons();
}

void PrismView::updateHistoryButtons()
{
    undoButton.setEnabled (processor.history.canUndo());
    redoButton.setEnabled (processor.history.canRedo());
}

void PrismView::showManual (bool show)
{
    if (! show)
    {
        manual.reset();
        return;
    }
    manual = std::make_unique<ManualPanel>();
    manual->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PrismView> (this)]
                                                                 { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*manual);
    manual->setBounds (getLocalBounds());
    manual->grabKeyboardFocus();
    if (activation != nullptr)
        activation->toFront (false);
}

void PrismView::showActivation (bool show)
{
    if (! show)
    {
        activation.reset();
        return;
    }
    activation = std::make_unique<rba::ActivationPanel> (licence, activationStyle (Theme::modeColour (0)));
    activation->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PrismView> (this)]
                                                                     { if (safe != nullptr) safe->showActivation (false); }); };
    addAndMakeVisible (*activation);
    activation->setBounds (getLocalBounds());
}

void PrismView::poll()
{
    // The display runs at 60 frames a second; meters and strips at 30.
    display.poll();
    if ((++frame & 1) != 0)
        return;
    auto& m = processor.meters();
    inMeter.setLevel (m.inputPeak.exchange (0.0f));
    outMeter.setLevel (m.outputPeak.exchange (0.0f));
    for (auto* s : strips)
        s->refresh();
}

void PrismView::paint (juce::Graphics& g)
{
    const int mode = modes.getIndex();
    const auto accent = Theme::modeColour (mode);
    g.fillAll (Theme::bg);

    // Soft glow behind the header, tinted by the mode.
    g.setGradientFill (juce::ColourGradient (Theme::brandTop.withAlpha (0.18f), (float) getWidth() * 0.5f, -40.0f,
                                             Theme::bg.withAlpha (0.0f), (float) getWidth() * 0.5f, 220.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (240));
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.08f), (float) getWidth() * 0.75f, 0.0f,
                                             accent.withAlpha (0.0f), (float) getWidth() * 0.75f, 260.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (260));

    // Brand block.
    const auto mark = juce::Rectangle<float> ((float) margin, 18.0f, 40.0f, 40.0f);
    drawBrandMark (g, mark);
    const auto textX = mark.getRight() + 14.0f;
    g.setColour (Theme::text);
    g.setFont (Theme::font (25.0f).withExtraKerningFactor (0.32f));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(), juce::Rectangle<float> (textX, 15.0f, 240.0f, 28.0f), juce::Justification::centredLeft, false);
    g.setColour (Theme::brandCyan.withAlpha (0.85f));
    g.setFont (Theme::caps (9.5f, 0.22f));
    g.drawText ("RAZ BURG AUDIOCRAFTS", juce::Rectangle<float> (textX + 1.0f, 43.0f, 260.0f, 14.0f), juce::Justification::centredLeft, false);

    // Mode caption under the selector.
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.0f, 0.2f));
    g.drawText ("MULTI-CHARACTER EQ", juce::Rectangle<int> (modes.getX(), modes.getBottom() + 4, 160, 14).toFloat(), juce::Justification::centredLeft, false);
    g.setColour (Theme::muted);
    g.setFont (Theme::font (11.5f));
    g.drawText (modeBlurb (mode), juce::Rectangle<int> (modes.getX() + 150, modes.getBottom() + 3, modes.getWidth() - 150, 16).toFloat(),
                juce::Justification::centredRight, true);

    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.5f));
    g.drawText ("PRESET", juce::Rectangle<int> (margin, presets.getY(), 60, presets.getHeight()).toFloat(), juce::Justification::centredLeft, false);

    paintCard (g, outputCard.toFloat(), accent, true);
    paintCardTitle (g, outputCard.reduced (14, 0).removeFromTop (40).toFloat().withTrimmedTop (4.0f), "OUTPUT", {}, true);

    // Footer.
    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (margin, 0).toFloat();
    g.setFont (Theme::caps (9.5f, 0.14f));
    g.setColour (Theme::faint);
    g.drawText (juce::String::fromUTF8 ("RAZ BURG AUDIOCRAFTS  \xc2\xb7  ") + juce::String (JucePlugin_Name).toUpperCase() + " " + JucePlugin_VersionString,
                footer, juce::Justification::centredLeft, false);
    g.setFont (Theme::font (11.0f));
    if (! update.isVisible())   // the update pill takes this spot when shown
        g.drawText (juce::String::fromUTF8 ("Drag: freq + gain  \xc2\xb7  Shift: fine  \xc2\xb7  Option/Alt: one axis  \xc2\xb7  Cmd/Ctrl-drag or scroll: Q  \xc2\xb7  Hover a point, hold the speaker: listen"),
                footer, juce::Justification::centredRight, false);
}

void PrismView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (margin, 0);
    auto toolbar = r.removeFromTop (toolbarHeight).reduced (margin, 0).withTrimmedBottom (10);
    {
        const auto footer = r.removeFromBottom (footerHeight).reduced (margin, 0);
        update.setBounds (footer.withLeft (footer.getRight() - update.getIdealWidth()).withSizeKeepingCentre (update.getIdealWidth(), 24));
    }
    r = r.reduced (margin, 0);

    modes.setBounds (header.removeFromRight (600).withSizeKeepingCentre (600, 34).translated (0, -6));
    header.removeFromRight (18);
    licenceBadge.setBounds (header.removeFromRight (124).withSizeKeepingCentre (124, 26).translated (0, -6));
    if (activation != nullptr) activation->setBounds (getLocalBounds());
    if (manual != nullptr)     manual->setBounds (getLocalBounds());

    toolbar.removeFromLeft (64);
    presets.setBounds (toolbar.removeFromLeft (360));
    helpButton.setBounds (toolbar.removeFromRight (34));
    toolbar.removeFromRight (16);
    resetButton.setBounds (toolbar.removeFromRight (70));
    toolbar.removeFromRight (8);
    redoButton.setBounds (toolbar.removeFromRight (34));
    toolbar.removeFromRight (6);
    undoButton.setBounds (toolbar.removeFromRight (34));

    display.setBounds (r.removeFromTop (displayHeight));
    linearPhase.setBounds (display.getRight() - 30 - 12 - 116, display.getY() + 10, 116, 22);
    r.removeFromTop (gap);
    r.removeFromBottom (4);

    outputCard = r.removeFromRight (196);
    r.removeFromRight (gap);
    const int stripW = (r.getWidth() - gap * (Band::numBands - 1)) / Band::numBands;
    for (int b = 0; b < Band::numBands; ++b)
    {
        strips[b]->setBounds (r.removeFromLeft (stripW));
        r.removeFromLeft (gap);
    }

    auto o = outputCard.reduced (10, 0).withTrimmedTop (42).withTrimmedBottom (12);
    auto meters = o.removeFromRight (88);
    inMeter.setBounds (meters.removeFromLeft (44));
    outMeter.setBounds (meters);
    drive.setBounds (o.removeFromTop (o.getHeight() / 2));
    output.setBounds (o);
    repaint();
}

//==============================================================================
PrismEditor::PrismEditor (PrismProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    view.setLookAndFeel (&lookAndFeel);
    view.setBounds (0, 0, PrismView::designWidth, PrismView::designHeight);
    addAndMakeVisible (view);

    constexpr double aspect = (double) PrismView::designWidth / (double) PrismView::designHeight;
    setResizable (true, true);
    setResizeLimits (PrismView::designWidth * 6 / 10, PrismView::designHeight * 6 / 10,
                     PrismView::designWidth * 2, PrismView::designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (aspect);
    setSize (PrismView::designWidth, PrismView::designHeight);

    startTimerHz (60);
}

PrismEditor::~PrismEditor()
{
    view.setLookAndFeel (nullptr);
}

void PrismEditor::resized()
{
    const auto scale = (float) getWidth() / (float) PrismView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
}
