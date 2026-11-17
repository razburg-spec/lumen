#include "Toolbar.h"
#include "../presets/FactoryPresets.h"
#include <BinaryData.h>

namespace
{
    const juce::Identifier presetNameProperty { "presetName" };
    const juce::Identifier presetIndexProperty { "presetIndex" };
}

//==============================================================================
void ToolButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto enabled = isEnabled();
    const auto corner = juce::jmin (8.0f, r.getHeight() * 0.5f);

    g.setColour (down ? Theme::line : (highlighted && enabled ? Theme::raised.brighter (0.08f) : Theme::raised));
    g.fillRoundedRectangle (r, corner);
    g.setColour (highlighted && enabled ? Theme::faint.brighter (0.3f) : Theme::line);
    g.drawRoundedRectangle (r, corner, 1.0f);

    const auto colour = ! enabled ? Theme::faint : (highlighted ? Theme::text : Theme::muted);
    g.setColour (colour);

    const auto c = r.getCentre();
    const auto s = juce::jmin (r.getWidth(), r.getHeight()) * 0.26f;
    const auto stroke = juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;

    switch (glyph)
    {
        case Glyph::undo:
        case Glyph::redo:
        {
            const float dir = glyph == Glyph::undo ? 1.0f : -1.0f;
            // A hooked arrow: an arc over the top that ends in an arrow head.
            p.addCentredArc (c.x, c.y + s * 0.35f, s, s * 0.9f, 0.0f,
                             dir > 0 ? -juce::MathConstants<float>::halfPi * 1.1f : juce::MathConstants<float>::halfPi * 1.1f,
                             dir > 0 ? juce::MathConstants<float>::halfPi * 1.05f : -juce::MathConstants<float>::halfPi * 1.05f, true);
            g.strokePath (p, stroke);
            const auto tip = juce::Point<float> (c.x - dir * s * 1.0f, c.y + s * 0.15f);
            juce::Path head;
            head.startNewSubPath (tip.translated (dir * s * 0.05f, -s * 0.75f));
            head.lineTo (tip);
            head.lineTo (tip.translated (dir * s * 0.75f, s * 0.1f));
            g.strokePath (head, stroke);
            break;
        }
        case Glyph::previous:
        case Glyph::next:
        {
            const float dir = glyph == Glyph::next ? 1.0f : -1.0f;
            p.startNewSubPath (c.x - dir * s * 0.35f, c.y - s * 0.7f);
            p.lineTo (c.x + dir * s * 0.35f, c.y);
            p.lineTo (c.x - dir * s * 0.35f, c.y + s * 0.7f);
            g.strokePath (p, stroke);
            break;
        }
        case Glyph::help:
            g.setFont (Theme::font (r.getHeight() * 0.55f, true));
            g.drawText ("?", r, juce::Justification::centred, false);
            break;
        case Glyph::none:
            g.setFont (Theme::caps (10.0f));
            g.drawText (getButtonText().toUpperCase(), r, juce::Justification::centred, false);
            break;
    }
}

//==============================================================================
PresetBar::PresetBar (APVTS& s, ParamHistory& h) : state (s), history (h)
{
    addAndMakeVisible (previous);
    addAndMakeVisible (next);
    previous.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    setTooltip ("Click to browse the factory presets");

    const auto count = (int) getFactoryPresets().size();
    previous.onClick = [this, count] { const auto i = currentIndex(); loadPreset (i <= 0 ? count - 1 : i - 1); };
    next.onClick     = [this, count] { const auto i = currentIndex(); loadPreset (i < 0 || i >= count - 1 ? 0 : i + 1); };
}

juce::String PresetBar::currentName() const
{
    return state.state.getProperty (presetNameProperty, "Default").toString();
}

int PresetBar::currentIndex() const
{
    return (int) state.state.getProperty (presetIndexProperty, -1);
}

juce::Rectangle<int> PresetBar::nameArea() const
{
    return getLocalBounds().reduced (getHeight() + 6, 0);
}

