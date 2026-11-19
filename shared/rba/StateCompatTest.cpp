// Backward-compatibility test for saved plugin state (see StateIO.h).
//
//   <Plugin>StateTest                  check every golden state in RBA_FIXTURES_DIR
//   <Plugin>StateTest write <file>     write a golden state of THIS build (do this once per release)
//
// For each fixture <name>.xml (a state exactly as an older release saved it) the test:
//   1. takes an instance whose every parameter was moved away from its default,
//   2. loads the fixture through setStateInformation (the host's path),
//   3. checks each parameter equals: <name>.expect.xml override > value in the fixture > default,
//   4. checks root properties of the fixture survive, and that save -> load round-trips.
#include <juce_audio_processors/juce_audio_processors.h>
#include <rba/StateIO.h>
#include <cstdio>


namespace
{
using Proc = std::unique_ptr<juce::AudioProcessor>;

Proc make()
{
    Proc p (createPluginFilter());
    p->setPlayConfigDetails (2, 2, 48000.0, 512);
    return p;
}

juce::Array<juce::RangedAudioParameter*> params (juce::AudioProcessor& p)
{
    juce::Array<juce::RangedAudioParameter*> out;
    for (auto* x : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x))
            out.add (r);
    return out;
}

float plain (juce::RangedAudioParameter& r) { return r.convertFrom0to1 (r.getValue()); }
float snapped (juce::RangedAudioParameter& r, float v) { return r.convertFrom0to1 (r.convertTo0to1 (v)); }

// Moves every parameter somewhere that is neither its default nor (usually) a fixture value.
void dirty (juce::AudioProcessor& p, float where)
{
    for (auto* r : params (p))
    {
        auto n = std::fmod (r->getDefaultValue() + where, 1.0f);
        r->setValueNotifyingHost (n);
    }
}

bool close (float a, float b) { return std::abs (a - b) <= 1.0e-4f * juce::jmax (1.0f, std::abs (a), std::abs (b)); }

juce::MemoryBlock toBinary (const juce::XmlElement& xml)
{
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (xml, mb);
    return mb;
}

int checkFixture (const juce::File& file)
{
    int failures = 0;
    auto fail = [&] (const juce::String& msg) { ++failures; std::printf ("  FAIL %s\n", msg.toRawUTF8()); };

    const auto fixture = juce::XmlDocument::parse (file);
    if (fixture == nullptr) { fail ("cannot parse " + file.getFileName()); return failures; }
    const auto tree = juce::ValueTree::fromXml (*fixture);

    juce::ValueTree expect;
    const auto expectFile = file.getSiblingFile (file.getFileNameWithoutExtension() + ".expect.xml");
    if (expectFile.existsAsFile())
        if (auto e = juce::XmlDocument::parse (expectFile))
            expect = juce::ValueTree::fromXml (*e);

    for (const float where : { 0.37f, 0.71f })
    {
        auto p = make();
        dirty (*p, where);
        const auto bin = toBinary (*fixture);
        p->setStateInformation (bin.getData(), (int) bin.getSize());

        for (auto* r : params (*p))
        {
            const auto id = r->getParameterID();
            float want;
            if (expect.isValid() && rba::state::hasParam (expect, id))
                want = rba::state::getParam (expect, id, 0.0f);
            else if (rba::state::hasParam (tree, id))
                want = rba::state::getParam (tree, id, 0.0f);
            else
                want = r->convertFrom0to1 (r->getDefaultValue());
            want = snapped (*r, want);
            if (! close (plain (*r), want))
                fail (id + ": got " + juce::String (plain (*r)) + ", want " + juce::String (want));
        }

        // Non-parameter settings (e.g. view options) survive, and save -> load is lossless.
        juce::MemoryBlock saved;
        p->getStateInformation (saved);
        const auto savedXml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        if (savedXml == nullptr) { fail ("saved state unreadable"); continue; }
        for (int i = 0; i < tree.getNumProperties(); ++i)
        {
            const auto name = tree.getPropertyName (i);
            if (name != rba::state::versionProperty && name != rba::state::pluginVersionProperty
                && savedXml->getStringAttribute (name.toString()) != tree.getProperty (name).toString())
                fail ("root property " + name.toString() + " lost");
        }

        auto q = make();
        dirty (*q, 1.0f - where);
        q->setStateInformation (saved.getData(), (int) saved.getSize());
        auto pp = params (*p), qq = params (*q);
        for (int i = 0; i < pp.size(); ++i)
            if (! close (plain (*pp[i]), plain (*qq[i])))
                fail ("round trip " + pp[i]->getParameterID());
    }
    return failures;
}

int writeFixture (const juce::File& out)
{
    auto p = make();
    // Deterministic, non-default values that exercise every parameter.
    int i = 0;
    for (auto* r : params (*p))
        r->setValueNotifyingHost (std::fmod (r->getDefaultValue() + 0.13f + 0.29f * (float) (i++ % 7), 1.0f));
    juce::MemoryBlock mb;
    p->getStateInformation (mb);
    auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (xml == nullptr || ! out.replaceWithText (xml->toString()))
        return 1;
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "write")
        return writeFixture (juce::File (juce::String (argv[2])));

    const juce::File dir (RBA_FIXTURES_DIR);
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    int failures = 0, checked = 0;
    for (auto& f : files)
    {
        if (f.getFileName().endsWith (".expect.xml"))
            continue;
        std::printf ("%s\n", f.getFileName().toRawUTF8());
        const auto n = checkFixture (f);
        std::printf ("  %s\n", n == 0 ? "ok" : "FAILED");
        failures += n;
        ++checked;
    }
    if (checked == 0) { std::printf ("no fixtures in %s\n", dir.getFullPathName().toRawUTF8()); return 1; }
    std::printf ("%d fixture(s), %d failure(s)\n", checked, failures);
    return failures == 0 ? 0 : 1;
}
