#pragma once
#include <cmath>
#include <algorithm>

// The six EQ modes. The controls are the same in every mode; a mode changes how the EQ responds
// and sounds:
//  - how wide the bells are, and how their width follows the gain (constant or proportional Q),
//  - the shape of the shelves (gentle, or with the overshoot and dip of classic circuits),
//  - the knee of the cut filters,
//  - and the analog stage that the Drive control pushes (harmonics, transformer weight, top-end softness).
// With every band at 0 dB, both cuts off and Drive at 0, every mode passes audio untouched.
struct ModeVoicing
{
    enum class Shape { tube, console, iron, grit, clean, master };

    const char* name;

    // Bells
    float bandwidth;      // Q multiplier (below 1 = broader, musical bells; above 1 = tighter)
    float proportionalQ;  // 0 = constant Q; higher = wide at small gains, narrower as gain grows
    float cutQ;           // extra Q multiplier for cuts (cuts narrower than boosts when above 1)

    // Shelves
    float lowShelfQ;      // shelf Q multipliers: above 1 adds overshoot (bump) and undershoot (dip)
    float highShelfQ;
    float lowShelfDip;    // a boosting low shelf also dips the low mids above it, dB per dB of boost
    float lowDipRatio;    // where that dip sits, as a multiple of the shelf frequency

    // Cut filters
    float cutResonance;   // corner resonance multiplier (1 = Butterworth)

    // Analog stage (driven by the Drive control)
    Shape shape;
    float drive;          // drive at Drive 100%
    float asymmetry;      // even-harmonic bias
    float ironLow;        // transformer weight: extra saturation of the lows
    float softTop;        // top-end softening at full drive (0..1)
};

inline const ModeVoicing& modeVoicing (int index)
{
    using S = ModeVoicing::Shape;
    static const ModeVoicing table[] = {
        // Vintage: passive tube program EQ blended with a broad 70s solid-state parametric.
        // Very broad bells, gentle shelves, the low-shelf boost-and-dip trick, tube harmonics.
        { "Vintage", 0.60f, 0.20f, 1.25f,   0.80f, 0.90f, 0.30f, 3.2f,   1.00f,   S::tube,    4.0f, 0.30f, 0.50f, 0.35f },

        // Retro: 70s American console with proportional bandwidth plus British inductor-era console.
        // Bells start wide and tighten as you push, slightly resonant corners, silky top.
        { "Retro",   0.85f, 0.90f, 1.00f,   0.95f, 1.05f, 0.00f, 3.0f,   1.15f,   S::console, 3.5f, 0.12f, 0.25f, 0.25f },

        // Chubby: British class-A inductor EQ with transformers, blended with a 60s studio desk.
        // Broad bells, resonant low shelf (bump then dip), soft cut knees, heavy iron.
        { "Chubby",  0.70f, 0.30f, 1.10f,   1.45f, 1.15f, 0.00f, 3.0f,   0.85f,   S::iron,    5.0f, 0.35f, 1.00f, 0.30f },

        // Indie: British large-format console EQ blended with its 80s rival.
        // Tighter bells, mild proportional Q, shelves with overshoot and undershoot, gritty edge.
        { "Indie",   1.15f, 0.35f, 1.00f,   1.35f, 1.35f, 0.00f, 3.0f,   1.05f,   S::grit,    5.0f, 0.05f, 0.20f, 0.00f },

        // Modern: clean discrete class-A and transformerless designs. Exact, open, airy.
        { "Modern",  1.00f, 0.00f, 1.00f,   1.00f, 0.95f, 0.00f, 3.0f,   1.00f,   S::clean,   2.0f, 0.08f, 0.00f, 0.00f },

        // Bus: mastering. Passive tube broadness, precise constant-Q parametrics, smooth air,
        // shelves with no overshoot, subtle 2nd/3rd harmonic color.
        { "Bus",     0.90f, 0.15f, 1.00f,   0.85f, 0.80f, 0.15f, 3.5f,   0.95f,   S::master,  2.5f, 0.18f, 0.30f, 0.15f },
    };
    return table[std::clamp (index, 0, 5)];
}

// Q actually used by a bell, after the mode's bandwidth rules.
// Vintage: highs behave like the classic passive tube program EQ. Its high "shelf" boost is really
// a broad resonant peak that keeps the very top lifted, with a bandwidth that does not change with
// the amount of boost; its attenuation is a gentle, wide first-order-like shelf.
inline bool passiveHighs (int mode) { return mode == 0; }

// Peak bandwidth of that high boost for the band's Q (0.71 default = the "broad" setting).
inline float passiveHighBoostQ (float q) { return std::clamp (0.55f * q / 0.7071f, 0.3f, 4.0f); }

inline float effectiveBellQ (const ModeVoicing& v, float q, float gainDb)
{
    const float amount = std::min (1.0f, std::abs (gainDb) / 18.0f);
    float scaled = q * v.bandwidth * std::pow (2.0f, v.proportionalQ * (2.0f * amount - 0.5f));
    if (gainDb < 0.0f)
        scaled *= v.cutQ;
    return scaled;
}

// Waveshapers of the analog stage, all with unity slope at zero so quiet material keeps its level.
inline float shapeSample (float x, ModeVoicing::Shape shape, float bias)
{
    using S = ModeVoicing::Shape;
    switch (shape)
    {
        case S::clean:
            return 0.6f * x + 0.4f * std::tanh (x);

        case S::grit:
        {
            // Sharper knee than tanh: more upper harmonics, a gritty edge.
            const auto f = [] (float u) { return u / std::pow (1.0f + u * u * u * u, 0.25f); };
            const float slope = std::pow (1.0f + bias * bias * bias * bias, -1.25f);
            return (f (x + bias) - f (bias)) / slope;
        }

        case S::tube:
        case S::console:
        case S::iron:
        case S::master:
        default:
        {
            const float t = std::tanh (bias);
            return (std::tanh (x + bias) - t) / (1.0f - t * t);
        }
    }
}
