#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/Router.h"
#include "dsp/Coloration.h"
#include "licensing/LicenseManager.h"
#include "ParamHistory.h"

class LumenProcessor : public juce::AudioProcessor
{
public:
    LumenProcessor();

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
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "LumenState", createParameterLayout() };
    ParamHistory history { *this };   // undo / redo of the user's edits (lives here so it survives closing the window)

    juce::SharedResourcePointer<LicenseManager> licence;

private:
    void applyEvaluationGaps (juce::AudioBuffer<float>&, int numSamples);

    Router router;
    Coloration coloration;
    std::atomic<float>* colorationParam = nullptr;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> mixSmoothed, outputSmoothed, activeSmoothed;
    bool anyEngineOn() const { return delayOnParam->get() || shimOnParam->get() || revOnParam->get(); }

    juce::AudioParameterChoice* routingParam = nullptr;
    juce::AudioParameterBool* delayOnParam = nullptr;
    juce::AudioParameterBool* shimOnParam = nullptr;
    juce::AudioParameterBool* revOnParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    // Evaluation gaps: while unlicensed the effect drops out for a moment every 40 s.
    const juce::uint32 expectedGrant = LicenseManager::grantFor (LicenseManager::thisMachine());
    juce::int64 evaluationClock = 0;
    int gapPeriod = 0, gapLength = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenProcessor)
};
