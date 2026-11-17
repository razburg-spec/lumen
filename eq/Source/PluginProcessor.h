#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/PrismEngine.h"
#include "ParamHistory.h"

class PrismProcessor : public juce::AudioProcessor
{
public:
    PrismProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Current settings of every band, read from the parameters (audio and GUI threads).
    PrismEngine::Params currentParams() const;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PrismState", createParameterLayout() };
    ParamHistory history { *this };   // undo / redo of the user's edits (lives here so it survives closing the window)

    PrismEngine::Meters& meters() { return engine.meters; }
    AnalyzerTap& preTap() { return engine.preTap; }
    AnalyzerTap& postTap() { return engine.postTap; }
    double getEngineSampleRate() const { return sampleRateForDisplay.load(); }

private:
    PrismEngine engine;
    std::atomic<double> sampleRateForDisplay { 48000.0 };

    struct BandRaw { std::atomic<float> *on, *freq, *gain, *q, *type = nullptr, *slope = nullptr; };
    std::array<BandRaw, Band::numBands> bandRaw {};
    std::atomic<float> *mode = nullptr, *output = nullptr, *drive = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismProcessor)
};
