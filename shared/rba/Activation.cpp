#include "Activation.h"
#include <cstring>
#include <random>

#ifndef RBA_PRODUCT_SLUG
 #error "Define RBA_PRODUCT_SLUG for this plugin target, e.g. target_compile_definitions(MyPlugin PRIVATE RBA_PRODUCT_SLUG=\"lumen\")"
#endif

namespace rba
{
namespace
{
    // The site's Supabase project. The publishable key is public by design: the database
    // only lets it call the activation functions (see the site's supabase/schema.sql).
    constexpr auto apiBase = "https://cmolzvodpvktkjgrfrtt.supabase.co/rest/v1/rpc/";
    constexpr auto apiKey  = "sb_publishable_2AvUWy3rpblb4qEOgNutVw_4Gfx197l";
    constexpr auto siteAccount = "https://audiocrafts.razburg.com/account/";

    constexpr juce::int64 msPerDay = 24 * 60 * 60 * 1000LL;
    juce::int64 now() { return juce::Time::currentTimeMillis(); }

    // Assembled at runtime so the salt is not a readable string in the binary.
    juce::String salt()
    {
        static constexpr unsigned char bytes[] = { 0x52 ^ 0x2a, 0x42 ^ 0x2a, 0x41 ^ 0x2a, 0x2d ^ 0x2a, 0x61 ^ 0x2a,
                                                   0x63 ^ 0x2a, 0x74 ^ 0x2a, 0x76 ^ 0x2a, 0x31 ^ 0x2a, 0x9e ^ 0x2a };
        juce::String s;
        for (auto b : bytes)
            s << juce::String::toHexString ((int) (b ^ 0x2a));
        return s + RBA_PRODUCT_SLUG;
    }

    juce::BlowFish cipher()
    {
        const juce::SHA256 hash (juce::String (Activation::thisMachine() + salt()).toUTF8());
        const auto keyBytes = hash.getRawData();
        return juce::BlowFish (keyBytes.getData(), (int) keyBytes.getSize());
    }

