#include "PluginEditor.h"

namespace
{
    constexpr int margin = 20;
    constexpr int headerHeight = 72;
    constexpr int toolbarHeight = 44;
    constexpr int footerHeight = 30;
    constexpr int gap = 14;
    constexpr int masterWidth = 160;
    const juce::Identifier advancedProperty { "advancedView" };
}

//==============================================================================
LumenView::LumenView (LumenProcessor& p)
    : processor (p),
      display (p.apvts),
      delay (p.apvts), shimmer (p.apvts), reverb (p.apvts), master (p.apvts),
      routing (*p.apvts.getParameter (ParamID::routing), { "Serial", "Parallel", juce::String::fromUTF8 ("Reverb \xe2\x80\xba Delay") }, Theme::brandBlue),
      licence (*p.licence),
      presets (p.apvts, p.history)
{
    for (auto* c : std::initializer_list<juce::Component*> { &presets, &undoButton, &redoButton, &resetButton, &helpButton })
        addAndMakeVisible (c);
    undoButton.setTooltip ("Undo (up to 100 steps)");
    redoButton.setTooltip ("Redo");
    resetButton.setTooltip ("Reset everything to the initial state: all engines off, fully dry");
    helpButton.setTooltip ("User guide");
    undoButton.onClick = [this] { processor.history.undo(); };
    redoButton.onClick = [this] { processor.history.redo(); };
    resetButton.onClick = [this] { presets.resetAll(); };
    helpButton.onClick = [this] { showManual (manual == nullptr); };
    processor.history.addChangeListener (this);
    updateHistoryButtons();

    for (auto* c : std::initializer_list<juce::Component*> { &display, &delay, &shimmer, &reverb, &master, &routing, &advancedToggle })
        addAndMakeVisible (c);

    advancedToggle.setTooltip ("Show every parameter");
    advancedToggle.onClick = [this] { setAdvanced (advancedToggle.getToggleState()); };

    const bool startAdvanced = processor.apvts.state.getProperty (advancedProperty, false);
    advancedToggle.setToggleState (startAdvanced, juce::dontSendNotification);
    setAdvanced (startAdvanced);

    addAndMakeVisible (licenceBadge);
    licenceBadge.onClick = [this] { showActivation (true); };
    licence.addChangeListener (this);
    changeListenerCallback (nullptr);

    // Ask straight away whenever Lumen is not free to run.
    const auto state = licence.getStatus().state;
    if (state != LicenseManager::State::licensed && state != LicenseManager::State::trial)
        showActivation (true);
}

LumenView::~LumenView()
{
    processor.history.removeChangeListener (this);
    licence.removeChangeListener (this);
}

void LumenView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.history)
    {
        updateHistoryButtons();
        return;
    }
    licenceBadge.setStatus (licence.getStatus());
}

void LumenView::updateHistoryButtons()
{
    undoButton.setEnabled (processor.history.canUndo());
    redoButton.setEnabled (processor.history.canRedo());
}

void LumenView::showManual (bool show)
{
    if (! show)
    {
        manual.reset();
        return;
    }

    manual = std::make_unique<ManualPanel>();
    manual->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LumenView> (this)]
                                                                 { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*manual);
    manual->setBounds (getLocalBounds());
    manual->grabKeyboardFocus();
    if (activation != nullptr)
        activation->toFront (false);
}

void LumenView::showActivation (bool show)
{
    if (! show)
    {
        activation.reset();
        return;
    }

    activation = std::make_unique<ActivationPanel> (licence);
    activation->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LumenView> (this)]
                                                                     { if (safe != nullptr) safe->showActivation (false); }); };
    addAndMakeVisible (*activation);
    activation->setBounds (getLocalBounds());
}

void LumenView::setAdvanced (bool shouldShow)
{
    advanced = shouldShow;
    processor.apvts.state.setProperty (advancedProperty, advanced, nullptr);
    for (auto* section : std::initializer_list<EngineSection*> { &delay, &shimmer, &reverb })
        section->setAdvanced (advanced);
    resized();
}

void LumenView::poll()
{
    display.poll();
    delay.poll();
    shimmer.poll();
    reverb.poll();
}

