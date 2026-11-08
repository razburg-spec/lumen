#include "LemonSqueezyBackend.h"
#include "LicenseConfig.h"

namespace
{
    LicenseReply fromJson (const juce::var& json, int statusCode, const char* successField)
    {
        LicenseReply reply;
        if (! json.isObject())
        {
            reply.outcome = LicenseReply::Outcome::unreachable;
            return reply;
        }

        const bool ok = (bool) json.getProperty (successField, false);
        reply.message = json.getProperty ("error", {}).toString();
        reply.instanceId = json["instance"].getProperty ("id", {}).toString();
        reply.customerName = json["meta"].getProperty ("customer_name", {}).toString();
        reply.customerEmail = json["meta"].getProperty ("customer_email", {}).toString();

        if (ok)
            reply.outcome = LicenseReply::Outcome::accepted;
        else if (statusCode >= 500 || statusCode == 0)
            reply.outcome = LicenseReply::Outcome::unreachable;
        else
            reply.outcome = LicenseReply::Outcome::rejected;
        return reply;
    }
}

LemonSqueezyBackend::LemonSqueezyBackend (juce::int64 store, juce::int64 product)
    : storeId (store), productId (product) {}

juce::var LemonSqueezyBackend::post (const char* endpoint, const juce::StringPairArray& fields, int& statusCode) const
{
    juce::String body;
    for (const auto& name : fields.getAllKeys())
        body << (body.isEmpty() ? "" : "&") << juce::URL::addEscapeChars (name, true) << '='
             << juce::URL::addEscapeChars (fields[name], true);

    const auto url = juce::URL (juce::String (LicenseConfig::apiBase) + endpoint).withPOSTData (body);
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                             .withExtraHeaders ("Accept: application/json\r\nContent-Type: application/x-www-form-urlencoded")
                             .withConnectionTimeoutMs (12000)
                             .withStatusCode (&statusCode);

    statusCode = 0;
    if (auto stream = url.createInputStream (options))
        return juce::JSON::parse (stream->readEntireStreamAsString());
    return {};
}

bool LemonSqueezyBackend::belongsToUs (const juce::var& reply) const
{
    const auto meta = reply["meta"];
    return (juce::int64) meta.getProperty ("store_id", 0) == storeId
        && (juce::int64) meta.getProperty ("product_id", 0) == productId;
}

LicenseReply LemonSqueezyBackend::activate (const juce::String& key, const juce::String& machineName)
{
    juce::StringPairArray fields;
    fields.set ("license_key", key.trim());
    fields.set ("instance_name", machineName);

    int status = 0;
    const auto json = post ("activate", fields, status);
    auto reply = fromJson (json, status, "activated");

    if (reply.accepted() && ! belongsToUs (json))
    {
        // Valid key for some other product: give the slot back and refuse.
        if (reply.instanceId.isNotEmpty())
            deactivate (key, reply.instanceId);
        reply.outcome = LicenseReply::Outcome::rejected;
        reply.message = "This key is for a different product.";
    }
    return reply;
}

LicenseReply LemonSqueezyBackend::validate (const juce::String& key, const juce::String& instanceId)
{
    juce::StringPairArray fields;
    fields.set ("license_key", key);
    fields.set ("instance_id", instanceId);

    int status = 0;
    const auto json = post ("validate", fields, status);
    auto reply = fromJson (json, status, "valid");
    if (reply.accepted() && ! belongsToUs (json))
        reply.outcome = LicenseReply::Outcome::rejected;
    return reply;
}

LicenseReply LemonSqueezyBackend::deactivate (const juce::String& key, const juce::String& instanceId)
{
    juce::StringPairArray fields;
    fields.set ("license_key", key);
    fields.set ("instance_id", instanceId);

    int status = 0;
    return fromJson (post ("deactivate", fields, status), status, "deactivated");
}
