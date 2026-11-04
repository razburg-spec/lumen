#include "LicenseManager.h"
#include <cstring>
#include <iterator>
#include "LicenseConfig.h"
#include "LemonSqueezyBackend.h"

namespace
{
    constexpr juce::int64 msPerDay = 24 * 60 * 60 * 1000LL;

    juce::int64 now() { return juce::Time::currentTimeMillis(); }

    // Assembled at runtime so the salt is not a readable string in the binary.
    juce::String salt()
    {
        static constexpr unsigned char bytes[] = { 0x52 ^ 0x2a, 0x42 ^ 0x2a, 0x41 ^ 0x2a, 0x43 ^ 0x2a, 0x2d ^ 0x2a,
                                                   0x4c ^ 0x2a, 0x6d ^ 0x2a, 0x6e ^ 0x2a, 0x37 ^ 0x2a, 0x91 ^ 0x2a };
        juce::String s;
        for (auto b : bytes)
            s << juce::String::toHexString ((int) (b ^ 0x2a));
        return s;
    }

    juce::BlowFish cipher()
    {
        const juce::SHA256 hash (juce::String (LicenseManager::thisMachine() + salt()).toUTF8());
        const auto keyBytes = hash.getRawData();
        return juce::BlowFish (keyBytes.getData(), (int) keyBytes.getSize());
    }

    // The main record, plus a second hidden copy of the trial start so that
    // deleting one file does not restart the trial.
    juce::File stateFile()
    {
       #if JUCE_WINDOWS
        // %APPDATA%\Raz Burg AudioCrafts\Lumen\state.bin
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Raz Burg AudioCrafts/Lumen/state.bin");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Application Support/Raz Burg AudioCrafts/Lumen/state.bin");
       #endif
    }

    juce::File shadowFile()
    {
       #if JUCE_WINDOWS
        // %LOCALAPPDATA%-style second location, kept apart from the main record.
        auto f = juce::File::getSpecialLocation (juce::File::windowsLocalAppData)
            .getChildFile ("RBAC/cache.dat");
        return f;
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Preferences/.com.razburgaudiocrafts.cache");
       #endif
    }

    void writeSealed (const juce::File& file, const juce::String& text)
    {
        juce::MemoryBlock data;
        const juce::SHA256 check (juce::String (text + salt()).toUTF8());
        data.append (check.getRawData().getData(), check.getRawData().getSize());
        data.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
        cipher().encrypt (data);
        file.getParentDirectory().createDirectory();
        file.replaceWithData (data.getData(), data.getSize());
    }

    juce::String readSealed (const juce::File& file)
    {
        juce::MemoryBlock data;
        if (! file.loadFileAsData (data) || data.getSize() <= 32)
            return {};
        const auto size = cipher().decrypt (data.getData(), data.getSize());
        if (size <= 32)
            return {};
        data.setSize ((size_t) size);
        const auto text = juce::String::fromUTF8 (static_cast<const char*> (data.getData()) + 32, (int) data.getSize() - 32);
        const juce::SHA256 check (juce::String (text + salt()).toUTF8());
        return std::memcmp (check.getRawData().getData(), data.getData(), 32) == 0 ? text : juce::String();
    }

    juce::String mask (const juce::String& key)
    {
        return key.length() <= 8 ? key : key.substring (0, 4) + juce::String::repeatedString (juce::String::fromUTF8 ("\xe2\x80\xa2"), 6) + key.getLastCharacters (4);
    }
}

//==============================================================================
juce::String LicenseManager::thisMachine()
{
    static const auto id = juce::SystemStats::getUniqueDeviceID();
    return id;
}

juce::uint32 LicenseManager::grantFor (const juce::String& machine) noexcept
{
    const juce::SHA256 hash (juce::String (machine + "|lumen").toUTF8());
    juce::uint32 word = 0;
    std::memcpy (&word, hash.getRawData().getData(), sizeof (word));
    return word | 1u;
}

LicenseManager::LicenseManager()
    : backend (std::make_shared<LemonSqueezyBackend> (LicenseConfig::storeId, LicenseConfig::productId))
{
    load();
    refresh();
    startTimer (60 * 1000);

    // Periodic online re-check of an activated licence (quiet, in the background).
    const juce::ScopedLock sl (lock);
    if (record.instanceId.isNotEmpty() && now() - record.lastCheck > LicenseConfig::revalidateEveryDays * msPerDay)
        juce::MessageManager::callAsync ([weak = juce::WeakReference<LicenseManager> (this)]
        {
            if (weak != nullptr)
                weak->verifyNow ({});
        });
}

LicenseManager::~LicenseManager()
{
    stopTimer();
}

void LicenseManager::timerCallback()
{
    refresh();
}

LicenseManager::Status LicenseManager::getStatus() const
{
    const juce::ScopedLock sl (lock);
    Status s;
    s.state = state;
    s.trialDaysLeft = daysLeft;
    s.customerName = record.name;
    s.customerEmail = record.email;
    s.maskedKey = mask (record.key);
    return s;
}

