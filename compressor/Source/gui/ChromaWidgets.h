#pragma once
#include "Widgets.h"

// Card background shared by every section: rounded surface with an accent hairline on top.
void paintCard (juce::Graphics&, juce::Rectangle<float> area, juce::Colour accent, bool lit = true);
void paintCardTitle (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, const juce::String& subtitle, bool lit = true);

// One-line description of each character, shown under the meter.
juce::String characterBlurb (int index);

// Six-position character selector, drawn as a large stepped knob with the names around it.
// Drag, scroll, or click a name. Double-click returns to the default.
class CharacterDial : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit CharacterDial (juce::RangedAudioParameter&);

    int getIndex() const { return index; }
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }

private:
    float angleFor (float position) const;
    juce::Point<float> centre() const;
    float radius() const;
    juce::Rectangle<float> labelBounds (int i) const;
    int labelAt (juce::Point<float>) const;
    void select (int i);

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    int index = 0, hovered = -1, dragStartIndex = 0;
    bool dragging = false, clickedLabel = false;
    float shownPosition = 0.0f;
};

// Analog-style gain-reduction meter with the character name and key status underneath.
class GrMeter : public juce::Component
{
public:
    void setState (float grDb, int character, const juce::String& keyStatus, bool keyWarning);
    void paint (juce::Graphics&) override;

private:
    float needleDb = 0.0f, peakDb = 0.0f;
    int peakHold = 0;
    int character = 0;
    juce::String keyStatus;
    bool keyWarning = false;
};

// Vertical peak meter (-48 .. +6 dBFS) with a short peak hold.
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& labelText) : label (labelText) {}
    void setLevel (float linearPeak);
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    juce::Colour accent = Theme::brandCyan;
    float levelDb = -100.0f, holdDb = -100.0f;
    int holdFrames = 0;
};
