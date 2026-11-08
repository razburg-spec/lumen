#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/ChromaEngine.h"
#include "licensing/LicenseManager.h"
#include "ParamHistory.h"

class ChromaProcessor : public juce::AudioProcessor
{
public:
    ChromaProcessor();

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

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "ChromaState", createParameterLayout() };
    ParamHistory history { *this };   // undo / redo of the user's edits (lives here so it survives closing the window)

    juce::SharedResourcePointer<LicenseManager> licence;

    ChromaEngine::Meters& meters() { return engine.meters; }
    bool hasSidechainInput() const;

private:
    void applyEvaluationGaps (juce::AudioBuffer<float>& main, int numSamples);

    ChromaEngine engine;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }
    std::atomic<float> *character, *input, *output, *attack, *release, *ratio, *saturation,
                       *softClip, *parallel, *keyOn, *keySource, *keyFreq, *keyListen;

    // Evaluation gaps: while unlicensed the effect drops out for a moment every 40 s.
    const juce::uint32 expectedGrant = LicenseManager::grantFor (LicenseManager::thisMachine());
    juce::int64 evaluationClock = 0;
    int gapPeriod = 0, gapLength = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaProcessor)
};
