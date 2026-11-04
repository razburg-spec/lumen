#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Raz Burg AudioCrafts palette (dark UI tokens from the brand) plus one accent per engine.
namespace Theme
{
    inline const juce::Colour bg      { 0xff11161d };
    inline const juce::Colour surface { 0xff19212b };
    inline const juce::Colour raised  { 0xff202a36 };
    inline const juce::Colour line    { 0xff2b3542 };
    inline const juce::Colour text    { 0xfff2f5f9 };
    inline const juce::Colour muted   { 0xff97a3b2 };
    inline const juce::Colour faint   { 0xff4a5666 };
    inline const juce::Colour display { 0xff0c1016 };

    inline const juce::Colour brandTop    { 0xff1a4fc4 };
    inline const juce::Colour brandBottom { 0xff11287a };
    inline const juce::Colour brandBlue   { 0xff6f9cff };
    inline const juce::Colour brandCyan   { 0xff8ddcff };

    inline const juce::Colour delay   { 0xffffb45e };
    inline const juce::Colour shimmer { 0xffb59cff };
    inline const juce::Colour reverb  = brandCyan;
    inline const juce::Colour master  = brandBlue;

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Small spaced capitals used for every label.
    inline juce::Font caps (float height, float kerning = 0.12f)
    {
        return font (height, true).withExtraKerningFactor (kerning);
    }
}
