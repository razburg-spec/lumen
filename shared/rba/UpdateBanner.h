#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UpdateChecker.h"

// A small pill that appears only when a newer version is on the website:
//   (•) Lumen 0.3.2 is available   [ Download ]  ×
// "Download" opens the site in the browser; × hides it until the next release.
// Shared by every Raz Burg AudioCrafts plugin. Give it the plugin's accent colour.
namespace rba
{
class UpdateBanner : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::ChangeListener
{
public:
    struct Style
    {
        juce::Colour background { 0xff19212b }, outline { 0xff2b3542 },
                     text { 0xfff2f5f9 }, muted { 0xff97a3b2 }, accent { 0xff6f9cff };
    };

    UpdateBanner (juce::String productName, juce::String productSlug, juce::String runningVersion, Style);
    ~UpdateBanner() override;

    // Width the pill wants at the current text; parents place it with this.
    int getIdealWidth() const;

    // Called when the banner shows or hides, so the parent can re-layout.
    std::function<void()> onVisibilityChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    juce::String message() const;
    juce::Font font (float h, bool bold = false) const;

    const juce::String name, slug, version;
    const Style style;
    juce::Rectangle<float> downloadArea, closeArea;
    enum class Hover { none, download, close } hover = Hover::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateBanner)
};
} // namespace rba
