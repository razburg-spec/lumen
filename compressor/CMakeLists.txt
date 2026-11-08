cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(Chroma VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically (or point FETCHCONTENT_SOURCE_DIR_JUCE at a local copy)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# JUCE 8 ships the AAX SDK; point CHROMA_AAX_SDK_PATH at a newer Avid SDK to override it.
set(CHROMA_AAX_SDK_PATH "" CACHE PATH "Path to an Avid AAX SDK (optional, overrides JUCE's copy)")
set(CHROMA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CHROMA_FORMATS AU)
endif()
if(CHROMA_AAX_SDK_PATH)
    juce_set_aax_sdk_path("${CHROMA_AAX_SDK_PATH}")
endif()
if(APPLE OR WIN32)
    list(APPEND CHROMA_FORMATS AAX)
endif()
message(STATUS "Chroma formats: ${CHROMA_FORMATS}")

juce_add_plugin(Chroma
    COMPANY_NAME "Raz Burg AudioCrafts"
    BUNDLE_ID "com.razburgaudiocrafts.chroma"
    PLUGIN_MANUFACTURER_CODE Rzau
    PLUGIN_CODE Chrm
    FORMATS ${CHROMA_FORMATS}
    PRODUCT_NAME "Chroma"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AAX_CATEGORY AAX_ePlugInCategory_Dynamics
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Chroma PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/ChromaEngine.cpp
    Source/gui/RbaLookAndFeel.cpp
    Source/gui/Widgets.cpp
    Source/gui/ChromaWidgets.cpp
    Source/gui/ActivationPanel.cpp
    Source/gui/Toolbar.cpp
    Source/presets/FactoryPresets.cpp
    Source/licensing/LemonSqueezyBackend.cpp
    Source/licensing/LicenseManager.cpp)

# The built-in user guide (also published on the website).
juce_add_binary_data(ChromaData SOURCES Resources/Manual.md)

target_compile_definitions(Chroma PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Chroma
    PRIVATE ChromaData juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Optional: renders the interface to PNG/JPG (cmake -DCHROMA_SCREENSHOT_TOOL=ON, target ChromaScreenshot).
option(CHROMA_SCREENSHOT_TOOL "Build the screenshot tool" OFF)
if(CHROMA_SCREENSHOT_TOOL)
    juce_add_console_app(ChromaScreenshot)
    target_sources(ChromaScreenshot PRIVATE tools/Screenshot.cpp)
    target_compile_definitions(ChromaScreenshot PRIVATE $<TARGET_PROPERTY:Chroma,COMPILE_DEFINITIONS>)
    target_include_directories(ChromaScreenshot PRIVATE $<TARGET_PROPERTY:Chroma,INCLUDE_DIRECTORIES>)
    target_link_libraries(ChromaScreenshot PRIVATE Chroma ChromaData juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography)
endif()
