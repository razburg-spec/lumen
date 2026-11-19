#pragma once
#include "LinearConvolver.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include "Filters.h"
#include "Modes.h"
#include "../Parameters.h"

// One audio-thread to GUI-thread sample queue (mono), used by the spectrum analyzer.
class AnalyzerTap
{
public:
    static constexpr int capacity = 1 << 15;

    void push (const float* data, int n)
    {
        const auto scope = fifo.write (n);   // drops what does not fit (the GUI is behind)
        if (scope.blockSize1 > 0) std::copy (data, data + scope.blockSize1, buffer.begin() + scope.startIndex1);
        if (scope.blockSize2 > 0) std::copy (data + scope.blockSize1, data + scope.blockSize1 + scope.blockSize2, buffer.begin() + scope.startIndex2);
    }

    int pull (float* dest, int maxSamples)
    {
        const auto scope = fifo.read (juce::jmin (maxSamples, fifo.getNumReady()));
        if (scope.blockSize1 > 0) std::copy (buffer.begin() + scope.startIndex1, buffer.begin() + scope.startIndex1 + scope.blockSize1, dest);
        if (scope.blockSize2 > 0) std::copy (buffer.begin() + scope.startIndex2, buffer.begin() + scope.startIndex2 + scope.blockSize2, dest + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<float, (size_t) capacity> buffer {};
};

class PrismEngine
{
public:
    struct BandParams
    {
        bool on = false;
        float freq = 1000.0f, gainDb = 0.0f, q = 1.0f;
        int type = 0;    // edge bands: 0 = cut, 1 = shelf
        int slope = 1;   // edge bands: 0..3 = 12 / 24 / 36 / 48 dB per octave
        bool operator== (const BandParams&) const = default;
    };

    struct Params
    {
        int mode = 4;
        float outputDb = 0.0f;
        float drive = 0.0f;   // 0..1
        bool linear = false;  // Linear Phase: the bands run as one FIR filter instead of the biquads
        std::array<BandParams, Band::numBands> bands {};
        bool operator== (const Params&) const = default;
    };

    struct Meters
    {
        std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    };

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void process (juce::AudioBuffer<float>&, const Params&);

    // The exact filters a band uses for these settings (the display draws its curve from this too).
    static PrismFilters::Design design (int band, const BandParams&, int mode, double sampleRate);

    // Band listen: the part of the input one band works on (a band-pass around a bell, everything
    // below the Low band's corner, everything above the High band's corner).
    static PrismFilters::Design soloDesign (int band, const BandParams&, int mode, double sampleRate);

    // Set from the editor, never automated. band -1 = off. levelDb trims the listening level.
    struct Solo
    {
        std::atomic<int> band { -1 };
        std::atomic<float> levelDb { 0.0f };
    };
    Solo solo;

    // Linear Phase. The impulse response is built off the audio thread from the same band designs,
    // with the response's magnitude and no phase shift, centred in an FIR of linearLength() taps.
    static int linearLength (double sampleRate);
    static int linearLatency (double sampleRate) { return linearLength (sampleRate) / 2 + LinearConvolver::blockSize; }
    static juce::AudioBuffer<float> buildLinearIr (const Params&, double sampleRate);
    void loadLinearIr (juce::AudioBuffer<float>&& ir);   // any thread except the audio thread

    Meters meters;
    AnalyzerTap preTap, postTap;

private:
    static constexpr int maxChannels = 2;
    static constexpr int chunk = 16;   // coefficients are re-computed at most every 16 samples while a knob moves

    struct BandState
    {
        // Smoothed continuous settings
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq { 1000.0f };
        juce::SmoothedValue<float> gain { 0.0f }, q { 1.0f };

        // Structure currently running (changes only while faded out)
        bool on = false;
        int type = 0, slope = 1, mode = 4;
        float mix = 0.0f;          // fade in/out of the band: 0 = bypassed
        float targetMix = 0.0f;

        PrismFilters::Design coeffs;
        std::array<std::array<PrismFilters::Biquad, 4>, maxChannels> state {};
        bool coeffsValid = false;
    };

    void updateBand (int b, const BandParams&, int mode);
    bool bandIsIdentity (int b) const;
    void runBand (int b, juce::AudioBuffer<float>&, int start, int n);
    void runColour (juce::AudioBuffer<float>&, int mode);
    void runSolo (juce::AudioBuffer<float>&, const Params&, int numCh, int numSamples);

    // Band listen state
    juce::AudioBuffer<float> dry;
    std::array<std::array<PrismFilters::Biquad, 4>, maxChannels> soloState {};
    PrismFilters::Design soloCoeffs;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> soloFreq { 1000.0f };
    juce::SmoothedValue<float> soloLevel { 1.0f };
    float soloMix = 0.0f;
    int soloRunning = -1;

    struct ColourState
    {
        float low = 0.0f, top = 0.0f, dcIn = 0.0f, dcOut = 0.0f;
    };
    std::array<ColourState, maxChannels> colour {};
    juce::SmoothedValue<float> driveAmount { 0.0f };
    float lowCoef = 0.0f, dcCoef = 0.0f;

    double sr = 48000.0;
    int channels = 2;
    float mixStep = 0.0f;
    std::array<BandState, Band::numBands> bands;
    juce::SmoothedValue<float> outGain { 1.0f };
    juce::HeapBlock<float> mono;
    int monoSize = 0;
    bool first = true;
    bool wasLinear = false;
    LinearConvolver convolver;
};
