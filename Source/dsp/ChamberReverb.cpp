#include "ChamberReverb.h"

namespace
{
    constexpr float soundMs = 1000.0f / 343.0f;   // milliseconds per metre

    struct Vec { float x, y, z; };

    struct Room
    {
        Vec dims, speaker, mics[2];
        float wallReflection;     // pressure kept per bounce
        float speakerDirect;      // the speaker faces away from the microphones
        float lineScale;
        float bassRatio, midCornerHz, highRatio, highCornerScale;
        float speakerHighCutHz, speakerLowCutHz;
        float earlyLevel, body, outputGain;
    };

    // Tile: about 6.4 x 3.7 x 3 m, half tiled. Reported at about 1.8 s with a warm low-mid.
    // Mirror: smaller and glass walled, so little is lost at each bounce and the top stays open.
    const Room rooms[2] = {
        { { 6.4f, 3.7f, 3.0f }, { 1.3f, 1.1f, 1.3f }, { { 4.7f, 1.2f, 1.6f }, { 4.9f, 2.6f, 1.5f } },
          0.86f, 0.35f, 1.25f, 1.15f, 350.0f, 0.55f, 0.50f, 9500.0f, 110.0f, 1.00f, 0.42f, 1.05f },
        { { 5.1f, 3.3f, 2.7f }, { 1.0f, 0.9f, 1.2f }, { { 3.9f, 1.0f, 1.5f }, { 4.1f, 2.4f, 1.4f } },
          0.95f, 0.50f, 1.00f, 1.00f, 260.0f, 0.80f, 0.85f, 12500.0f, 150.0f, 1.25f, 0.00f, 0.85f },
    };

    // Delay lengths in ms at scale 1. Coprime-ish, spread around the room's mean free path.
    constexpr float lineMs[16] = { 6.7f, 8.3f, 9.7f, 11.3f, 13.1f, 14.9f, 16.3f, 17.9f,
                                   21.7f, 23.3f, 24.9f, 27.1f, 29.3f, 31.3f, 34.7f, 39.1f };
    constexpr float diffuserMs[3] = { 2.3f, 3.7f, 5.9f };

    inline void hadamard16 (float* x)
    {
        for (int h = 1; h < 16; h <<= 1)
            for (int i = 0; i < 16; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        for (int i = 0; i < 16; ++i) x[i] *= 0.25f;
    }

    // Position of the image of a source after n reflections along one axis.
    inline float imageCoord (int n, float length, float source)
    {
        return (n % 2 == 0) ? (float) n * length + source : (float) (n + 1) * length - source;
    }
}

void ChamberReverb::prepare (float newSampleRate)
{
    sampleRate = newSampleRate;
    const auto ms = [this] (float m) { return (int) (m * 0.001f * sampleRate) + 64; };

    tape.setMaxSamples (ms (530.0f));
    early.setMaxSamples (ms (130.0f));
    for (int d = 0; d < numDiffusers; ++d)
        diffuser[d].line.setMaxSamples (ms (diffuserMs[d] * 1.5f + 2.0f));
    for (auto& l : lines)
        l.setMaxSamples (ms (39.1f * 1.5f + 4.0f));

    // Image-source model, first and second order, computed once for each room and microphone.
    for (int v = 0; v < 2; ++v)
        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& room = rooms[v];
            int count = 0;
            for (int nx = -2; nx <= 2; ++nx)
                for (int ny = -2; ny <= 2; ++ny)
                    for (int nz = -2; nz <= 2; ++nz)
                    {
                        const int order = std::abs (nx) + std::abs (ny) + std::abs (nz);
                        if (order > 2 || count >= numTaps)
                            continue;
                        const float dx = imageCoord (nx, room.dims.x, room.speaker.x) - room.mics[mic].x;
                        const float dy = imageCoord (ny, room.dims.y, room.speaker.y) - room.mics[mic].y;
                        const float dz = imageCoord (nz, room.dims.z, room.speaker.z) - room.mics[mic].z;
                        const float dist = std::sqrt (dx * dx + dy * dy + dz * dz);
                        float gain = std::pow (room.wallReflection, (float) order) / juce::jmax (0.5f, dist);
                        if (order == 0)
                            gain *= room.speakerDirect;
                        taps[v][mic][count++] = { dist * soundMs, gain };
                    }
        }

