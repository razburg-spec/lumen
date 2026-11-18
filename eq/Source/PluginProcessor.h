#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/PrismEngine.h"
#include "ParamHistory.h"

class PrismProcessor : public juce::AudioProcessor,
                       private juce::AsyncUpdater
{
public:
    PrismProcessor();
    ~PrismProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool isLinearPhase() const { return linear->load() > 0.5f; }

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

    // Band listen (from the display; not a parameter, so it is never automated or saved).
    void setSolo (int band, float levelDb = 0.0f) { engine.solo.levelDb.store (levelDb); engine.solo.band.store (band); }
    int getSoloBand() const { return engine.solo.band.load(); }
    double getEngineSampleRate() const { return sampleRateForDisplay.load(); }

private:
    void handleAsyncUpdate() override;   // reports the latency to the host when Linear Phase changes
    void updateLatency();

    // Builds the Linear Phase filter in the background whenever a setting changes.
    class IrBuilder : public juce::Thread
    {
    public:
        explicit IrBuilder (PrismProcessor& p) : juce::Thread ("Prism linear phase"), owner (p) {}
        void run() override;
        void invalidate() { dirty.store (true); notify(); }
    private:
        PrismProcessor& owner;
        std::atomic<bool> dirty { true };
    };

    PrismEngine engine;
    IrBuilder irBuilder { *this };
    std::atomic<bool> reportedLinear { false };
    std::atomic<double> sampleRateForDisplay { 48000.0 };

    struct BandRaw { std::atomic<float> *on, *freq, *gain, *q, *type = nullptr, *slope = nullptr; };
    std::array<BandRaw, Band::numBands> bandRaw {};
    std::atomic<float> *mode = nullptr, *output = nullptr, *drive = nullptr, *linear = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismProcessor)
};
