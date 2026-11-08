#include "PluginEditor.h"

namespace
{
    constexpr int margin = 20;
    constexpr int headerHeight = 72;
    constexpr int toolbarHeight = 44;
    constexpr int footerHeight = 30;
    constexpr int gap = 14;
    constexpr int titleHeight = 40;
}

//==============================================================================
ChromaView::ChromaView (ChromaProcessor& p)
    : processor (p), state (p.apvts),
      dial (*p.apvts.getParameter (ParamID::character)),
      input      (p.apvts, ParamID::input,      "Input",      Theme::characterColour (0), Knob::Size::Large, true),
      output     (p.apvts, ParamID::output,     "Output",     Theme::characterColour (0), Knob::Size::Large, true),
      attack     (p.apvts, ParamID::attack,     "Attack",     Theme::characterColour (0)),
      release    (p.apvts, ParamID::release,    "Release",    Theme::characterColour (0)),
      ratio      (p.apvts, ParamID::ratio,      "Ratio",      Theme::characterColour (0)),
      saturation (p.apvts, ParamID::saturation, "Saturation", Theme::characterColour (0)),
      parallel   (p.apvts, ParamID::parallel,   "Parallel",   Theme::characterColour (0)),
      keyFreq    (p.apvts, ParamID::keyFreq,    "Freq",       Theme::key),
      keyPowerAttachment (p.apvts, ParamID::keyOn, keyPower),
      keySource (*p.apvts.getParameter (ParamID::keySource), { "External", "Freq Key" }, Theme::key),
      licence (*p.licence),
      presets (p.apvts, p.history)
{
    for (auto* c : std::initializer_list<juce::Component*> { &dial, &grMeter, &inMeter, &outMeter, &input, &output, &attack, &release,
                                                             &ratio, &saturation, &parallel, &keyFreq, &keyPower, &keySource })
        addAndMakeVisible (c);

    softClip = &addPill (ParamID::softClip, "Soft Clip", Theme::characterColour (0));
    listen = &addPill (ParamID::keyListen, "Listen", Theme::key);

    input.slider.setTooltip ("Drives the signal into the compressor: more Input, more compression");
    output.slider.setTooltip ("Level of the processed signal (make-up gain)");
    attack.slider.setTooltip ("How fast the compressor grabs");
    release.slider.setTooltip ("How fast it lets go");
    ratio.slider.setTooltip ("How hard it compresses. 1:1 = no compression");
    saturation.slider.setTooltip ("Harmonic saturation in the voice of the selected character");
    parallel.slider.setTooltip ("Blend of the untouched source (0%) and the processed signal (100%)");
    keyFreq.slider.setTooltip ("Freq Key: the band (50 Hz - 16 kHz) that triggers the compressor");
    keyPower.setTooltip ("Key: trigger the compressor from the sidechain or from one frequency band");
    softClip->setTooltip ("Soft clipper on the output: rounds off peaks above about -4 dBFS");
    listen->setTooltip ("Hear what triggers the compressor");

    dial.onChange = [this] (int c) { applyCharacterColour (c); };
    applyCharacterColour (dial.getIndex());

    // Toolbar
    for (auto* c : std::initializer_list<juce::Component*> { &presets, &undoButton, &redoButton, &resetButton, &helpButton })
        addAndMakeVisible (c);
    undoButton.setTooltip ("Undo (up to 100 steps)");
    redoButton.setTooltip ("Redo");
    resetButton.setTooltip ("Reset everything to the initial state: no processing");
    helpButton.setTooltip ("User guide");
    undoButton.onClick = [this] { processor.history.undo(); };
    redoButton.onClick = [this] { processor.history.redo(); };
    resetButton.onClick = [this] { presets.resetAll(); };
    helpButton.onClick = [this] { showManual (manual == nullptr); };
    processor.history.addChangeListener (this);
    updateHistoryButtons();

    addAndMakeVisible (licenceBadge);
    licenceBadge.onClick = [this] { showActivation (true); };
    licence.addChangeListener (this);
    changeListenerCallback (nullptr);

    const auto licenceState = licence.getStatus().state;
    if (licenceState != LicenseManager::State::licensed && licenceState != LicenseManager::State::trial)
        showActivation (true);
}