    juce::File stateFile()
    {
       #if JUCE_WINDOWS
        // %APPDATA%\Raz Burg AudioCrafts\<Plugin>\activation.bin
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Raz Burg AudioCrafts").getChildFile (JucePlugin_Name).getChildFile ("activation.bin");
       #else
        // ~/Library/Application Support/Raz Burg AudioCrafts/<Plugin>/activation.bin
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Application Support/Raz Burg AudioCrafts").getChildFile (JucePlugin_Name).getChildFile ("activation.bin");
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

    template <typename Fn>
    void onMessageThread (juce::WeakReference<Activation> weak, Fn&& fn)
    {
        juce::MessageManager::callAsync ([weak, fn = std::forward<Fn> (fn)]() mutable
        {
            if (auto* a = weak.get())
                fn (*a);
        });
    }

    const juce::String unreachableText ("Could not reach audiocrafts.razburg.com. Check your internet connection and try again.");
}

//==============================================================================
juce::String Activation::thisMachine()
{
    static const auto id = juce::SHA256 (juce::String (juce::SystemStats::getUniqueDeviceID() + "|rba-machine").toUTF8())
                               .toHexString().substring (0, 40);
    return id;
}

juce::uint32 Activation::grantFor (const juce::String& machine) noexcept
{
    const juce::SHA256 hash (juce::String (machine + "|" RBA_PRODUCT_SLUG).toUTF8());
    juce::uint32 word = 0;
    std::memcpy (&word, hash.getRawData().getData(), sizeof (word));
    return word | 1u;
}

juce::String Activation::newNonce()
{
    std::random_device rd;   // OS entropy on macOS and Windows
    juce::String s;
    for (int i = 0; i < 4; ++i)
        s << juce::String::toHexString ((juce::int64) (((juce::uint64) rd() << 32) | rd())).paddedLeft ('0', 16);
    return s;
}

juce::String Activation::activationUrl (const juce::String& slug, const juce::String& machine,
                                        const juce::String& computerName, const juce::String& nonce)
{
    return juce::String (siteAccount) + "#activate=" + slug
         + "&m=" + machine
         + "&n=" + juce::URL::addEscapeChars (computerName.substring (0, 60), true)
         + "&r=" + nonce;
}

Activation::Reply Activation::parsePoll (int status, const juce::String& body, juce::String& token, juce::String& email)
{
    if (status != 200)
        return Reply::unreachable;
    const auto json = juce::JSON::parse (body);
    if (json.isVoid() || body.trim() == "null")
        return Reply::no;   // not approved yet
    token = json["token"].toString();
    email = json["email"].toString();
    return token.length() >= 32 ? Reply::ok : Reply::unreachable;
}

Activation::Reply Activation::parseOk (int status, const juce::String& body)
{
    if (status != 200)
        return Reply::unreachable;
    const auto ok = juce::JSON::parse (body)["ok"];
    if (! ok.isBool())
        return Reply::unreachable;
    return (bool) ok ? Reply::ok : Reply::no;
}

int Activation::rpc (const juce::String& function, const juce::var& args, juce::String& body)
{
    int status = 0;
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                             .withConnectionTimeoutMs (10000)
                             .withHttpRequestCmd ("POST")
                             .withExtraHeaders (juce::String ("apikey: ") + apiKey
                                                + "\r\nContent-Type: application/json\r\nAccept: application/json")
                             .withStatusCode (&status);

    const auto url = juce::URL (juce::String (apiBase) + function).withPOSTData (juce::JSON::toString (args, true));
    if (auto stream = url.createInputStream (options))
    {
        juce::MemoryOutputStream out;
        while (! stream->isExhausted() && out.getDataSize() < 64 * 1024)
        {
            char buffer[2048];
            const auto n = stream->read (buffer, (int) sizeof (buffer));
            if (n <= 0)
                break;
            out.write (buffer, (size_t) n);
        }
        body = out.toUTF8();
        return status;
    }
    body = {};
    return 0;
}

//==============================================================================
Activation::Activation()
{
    load();
    refresh();
    startTimer (1000);
}

Activation::~Activation()
{
    stopTimer();
    // Let a request in flight finish (it times out after 10 s) before the plugin binary can unload.
    pool.removeAllJobs (true, 15000);
}

Activation::Status Activation::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return { state, record.email, problem };
}

bool Activation::isActivated() const
{
    const juce::ScopedLock sl (lock);
    return state == State::activated;
}

void Activation::refresh()
{
    State newState;
    {
        const juce::ScopedLock sl (lock);
        const auto t = now();
        const bool clockWentBack = record.lastSeen > 0 && t < record.lastSeen - msPerDay;

        if (record.token.isNotEmpty())
            newState = (t - record.lastCheck <= offlineGraceDays * msPerDay && ! clockWentBack) ? State::activated
                                                                                                 : State::verifyNeeded;
        else if (nonce.isNotEmpty())
            newState = State::waiting;
        else
            newState = State::notActivated;

        if (t > record.lastSeen + 60 * 60 * 1000LL)
        {
            record.lastSeen = t;
            save();
        }

        state = newState;
    }

    grantWord.store (newState == State::activated ? grantFor (thisMachine()) : 0u, std::memory_order_relaxed);
}

void Activation::timerCallback()
{
    const auto before = getStatus();
    const auto t = now();

    bool wantPoll = false, wantVerify = false;
    {
        const juce::ScopedLock sl (lock);

        // Another process (a second DAW, the standalone app) may have activated meanwhile.
        if (record.token.isEmpty() && t - lastFileCheck > 5000)
        {
            lastFileCheck = t;
            load();
            if (record.token.isNotEmpty())
                nonce = {};
        }

        if (nonce.isNotEmpty())
        {
            if (t - waitingSince > waitMinutes * 60 * 1000LL)
            {
                nonce = {};
                problem = "Activation timed out. Click Activate to try again.";
            }
            else if (t - lastPoll >= 3000 && ! polling)
            {
                wantPoll = true;
            }
        }

        if (record.token.isNotEmpty() && t - record.lastCheck > revalidateEveryDays * msPerDay
            && ! checking && t - lastPoll > 10 * 60 * 1000LL)
        {
            lastPoll = t;   // at most one quiet re-check attempt every 10 minutes
            wantVerify = true;
        }
    }

    if (wantPoll)
        pollOnce();
    if (wantVerify)
        verifyNow ({});

    refresh();
    const auto after = getStatus();
    if (after.state != before.state || after.problem != before.problem)
        sendChangeMessage();
}

//==============================================================================
void Activation::startBrowserActivation()
{
    {
        const juce::ScopedLock sl (lock);
        if (record.token.isNotEmpty())
            return;
        nonce = newNonce();
        waitingSince = now();
        lastPoll = now();   // give the browser a moment before the first poll
        problem = {};
    }
    reopenBrowser();
    refresh();
    sendChangeMessage();
}

void Activation::reopenBrowser()
{
    juce::String n;
    {
        const juce::ScopedLock sl (lock);
        n = nonce;
    }
    if (n.isNotEmpty())
        juce::URL (activationUrl (RBA_PRODUCT_SLUG, thisMachine(), juce::SystemStats::getComputerName(), n))
            .launchInDefaultBrowser();
}

void Activation::cancelWaiting()
{
    {
        const juce::ScopedLock sl (lock);
        nonce = {};
        problem = {};
    }
    refresh();
    sendChangeMessage();
}

void Activation::openAccountPage()
{
    juce::URL (siteAccount).launchInDefaultBrowser();
}

void Activation::pollOnce()
{
    juce::String n;
    {
        const juce::ScopedLock sl (lock);
        n = nonce;
        lastPoll = now();
    }
    if (n.isEmpty())
        return;

    polling = true;
    pool.addJob ([n, weak = juce::WeakReference<Activation> (this)]
    {
        auto* args = new juce::DynamicObject();
        args->setProperty ("p_nonce", n);
        juce::String body, token, email;
        const auto status = rpc ("activation_poll", juce::var (args), body);
        const auto reply = parsePoll (status, body, token, email);

        onMessageThread (weak, [=] (Activation& a)
        {
            a.polling = false;
            if (reply != Reply::ok)
                return;   // not yet approved, or offline for a moment: keep polling
            {
                const juce::ScopedLock sl (a.lock);
                if (a.nonce != n)
                    return;   // cancelled meanwhile
                a.nonce = {};
                a.problem = {};
                a.record.token = token;
                a.record.email = email;
                a.record.activatedAt = a.record.lastCheck = now();
                a.save();
            }
            a.refresh();
            a.sendChangeMessage();
        });
    });
}

void Activation::verifyNow (Done done)
{
    juce::String token;
    {
        const juce::ScopedLock sl (lock);
        token = record.token;
    }
    if (token.isEmpty() || checking.exchange (true))
    {
        if (done)
            done (false, {});
        return;
    }

    pool.addJob ([token, done, weak = juce::WeakReference<Activation> (this)]
    {
        auto* args = new juce::DynamicObject();
        args->setProperty ("p_token", token);
        args->setProperty ("p_machine", thisMachine());
        juce::String body;
        const auto status = rpc ("activation_check", juce::var (args), body);
        const auto reply = parseOk (status, body);

        onMessageThread (weak, [=] (Activation& a)
        {
            a.checking = false;
            {
                const juce::ScopedLock sl (a.lock);
                if (a.record.token == token)
                {
                    if (reply == Reply::ok)
                    {
                        a.record.lastCheck = now();
                        a.problem = {};
                    }
                    else if (reply == Reply::no)
                    {
                        a.record = { {}, {}, 0, 0, a.record.lastSeen };
                        a.problem = "This computer was removed from your account. Click Activate to activate it again.";
                    }
                    a.save();
                }
            }
            a.refresh();
            a.sendChangeMessage();
            if (done)
                done (reply == Reply::ok, reply == Reply::unreachable ? unreachableText : juce::String());
        });
    });
}

void Activation::deactivate (Done done)
{
    juce::String token;
    {
        const juce::ScopedLock sl (lock);
        token = record.token;
    }
    if (token.isEmpty())
    {
        if (done)
            done (true, {});
        return;
    }

    pool.addJob ([token, done, weak = juce::WeakReference<Activation> (this)]
    {
        auto* args = new juce::DynamicObject();
        args->setProperty ("p_token", token);
        juce::String body;
        const auto status = rpc ("activation_release", juce::var (args), body);
        const auto reply = parseOk (status, body);

        onMessageThread (weak, [=] (Activation& a)
        {
            if (reply != Reply::unreachable)
            {
                const juce::ScopedLock sl (a.lock);
                a.record = { {}, {}, 0, 0, a.record.lastSeen };
                a.problem = {};
                a.save();
            }
            a.refresh();
            a.sendChangeMessage();
            if (done)
                done (reply != Reply::unreachable, reply == Reply::unreachable ? unreachableText : juce::String());
        });
    });
}

//==============================================================================
void Activation::save() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("t", record.token);
    obj->setProperty ("e", record.email);
    obj->setProperty ("a", record.activatedAt);
    obj->setProperty ("c", record.lastCheck);
    obj->setProperty ("s", record.lastSeen);
    writeSealed (stateFile(), juce::JSON::toString (juce::var (obj), true));
}

void Activation::load()
{
    const auto json = juce::JSON::parse (readSealed (stateFile()));
    if (! json.isObject())
        return;
    record.token = json["t"].toString();
    record.email = json["e"].toString();
    record.activatedAt = (juce::int64) json["a"];
    record.lastCheck = (juce::int64) json["c"];
    record.lastSeen = juce::jmax (record.lastSeen, (juce::int64) json["s"]);
}
} // namespace rba
