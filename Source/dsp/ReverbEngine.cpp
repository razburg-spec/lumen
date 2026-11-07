#include "ReverbEngine.h"

//==============================================================================
ReverbCore::Kind ReverbCore::kindFor (const ReverbParams& p)
{
    if (p.era == 0 && (p.mode == 3 || p.mode == 2)) return Kind::Chamber;   // 60s Chamber / Room
    if (p.era == 3 && (p.mode == 0 || p.mode == 5)) return Kind::Ring;      // 90s Hall / Cloud
    return Kind::Fdn;
}

int ReverbCore::variantFor (const ReverbParams& p)
{
    switch (kindFor (p))
    {
        case Kind::Chamber: return p.mode == 3 ? 0 : 1;   // tiled chamber, mirror room
        case Kind::Ring:    return p.mode == 0 ? 0 : 1;   // hall, concert
        case Kind::Fdn:     break;
    }
    return 0;
}

void ReverbCore::prepare (const juce::dsp::ProcessSpec& spec)
{
    const auto fs = (float) spec.sampleRate;
    fdn.prepare (fs);
    chamber.prepare (fs);
    ring.prepare (fs);
    fadeBuffer.setSize (2, (int) spec.maximumBlockSize);
    fadeLength = juce::jmax (1, (int) (0.03f * fs));
}

void ReverbCore::reset (const ReverbParams& p)
{
    fdn.reset (p);
    chamber.reset();
    ring.reset();
    activeKind = fadingKind = kindFor (p);
    activeVariant = fadingVariant = variantFor (p);
    fadeLeft = 0;
}

void ReverbCore::resetKind (Kind kind, const ReverbParams& p)
{
    switch (kind)
    {
        case Kind::Fdn:     fdn.reset (p); break;
        case Kind::Chamber: chamber.reset(); break;
        case Kind::Ring:    ring.reset(); break;
    }
}

void ReverbCore::processKind (Kind kind, juce::AudioBuffer<float>& buffer, const ReverbParams& p, int variant)
{
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    switch (kind)
    {
        case Kind::Fdn:     fdn.process (L, R, buffer.getNumSamples(), p); break;
        case Kind::Chamber: chamber.process (L, R, buffer.getNumSamples(), p, variant); break;
        case Kind::Ring:    ring.process (L, R, buffer.getNumSamples(), p, variant); break;
    }
}

void ReverbCore::process (juce::AudioBuffer<float>& buffer, const ReverbParams& p)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const auto wanted = kindFor (p);
    if (wanted != activeKind)
    {
        // Mid-fade changes drop the older algorithm: it has been fading for a while already.
        if (fadeLeft > 0)
            resetKind (fadingKind, p);
        fadingKind = activeKind;
        fadingVariant = activeVariant;
        activeKind = wanted;
        resetKind (activeKind, p);   // the incoming algorithm starts clean
        fadeLeft = fadeLength;
    }
    activeVariant = variantFor (p);

    if (fadeLeft <= 0 || n > fadeBuffer.getNumSamples())
    {
        processKind (activeKind, buffer, p, activeVariant);
        return;
    }

    // Both algorithms run for the 30 ms crossfade.
    juce::AudioBuffer<float> old (fadeBuffer.getArrayOfWritePointers(), numCh, n);
    for (int ch = 0; ch < numCh; ++ch)
        old.copyFrom (ch, 0, buffer, ch, 0, n);
    processKind (fadingKind, old, p, fadingVariant);
    processKind (activeKind, buffer, p, activeVariant);

    const float start = (float) fadeLeft / (float) fadeLength;
    const float end = (float) juce::jmax (0, fadeLeft - n) / (float) fadeLength;
    for (int ch = 0; ch < numCh; ++ch)
    {
        buffer.applyGainRamp (ch, 0, n, 1.0f - start, 1.0f - end);
        buffer.addFromWithRamp (ch, 0, old.getReadPointer (ch), n, start, end);
    }
    fadeLeft -= n;
    if (fadeLeft <= 0)
        resetKind (fadingKind, p);
}

//==============================================================================
void ReverbEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    for (auto& slot : slots)
        slot.core.prepare (spec);
    input.setSize (2, (int) spec.maximumBlockSize);
    work.setSize (2, (int) spec.maximumBlockSize);
    rampStep = 1.0f / (0.010f * (float) spec.sampleRate);   // input hand-over: 10 ms
    dieStep  = 1.0f / (0.060f * (float) spec.sampleRate);   // stealing a ringing reverb: 60 ms fade
    minHandOffInterval = (juce::int64) (0.1 * spec.sampleRate);
    reset();
}