ChromaView::~ChromaView()
{
    processor.history.removeChangeListener (this);
    licence.removeChangeListener (this);
}

PillToggle& ChromaView::addPill (const char* paramID, const juce::String& text, juce::Colour accent)
{
    auto* pill = pills.add (new PillToggle (text, accent));
    addAndMakeVisible (pill);
    pillAttachments.add (new APVTS::ButtonAttachment (state, paramID, *pill));
    return *pill;
}

void ChromaView::applyCharacterColour (int character)
{
    const auto c = Theme::characterColour (character);
    for (auto* k : { &input, &output, &attack, &release, &ratio, &saturation, &parallel })
        k->setAccent (c);
    inMeter.setAccent (c);
    outMeter.setAccent (c);

    if (softClip != nullptr)
        softClip->setAccent (c);
    repaint();
}

void ChromaView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.history)
    {
        updateHistoryButtons();
        return;
    }
    licenceBadge.setStatus (licence.getStatus());
}

void ChromaView::updateHistoryButtons()
{
    undoButton.setEnabled (processor.history.canUndo());
    redoButton.setEnabled (processor.history.canRedo());
}

void ChromaView::showManual (bool show)
{
    if (! show)
    {
        manual.reset();
        return;
    }
    manual = std::make_unique<ManualPanel>();
    manual->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ChromaView> (this)]
                                                                 { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*manual);
    manual->setBounds (getLocalBounds());
    manual->grabKeyboardFocus();
    if (activation != nullptr)
        activation->toFront (false);
}

void ChromaView::showActivation (bool show)
{
    if (! show)
    {
        activation.reset();
        return;
    }
    activation = std::make_unique<ActivationPanel> (licence);
    activation->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ChromaView> (this)]
                                                                     { if (safe != nullptr) safe->showActivation (false); }); };
    addAndMakeVisible (*activation);
    activation->setBounds (getLocalBounds());
}

void ChromaView::poll()
{
    auto& m = processor.meters();
    inMeter.setLevel (m.inputPeak.exchange (0.0f));
    outMeter.setLevel (m.outputPeak.exchange (0.0f));

    const bool keyOn = state.getRawParameterValue (ParamID::keyOn)->load() > 0.5f;
    const bool external = juce::roundToInt (state.getRawParameterValue (ParamID::keySource)->load()) == 0;
    const bool listening = state.getRawParameterValue (ParamID::keyListen)->load() > 0.5f;

    juce::String status;
    bool warning = false;
    if (keyOn)
    {
        const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
        if (external)
        {
            warning = ! processor.hasSidechainInput() || m.externalKeyMissing.load();
            status = warning ? "KEY" + dot + "NO SIDECHAIN ROUTED, USING THE TRACK" : "KEY" + dot + "EXTERNAL SIDECHAIN";
        }
        else
        {
            const auto hz = state.getRawParameterValue (ParamID::keyFreq)->load();
            status = "KEY" + dot + "FREQ " + (hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " KHZ" : juce::String (juce::roundToInt (hz)) + " HZ");
        }
        if (listening)
            status << dot << "LISTENING";
    }
    grMeter.setState (m.gainReductionDb.exchange (0.0f), dial.getIndex(), status, warning);

    // Key controls recede while Key is off; Freq only matters in Freq Key mode.
    const int keyState = (keyOn ? 1 : 0) | (external ? 2 : 0);
    if (keyState != lastKeyState)
    {
        lastKeyState = keyState;
        keySource.setAlpha (keyOn ? 1.0f : 0.45f);
        listen->setAlpha (keyOn ? 1.0f : 0.45f);
        keyFreq.setAlpha (keyOn && ! external ? 1.0f : 0.4f);
        repaint (keyCard);
    }
}

