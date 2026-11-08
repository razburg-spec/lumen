#pragma once
#include <cmath>

// The six compressor characters. Each one is a complete voicing: how the detector listens,
// how gain reduction bends in over the threshold, how it lets go, and what the saturation
// stage sounds like. None of them colours the signal by itself: with the ratio at 1:1 and
// Saturation at 0 every character passes the audio through untouched.
struct Character
{
    enum class Release { plain, opto, dual };
    enum class Shape { tube, fet, thick, grit, clean, console };

    const char* name;

    // Detector and gain computer
    float thresholdDb;   // fixed threshold: the Input knob drives the signal into it
    float kneeDb;        // width of the soft knee (0 = hard knee)
    float rmsMs;         // averaging window of the level detector (0 = peak)
    float minAttackMs;   // the attack never gets faster than this
    float releaseScale;
    Release release;
    bool overCompress;   // past 10:1 the curve bends down: louder in comes out quieter

    // Saturation voicing
    Shape shape;
    float drive;         // drive at Saturation 100%
    float bias;          // asymmetry: more bias, more even harmonics
    float toneLowHz;     // the harmonics are kept between these two frequencies
    float toneHighHz;
    float grDrive;       // extra drive per dB of gain reduction (distortion that grows as it squeezes)
};

inline const Character& characterAt (int index)
{
    static const Character table[] = {
        //  name       thr    knee  rms   minAtk relSc  release                     overC  shape                         drive bias   lowHz  highHz   grDrive
        { "Vintage", -18.0f, 12.0f, 12.0f, 6.0f, 1.0f, Character::Release::opto,  false, Character::Shape::tube,    5.0f, 0.30f,  30.0f,  9000.0f, 0.00f },
        { "Retro",   -18.0f,  2.5f,  0.0f, 0.05f, 1.0f, Character::Release::plain, false, Character::Shape::fet,     4.0f, 0.12f,  40.0f, 16000.0f, 0.06f },
        { "Chubby",  -20.0f, 20.0f, 25.0f, 0.3f, 1.3f, Character::Release::plain, false, Character::Shape::thick,   6.0f, 0.45f,  25.0f,  7000.0f, 0.00f },
        { "Indie",   -18.0f,  0.0f,  0.0f, 0.05f, 0.8f, Character::Release::plain, true,  Character::Shape::grit,    7.0f, 0.05f, 150.0f, 11000.0f, 0.03f },
        { "Modern",  -18.0f,  6.0f,  0.0f, 0.05f, 1.0f, Character::Release::plain, false, Character::Shape::clean,   2.5f, 0.00f,  20.0f, 20000.0f, 0.00f },
        { "Bus",     -16.0f,  3.0f,  4.0f, 0.1f, 1.0f, Character::Release::dual,  false, Character::Shape::console, 3.0f, 0.06f,  20.0f, 20000.0f, 0.00f },
    };
    return table[index < 0 ? 0 : (index > 5 ? 5 : index)];
}

// Waveshapers, all with unity slope at zero so quiet material passes at the same level.
inline float shapeSample (float x, Character::Shape shape, float bias)
{
    switch (shape)
    {
        case Character::Shape::clean:
            return 0.5f * x + 0.5f * std::tanh (x);

        case Character::Shape::grit:
        {
            // Sharper knee than tanh: more upper harmonics, a gritty edge.
            const auto f = [] (float u) { return u / std::pow (1.0f + u * u * u * u, 0.25f); };
            const float slope = std::pow (1.0f + bias * bias * bias * bias, -1.25f);
            return (f (x + bias) - f (bias)) / slope;
        }

        case Character::Shape::tube:
        case Character::Shape::fet:
        case Character::Shape::thick:
        case Character::Shape::console:
        default:
        {
            const float t = std::tanh (bias);
            return (std::tanh (x + bias) - t) / (1.0f - t * t);
        }
    }
}
