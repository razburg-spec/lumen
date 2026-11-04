#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// Small, allocation-free building blocks shared by the Lumen engines.
namespace lumen
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // Topology-preserving one-pole filter (stable under fast cutoff changes).
    struct OnePole
    {
        float g = 0.0f, s = 0.0f;

        void setCutoff (float hz, float sampleRate)
        {
            hz = juce::jlimit (5.0f, 0.49f * sampleRate, hz);
            const float wc = std::tan (juce::MathConstants<float>::pi * hz / sampleRate);
            g = wc / (1.0f + wc);
        }
        void reset() { s = 0.0f; }

        float lowpass (float x)
        {
            const float v = (x - s) * g;
            const float y = v + s;
            s = y + v;
            return y;
        }
        float highpass (float x) { return x - lowpass (x); }
    };

    // Soft saturator with unity gain for small signals.
    inline float softClip (float x) { return std::tanh (x); }

    // Circular buffer with fractional (cubic) reads. Size is fixed in prepare().
    class DelayLine
    {
    public:
        void setMaxSamples (int n)
        {
            int size = 1;
            while (size < n + 4) size <<= 1;
            buffer.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0;
        }
        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }
        int capacity() const { return mask - 3; }

        void push (float x)
        {
            buffer[(size_t) writePos] = x;
            writePos = (writePos + 1) & mask;
        }

        // delay = samples behind the most recently pushed sample (>= 1 is safe).
        float readCubic (float delay) const
        {
            delay = juce::jlimit (1.0f, (float) capacity(), delay);
            const float pos = (float) writePos - delay;
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            const float ym1 = at (i - 1), y0 = at (i), y1 = at (i + 1), y2 = at (i + 2);
            const float c0 = y0;
            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * f + c2) * f + c1) * f + c0;
        }

        float readLinear (float delay) const
        {
            delay = juce::jlimit (1.0f, (float) capacity(), delay);
            const float pos = (float) writePos - delay;
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            return at (i) + f * (at (i + 1) - at (i));
        }

        float readInt (int delay) const { return at (writePos - juce::jlimit (1, capacity(), delay)); }

        // Absolute-position access for granular reads.
        int getWritePos() const { return writePos; }
        float readAbsolute (double pos) const
        {
            const int i = (int) std::floor (pos);
            const float f = (float) (pos - (double) i);
            const float ym1 = at (i - 1), y0 = at (i), y1 = at (i + 1), y2 = at (i + 2);
            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * f + c2) * f + c1) * f + y0;
        }

    private:
        float at (int i) const { return buffer[(size_t) (i & mask)]; }
        std::vector<float> buffer { 0.0f };
        int mask = 0, writePos = 0;
    };

    // Schroeder allpass used for diffusion.
    struct Allpass
    {
        DelayLine line;
        float delaySamples = 100.0f;

        float process (float x, float g, float modOffset = 0.0f)
        {
            const float d = line.readLinear (delaySamples + modOffset);
            const float v = x + g * d;
            line.push (v);
            return d - g * v;
        }
    };

    // Cheap deterministic random numbers for the audio thread.
    struct Rng
    {
        uint32_t state = 0x12345678u;
        float next01() { state = state * 1664525u + 1013904223u; return (float) (state >> 8) * (1.0f / 16777216.0f); }
        float nextBipolar() { return next01() * 2.0f - 1.0f; }
    };
}
