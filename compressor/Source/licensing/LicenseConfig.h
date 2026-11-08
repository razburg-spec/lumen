#pragma once
#include <juce_core/juce_core.h>

// Store and policy settings. The IDs are placeholders until the Lemon Squeezy
// product exists: replace them with the real store and product IDs before release.
namespace LicenseConfig
{
    inline constexpr juce::int64 storeId   = 0;    // TODO: Lemon Squeezy store ID (same store as Lumen)
    inline constexpr juce::int64 productId = 0;    // TODO: Lemon Squeezy product ID for this compressor

    // Fixed per product: keeps licence files and trial clocks separate from Lumen's.
    // Never change these after release, or every user's activation is lost.
    inline constexpr auto slug       = "rbacomp";
    inline constexpr auto folderName = JucePlugin_Name;

    inline constexpr auto apiBase = "https://api.lemonsqueezy.com/v1/licenses/";
    inline constexpr auto buyUrl  = "https://audiocrafts.razburg.com/#chroma";

    inline constexpr int trialDays = 14;             // 0 = no trial: only an activated licence runs
    inline constexpr int revalidateEveryDays = 14;   // online re-check of an activated licence
    inline constexpr int offlineGraceDays = 45;      // how long it keeps working without reaching the server
}
