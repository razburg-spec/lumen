#include "UpdateChecker.h"

namespace rba
{
JUCE_IMPLEMENT_SINGLETON (UpdateChecker)

UpdateChecker::UpdateChecker() : juce::Thread ("RBA update check") {}

UpdateChecker::~UpdateChecker()
{
    // The fetch aborts within a moment once asked to (see the progress callback).
    signalThreadShouldExit();
    stopThread (3000);
    cancelPendingUpdate();
    clearSingletonInstance();
}

void UpdateChecker::start (const juce::String& productSlug, const juce::String& runningVersion)
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (started)
        return;
    started = true;
    slug = productSlug;
    version = runningVersion;
    startThread (juce::Thread::Priority::background);
}

void UpdateChecker::run()
{
    // Give the host a moment to finish loading sessions before we touch the network.
    for (int i = 0; i < 30 && ! threadShouldExit(); ++i)
        wait (100);
    if (threadShouldExit())
        return;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (5000)
                             .withExtraHeaders ("Accept: application/json\r\nCache-Control: no-cache")
                             .withProgressCallback ([this] (int, int) { return ! threadShouldExit(); });

    juce::String body;
    if (auto stream = juce::URL (manifestUrl).createInputStream (options))
    {
        // The manifest is a few hundred bytes; refuse anything absurd.
        juce::MemoryOutputStream out;
        while (! stream->isExhausted() && ! threadShouldExit() && out.getDataSize() < 64 * 1024)
        {
            char buffer[2048];
            const auto n = stream->read (buffer, (int) sizeof (buffer));
            if (n <= 0)
                break;
            out.write (buffer, (size_t) n);
        }
        body = out.toUTF8();
    }

    if (threadShouldExit() || body.isEmpty())
        return;   // offline or cancelled: stay silent

    const auto parsed = parseManifest (body, slug, version);
    {
        const juce::ScopedLock sl (lock);
        pending = parsed;
    }
    triggerAsyncUpdate();
}

void UpdateChecker::handleAsyncUpdate()
{
    {
        const juce::ScopedLock sl (lock);
        result = pending;
    }
    sendChangeMessage();
}

UpdateChecker::Result UpdateChecker::getResult() const
{
    const juce::ScopedLock sl (lock);
    return result;
}

void UpdateChecker::setResultForTesting (const Result& r)
{
    started = true;
    {
        const juce::ScopedLock sl (lock);
        pending = result = r;
    }
    sendChangeMessage();
}

UpdateChecker::Result UpdateChecker::parseManifest (const juce::String& json, const juce::String& productSlug,
                                                    const juce::String& runningVersion)
{
    Result r;
    const auto root = juce::JSON::parse (json);
    const auto entry = root.getProperty ("products", {}).getProperty (productSlug, {});
    if (! entry.isObject())
        return r;

    r.latestVersion = entry.getProperty ("latest", {}).toString().trim();
    if (r.latestVersion.isEmpty())
        return r;

    r.checked = true;
    r.notes = entry.getProperty ("notes", {}).toString().trim();
    r.downloadUrl = entry.getProperty ("download_url", {}).toString().trim();
    // Only ever open our own site.
    if (! r.downloadUrl.startsWith ("https://audiocrafts.razburg.com/"))
        r.downloadUrl = defaultDownloadUrl;
    r.updateAvailable = compareVersions (r.latestVersion, runningVersion) > 0;
    return r;
}

int UpdateChecker::compareVersions (const juce::String& a, const juce::String& b)
{
    auto parts = [] (const juce::String& s)
    {
        juce::StringArray tokens;
        tokens.addTokens (s.trim().trimCharactersAtStart ("vV"), ".", {});
        juce::Array<int> numbers;
        for (auto& t : tokens)
            numbers.add (t.initialSectionContainingOnly ("0123456789").getIntValue());
        return numbers;
    };

    const auto pa = parts (a), pb = parts (b);
    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const auto x = pa[i], y = pb[i];   // out of range reads as 0
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

juce::PropertiesFile& UpdateChecker::settings() const
{
    if (props == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Updates";
        o.folderName = "Raz Burg AudioCrafts";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);
    }
    return *props;
}

bool UpdateChecker::isDismissed() const
{
    const auto r = getResult();
    return r.latestVersion.isNotEmpty()
        && settings().getValue ("dismissed." + slug) == r.latestVersion;
}

void UpdateChecker::dismiss()
{
    const auto r = getResult();
    if (r.latestVersion.isEmpty())
        return;
    settings().setValue ("dismissed." + slug, r.latestVersion);
    settings().saveIfNeeded();
    sendChangeMessage();
}

void UpdateChecker::openDownloadPage() const
{
    const auto r = getResult();
    juce::URL (r.downloadUrl.isNotEmpty() ? r.downloadUrl : juce::String (defaultDownloadUrl)).launchInDefaultBrowser();
}
} // namespace rba
