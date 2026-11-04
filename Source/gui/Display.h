#pragma once
#include "Widgets.h"

// The large display: one time axis showing the dry hit, the delay taps, the shimmer
// grains and the reverb envelope, each in its engine colour. Drawn from the current
// parameter values, so it answers every knob move immediately.
class Display : public juce::Component
{
public:
    explicit Display (APVTS&);

    void paint (juce::Graphics&) override;

    // Called from the editor's timer; repaints only when a parameter changed.
    void poll();

private:
    float get (const char* id) const { return state.getRawParameterValue (id)->load(); }
    float delaySeconds() const;
    juce::String delayReadout() const;
    float reverbLevelAt (float t) const;   // dB, without the level offset

    APVTS& state;
    std::vector<std::atomic<float>*> watched;
    std::vector<float> lastValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Display)
};
