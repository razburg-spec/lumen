#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary control with its name above and a formatted value below.
// Double-click resets, Cmd/Ctrl/Alt-drag switches to fine (velocity) mode.
class Knob : public juce::Component
{
public:
    enum class Size { Large, Small };

    Knob (APVTS&, const juce::String& paramID, const juce::String& title, juce::Colour accent,
          Size = Size::Large, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    juce::Slider slider;

private:
    juce::String valueText() const;

    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    juce::Colour accent;
    Size size;
    std::unique_ptr<APVTS::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

// Round on/off switch with a power glyph.
class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (juce::Colour accentColour) : accent (accentColour) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour accent;
};

// Small rounded toggle with a text label (Sync, Freeze, Advanced).
class PillToggle : public juce::ToggleButton
{
public:
    PillToggle (const juce::String& text, juce::Colour accentColour) : accent (accentColour) { setButtonText (text); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour accent;
};

// Segmented selector bound to a choice parameter.
class Segmented : public juce::Component
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int segmentAt (juce::Point<float>) const;

    juce::StringArray labels;
    juce::Colour accent;
    int selected = 0, hovered = -1;
    juce::ParameterAttachment attachment;
};

// The Raz Burg AudioCrafts mark: royal-blue tile with three nested arcs around a dot.
void drawBrandMark (juce::Graphics&, juce::Rectangle<float> area);
