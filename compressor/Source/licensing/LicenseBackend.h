#pragma once
#include <juce_core/juce_core.h>

// What the store's licence server answered. Kept store-neutral so a different
// backend (for example a server that returns signed certificates) can replace
// Lemon Squeezy without touching the manager or the UI.
struct LicenseReply
{
    enum class Outcome
    {
        accepted,       // activation / validation / deactivation succeeded
        rejected,       // the server answered and said no (bad key, limit reached, refunded...)
        unreachable     // no usable answer (offline, timeout, server error)
    };

    Outcome outcome = Outcome::unreachable;
    juce::String message;        // human-readable reason from the server, if any
    juce::String instanceId;     // activation slot id, returned by activate()
    juce::String customerName, customerEmail;

    bool accepted() const noexcept   { return outcome == Outcome::accepted; }
    bool unreachable() const noexcept { return outcome == Outcome::unreachable; }
};

class LicenseBackend
{
public:
    virtual ~LicenseBackend() = default;

    // All three block while talking to the network: call them off the message thread.
    virtual LicenseReply activate (const juce::String& key, const juce::String& machineName) = 0;
    virtual LicenseReply validate (const juce::String& key, const juce::String& instanceId) = 0;
    virtual LicenseReply deactivate (const juce::String& key, const juce::String& instanceId) = 0;
};