void ChromaView::paint (juce::Graphics& g)
{
    const auto accent = Theme::characterColour (dial.getIndex());
    g.fillAll (Theme::bg);

    // Soft glow behind the header, tinted by the character.
    g.setGradientFill (juce::ColourGradient (Theme::brandTop.withAlpha (0.18f), (float) getWidth() * 0.5f, -40.0f,
                                             Theme::bg.withAlpha (0.0f), (float) getWidth() * 0.5f, 220.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (240));
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.07f), (float) getWidth() * 0.75f, 0.0f,
                                             accent.withAlpha (0.0f), (float) getWidth() * 0.75f, 260.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (260));

    // Brand block.
    const auto mark = juce::Rectangle<float> ((float) margin, 16.0f, 40.0f, 40.0f);
    drawBrandMark (g, mark);
    const auto textX = mark.getRight() + 14.0f;
    g.setColour (Theme::text);
    g.setFont (Theme::font (25.0f).withExtraKerningFactor (0.32f));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(), juce::Rectangle<float> (textX, 13.0f, 240.0f, 28.0f), juce::Justification::centredLeft, false);
    g.setColour (Theme::brandCyan.withAlpha (0.85f));
    g.setFont (Theme::caps (9.5f, 0.22f));
    g.drawText ("RAZ BURG AUDIOCRAFTS", juce::Rectangle<float> (textX + 1.0f, 41.0f, 260.0f, 14.0f), juce::Justification::centredLeft, false);

    // Six colour dots under the header centre: the character family.
    {
        const float cx = (float) getWidth() * 0.5f, cy = 36.0f;
        for (int i = 0; i < 6; ++i)
        {
            const auto col = Theme::characterColour (i);
            const auto dot = juce::Rectangle<float> (i == dial.getIndex() ? 10.0f : 7.0f, i == dial.getIndex() ? 10.0f : 7.0f)
                                 .withCentre ({ cx + ((float) i - 2.5f) * 18.0f, cy });
            g.setColour (i == dial.getIndex() ? col : col.withAlpha (0.4f));
            g.fillEllipse (dot);
        }
        g.setColour (Theme::faint);
        g.setFont (Theme::caps (9.0f, 0.2f));
        g.drawText ("MULTI-CHARACTER COMPRESSOR", juce::Rectangle<float> (cx - 150.0f, cy + 10.0f, 300.0f, 14.0f), juce::Justification::centred, false);
    }

    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.5f));
    g.drawText ("PRESET", juce::Rectangle<int> (margin, presets.getY(), 60, presets.getHeight()).toFloat(), juce::Justification::centredLeft, false);

    // Cards
    const bool keyOn = state.getRawParameterValue (ParamID::keyOn)->load() > 0.5f;
    auto card = [&] (juce::Rectangle<int> r, juce::Colour col, const juce::String& title, const juce::String& subtitle, bool lit)
    {
        paintCard (g, r.toFloat(), col, lit);
        paintCardTitle (g, r.reduced (16, 0).removeFromTop (titleHeight).toFloat().withTrimmedTop (6.0f), title, subtitle, lit);
    };
    card (inputCard, accent, "INPUT", {}, true);
    card (outputCard, accent, "OUTPUT", {}, true);
    card (centreCard, accent, "CHARACTER", "TURN THE KNOB TO CHANGE THE VOICE", true);
    card (dynamicsCard, accent, "DYNAMICS", "ATTACK  RELEASE  RATIO", true);
    card (colourCard, accent, "COLOR", "SATURATION  PARALLEL", true);
    card (keyCard, Theme::key, "KEY", "SIDECHAIN", keyOn);

    // Footer.
    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (margin, 0).toFloat();
    g.setFont (Theme::caps (9.5f, 0.14f));
    g.setColour (Theme::faint);
    g.drawText (juce::String::fromUTF8 ("RAZ BURG AUDIOCRAFTS  \xc2\xb7  ") + juce::String (JucePlugin_Name).toUpperCase() + " " + JucePlugin_VersionString,
                footer, juce::Justification::centredLeft, false);
    g.setFont (Theme::font (11.0f));
   #if JUCE_MAC
    const auto fineHint = juce::String::fromUTF8 ("Double-click resets  \xc2\xb7  \xe2\x8c\x98 / Ctrl-drag for fine control");
   #else
    const auto fineHint = juce::String::fromUTF8 ("Double-click resets  \xc2\xb7  Ctrl-drag for fine control");
   #endif
    g.drawText (fineHint, footer, juce::Justification::centredRight, false);
}