void ReverbEngine::reset()
{
    for (auto& slot : slots)
    {
        slot.core.reset (requested);
        slot.state = State::free;
        slot.quietBlocks = 0;
    }
    active = 0;
    slots[0].state = State::active;
    slots[0].structure = structureOf (requested);
    slots[0].inputGain = 1.0f;
    slots[0].outputGain = 1.0f;
}

ReverbEngine::Params ReverbEngine::withStructure (const Structure& s) const
{
    auto p = requested;
    p.mode = s.mode; p.era = s.era; p.size = s.size; p.preDelayMs = s.preDelayMs;
    return p;
}

void ReverbEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;
    if (n > input.getNumSamples())
    {
        // Host broke the block-size promise: run the active reverb alone.
        slots[active].core.process (buffer, withStructure (slots[active].structure));
        return;
    }

    // A structural change hands the input to a fresh reverb; the old one rings out.
    const auto wanted = structureOf (requested);
    if (wanted.differs (slots[active].structure) && clock - lastHandOff >= minHandOffInterval)
    {
        int freeSlot = -1;
        for (int i = 0; i < poolSize; ++i)
            if (slots[i].state == State::free) { freeSlot = i; break; }

        if (freeSlot >= 0)
        {
            auto& old = slots[active];
            old.state = State::ringing;
            old.since = clock;
            old.quietBlocks = 0;
            auto& fresh = slots[freeSlot];
            fresh.structure = wanted;
            fresh.state = State::active;
            fresh.inputGain = 0.0f;
            fresh.outputGain = 1.0f;
            active = freeSlot;
            lastHandOff = clock;
        }
        else
        {
            // Pool full (the knob keeps moving): fade out the oldest ringing reverb to make room.
            bool alreadyDying = false;
            int oldest = -1;
            for (int i = 0; i < poolSize; ++i)
            {
                if (slots[i].state == State::dying) alreadyDying = true;
                if (slots[i].state == State::ringing && (oldest < 0 || slots[i].since < slots[oldest].since)) oldest = i;
            }
            if (! alreadyDying && oldest >= 0)
                slots[oldest].state = State::dying;
        }
    }

    juce::AudioBuffer<float> in (input.getArrayOfWritePointers(), numCh, n);
    juce::AudioBuffer<float> tmp (work.getArrayOfWritePointers(), numCh, n);
    for (int ch = 0; ch < numCh; ++ch)
        in.copyFrom (ch, 0, buffer, ch, 0, n);
    buffer.clear();

    for (int i = 0; i < poolSize; ++i)
    {
        auto& slot = slots[i];
        if (slot.state == State::free)
            continue;

        // Input: ramps in for the active reverb, out for the others.
        const float target = slot.state == State::active ? 1.0f : 0.0f;
        const float g0 = slot.inputGain;
        const float g1 = target > g0 ? juce::jmin (target, g0 + rampStep * (float) n) : juce::jmax (target, g0 - rampStep * (float) n);
        slot.inputGain = g1;
        if (g0 <= 0.0f && g1 <= 0.0f)
            tmp.clear();
        else
            for (int ch = 0; ch < numCh; ++ch)
            {
                tmp.copyFrom (ch, 0, in, ch, 0, n);
                if (g0 < 1.0f || g1 < 1.0f) tmp.applyGainRamp (ch, 0, n, g0, g1);
            }

        slot.core.process (tmp, withStructure (slot.structure));

        // Output: a dying reverb fades away.
        float o0 = slot.outputGain, o1 = o0;
        if (slot.state == State::dying)
            o1 = juce::jmax (0.0f, o0 - dieStep * (float) n);
        slot.outputGain = o1;
        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFromWithRamp (ch, 0, tmp.getReadPointer (ch), n, o0, o1);

        if (slot.state == State::ringing)
        {
            // Free it once its tail is inaudible (-100 dBFS) for a little while, or after 30 s.
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = juce::jmax (peak, tmp.getMagnitude (ch, 0, n));
            slot.quietBlocks = peak < 1.0e-5f ? slot.quietBlocks + 1 : 0;
            if (slot.quietBlocks > 8 || clock - slot.since > 30 * minHandOffInterval * 10)
                slot.state = State::dying;
        }
        if (slot.state == State::dying && o1 <= 0.0f)
        {
            slot.core.reset (withStructure (slot.structure));
            slot.state = State::free;
        }
    }
    clock += n;
}
