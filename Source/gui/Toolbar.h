#pragma once
#include "Widgets.h"
#include "../ParamHistory.h"

// Small rounded toolbar button that shows a glyph or a short caps label.
class ToolButton : public juce::Button
{
public:
    enum class Glyph { none, undo, redo, previous, next, help };

    ToolButton (const juce::String& label, Glyph g = Glyph::none) : juce::Button (label), glyph (g) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Glyph glyph;
};

// Preset browser: previous / next arrows around the current preset's name. Clicking the
// name opens a menu of the factory presets grouped by category.
class PresetBar : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    PresetBar (APVTS&, ParamHistory&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void loadPreset (int index);
    void resetAll();
    void refresh() { repaint(); }

private:
    juce::String currentName() const;
    int currentIndex() const;
    juce::Rectangle<int> nameArea() const;
    void showMenu();

    APVTS& state;
    ParamHistory& history;
    ToolButton previous { "Previous preset", ToolButton::Glyph::previous },
               next { "Next preset", ToolButton::Glyph::next };
    bool hover = false;
};

// Full-window overlay with the built-in user guide, rendered from Resources/Manual.md.
class ManualPanel : public juce::Component
{
public:
    ManualPanel();
    ~ManualPanel() override;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr const char* onlineUrl = "https://audiocrafts.razburg.com/lumen/manual/";

private:
    class Page;
    juce::Rectangle<int> card;
    juce::Viewport viewport;
    std::unique_ptr<Page> page;
    ToolButton close { "Close" }, online { "Open online" };
};
