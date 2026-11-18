#include "LinearConvolver.h"

void LinearConvolver::prepare (int irLength, int numChannels)
{
    const juce::ScopedLock sl (loaderLock);
    length = irLength;
    channels = juce::jlimit (1, maxChannels, numChannels);
    partitions = (irLength + blockSize - 1) / blockSize;
    for (auto& s : slots)
    {
        s.spectra.assign ((size_t) (partitions * bins), Complex {});
        s.valid = false;
    }
    front = 0; retired = 1; back = 3;
    middle.store (2);
    for (auto& c : chans)
    {
        c.history.assign ((size_t) (partitions * bins), Complex {});
        c.input.assign ((size_t) fftSize, 0.0f);
        c.output.assign ((size_t) blockSize, 0.0f);
    }
    work.assign ((size_t) fftSize * 2, 0.0f);
    outOld.assign ((size_t) blockSize, 0.0f);
    outNew.assign ((size_t) blockSize, 0.0f);
    acc.assign ((size_t) bins, Complex {});
    loaderWork.assign ((size_t) fftSize * 2, 0.0f);
    historyPos = fifoPos = 0;
    haveFront = false;
}

void LinearConvolver::setImpulse (const float* ir, int irLength)
{
    const juce::ScopedLock sl (loaderLock);
    if (irLength != length || partitions == 0)
        return;

    auto& slot = slots[(size_t) back];
    for (int p = 0; p < partitions; ++p)
    {
        std::fill (loaderWork.begin(), loaderWork.end(), 0.0f);
        const int n = juce::jmin (blockSize, irLength - p * blockSize);
        std::copy (ir + p * blockSize, ir + p * blockSize + n, loaderWork.begin());
        loaderFft.performRealOnlyForwardTransform (loaderWork.data(), true);
        auto* dst = slot.spectra.data() + (size_t) (p * bins);
        for (int k = 0; k < bins; ++k)
            dst[k] = { loaderWork[(size_t) (2 * k)], loaderWork[(size_t) (2 * k + 1)] };
    }
    slot.valid = true;
    back = middle.exchange (back | freshBit) & 3;
}

void LinearConvolver::reset()
{
    for (int c = 0; c < channels; ++c)
    {
        auto& ch = chans[(size_t) c];
        std::fill (ch.history.begin(), ch.history.end(), Complex {});
        std::fill (ch.input.begin(), ch.input.end(), 0.0f);
        std::fill (ch.output.begin(), ch.output.end(), 0.0f);
    }
    historyPos = fifoPos = 0;
}

void LinearConvolver::convolve (const Slot& slot, int channel, float* out)
{
    auto& ch = chans[(size_t) channel];
    std::fill (acc.begin(), acc.end(), Complex {});
    for (int p = 0; p < partitions; ++p)
    {
        const int h = (historyPos - p + partitions) % partitions;
        const auto* x = ch.history.data() + (size_t) (h * bins);
        const auto* f = slot.spectra.data() + (size_t) (p * bins);
        for (int k = 0; k < bins; ++k)
            acc[(size_t) k] += x[k] * f[k];
    }
    std::fill (work.begin(), work.end(), 0.0f);
    for (int k = 0; k < bins; ++k)
    {
        work[(size_t) (2 * k)] = acc[(size_t) k].real();
        work[(size_t) (2 * k + 1)] = acc[(size_t) k].imag();
    }
    fft.performRealOnlyInverseTransform (work.data());
    std::copy (work.begin() + blockSize, work.begin() + fftSize, out);   // overlap-save: the valid half
}

void LinearConvolver::processFrame()
{
    // A new response waiting? Take it, and crossfade into it over this block.
    int fadeFrom = -1;
    if ((middle.load() & freshBit) != 0)
    {
        const int old = front;
        front = middle.exchange (retired) & 3;
        retired = old;
        if (haveFront)
            fadeFrom = old;
        haveFront = slots[(size_t) front].valid;
    }

    historyPos = (historyPos + 1) % partitions;
    for (int c = 0; c < channels; ++c)
    {
        auto& ch = chans[(size_t) c];
        std::copy (ch.input.begin(), ch.input.end(), work.begin());
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        fft.performRealOnlyForwardTransform (work.data(), true);
        auto* x = ch.history.data() + (size_t) (historyPos * bins);
        for (int k = 0; k < bins; ++k)
            x[k] = { work[(size_t) (2 * k)], work[(size_t) (2 * k + 1)] };

        if (! haveFront)
        {
            std::fill (ch.output.begin(), ch.output.end(), 0.0f);
        }
        else if (fadeFrom < 0)
        {
            convolve (slots[(size_t) front], c, ch.output.data());
        }
        else
        {
            convolve (slots[(size_t) fadeFrom], c, outOld.data());
            convolve (slots[(size_t) front], c, outNew.data());
            for (int i = 0; i < blockSize; ++i)
            {
                const float t = (float) (i + 1) / (float) blockSize;
                const float g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);   // smooth S-curve
                ch.output[(size_t) i] = outOld[(size_t) i] + g * (outNew[(size_t) i] - outOld[(size_t) i]);
            }
        }

        // Slide the input window: the current block becomes the previous one.
        std::copy (ch.input.begin() + blockSize, ch.input.end(), ch.input.begin());
    }
}

void LinearConvolver::process (float* const* data, int numChannels, int numSamples)
{
    const int nc = juce::jmin (numChannels, channels);
    if (partitions == 0)
        return;
    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < nc; ++c)
        {
            auto& ch = chans[(size_t) c];
            ch.input[(size_t) (blockSize + fifoPos)] = data[c][i];
            data[c][i] = ch.output[(size_t) fifoPos];
        }
        if (++fifoPos == blockSize)
        {
            fifoPos = 0;
            processFrame();
        }
    }
}