void PresetBar::loadPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    const auto& preset = presets[(size_t) index];
    history.performAsOneStep ([&] { applyFactoryPreset (state, preset); });
    state.state.setProperty (presetNameProperty, juce::String (preset.name), nullptr);
    state.state.setProperty (presetIndexProperty, index, nullptr);
    repaint();
}

void PresetBar::resetAll()
{
    history.performAsOneStep ([&] { resetToDefaults (state); });
    state.state.setProperty (presetNameProperty, "Default", nullptr);
    state.state.setProperty (presetIndexProperty, -1, nullptr);
    repaint();
}

void PresetBar::showMenu()
{
    const auto& presets = getFactoryPresets();
    const auto current = currentIndex();
    juce::PopupMenu menu;
    menu.addSectionHeader ("FACTORY PRESETS");

    for (const auto& category : getPresetCategories())
    {
        juce::PopupMenu sub;
        bool containsCurrent = false;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (category == presets[(size_t) i].category)
            {
                sub.addItem (i + 1, presets[(size_t) i].name, true, i == current);
                containsCurrent |= i == current;
            }
        menu.addSubMenu (category, sub, true, nullptr, containsCurrent);
    }
    menu.addSeparator();
    menu.addItem (10000, "Reset to default (no processing)");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0) return;
                            if (result == 10000) safe->resetAll();
                            else safe->loadPreset (result - 1);
                        });
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (nameArea().contains (e.getPosition()))
        showMenu();
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto r = nameArea().toFloat().reduced (0.5f);
    g.setColour (Theme::display);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (hover ? Theme::faint.brighter (0.3f) : Theme::line);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    auto text = r.reduced (12.0f, 0.0f);
    const auto index = currentIndex();
    if (juce::isPositiveAndBelow (index, (int) getFactoryPresets().size()))
    {
        g.setColour (Theme::faint);
        g.setFont (Theme::caps (9.0f));
        const juce::String cat (getFactoryPresets()[(size_t) index].category);
        const auto w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), cat.toUpperCase()) + 10.0f;
        g.drawText (cat.toUpperCase(), text.removeFromLeft (w), juce::Justification::centredLeft, false);
    }
    g.setColour (Theme::text);
    g.setFont (Theme::font (13.5f));
    g.drawText (currentName(), text.withTrimmedRight (16.0f), juce::Justification::centredLeft, true);

    // Drop-down chevron.
    const auto cx = r.getRight() - 14.0f, cy = r.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (Theme::muted);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    previous.setBounds (r.removeFromLeft (getHeight()));
    next.setBounds (r.removeFromRight (getHeight()));
}

//==============================================================================
// The guide's text, laid out from a small Markdown subset: "# ", "## ", "- " and paragraphs.
class ManualPanel::Page : public juce::Component
{
public:
    Page()
    {
        const auto text = juce::String::fromUTF8 (BinaryData::Manual_md, BinaryData::Manual_mdSize);
        for (auto line : juce::StringArray::fromLines (text))
        {
            line = line.trimEnd();
            if (line.isEmpty()) continue;
            if (line.startsWith ("## "))     blocks.push_back ({ Kind::heading, line.substring (3) });
            else if (line.startsWith ("# ")) blocks.push_back ({ Kind::title, line.substring (2) });
            else if (line.startsWith ("- ")) blocks.push_back ({ Kind::bullet, line.substring (2) });
            else                             blocks.push_back ({ Kind::paragraph, line });
        }
    }

    int layoutForWidth (int width)
    {
        layouts.clear();
        int y = 0;
        for (const auto& b : blocks)
        {
            const auto indent = b.kind == Kind::bullet ? bulletIndent : 0;
            juce::TextLayout layout;
            layout.createLayout (attributed (b), (float) (width - indent));
            const auto top = y + (b.kind == Kind::heading ? 22 : (b.kind == Kind::title ? 0 : 6));
            layouts.push_back ({ std::move (layout), top, indent, b.kind });
            y = top + (int) std::ceil (layouts.back().layout.getHeight()) + (b.kind == Kind::title ? 6 : 2);
        }
        setSize (width, y + 24);
        return y + 24;
    }

