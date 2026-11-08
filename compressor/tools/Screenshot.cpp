// Renders the Chroma interface to image files, for the website and for checking the layout.
// Usage: ChromaScreenshot <out-dir> [character 0-5]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/gui/ActivationPanel.h"

static void hideOverlays (juce::Component& c)
{
    for (auto* child : c.getChildren())
    {
        if (dynamic_cast<ActivationPanel*> (child) != nullptr)
            child->setVisible (false);
        hideOverlays (*child);
    }
}

static void set (ChromaProcessor& p, const char* id, float plain)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const int character = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    outDir.createDirectory();

    std::unique_ptr<ChromaProcessor> proc (dynamic_cast<ChromaProcessor*> (createPluginFilter()));
    set (*proc, ParamID::character, (float) character);
    set (*proc, ParamID::input, 10.0f);
    set (*proc, ParamID::ratio, 4.0f);
    set (*proc, ParamID::attack, 8.0f);
    set (*proc, ParamID::release, 220.0f);
    set (*proc, ParamID::saturation, 30.0f);
    set (*proc, ParamID::parallel, 85.0f);
    set (*proc, ParamID::output, -4.0f);
    set (*proc, ParamID::keyOn, 1.0f);
    set (*proc, ParamID::keySource, 1.0f);
    set (*proc, ParamID::keyFreq, 2500.0f);

    proc->setPlayConfigDetails (2, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    editor->setSize (ChromaView::designWidth, ChromaView::designHeight);
    hideOverlays (*editor);

    // Feed some audio so the meters show movement.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    int t = 0;
    for (int frame = 0; frame < 30; ++frame)
    {
        for (int b = 0; b < 3; ++b)
        {
            for (int i = 0; i < 512; ++i, ++t)
            {
                const float env = 0.35f + 0.25f * std::sin ((float) t / 48000.0f * 6.0f);
                const float x = env * (std::sin ((float) t * 0.0144f) + 0.5f * std::sin ((float) t * 0.331f));
                buf.setSample (0, i, x);
                buf.setSample (1, i, x * 0.95f);
            }
            proc->processBlock (buf, midi);
        }
        if (auto* view = dynamic_cast<ChromaView*> (editor->getChildComponent (0)))
            view->poll();
    }

    auto save = [&] (const juce::String& name, float scale, bool jpeg)
    {
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        auto file = outDir.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream os (file);
        if (jpeg) { juce::JPEGImageFormat f; f.setQuality (0.92f); f.writeImageToStream (img, os); }
        else      { juce::PNGImageFormat f; f.writeImageToStream (img, os); }
        std::printf ("wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    save ("chroma-" + juce::String (character) + ".png", 2.0f, false);
    save ("chroma-" + juce::String (character) + "-1400.jpg", 1400.0f / (float) ChromaView::designWidth, true);
    editor.reset();
    return 0;
}
