#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Widgets.h"
#include "../dsp/PrismEngine.h"

class PrismProcessor;

// Card background shared by every section: rounded surface with an accent hairline on top.
void paintCard (juce::Graphics&, juce::Rectangle<float> area, juce::Colour accent, bool lit = true);
void paintCardTitle (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, const juce::String& subtitle, bool lit = true);

// One-line description of each mode.
juce::String modeBlurb (int index);

// Six-way mode selector: one pill per mode, each in its own color.
class ModeSelector : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit ModeSelector (juce::RangedAudioParameter&);

    int getIndex() const { return selected; }
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }

private:
    int segmentAt (juce::Point<float>) const;
    juce::ParameterAttachment attachment;
    int selected = 4, hovered = -1;
};

// Spectrum of one signal (before or after the EQ), computed on the message thread.
class Spectrum
{
public:
    static constexpr int order = 12, size = 1 << order, points = 240;

    // Pulls new samples from the tap and updates the smoothed curve. Returns true if it changed.
    bool update (AnalyzerTap&, double sampleRate);
    void clear();
    float levelAt (int point) const { return curve[(size_t) point]; }   // dB, at the log-spaced display point
    static float frequencyAt (int point) { return 20.0f * std::pow (1000.0f, (float) point / (float) (points - 1)); }

private:
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann, false };
    std::array<float, (size_t) size> history {};
    int writePos = 0, newSamples = 0;
    std::array<float, (size_t) size * 2> work {};
    std::array<float, (size_t) points> curve {};
    std::vector<float> scratch = std::vector<float> (8192);
};

// The big display: the live spectrum before (gray) and after (mode color) the EQ, the EQ curve on
// top, and one draggable node per band. Drag a node to set frequency and gain, scroll to set Q,
// double-click to switch the band on or off.
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (PrismProcessor&);

    void poll();                      // 30 Hz from the editor
    void setSelectedBand (int b);
    int getSelectedBand() const { return selected; }
    std::function<void (int)> onSelect;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    enum class AnalyzerMode { prePost = 0, post = 1, off = 2 };

private:
    juce::Rectangle<float> plot() const;
    float xForFreq (float hz) const;
    float freqForX (float x) const;
    float yForGain (float db) const;
    float gainForY (float y) const;
    float yForLevel (float db, float hz) const;
    juce::Point<float> nodePosition (int band) const;
    int nodeAt (juce::Point<float>) const;
    juce::Rectangle<float> analyzerButton (int i) const;
    AnalyzerMode analyzerMode() const;
    void setParam (const juce::String& id, float plain);
    void drawSpectrum (juce::Graphics&, const Spectrum&, juce::Colour fill, juce::Colour line);
    void drawNodeBubble (juce::Graphics&, int band);

    PrismProcessor& processor;
    Spectrum pre, post;
    PrismEngine::Params params;
    int selected = -1, hovered = -1, dragging = -1;
    bool dragTurnedOn = false;
};

// Controls of one band in a card: power, type and slope for the edge bands, Freq, Gain and Q.
class BandStrip : public juce::Component
{
public:
    BandStrip (APVTS&, int band);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect (band); }
    void refresh();                   // dims what does not apply (gain on a cut, slope on a shelf, everything when off)
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    std::function<void (int)> onSelect;

private:
    APVTS& state;
    int band;
    bool selected = false;
    PowerButton power;
    APVTS::ButtonAttachment powerAttachment;
    std::unique_ptr<Segmented> type, slope;
    Knob freq, gain, q;
    int lastState = -1;
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
