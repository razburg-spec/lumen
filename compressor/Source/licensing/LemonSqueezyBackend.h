#pragma once
#include "LicenseBackend.h"

// Lemon Squeezy License API (public endpoints, no API key):
//   POST {apiBase}activate    license_key, instance_name
//   POST {apiBase}validate    license_key, instance_id
//   POST {apiBase}deactivate  license_key, instance_id
// Replies are only accepted when meta.store_id / meta.product_id match ours,
// so a key bought for another product cannot unlock this one.
class LemonSqueezyBackend : public LicenseBackend
{
public:
    LemonSqueezyBackend (juce::int64 storeId, juce::int64 productId);

    LicenseReply activate (const juce::String& key, const juce::String& machineName) override;
    LicenseReply validate (const juce::String& key, const juce::String& instanceId) override;
    LicenseReply deactivate (const juce::String& key, const juce::String& instanceId) override;

private:
    juce::var post (const char* endpoint, const juce::StringPairArray& fields, int& statusCode) const;
    bool belongsToUs (const juce::var& reply) const;

    juce::int64 storeId, productId;
};
