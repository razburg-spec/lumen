#pragma once
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

// Shared by every Raz Burg AudioCrafts plugin (Lumen, Chroma, Prism, ...).
//
// Once per host session, a background thread fetches the small version manifest
// from the website and compares the entry for this product with the running build.
// Nothing here ever touches the audio thread, the message thread only gets a short
// callback, and every failure (offline, timeout, bad JSON) is silent.
//
// Manifest (https://audiocrafts.razburg.com/versions.json):
//   { "schema": 1,
//     "products": { "<slug>": { "latest": "0.3.2",
//                               "download_url": "https://...",
//                               "notes": "One line about what changed" } } }
namespace rba
{
class UpdateChecker : public juce::ChangeBroadcaster,
                      public juce::DeletedAtShutdown,
                      private juce::Thread,
                      private juce::AsyncUpdater
{
public:
    struct Result
    {
        bool checked = false;           // the manifest was read successfully
        bool updateAvailable = false;   // newer than the running build
        juce::String latestVersion, downloadUrl, notes;
    };

    static constexpr const char* manifestUrl = "https://audiocrafts.razburg.com/versions.json";
    static constexpr const char* defaultDownloadUrl = "https://audiocrafts.razburg.com/account/";

    // Starts the check for this product if it has not run yet in this session.
    // Call from the message thread (e.g. when the editor opens). Cheap to call again.
    void start (const juce::String& productSlug, const juce::String& runningVersion);

    // Message thread only.
    Result getResult() const;

    // Hide the notice for this version (remembered across sessions). A newer
    // release shows the notice again.
    void dismiss();
    bool isDismissed() const;

    // Opens the download page in the default browser.
    void openDownloadPage() const;

    // "1.10.0" > "1.9.2"; missing parts count as 0; any non-digit suffix is ignored.
    static int compareVersions (const juce::String& a, const juce::String& b);

    // Parses a manifest; exposed for tests and for the screenshot tool.
    static Result parseManifest (const juce::String& json, const juce::String& productSlug,
                                 const juce::String& runningVersion);

    // Test/screenshot hook: pretend the manifest returned this.
    void setResultForTesting (const Result&);

    ~UpdateChecker() override;
    JUCE_DECLARE_SINGLETON (UpdateChecker, false)

private:
    UpdateChecker();
    void run() override;
    void handleAsyncUpdate() override;
    juce::PropertiesFile& settings() const;

    juce::String slug, version;
    bool started = false;

    mutable juce::CriticalSection lock;
    Result pending, result;   // pending: written by the worker; result: message thread copy
    mutable std::unique_ptr<juce::PropertiesFile> props;

    JUCE_DECLARE_NON_COPYABLE (UpdateChecker)
};
} // namespace rba
