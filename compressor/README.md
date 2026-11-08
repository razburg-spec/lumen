# Chroma

Multi-character compressor by Raz Burg AudioCrafts. JUCE 8, C++20, CMake. Lives next to Lumen in this
repository but builds on its own (this folder has its own `CMakeLists.txt`).

## Features

- Six characters on one knob: Vintage, Retro, Chubby, Indie, Modern, Bus. Each sets the detector, knee,
  release behaviour and saturation voice (`Source/dsp/Characters.h`).
- Input (drive into a fixed threshold), Output, Attack, Release, Ratio, Saturation, Parallel, Soft Clip.
- Key: External (the host's native sidechain input, mono or stereo) or Freq Key (50 Hz to 16 kHz band), with Listen.
- Opens with no processing (bit-identical output), zero latency, Undo/Redo (100 steps), Reset,
  29 factory presets in 6 categories, built-in user guide (`Resources/Manual.md`), Lemon Squeezy licensing.

## Build (macOS)

```bash
cd compressor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

Outputs in `compressor/build/Chroma_artefacts/Release/`: VST3, AU, AAX and Standalone.

## Build (Windows)

The **Build Chroma Windows** GitHub Action builds the VST3 and Standalone, runs the render test and
uploads `Chroma-Windows` as a downloadable artifact.

## Tests and screenshots

```bash
cmake -S test -B test/build -DJUCE_DIR=$(pwd)/build/_deps/juce-src && cmake --build test/build
./test/build/ChromaRenderTest_artefacts/Release/ChromaRenderTest      # DSP checks, exits non-zero on failure

cmake -B build -DCHROMA_SCREENSHOT_TOOL=ON && cmake --build build --target ChromaScreenshot
./build/ChromaScreenshot_artefacts/Release/ChromaScreenshot out 0     # renders the UI (character 0-5)
```

## Layout

```
Source/
  PluginProcessor.*   buses (main + sidechain), parameters to the engine, state, evaluation gaps
  PluginEditor.*      window and layout
  Parameters.*        every parameter
  dsp/                ChromaEngine (detector, gain computer, saturation, soft clip, parallel), Characters
  gui/                look and feel, knobs, character dial, meters, toolbar, activation panel (shared RBA style)
  licensing/          Lemon Squeezy licence keys, trial, offline grace (separate from Lumen's)
  presets/            factory presets
test/                 offline render test
tools/                screenshot tool
```
