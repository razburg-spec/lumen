#pragma once
#include <juce_events/juce_events.h>
#include <juce_cryptography/juce_cryptography.h>

// Free activation through the user's audiocrafts.razburg.com account, shared by every
// Raz Burg AudioCrafts plugin. No licence keys and no iLok:
//
//   1. "Activate" opens the browser at /account/#activate=<slug>&m=<machine>&n=<name>&r=<nonce>.
//   2. The user signs in (or signs up) and approves this computer. Max 2 computers per plugin.
//   3. The plugin polls the site's database with the random nonce and receives a secret
//      token for this computer, kept in an encrypted file bound to the machine.
//   4. Every few days it re-checks the token quietly. Removing the computer in My plugins
//      (or deleting the account) deactivates it here on the next check. Offline is fine
//      for up to offlineGraceDays.
//
// One instance per process: use it through juce::SharedResourcePointer<rba::Activation>.
// Each plugin target defines RBA_PRODUCT_SLUG (e.g. "lumen"); the name comes from JucePlugin_Name.
namespace rba
{
class Activation : public juce::ChangeBroadcaster,
                   private juce::Timer
{
public:
    enum class State
    {
        notActivated,   // never activated, removed on the site, or released
        waiting,        // browser opened, waiting for the user to approve it there
        activated,
        verifyNeeded    // activated, but the site has not been reached for too long
    };

    struct Status
    {
        State state = State::notActivated;
        juce::String email;       // account that activated this computer
        juce::String problem;     // last error to show (empty when fine)
    };

    using Done = std::function<void (bool ok, const juce::String& message)>;

    Activation();
    ~Activation() override;

    Status getStatus() const;
    bool isActivated() const;

    void startBrowserActivation();   // opens the browser and starts polling
    void reopenBrowser();            // same request, if the user closed the tab
    void cancelWaiting();
    void deactivate (Done);          // frees this computer's slot on the site
    void verifyNow (Done);
    static void openAccountPage();

    // Audio thread: equals grantFor (thisMachine()) only while the plugin may run freely.
    juce::uint32 grant() const noexcept { return grantWord.load (std::memory_order_relaxed); }
    static juce::uint32 grantFor (const juce::String& machine) noexcept;
    static juce::String thisMachine();     // hashed device id (hex), never the raw id

    static constexpr int maxComputers = 2;
    static constexpr int revalidateEveryDays = 7;
    static constexpr int offlineGraceDays = 60;
    static constexpr int waitMinutes = 15;

    // Exposed for tests.
    static juce::String activationUrl (const juce::String& slug, const juce::String& machine,
                                       const juce::String& computerName, const juce::String& nonce);
    enum class Reply { ok, no, unreachable };
    static Reply parsePoll (int status, const juce::String& body, juce::String& token, juce::String& email);
    static Reply parseOk (int status, const juce::String& body);

private:
    struct Record
    {
        juce::String token, email;
        juce::int64 activatedAt = 0, lastCheck = 0, lastSeen = 0;
    };

    void timerCallback() override;
    void refresh();
    void load();
    void save() const;
    void pollOnce();
    static juce::String newNonce();
    static int rpc (const juce::String& function, const juce::var& args, juce::String& body);   // HTTP status, 0 if none

    mutable juce::CriticalSection lock;
    Record record;
    State state = State::notActivated;
    juce::String problem, nonce;
    juce::int64 waitingSince = 0, lastPoll = 0, lastFileCheck = 0;
    std::atomic<bool> polling { false }, checking { false };
    std::atomic<juce::uint32> grantWord { 0 };
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withThreadName ("RBA activation").withNumberOfThreads (1) };

    JUCE_DECLARE_WEAK_REFERENCEABLE (Activation)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Activation)
};
} // namespace rba