void ChromaView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (margin, 0);
    auto toolbar = r.removeFromTop (toolbarHeight).reduced (margin, 0).withTrimmedBottom (10);
    r.removeFromBottom (footerHeight);
    r = r.reduced (margin, 0);

    licenceBadge.setBounds (header.removeFromRight (124).withSizeKeepingCentre (124, 26));
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

    // Top row: Input | Character | Output
    auto top = r.removeFromTop (300);
    r.removeFromTop (gap);
    inputCard = top.removeFromLeft (170);
    top.removeFromLeft (gap);
    outputCard = top.removeFromRight (170);
    top.removeFromRight (gap);
    centreCard = top;

    auto side = [&] (juce::Rectangle<int> cardArea, Knob& knob, LevelMeter& meter, juce::Component* below)
    {
        auto a = cardArea.reduced (12, 0).withTrimmedTop (titleHeight).withTrimmedBottom (14);
        meter.setBounds (a.removeFromRight (40));
        if (below != nullptr)
            below->setBounds (a.removeFromBottom (28).withSizeKeepingCentre (96, 26));
        knob.setBounds (a.withSizeKeepingCentre (a.getWidth(), juce::jmin (a.getHeight(), 170)));
    };
    side (inputCard, input, inMeter, nullptr);
    side (outputCard, output, outMeter, softClip);

    auto centre = centreCard.reduced (14, 0).withTrimmedTop (titleHeight).withTrimmedBottom (14);
    grMeter.setBounds (centre.removeFromLeft (318));
    centre.removeFromLeft (8);
    dial.setBounds (centre);

    // Bottom row: Dynamics | Color | Key
    auto bottom = r;
    dynamicsCard = bottom.removeFromLeft (400);
    bottom.removeFromLeft (gap);
    colourCard = bottom.removeFromLeft (270);
    bottom.removeFromLeft (gap);
    keyCard = bottom;

    auto knobRow = [&] (juce::Rectangle<int> cardArea, std::initializer_list<Knob*> knobs)
    {
        auto a = cardArea.reduced (10, 0).withTrimmedTop (titleHeight + 6).withTrimmedBottom (18);
        const auto w = a.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (a.removeFromLeft (w).reduced (4, 0));
    };
    knobRow (dynamicsCard, { &attack, &release, &ratio });
    knobRow (colourCard, { &saturation, &parallel });

    keyPower.setBounds (keyCard.getRight() - 16 - 28, keyCard.getY() + 12, 28, 28);
    auto k = keyCard.reduced (14, 0).withTrimmedTop (titleHeight + 2).withTrimmedBottom (16);
    keySource.setBounds (k.removeFromTop (30));
    k.removeFromTop (8);
    listen->setBounds (k.removeFromRight (84).withSizeKeepingCentre (80, 26));
    keyFreq.setBounds (k);
    repaint();
}

//==============================================================================
ChromaEditor::ChromaEditor (ChromaProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    view.setLookAndFeel (&lookAndFeel);
    view.setBounds (0, 0, ChromaView::designWidth, ChromaView::designHeight);
    addAndMakeVisible (view);

    constexpr double aspect = (double) ChromaView::designWidth / (double) ChromaView::designHeight;
    setResizable (true, true);
    setResizeLimits (ChromaView::designWidth * 7 / 10, ChromaView::designHeight * 7 / 10,
                     ChromaView::designWidth * 2, ChromaView::designHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (aspect);
    setSize (ChromaView::designWidth, ChromaView::designHeight);

    startTimerHz (30);
}

ChromaEditor::~ChromaEditor()
{
    view.setLookAndFeel (nullptr);
}

void ChromaEditor::resized()
{
    const auto scale = (float) getWidth() / (float) ChromaView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
}
