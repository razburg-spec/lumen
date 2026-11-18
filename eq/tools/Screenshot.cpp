// Renders the Prism interface to image files, for the website and for checking the layout.
// Usage: PrismScreenshot <out-dir> [mode 0-5]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <random>

static void set (PrismProcessor& p, const juce::String& id, float plain)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

static void pump (juce::Component& c)
{
    if (auto* view = dynamic_cast<PrismView*> (&c)) { view->poll(); return; }
    for (auto* child : c.getChildren())
        pump (*child);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const int mode = argc > 2 ? juce::String (argv[2]).getIntValue() : 4;
    outDir.createDirectory();

    std::unique_ptr<PrismProcessor> proc (dynamic_cast<PrismProcessor*> (createPluginFilter()));
    set (*proc, ParamID::mode, (float) mode);
    set (*proc, Band::id (0, ParamID::on), 1.0f);
    set (*proc, Band::id (0, ParamID::freq), 45.0f);
    set (*proc, Band::id (1, ParamID::freq), 120.0f);  set (*proc, Band::id (1, ParamID::gain), 3.5f);  set (*proc, Band::id (1, ParamID::q), 0.9f);
    set (*proc, Band::id (2, ParamID::freq), 420.0f);  set (*proc, Band::id (2, ParamID::gain), -4.0f); set (*proc, Band::id (2, ParamID::q), 1.6f);
    set (*proc, Band::id (3, ParamID::freq), 3200.0f); set (*proc, Band::id (3, ParamID::gain), 3.0f);  set (*proc, Band::id (3, ParamID::q), 1.1f);
    set (*proc, Band::id (4, ParamID::freq), 7500.0f); set (*proc, Band::id (4, ParamID::gain), -2.0f); set (*proc, Band::id (4, ParamID::q), 3.0f);
    set (*proc, Band::id (5, ParamID::on), 1.0f);
    set (*proc, Band::id (5, ParamID::type), 1.0f);
    set (*proc, Band::id (5, ParamID::freq), 11000.0f); set (*proc, Band::id (5, ParamID::gain), 4.0f);
    set (*proc, ParamID::drive, 20.0f);
    if (argc > 3 && juce::String (argv[3]) == "linear")
        set (*proc, ParamID::linear, 1.0f);

    const double sr = 48000.0;
    proc->setPlayConfigDetails (2, 2, sr, 512);
    proc->prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    editor->setSize (PrismView::designWidth, PrismView::designHeight);

    // Feed a music-like signal (pink noise with a few tones) so the analyzer shows both spectra.
    std::mt19937 rng (3);
    std::normal_distribution<float> noise (0.0f, 1.0f);
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int blk = 0; blk < 120; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float w = noise (rng);
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            const float pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.02f;
            b6 = w * 0.115926f;
            phase += 1.0 / sr;
            const float tones = 0.12f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * phase)
                              + 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * phase)
                              + 0.02f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1320.0 * phase);
            buf.setSample (0, i, pink + tones);
            buf.setSample (1, i, pink * 0.9f + tones);
        }
        proc->processBlock (buf, midi);
        if (blk % 4 == 0)
            pump (*editor);
    }
    pump (*editor);
    if (auto* view = dynamic_cast<PrismView*> (editor->getChildComponent (0)))
        for (auto* c : view->getChildren())
            if (auto* d = dynamic_cast<EqDisplay*> (c))
                { d->setSelectedBand (3); d->previewHover (3); }

    for (float scale : { 1.0f, 2.0f })
    {
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        const auto suffix = scale > 1.5f ? "-2x" : "";
        auto file = outDir.getChildFile ("prism-" + juce::String (mode) + suffix + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    editor.reset();
    return 0;
}
