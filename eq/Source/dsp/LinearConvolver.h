#pragma once
#include <juce_dsp/juce_dsp.h>
#include <complex>

// FIR engine for Linear Phase: uniformly partitioned FFT convolution (blocks of 512) with a
// lock-free hand-over of new impulse responses. When the curve changes, the old and new filters
// both run for one block and are crossfaded, so moving a band never clicks or stutters.
// Fixed latency: blockSize samples (on top of the FIR's own centre delay).
class LinearConvolver
{
public:
    static constexpr int blockSize = 512;

    // Allocates everything for impulse responses of up to irLength samples. Not on the audio thread.
    void prepare (int irLength, int numChannels);

    // Any thread except the audio thread. The response must be irLength samples (others are ignored).
    void setImpulse (const float* ir, int length);

    void reset();                                            // audio thread (no allocation)
    void process (float* const* channels, int numChannels, int numSamples);   // audio thread

private:
    using Complex = std::complex<float>;
    static constexpr int fftOrder = 10, fftSize = 1 << fftOrder, bins = fftSize / 2 + 1;
    static constexpr int maxChannels = 2;

    struct Slot { std::vector<Complex> spectra; bool valid = false; };   // partitions x bins

    void processFrame();
    void convolve (const Slot&, int channel, float* out);

    juce::dsp::FFT fft { fftOrder }, loaderFft { fftOrder };
    int partitions = 0, length = 0, channels = 2;

    // Four slots: the audio thread owns `front` and `retired` (the one it faded out of last time, so
    // a slot is never handed back while it is still being read), the loader owns `back`, and
    // `middle` passes a new response across.
    std::array<Slot, 4> slots;
    int front = 0, retired = 1, back = 3;
    std::atomic<int> middle { 2 };          // bit 4 = holds a new response
    static constexpr int freshBit = 4;
    juce::CriticalSection loaderLock;
    std::vector<float> loaderWork;

    struct Channel
    {
        std::vector<Complex> history;       // frequency-domain delay line: partitions x bins
        std::vector<float> input;           // last fftSize input samples
        std::vector<float> output;          // finished block being played out
    };
    std::array<Channel, maxChannels> chans;
    std::vector<float> work, outOld, outNew;
    std::vector<Complex> acc;
    int historyPos = 0, fifoPos = 0;
    bool haveFront = false;
};
