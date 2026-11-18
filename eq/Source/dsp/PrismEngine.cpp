#include "PrismEngine.h"

using namespace PrismFilters;

Design PrismEngine::design (int band, const BandParams& p, int mode, double sampleRate)
{
    const auto& voice = modeVoicing (mode);
    const double w0 = 2.0 * pi * juce::jlimit (10.0, 0.499 * sampleRate, (double) p.freq) / sampleRate;

    Design d;
    if (Band::isEdge (band) && p.type == (int) Band::EdgeType::cut)
    {
        // Q above the Butterworth value adds a resonant bump at the corner.
        const double resonance = juce::jlimit (0.5, 8.0, (double) (p.q / 0.7071f * voice.cutResonance));
        return cut (band == Band::low, w0, p.slope + 1, resonance);
    }

    d.count = 1;
    if (Band::isEdge (band))
    {
        const float scale = band == Band::low ? voice.lowShelfQ : voice.highShelfQ;
        const double q = juce::jlimit (0.3, 4.0, (double) (p.q * scale));
        d.sections[0] = band == Band::low ? lowShelf (w0, p.gainDb, q) : highShelf (w0, p.gainDb, q);

        // The classic passive trick: a boosting low shelf also scoops the low mids just above it.
        if (band == Band::low && p.gainDb > 0.0f && voice.lowShelfDip > 0.0f)
        {
            const double wd = juce::jmin (0.9 * pi, w0 * voice.lowDipRatio);
            d.sections[1] = bell (wd, -p.gainDb * voice.lowShelfDip, 0.8);
            d.count = 2;
        }
    }
    else
    {
        const double q = juce::jlimit (0.05, 30.0, (double) effectiveBellQ (voice, p.q, p.gainDb));
        d.sections[0] = bell (w0, p.gainDb, q);
    }
    return d;
}

int PrismEngine::linearLength (double sampleRate)
{
    // About 170 ms of filter at any rate: enough resolution for a steep cut at 20 Hz.
    return sampleRate <= 50000.0 ? 8192 : (sampleRate <= 100000.0 ? 16384 : 32768);
}

juce::AudioBuffer<float> PrismEngine::buildLinearIr (const Params& params, double sampleRate)
{
    const int n = linearLength (sampleRate);
    const int order = juce::roundToInt (std::log2 ((double) n));
    juce::dsp::FFT fft (order);

    std::array<Design, Band::numBands> designs;
    std::array<bool, Band::numBands> active {};
    for (int b = 0; b < Band::numBands; ++b)
    {
        active[(size_t) b] = params.bands[(size_t) b].on;
        if (active[(size_t) b])
            designs[(size_t) b] = design (b, params.bands[(size_t) b], params.mode, sampleRate);
    }

    // Zero-phase spectrum: the magnitude of the whole EQ on every bin.
    std::vector<float> data ((size_t) n * 2, 0.0f);
    for (int k = 0; k <= n / 2; ++k)
    {
        const double w = 2.0 * pi * (double) k / (double) n;
        double m = 1.0;
        for (int b = 0; b < Band::numBands; ++b)
            if (active[(size_t) b])
                m *= designs[(size_t) b].magnitude (juce::jmin (w, pi * 0.99999));
        data[(size_t) (2 * k)] = (float) m;
    }
    fft.performRealOnlyInverseTransform (data.data());

    // The result is centred on sample 0 and wraps around: move the centre to n/2 and window it.
    juce::AudioBuffer<float> ir (1, n);
    auto* out = ir.getWritePointer (0);
    for (int i = 0; i < n; ++i)
    {
        const int src = (i + n / 2) % n;
        const double x = (double) i / (double) n;
        const double window = 0.42 - 0.5 * std::cos (2.0 * pi * x) + 0.08 * std::cos (4.0 * pi * x);   // Blackman
        out[i] = (float) (data[(size_t) src] * window);
    }
    return ir;
}

