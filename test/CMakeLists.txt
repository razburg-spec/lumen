cmake_minimum_required(VERSION 3.22)
project(LumenRenderTest VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)
add_subdirectory(${JUCE_DIR} juce)
juce_add_console_app(RenderTest)
target_sources(RenderTest PRIVATE RenderTest.cpp ../Source/Parameters.cpp ../Source/dsp/DelayEngine.cpp
  ../Source/dsp/ShimmerEngine.cpp ../Source/dsp/ReverbEngine.cpp ../Source/dsp/Router.cpp)
target_compile_definitions(RenderTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(RenderTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