    for (int i = 0; i < numLines; ++i)
    {
        drift[i].seed (0x9E3779B9u * (uint32_t) (i + 1));
    }
    flutter.seed (0x51ED270Bu);
    level.reset ((double) sampleRate, 0.05);
    widthSm.reset ((double) sampleRate, 0.05);
    reset();
}

void ChamberReverb::reset()
{
    tape.clear();
    early.clear();
    tapeFbHigh.reset(); tapeFbLow.reset();
    inHigh.reset(); inLow.reset(); speakerHigh.reset(); speakerLow.reset();
    tapeFeedbackState = 0.0f;
    for (auto& d : diffuser) d.line.clear();
    for (int i = 0; i < numLines; ++i)
    {
        lines[i].clear();
        decay[i].reset();
    }
    for (int ch = 0; ch < 2; ++ch) { bodyHigh[ch].reset(); bodyLow[ch].reset(); }
    started = false;
}

void ChamberReverb::process (float* left, float* right, int numSamples, const ReverbParams& p, int variant)
{
    const auto& room = rooms[juce::jlimit (0, 1, variant)];
    const auto& tapSet = taps[juce::jlimit (0, 1, variant)];

    const float scale = 0.55f + 0.75f * p.size;      // Size 60% is the room at its real dimensions
    const float t60 = juce::jlimit (0.2f, 20.0f, p.decaySeconds);

    float targetLen[numLines];
    for (int i = 0; i < numLines; ++i)
        targetLen[i] = lineMs[i] * room.lineScale * scale * 0.001f * sampleRate;

    if (! started)
    {
        for (int i = 0; i < numLines; ++i) smoothedLen[i] = targetLen[i];
        level.setCurrentAndTargetValue (p.levelGain);
        widthSm.setCurrentAndTargetValue (p.width);
        started = true;
    }

    const float highCorner = juce::jlimit (room.midCornerHz * 2.0f, 16000.0f, p.highCutHz * room.highCornerScale);
    for (int i = 0; i < numLines; ++i)
        decay[i].set (targetLen[i], t60, room.bassRatio, room.highRatio, room.midCornerHz, highCorner, sampleRate);

    inHigh.setCutoff (juce::jmin (p.highCutHz, room.speakerHighCutHz), sampleRate);
    inLow.setCutoff (p.lowCutHz, sampleRate);
    speakerHigh.setCutoff (room.speakerHighCutHz, sampleRate);
    speakerLow.setCutoff (room.speakerLowCutHz, sampleRate);
    tapeFbHigh.setCutoff (5200.0f, sampleRate);
    tapeFbLow.setCutoff (240.0f, sampleRate);
    for (int ch = 0; ch < 2; ++ch)
    {
        bodyHigh[ch].setCutoff (750.0f, sampleRate);
        bodyLow[ch].setCutoff (190.0f, sampleRate);
    }

    // Close speaker and microphones put the reflections forward, distance diffuses them.
    const float diff = juce::jlimit (0.0f, 0.72f, p.diffusion * 0.8f);
    const float earlyGain = room.earlyLevel * juce::jlimit (0.25f, 1.3f, 1.35f - p.diffusion);
    for (int d = 0; d < numDiffusers; ++d)
        diffuser[d].delaySamples = std::round (diffuserMs[d] * scale * 0.001f * sampleRate);

    // Tape: wow follows Mod Rate, flutter sits much faster, Mod Depth scales both.
    const float wowDepth = p.modDepth * 0.0026f * sampleRate;
    const float flutterDepth = p.modDepth * 0.0005f * sampleRate;
    const float wowInc = lumen::twoPi * p.modRateHz / sampleRate;
    const float flutterRate = 5.0f + 2.0f * p.modRateHz;
    const float preSamples = p.preDelayMs * 0.001f * sampleRate;
    const float tapeDelay = juce::jmax (preSamples, wowDepth + flutterDepth + 2.0f);
    // Long slaps recirculate through the loop, as a tape send feeding a chamber did.
    const float tapeFeedback = 0.26f * juce::jlimit (0.0f, 1.0f, (p.preDelayMs - 25.0f) / 100.0f);

    // A real room does not move: only a trace of drift, to stop modes outliving each other.
    const float driftRange = 0.00022f * sampleRate;
    const float lenGlide = 1.0f - std::exp (-1.0f / (0.2f * sampleRate));
    const float tapScale = scale * 0.001f * sampleRate;

    level.setTargetValue (p.levelGain);
    widthSm.setTargetValue (p.width);

    for (int s = 0; s < numSamples; ++s)
    {
        const float send = 0.5f * (left[s] + (right != nullptr ? right[s] : left[s]));   // a mono send

        // Tape stage. Unity for small signals, rounded when pushed.
        const float tapeIn = send + tapeFeedbackState * tapeFeedback;
        tape.push (lumen::softClip (tapeIn * 1.25f) * 0.8f);
        wowPhase += wowInc;
        if (wowPhase > lumen::twoPi) wowPhase -= lumen::twoPi;
        const float tapeOut = tape.readCubic (tapeDelay + std::sin (wowPhase) * wowDepth
                                              + flutter.next (flutterRate, sampleRate) * flutterDepth);
        tapeFeedbackState = tapeFbLow.highpass (tapeFbHigh.lowpass (tapeOut));

        // Speaker.
        const float speaker = speakerLow.highpass (speakerHigh.lowpass (inLow.highpass (inHigh.lowpass (tapeOut))));

        // Early reflections.
        early.push (speaker);
        float mic[2] = { 0.0f, 0.0f };
        for (int m = 0; m < 2; ++m)
            for (int t = 0; t < numTaps; ++t)
                mic[m] += early.readLinear (tapSet[m][t].ms * tapScale) * tapSet[m][t].gain;

        // Late field.
        float tankIn = speaker;
        for (auto& d : diffuser)
            tankIn = d.process (tankIn, diff);

        float out[numLines], mixed[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            smoothedLen[i] += lenGlide * (targetLen[i] - smoothedLen[i]);
            out[i] = lines[i].readCubic (smoothedLen[i] + drift[i].next (0.31f + 0.02f * (float) i, sampleRate) * driftRange);
            mixed[i] = decay[i].process (out[i]);
        }
        hadamard16 (mixed);
        for (int i = 0; i < numLines; ++i)
        {
            float v = mixed[i] + tankIn * ((i & 1) != 0 ? -0.27f : 0.27f);
            if (std::abs (v) > 1.0f) v = std::copysign (1.0f + lumen::softClip (std::abs (v) - 1.0f), v);
            lines[i].push (v);
        }

        // A spaced pair: each microphone hears its own lines, so the room is wide and safe in mono.
        // The mixed polarities keep the mono sum from lining up with how the tank is fed, which
        // would leave a few modes ringing on after the rest.
        static constexpr float polarityA[4] = { 1.0f, -1.0f, 1.0f, 1.0f };
        static constexpr float polarityB[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
        float a = 0.0f, b = 0.0f;
        for (int i = 0; i < numLines; i += 4)
        {
            a += polarityA[i / 4] * (out[i] - out[i + 3]);
            b += polarityB[i / 4] * (out[i + 2] - out[i + 1]);
        }
        float wet[2] = { a * 0.33f + mic[0] * earlyGain, b * 0.33f + mic[1] * earlyGain };

        for (int ch = 0; ch < 2; ++ch)
        {
            // Low-mid weight of the tiled room.
            const float band = bodyLow[ch].highpass (bodyHigh[ch].lowpass (wet[ch]));
            wet[ch] = (wet[ch] + band * room.body) * room.outputGain;
        }

        const float w = widthSm.getNextValue();
        const float m = 0.5f * (wet[0] + wet[1]), sd = 0.5f * (wet[0] - wet[1]) * w;
        const float g = level.getNextValue();
        left[s] = (m + sd) * g;
        if (right != nullptr) right[s] = (m - sd) * g;
    }
}