void PrismEngine::loadLinearIr (juce::AudioBuffer<float>&& ir)
{
    convolution.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

void PrismEngine::prepare (double sampleRate, int maxBlock, int numChannels)
{
    sr = sampleRate;
    channels = juce::jlimit (1, maxChannels, numChannels);
    mixStep = (float) (1.0 / (0.02 * sr));   // 20 ms fades when a band is switched or changes structure

    for (auto& b : bands)
    {
        b.freq.reset (sr, 0.03);
        b.gain.reset (sr, 0.03);
        b.q.reset (sr, 0.03);
        b.mix = b.targetMix = 0.0f;
        b.on = false;
        b.coeffsValid = false;
        for (auto& ch : b.state)
            for (auto& s : ch)
                s.reset();
    }
    outGain.reset (sr, 0.03);
    driveAmount.reset (sr, 0.05);
    lowCoef = (float) (1.0 - std::exp (-2.0 * pi * 150.0 / sr));
    dcCoef = (float) std::exp (-2.0 * pi * 8.0 / sr);
    for (auto& c : colour)
        c = {};
    monoSize = juce::jmax (1, maxBlock);
    mono.allocate ((size_t) monoSize, true);
    first = true;
    wasLinear = false;
    convolution.prepare ({ sampleRate, (juce::uint32) juce::jmax (1, maxBlock), (juce::uint32) juce::jlimit (1, maxChannels, numChannels) });
    convolution.reset();
}

void PrismEngine::updateBand (int index, const BandParams& p, int mode)
{
    auto& b = bands[(size_t) index];
    const bool edge = Band::isEdge (index);
    const int type = edge ? p.type : 0;
    const int slope = edge ? p.slope : 0;

    // Gain does nothing on a cut filter: hold it at 0 so switching to Shelf later starts flat.
    const float gainTarget = (edge && type == (int) Band::EdgeType::cut) ? 0.0f : p.gainDb;

    const bool structureChanged = b.on != p.on || b.type != type || b.slope != slope;
    const bool silent = b.mix <= 0.0f;

    if (first || (silent && structureChanged) || (silent && ! b.on))
    {
        // Nothing audible is running: adopt the new settings at once.
        b.on = p.on;
        b.type = type;
        b.slope = slope;
        b.freq.setCurrentAndTargetValue (p.freq);
        b.gain.setCurrentAndTargetValue (gainTarget);
        b.q.setCurrentAndTargetValue (p.q);
        for (auto& ch : b.state)
            for (auto& s : ch)
                s.reset();
        b.coeffsValid = false;
        b.targetMix = b.on ? 1.0f : 0.0f;
        if (first)
            b.mix = b.targetMix;
    }
    else if (structureChanged)
    {
        b.targetMix = 0.0f;   // fade out first; the new structure starts once the band is silent
    }
    else
    {
        b.targetMix = b.on ? 1.0f : 0.0f;
    }

    if (b.on == p.on && b.type == type && b.slope == slope)
    {
        b.freq.setTargetValue (p.freq);
        b.gain.setTargetValue (gainTarget);
        b.q.setTargetValue (p.q);
    }

    if (b.mode != mode)
    {
        b.mode = mode;
        b.coeffsValid = false;
    }
}

bool PrismEngine::bandIsIdentity (int index) const
{
    const auto& b = bands[(size_t) index];
    const bool cutFilter = Band::isEdge (index) && b.type == (int) Band::EdgeType::cut;
    return ! cutFilter && b.mix >= 1.0f && b.targetMix >= 1.0f
           && ! b.gain.isSmoothing() && juce::exactlyEqual (b.gain.getCurrentValue(), 0.0f);
}

void PrismEngine::runBand (int index, juce::AudioBuffer<float>& buffer, int start, int n)
{
    auto& b = bands[(size_t) index];
    if (b.mix <= 0.0f && b.targetMix <= 0.0f)
        return;

    const bool moving = b.freq.isSmoothing() || b.gain.isSmoothing() || b.q.isSmoothing();
    if (moving)
    {
        b.freq.skip (n);
        b.gain.skip (n);
        b.q.skip (n);
    }
    if (moving || ! b.coeffsValid)
    {
        BandParams p;
        p.on = true;
        p.freq = b.freq.getCurrentValue();
        p.gainDb = b.gain.getCurrentValue();
        p.q = b.q.getCurrentValue();
        p.type = b.type;
        p.slope = b.slope;
        b.coeffs = design (index, p, b.mode, sr);
        b.coeffsValid = true;
    }

    if (bandIsIdentity (index))
    {
        for (auto& ch : b.state)
            for (auto& s : ch)
                s.reset();
        return;
    }

    const float startMix = b.mix;
    float mix = startMix;
    const int sections = b.coeffs.count;
    for (int ch = 0; ch < juce::jmin (channels, buffer.getNumChannels()); ++ch)
    {
        auto* data = buffer.getWritePointer (ch, start);
        auto& st = b.state[(size_t) ch];
        mix = startMix;
        for (int i = 0; i < n; ++i)
        {
            double x = data[i], y = x;
            for (int s = 0; s < sections; ++s)
                y = st[(size_t) s].process (b.coeffs.sections[(size_t) s], y);

            if (! juce::exactlyEqual (mix, b.targetMix))
                mix = b.targetMix > mix ? juce::jmin (b.targetMix, mix + mixStep) : juce::jmax (b.targetMix, mix - mixStep);
            data[i] = (float) (mix >= 1.0f ? y : x + (double) mix * (y - x));
        }
    }
    b.mix = mix;

    if (b.mix <= 0.0f)
        for (auto& chState : b.state)
            for (auto& s : chState)
                s.reset();
}

void PrismEngine::runColour (juce::AudioBuffer<float>& buffer, int mode)
{
    if (! driveAmount.isSmoothing() && driveAmount.getCurrentValue() <= 0.0f)
    {
        for (auto& c : colour)
            c = {};
        return;
    }

    const auto& v = modeVoicing (mode);
    const int numCh = juce::jmin (channels, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    const float topCoefBase = (float) (1.0 - std::exp (-2.0 * pi * juce::jmin (16000.0, 0.45 * sr) / sr));

    auto smoothing = driveAmount;   // each channel walks the same ramp
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        auto& st = colour[(size_t) ch];
        auto ramp = smoothing;
        for (int i = 0; i < n; ++i)
        {
            const float amt = ramp.getNextValue();
            const float x = d[i];
            const float g = 1.0f + v.drive * amt;            // pre-gain into the shaper
            const float bias = v.asymmetry * amt;

            float y = shapeSample (x * g, v.shape, bias) / g;

            // Transformer weight: the lows saturate a little harder.
            st.low += lowCoef * (x - st.low);
            if (v.ironLow > 0.0f)
            {
                const float lg = 1.0f + 2.0f * v.drive * amt;
                y += v.ironLow * amt * (std::tanh (st.low * lg) / lg - st.low);
            }

            // Asymmetry leaves a little DC: remove it.
            const float hp = y - st.dcIn + dcCoef * st.dcOut;
            st.dcIn = y;
            st.dcOut = hp;
            y = hp;

            // Top-end softening grows with drive.
            st.top += topCoefBase * (y - st.top);
            y += v.softTop * amt * (st.top - y);

            d[i] = y;
        }
        if (ch == numCh - 1)
            driveAmount = ramp;
    }
}

void PrismEngine::process (juce::AudioBuffer<float>& buffer, const Params& params)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (channels, buffer.getNumChannels());
    if (numSamples == 0 || numCh == 0)
        return;

    auto feedTap = [&] (AnalyzerTap& tap)
    {
        for (int pos = 0; pos < numSamples; pos += monoSize)
        {
            const int n = juce::jmin (monoSize, numSamples - pos);
            juce::FloatVectorOperations::copy (mono.get(), buffer.getReadPointer (0, pos), n);
            if (numCh > 1)
            {
                juce::FloatVectorOperations::add (mono.get(), buffer.getReadPointer (1, pos), n);
                juce::FloatVectorOperations::multiply (mono.get(), 0.5f, n);
            }
            tap.push (mono.get(), n);
        }
    };

    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));
    meters.inputPeak.store (juce::jmax (meters.inputPeak.load(), inPeak));
    feedTap (preTap);

    if (params.linear)
    {
        if (! wasLinear)
            convolution.reset();
        wasLinear = true;
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
    else if (wasLinear)
    {
        // Back from Linear Phase: start the biquads fresh with the current settings.
        wasLinear = false;
        first = true;
    }

    if (! params.linear)
        for (int b = 0; b < Band::numBands; ++b)
            updateBand (b, params.bands[(size_t) b], params.mode);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (params.outputDb));
    if (first)
        outGain.setCurrentAndTargetValue (outGain.getTargetValue());
    first = false;

    for (int start = 0; start < (params.linear ? 0 : numSamples); start += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - start);
        for (int b = 0; b < Band::numBands; ++b)
        {
            runBand (b, buffer, start, n);

            // A band that faded out to change its structure now starts again with the new one.
            auto& band = bands[(size_t) b];
            if (band.mix <= 0.0f && band.targetMix <= 0.0f)
                updateBand (b, params.bands[(size_t) b], params.mode);
        }
    }

    driveAmount.setTargetValue (juce::jlimit (0.0f, 1.0f, params.drive));
    runColour (buffer, params.mode);

    // Output gain (skipped at exactly 0 dB so the default path stays bit-exact).
    if (outGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else if (! juce::exactlyEqual (outGain.getCurrentValue(), 1.0f))
    {
        for (int ch = 0; ch < numCh; ++ch)
            buffer.applyGain (ch, 0, numSamples, outGain.getCurrentValue());
    }

    float outPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));
    meters.outputPeak.store (juce::jmax (meters.outputPeak.load(), outPeak));
    feedTap (postTap);
}
