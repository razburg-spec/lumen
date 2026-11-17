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
