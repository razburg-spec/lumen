#pragma once
#include "DspUtils.h"
#include "ReverbParams.h"
#include "FdnReverb.h"
#include "ChamberReverb.h"
#include "RingHall.h"

// One complete reverb: picks the algorithm for the Mode and Era, and crossfades when that changes.
//   60s Chamber -> ChamberReverb (tiled echo chamber), 60s Room -> ChamberReverb (mirror room)
//   90s Hall    -> RingHall (random hall), 90s Cloud -> RingHall (concert voice)
//   everything else -> FdnReverb with the era's voicing.
class ReverbCore
{
public:
    void prepare (const juce::dsp::ProcessSpec&);
    void reset (const ReverbParams&);
    void process (juce::AudioBuffer<float>&, const ReverbParams&);

private:
    enum class Kind { Fdn, Chamber, Ring };
    static Kind kindFor (const ReverbParams&);
    static int variantFor (const ReverbParams&);
    static float levelFor (const ReverbParams&);
    void processKind (Kind, juce::AudioBuffer<float>&, const ReverbParams&, int variant);
    void resetKind (Kind, const ReverbParams&);
    void processAlgorithms (juce::AudioBuffer<float>&, const ReverbParams&);
    void finish (juce::AudioBuffer<float>&, const ReverbParams&);   // era tone for the dedicated engines + level match

    FdnReverb fdn;
    ChamberReverb chamber;
    RingHall ring;
    Kind activeKind = Kind::Fdn, fadingKind = Kind::Fdn;
    int activeVariant = 0, fadingVariant = 0;
    int fadeLeft = 0, fadeLength = 1;
    juce::AudioBuffer<float> fadeBuffer;
    float fs = 44100.0f;
    lumen::OnePole postLp1[2], postLp2[2], postHp[2];
    juce::SmoothedValue<float> levelGain;
};

// The reverb engine. Size, Mode, Era and Pre-Delay never move a delay line while sound is in it
// (that bends the pitch, like a tape changing speed). Instead a change starts a fresh reverb with the
// new settings, which takes over the input, while the previous one stops receiving input and rings out
// naturally at its old settings. The two tails overlap, so there is no pitch change and no level dip.
// A small pool of reverbs covers a knob that keeps moving.
// Contract shared by all engines: process() replaces the buffer contents with this engine's wet signal.
class ReverbEngine
{
public:
    using Params = ReverbParams;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParams (const Params& p) { requested = p; }
    void process (juce::AudioBuffer<float>& buffer);

private:
    struct Structure
    {
        int mode = 0, era = 4;
        float size = 0.6f, preDelayMs = 20.0f;
        bool differs (const Structure& o) const
        {
            return mode != o.mode || era != o.era || std::abs (size - o.size) > 0.004f || std::abs (preDelayMs - o.preDelayMs) > 0.5f;
        }
    };
    static Structure structureOf (const Params& p) { return { p.mode, p.era, p.size, p.preDelayMs }; }
    Params withStructure (const Structure&) const;

    enum class State { free, active, ringing, dying };
    static constexpr int poolSize = 6;
    struct Slot
    {
        ReverbCore core;
        Structure structure;
        State state = State::free;
        float inputGain = 0.0f;      // ramps to 1 when active, to 0 when ringing
        float outputGain = 1.0f;     // ramps to 0 when dying
        int quietBlocks = 0;
        juce::int64 since = 0;       // when it stopped being active
    };

    Slot slots[poolSize];
    int active = 0;
    juce::int64 clock = 0;
    float rampStep = 0.01f, dieStep = 0.01f;
    juce::int64 lastHandOff = -1000000, minHandOffInterval = 4800;   // at most one hand-over per 100 ms
    Params requested;
    juce::AudioBuffer<float> input, work;
};