void LumenView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    // Soft brand-blue glow behind the header.
    g.setGradientFill (juce::ColourGradient (Theme::brandTop.withAlpha (0.18f), (float) getWidth() * 0.5f, -40.0f,
                                             Theme::bg.withAlpha (0.0f), (float) getWidth() * 0.5f, 220.0f, true));
    g.fillRect (getLocalBounds().removeFromTop (240));

    // Brand block.
    const auto mark = juce::Rectangle<float> ((float) margin, 16.0f, 40.0f, 40.0f);
    drawBrandMark (g, mark);
    const auto textX = mark.getRight() + 14.0f;
    g.setColour (Theme::text);
    g.setFont (Theme::font (25.0f).withExtraKerningFactor (0.32f));
    g.drawText ("LUMEN", juce::Rectangle<float> (textX, 13.0f, 220.0f, 28.0f), juce::Justification::centredLeft, false);
    g.setColour (Theme::brandCyan.withAlpha (0.85f));
    g.setFont (Theme::caps (9.5f, 0.22f));
    g.drawText ("RAZ BURG AUDIOCRAFTS", juce::Rectangle<float> (textX + 1.0f, 41.0f, 260.0f, 14.0f), juce::Justification::centredLeft, false);

    // Toolbar label.
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.5f));
    g.drawText ("PRESET", juce::Rectangle<int> (margin, presets.getY(), 60, presets.getHeight()).toFloat(),
                juce::Justification::centredLeft, false);

    // Label in front of the routing selector.
    g.setColour (Theme::faint);
    g.setFont (Theme::caps (9.5f));
    g.drawText ("SIGNAL FLOW", routing.getBounds().translated (-96, 0).withWidth (88).toFloat(), juce::Justification::centredRight, false);

    // Footer.
    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (margin, 0).toFloat();
    g.setFont (Theme::caps (9.5f, 0.14f));
    g.setColour (Theme::faint);
    g.drawText (juce::String::fromUTF8 ("RAZ BURG AUDIOCRAFTS  \xc2\xb7  LUMEN ") + JucePlugin_VersionString,
                footer, juce::Justification::centredLeft, false);
    g.setFont (Theme::font (11.0f));
   #if JUCE_MAC
    const auto fineHint = juce::String::fromUTF8 ("Double-click resets  \xc2\xb7  \xe2\x8c\x98 / Ctrl-drag for fine control");
   #else
    const auto fineHint = juce::String::fromUTF8 ("Double-click resets  \xc2\xb7  Ctrl-drag for fine control");
   #endif
    g.drawText (fineHint,
                footer, juce::Justification::centredRight, false);
}

void LumenView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (margin, 0);
    auto toolbar = r.removeFromTop (toolbarHeight).reduced (margin, 0).withTrimmedBottom (10);
    r.removeFromBottom (footerHeight);
    r = r.reduced (margin, 0);

    advancedToggle.setBounds (header.removeFromRight (112).withSizeKeepingCentre (112, 28));
    header.removeFromRight (10);
    licenceBadge.setBounds (header.removeFromRight (124).withSizeKeepingCentre (124, 26));
    if (activation != nullptr)
        activation->setBounds (getLocalBounds());
    if (manual != nullptr)
        manual->setBounds (getLocalBounds());

    // Toolbar: preset browser on the left, history and help on the right.
    toolbar.removeFromLeft (64);   // "PRESET" label
    presets.setBounds (toolbar.removeFromLeft (360));
    helpButton.setBounds (toolbar.removeFromRight (34));
    toolbar.removeFromRight (16);
    resetButton.setBounds (toolbar.removeFromRight (70));
    toolbar.removeFromRight (8);
    redoButton.setBounds (toolbar.removeFromRight (34));
    toolbar.removeFromRight (6);
    undoButton.setBounds (toolbar.removeFromRight (34));

    routing.setBounds (juce::Rectangle<int> (380, 32).withCentre ({ getWidth() / 2 + 40, header.getCentreY() }));

    display.setBounds (r.removeFromTop (advanced ? 118 : 200));
    r.removeFromTop (gap);

    master.setBounds (r.removeFromRight (masterWidth));
    r.removeFromRight (gap);
    const auto sectionWidth = (r.getWidth() - 2 * gap) / 3;
    delay.setBounds (r.removeFromLeft (sectionWidth));
    r.removeFromLeft (gap);
    shimmer.setBounds (r.removeFromLeft (sectionWidth));
    r.removeFromLeft (gap);
    reverb.setBounds (r);
    repaint();
}

//==============================================================================
LumenEditor::LumenEditor (LumenProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    view.setLookAndFeel (&lookAndFeel);
    view.setBounds (0, 0, LumenView::designWidth, LumenView::designHeight);
    addAndMakeVisible (view);

    constexpr double aspect = (double) LumenView::designWidth / (double) LumenView::designHeight;
    setResizable (true, true);
    setResizeLimits (LumenView::designWidth * 7 / 10, LumenView::designHeight * 7 / 10,
                     LumenView::designWidth * 2, LumenView::designHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (aspect);
    setSize (LumenView::designWidth, LumenView::designHeight);

    startTimerHz (30);
}

LumenEditor::~LumenEditor()
{
    view.setLookAndFeel (nullptr);
}

void LumenEditor::resized()
{
    const auto scale = (float) getWidth() / (float) LumenView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
}
