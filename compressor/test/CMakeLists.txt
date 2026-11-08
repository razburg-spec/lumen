cmake_minimum_required(VERSION 3.22)
project(ChromaRenderTest VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)
add_subdirectory(${JUCE_DIR} juce)
juce_add_console_app(ChromaRenderTest)
target_sources(ChromaRenderTest PRIVATE RenderTest.cpp ../Source/Parameters.cpp ../Source/dsp/ChromaEngine.cpp)
target_compile_definitions(ChromaRenderTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChromaRenderTest PRIVATE juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
