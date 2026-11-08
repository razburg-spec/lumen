#pragma once
#include <juce_events/juce_events.h>
#include <juce_cryptography/juce_cryptography.h>
#include "LicenseBackend.h"

// One per process (use through juce::SharedResourcePointer), shared by every plugin
// instance and format. Owns the activation, the trial clock and the encrypted state
// file, and publishes a grant word that the audio thread checks.
class LicenseManager : public juce::ChangeBroadcaster,
                       private juce::Timer
{
public:
    enum class State
    {
        none,           // never activated, trial not started
        trial,
        trialOver,
        licensed,
        verifyNeeded    // activated, but the server has not been reached for too long
    };

    struct Status
    {
        State state = State::none;
        int trialDaysLeft = 0;
        juce::String customerName, customerEmail, maskedKey;
    };

    using Done = std::function<void (bool ok, const juce::String& message)>;

    LicenseManager();
    ~LicenseManager() override;

    Status getStatus() const;
    void startTrial();
    void activate (const juce::String& key, Done);
    void deactivate (Done);
    void verifyNow (Done);

    // Audio thread: equals grantFor (thisMachine()) only while the plugin may run freely.
    juce::uint32 grant() const noexcept { return grantWord.load (std::memory_order_relaxed); }
    static juce::uint32 grantFor (const juce::String& machine) noexcept;
    static juce::String thisMachine();

private:
    struct Record
    {
        juce::String key, instanceId, name, email;
        juce::int64 activatedAt = 0, lastCheck = 0, trialStart = 0, lastSeen = 0;
    };

    void timerCallback() override;
    void refresh();
    void load();
    void save() const;
    void runInBackground (std::function<LicenseReply (LicenseBackend&)> job,
                          std::function<void (LicenseManager&, const LicenseReply&)> apply, Done done);

    std::shared_ptr<LicenseBackend> backend;
    mutable juce::CriticalSection lock;
    Record record;
    State state = State::none;
    int daysLeft = 0;
    std::atomic<juce::uint32> grantWord { 0 };
    bool checking = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (LicenseManager)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseManager)
};
