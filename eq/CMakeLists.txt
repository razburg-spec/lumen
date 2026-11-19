cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(Prism VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically (or point FETCHCONTENT_SOURCE_DIR_JUCE at a local copy)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# JUCE 8 ships the AAX SDK; point PRISM_AAX_SDK_PATH at a newer Avid SDK to override it.
set(PRISM_AAX_SDK_PATH "" CACHE PATH "Path to an Avid AAX SDK (optional, overrides JUCE's copy)")
set(PRISM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PRISM_FORMATS AU)
endif()
if(PRISM_AAX_SDK_PATH)
    juce_set_aax_sdk_path("${PRISM_AAX_SDK_PATH}")
endif()
if(APPLE OR WIN32)
    list(APPEND PRISM_FORMATS AAX)
endif()
message(STATUS "Prism formats: ${PRISM_FORMATS}")

# CFBundleVersion: give every release build a unique value (e.g. -DRBA_BUILD_VERSION=1.0.0.202611191030)
# so hosts that cache plug-ins by version rescan after an update.
set(RBA_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "CFBundleVersion of the built bundles")

juce_add_plugin(Prism
    BUILD_VERSION "${RBA_BUILD_VERSION}"
    COMPANY_NAME "Raz Burg AudioCrafts"
    BUNDLE_ID "com.razburgaudiocrafts.prism"
    PLUGIN_MANUFACTURER_CODE Rzau
    PLUGIN_CODE Prsm
    FORMATS ${PRISM_FORMATS}
    PRODUCT_NAME "Prism"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AAX_CATEGORY AAX_ePlugInCategory_EQ
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Prism PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/PrismEngine.cpp
    Source/dsp/LinearConvolver.cpp
    Source/gui/RbaLookAndFeel.cpp
    Source/gui/Widgets.cpp
    Source/gui/PrismWidgets.cpp
    Source/gui/Toolbar.cpp
    Source/presets/FactoryPresets.cpp
    ../shared/rba/UpdateChecker.cpp
    ../shared/rba/UpdateBanner.cpp
    ../shared/rba/Activation.cpp
    ../shared/rba/ActivationPanel.cpp)
target_include_directories(Prism PRIVATE ../shared)

# The built-in user guide (also published on the website).
juce_add_binary_data(PrismData SOURCES Resources/Manual.md)

target_compile_definitions(Prism PUBLIC
    RBA_PRODUCT_SLUG=\"prism\"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Prism
    PRIVATE PrismData juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Optional: renders the interface to PNG/JPG (cmake -DPRISM_SCREENSHOT_TOOL=ON, target PrismScreenshot).
option(PRISM_SCREENSHOT_TOOL "Build the screenshot tool" OFF)
if(PRISM_SCREENSHOT_TOOL)
    juce_add_console_app(PrismScreenshot)
    target_sources(PrismScreenshot PRIVATE tools/Screenshot.cpp)
    target_compile_definitions(PrismScreenshot PRIVATE $<TARGET_PROPERTY:Prism,COMPILE_DEFINITIONS>)
    target_include_directories(PrismScreenshot PRIVATE $<TARGET_PROPERTY:Prism,INCLUDE_DIRECTORIES>)
    target_link_libraries(PrismScreenshot PRIVATE Prism PrismData juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography)
endif()

# Backward-compatibility test: every golden state in test/state-fixtures must load
# correctly in this build (cmake -DPRISM_STATE_TEST=ON, target PrismStateTest).
option(PRISM_STATE_TEST "Build the saved-state compatibility test" OFF)
if(PRISM_STATE_TEST)
    juce_add_console_app(PrismStateTest)
    target_sources(PrismStateTest PRIVATE ../shared/rba/StateCompatTest.cpp)
    target_compile_definitions(PrismStateTest PRIVATE $<TARGET_PROPERTY:Prism,COMPILE_DEFINITIONS>
        RBA_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/state-fixtures")
    target_include_directories(PrismStateTest PRIVATE $<TARGET_PROPERTY:Prism,INCLUDE_DIRECTORIES>)
    target_link_libraries(PrismStateTest PRIVATE Prism PrismData)
endif()
