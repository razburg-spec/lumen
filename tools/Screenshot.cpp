// Renders the Lumen interface to image files, for the website and for checking the layout.
// Usage: LumenScreenshot <out-dir>
// Writes lumen-main.jpg (shimmer + reverb on) and lumen-advanced.jpg (every parameter shown).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static void set (LumenProcessor& p, const char* id, float plain)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    for (const bool advanced : { false, true })
    {
        std::unique_ptr<LumenProcessor> proc (dynamic_cast<LumenProcessor*> (createPluginFilter()));
        using namespace ParamID;
        set (*proc, shimOn, 1.0f);   set (*proc, shimPitchA, 12.0f); set (*proc, shimPitchB, 7.0f);
        set (*proc, shimFeedback, 55.0f); set (*proc, shimDecay, 9.0f);
        set (*proc, revOn, 1.0f);    set (*proc, revDecay, 3.2f);    set (*proc, revLevel, -4.0f);
        set (*proc, delayOn, 1.0f);  set (*proc, delayFeedback, 30.0f); set (*proc, delayLevel, -8.0f);
        set (*proc, mix, 40.0f);
        proc->apvts.state.setProperty ("advancedView", advanced, nullptr);
        proc->setPlayConfigDetails (2, 2, 48000.0, 512);
        proc->prepareToPlay (48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
        editor->setSize (LumenView::designWidth, LumenView::designHeight);
        if (auto* view = dynamic_cast<LumenView*> (editor->getChildComponent (0)))
            view->poll();

        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile (advanced ? "lumen-advanced.jpg" : "lumen-main.jpg");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::JPEGImageFormat f; f.setQuality (0.9f); f.writeImageToStream (img, os);
        std::printf ("wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        editor.reset();
    }
    return 0;
}
