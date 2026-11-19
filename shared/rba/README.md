# Shared Raz Burg AudioCrafts code

Code used by more than one plugin in this repo (Lumen, Chroma, Prism).

## Update notice (`UpdateChecker`, `UpdateBanner`)

When the editor opens, a background thread reads
`https://audiocrafts.razburg.com/versions.json` once per host session and compares
`products.<slug>.latest` with the running version. If it is newer, a small pill shows
"<Name> <version> is available" with a Download button (opens `download_url` in the
browser) and a × that hides it until the next release (remembered in
`~/Library/Application Support/Raz Burg AudioCrafts/Updates.settings`, or
`%APPDATA%\Raz Burg AudioCrafts\Updates.settings`). Offline or any error: nothing shows.
The audio thread is never involved.

Add to a plugin:

```cmake
target_sources(MyPlugin PRIVATE ../shared/rba/UpdateChecker.cpp ../shared/rba/UpdateBanner.cpp)
target_include_directories(MyPlugin PRIVATE ../shared)
```

```cpp
#include <rba/UpdateBanner.h>
rba::UpdateBanner update { "Prism", "prism", JucePlugin_VersionString, { /* colours */ } };
addChildComponent (update);
update.onVisibilityChanged = [this] { resized(); };
// in resized(): update.setBounds (area.withSizeKeepingCentre (update.getIdealWidth(), 24));
```

Releasing: bump `products.<slug>.latest` in the site's `versions.json` when the new
installers are live, otherwise users are sent to a page that still has the old build.

## Saved sessions stay compatible (`StateIO.h`)

Rule: a session saved with any older version must open exactly the same in every newer
version, for every plugin. Processors save and load through `rba::state::save/load`:

```cpp
void getStateInformation (juce::MemoryBlock& d) override { rba::state::save (apvts, stateVersion, d); }
void setStateInformation (const void* data, int size) override
{ rba::state::load (apvts, data, size, stateVersion, stateMigrations()); }
```

- Parameter IDs are permanent. Never rename, reuse or change the meaning/range of one in place.
- When a release must change a parameter (ID, range, meaning) or retire one: bump
  `stateVersion` in PluginProcessor.h and add a step to `stateMigrations()`, e.g.
  `{ 2, [] (juce::ValueTree& s) { using namespace rba::state;
         setParam (s, "newAmount", juce::jlimit (0.0f, 100.0f, getParam (s, "oldAmount", 30.0f) * 1.3f + 10.0f));
         removeParam (s, "oldAmount"); } }`
  Steps see the old state with all its old params (values in real units), before anything is dropped.
- Params missing from an old state get their defaults; unknown params are ignored.
- Every release adds a golden state: build with `-D<PLUGIN>_STATE_TEST=ON`, run
  `<Plugin>StateTest write test/state-fixtures/<plugin>-<version>.xml`, commit it. If a
  migration changes values, add `<plugin>-<version>.expect.xml` (`<EXPECT><PARAM id=".." value=".."/></EXPECT>`)
  with the translated values. `<Plugin>StateTest` must pass before a release (CI runs it).

## Activation (`Activation`, `ActivationPanel`)

Free activation through the user's audiocrafts.razburg.com account (no licence keys,
no iLok). Max 2 computers per user per plugin. Flow: the plugin opens
`/account/#activate=<slug>&m=<machine hash>&n=<computer name>&r=<nonce>`, the user
approves it while signed in, the plugin polls `activation_poll` and stores a token
(encrypted, machine-bound) under `Raz Burg AudioCrafts/<Plugin>/activation.bin`. It
re-checks weekly (`activation_check`); offline is fine for 60 days. Until activated,
`grant()` differs from `grantFor (thisMachine())` and the processor drops the effect out
briefly every 40 s. The database side lives in the site repo, `supabase/schema.sql`.

Add to a plugin:

```cmake
target_sources(MyPlugin PRIVATE ../shared/rba/Activation.cpp ../shared/rba/ActivationPanel.cpp)
target_compile_definitions(MyPlugin PUBLIC RBA_PRODUCT_SLUG=\"prism\")   # also add the product
# row on the site with needs_activation = true
```

```cpp
juce::SharedResourcePointer<rba::Activation> licence;            // in the processor
rba::ActivationBadge badge { *p.licence, style };                // header badge, onClick opens:
std::make_unique<rba::ActivationPanel> (*p.licence, style);      // full-window overlay
```