    void paint (juce::Graphics& g) override
    {
        for (auto& l : layouts)
        {
            const auto h = l.layout.getHeight();
            if (l.kind == Kind::bullet)
            {
                g.setColour (Theme::brandCyan.withAlpha (0.8f));
                g.fillEllipse ((float) bulletIndent - 11.0f, (float) l.top + 7.0f, 4.0f, 4.0f);
            }
            if (l.kind == Kind::heading)
            {
                g.setColour (Theme::line);
                g.drawHorizontalLine (l.top - 9, 0.0f, (float) getWidth());
            }
            l.layout.draw (g, juce::Rectangle<float> ((float) l.indent, (float) l.top, (float) getWidth() - (float) l.indent, h));
        }
    }

private:
    enum class Kind { title, heading, paragraph, bullet };
    struct Block { Kind kind; juce::String text; };
    struct Laid { juce::TextLayout layout; int top, indent; Kind kind; };
    static constexpr int bulletIndent = 18;

    static juce::AttributedString attributed (const Block& b)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        s.setLineSpacing (3.0f);
        switch (b.kind)
        {
            case Kind::title:     s.append (b.text, Theme::font (24.0f, true), Theme::text); break;
            case Kind::heading:   s.append (b.text.toUpperCase(), Theme::caps (12.5f, 0.14f), Theme::brandCyan); break;
            case Kind::paragraph: s.append (b.text, Theme::font (14.0f), Theme::muted); break;
            case Kind::bullet:
            {
                // "Label: description" gets a bright label.
                const auto colon = b.text.indexOf (": ");
                if (colon > 0 && colon < 40)
                {
                    s.append (b.text.substring (0, colon + 1), Theme::font (14.0f, true), Theme::text);
                    s.append (b.text.substring (colon + 1), Theme::font (14.0f), Theme::muted);
                }
                else
                {
                    s.append (b.text, Theme::font (14.0f), Theme::muted);
                }
                break;
            }
        }
        return s;
    }

    std::vector<Block> blocks;
    std::vector<Laid> layouts;
};

ManualPanel::ManualPanel() : page (std::make_unique<Page>())
{
    viewport.setViewedComponent (page.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    addAndMakeVisible (close);
    addAndMakeVisible (online);
    close.onClick = [this] { if (onClose) onClose(); };
    online.onClick = [] { juce::URL (onlineUrl).launchInDefaultBrowser(); };
    online.setTooltip ("Open the guide in your web browser");
    setWantsKeyboardFocus (true);
}

void ManualPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg.withAlpha (0.86f));
    const auto c = card.toFloat();
    g.setColour (Theme::surface);
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.0f);
    drawBrandMark (g, juce::Rectangle<float> (c.getX() + 28.0f, c.getY() + 20.0f, 28.0f, 28.0f));
    g.setColour (Theme::muted);
    g.setFont (Theme::caps (10.0f, 0.2f));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase() + juce::String::fromUTF8 ("  \xc2\xb7  USER GUIDE"), juce::Rectangle<float> (c.getX() + 66.0f, c.getY() + 20.0f, 300.0f, 28.0f),
                juce::Justification::centredLeft, false);
}

void ManualPanel::resized()
{
    card = getLocalBounds().reduced (110, 46);
    auto top = card.reduced (24, 0).removeFromTop (68).withTrimmedTop (18).withHeight (30);
    close.setBounds (top.removeFromRight (78));
    top.removeFromRight (8);
    online.setBounds (top.removeFromRight (112));

    const auto body = card.reduced (32, 0).withTrimmedTop (72).withTrimmedBottom (20);
    viewport.setBounds (body);
    page->layoutForWidth (body.getWidth() - 16);
}

void ManualPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition()) && onClose)
        onClose();
}

bool ManualPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && onClose)
    {
        onClose();
        return true;
    }
    return false;
}

ManualPanel::~ManualPanel() = default;