void LicenseManager::refresh()
{
    State newState;
    bool changed = false;
    {
        const juce::ScopedLock sl (lock);
        const auto t = now();
        const bool clockWentBack = record.lastSeen > 0 && t < record.lastSeen - msPerDay;

        if (record.instanceId.isNotEmpty())
        {
            newState = (t - record.lastCheck <= LicenseConfig::offlineGraceDays * msPerDay && ! clockWentBack)
                           ? State::licensed : State::verifyNeeded;
        }
        else if (record.trialStart > 0)
        {
            const auto used = (t - record.trialStart) / msPerDay;
            daysLeft = juce::jmax (0, LicenseConfig::trialDays - (int) used);
            newState = (daysLeft > 0 && ! clockWentBack) ? State::trial : State::trialOver;
        }
        else
        {
            newState = State::none;
        }

        if (t > record.lastSeen + 60 * 60 * 1000LL)
        {
            record.lastSeen = t;
            save();
        }

        changed = newState != state;
        state = newState;
    }

    const bool free = newState == State::licensed || newState == State::trial;
    grantWord.store (free ? grantFor (thisMachine()) : 0u, std::memory_order_relaxed);

    if (changed)
        sendChangeMessage();
}

void LicenseManager::startTrial()
{
    if (LicenseConfig::trialDays <= 0)
        return;
    {
        const juce::ScopedLock sl (lock);
        if (record.trialStart == 0)
            record.trialStart = now();
        save();
    }
    refresh();
    sendChangeMessage();
}

void LicenseManager::runInBackground (std::function<LicenseReply (LicenseBackend&)> job,
                                      std::function<void (LicenseManager&, const LicenseReply&)> apply, Done done)
{
    juce::Thread::launch ([backendCopy = backend, job, apply, done, weak = juce::WeakReference<LicenseManager> (this)]
    {
        const auto reply = job (*backendCopy);
        juce::MessageManager::callAsync ([reply, apply, done, weak]
        {
            if (weak != nullptr)
            {
                apply (*weak, reply);
                weak->refresh();
                weak->sendChangeMessage();
            }
            if (done)
                done (reply.accepted(), reply.message);
        });
    });
}

void LicenseManager::activate (const juce::String& key, Done done)
{
    const auto machineName = "Lumen - " + juce::SystemStats::getComputerName();
    const auto cleanKey = key.trim();

    runInBackground ([cleanKey, machineName] (LicenseBackend& b) { return b.activate (cleanKey, machineName); },
                     [cleanKey] (LicenseManager& m, const LicenseReply& reply)
                     {
                         if (! reply.accepted())
                             return;
                         const juce::ScopedLock sl (m.lock);
                         m.record.key = cleanKey;
                         m.record.instanceId = reply.instanceId;
                         m.record.name = reply.customerName;
                         m.record.email = reply.customerEmail;
                         m.record.activatedAt = m.record.lastCheck = now();
                         m.save();
                     },
                     [done] (bool ok, const juce::String& message)
                     {
                         if (done)
                             done (ok, ok ? juce::String() : (message.isNotEmpty() ? message : juce::String ("Could not reach the licence server. Check your connection.")));
                     });
}

void LicenseManager::deactivate (Done done)
{
    juce::String key, instance;
    {
        const juce::ScopedLock sl (lock);
        key = record.key;
        instance = record.instanceId;
    }

    runInBackground ([key, instance] (LicenseBackend& b) { return b.deactivate (key, instance); },
                     [] (LicenseManager& m, const LicenseReply& reply)
                     {
                         // A slot the server no longer knows about is free anyway.
                         if (reply.unreachable())
                             return;
                         const juce::ScopedLock sl (m.lock);
                         m.record.key = {};
                         m.record.instanceId = {};
                         m.record.name = {};
                         m.record.email = {};
                         m.save();
                     },
                     [done] (bool ok, const juce::String& message)
                     {
                         if (done)
                             done (ok, ok ? juce::String() : (message.isNotEmpty() ? message : juce::String ("Could not reach the licence server. Check your connection.")));
                     });
}

void LicenseManager::verifyNow (Done done)
{
    juce::String key, instance;
    {
        const juce::ScopedLock sl (lock);
        if (checking || record.instanceId.isEmpty())
            return;
        checking = true;
        key = record.key;
        instance = record.instanceId;
    }

    runInBackground ([key, instance] (LicenseBackend& b) { return b.validate (key, instance); },
                     [] (LicenseManager& m, const LicenseReply& reply)
                     {
                         const juce::ScopedLock sl (m.lock);
                         m.checking = false;
                         if (reply.accepted())
                         {
                             m.record.lastCheck = now();
                         }
                         else if (! reply.unreachable())
                         {
                             // Refunded, disabled or moved to another machine.
                             m.record.key = {};
                             m.record.instanceId = {};
                         }
                         m.save();
                     },
                     std::move (done));
}

//==============================================================================
void LicenseManager::save() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("k", record.key);
    obj->setProperty ("i", record.instanceId);
    obj->setProperty ("n", record.name);
    obj->setProperty ("e", record.email);
    obj->setProperty ("a", record.activatedAt);
    obj->setProperty ("c", record.lastCheck);
    obj->setProperty ("t", record.trialStart);
    obj->setProperty ("s", record.lastSeen);
    writeSealed (stateFile(), juce::JSON::toString (juce::var (obj), true));

    if (record.trialStart > 0)
        writeSealed (shadowFile(), juce::String (record.trialStart));
}

void LicenseManager::load()
{
    const auto json = juce::JSON::parse (readSealed (stateFile()));
    if (json.isObject())
    {
        record.key = json["k"].toString();
        record.instanceId = json["i"].toString();
        record.name = json["n"].toString();
        record.email = json["e"].toString();
        record.activatedAt = (juce::int64) json["a"];
        record.lastCheck = (juce::int64) json["c"];
        record.trialStart = (juce::int64) json["t"];
        record.lastSeen = (juce::int64) json["s"];
    }

    // The earliest trial start we know of wins.
    const auto shadow = readSealed (shadowFile()).getLargeIntValue();
    if (shadow > 0 && (record.trialStart == 0 || shadow < record.trialStart))
        record.trialStart = shadow;
}
