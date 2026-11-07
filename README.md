# Lumen

Reverb, delay and granular shimmer plugin by Raz Burg AudioCrafts. JUCE 8, C++20, CMake.

## Features

- Delay, Shimmer and Reverb engines in Serial, Parallel or Reverb > Delay routing, with Cross-Feed.
- Reverb modes: Hall, Plate, Room, Chamber, Bloom, Cloud, 60s Chamber, 60s Mirror, 90s Hall, 90s Concert; era Color for the first six.
- Coloration (Dark to Shine) on the output, alias-free (2x oversampled harmonic path).
- Opens fully dry, Undo/Redo (100 steps), Reset, 47 factory presets in 9 categories, built-in user guide (`Resources/Manual.md`).

## Build (macOS)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

Outputs in `build/Lumen_artefacts/Release/`: VST3, AU, AAX and Standalone.

## Build (Windows)

Pushing to `main` runs the **Build Windows** GitHub Action, which builds the VST3 and Standalone,
runs the offline render test and uploads `Lumen-Windows` as a downloadable artifact.
Install by copying `Lumen.vst3` to `C:\Program Files\Common Files\VST3`.

Locally (Visual Studio 2022 with the C++ workload):

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Lumen_VST3
```

## Layout

```
Source/
  PluginProcessor.*   host connection, dry/wet, state, evaluation gaps
  PluginEditor.*      window and layout
  Parameters.*        every parameter
  gui/                look and feel, widgets, display, sections, activation panel
  licensing/          Lemon Squeezy licence keys, trial, offline grace
  dsp/                DelayEngine, ShimmerEngine, ReverbEngine, ChamberReverb, RingHall, Coloration, Router
test/                 offline render test (all engines and modes); `RenderTest measure` and analyse_ir.py measure decay, density, ringing and stereo
```
