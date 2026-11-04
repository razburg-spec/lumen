#pragma once
#include <juce_core/juce_core.h>

// Store and policy settings. The IDs are placeholders until the Lemon Squeezy
// store exists: replace them with the real store and product IDs before release.
namespace LicenseConfig
{
    inline constexpr juce::int64 storeId   = 0;    // TODO: Lemon Squeezy store ID
    inline constexpr juce::int64 productId = 0;    // TODO: Lemon Squeezy product ID for Lumen

    inline constexpr auto apiBase = "https://api.lemonsqueezy.com/v1/licenses/";
    inline constexpr auto buyUrl  = "https://audiocrafts.razburg.com/#lumen";

    inline constexpr int trialDays = 14;             // 0 = no trial: only an activated licence runs
    inline constexpr int revalidateEveryDays = 14;   // online re-check of an activated licence
    inline constexpr int offlineGraceDays = 45;      // how long it keeps working without reaching the server
}
