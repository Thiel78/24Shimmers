// 24 Shimmers - shimmer reverb DSP engine.
//
// Plain C++ (no JUCE) so it can be unit-tested on its own.
//
// Signal flow:
//
//   input -> pre-delay -+-> early reflections ------------------------------+--> wet
//                       |         |                                         |
//                       +---------+--> diffuser (8 ch) --> late FDN (16) ---+
//                                         ^                       |
//                                         +--- octave shifter <---+   (shimmer loop)
//
// The pitch shifter sits outside the reverb: it takes the late reverb output,
// shifts it by an octave and feeds it back into the diffuser. The diffuser
// smears the shifter's grains, and every trip round the loop adds another octave.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace shimmer
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

//==============================================================================
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;

        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear()             { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    void push (float x)      { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }

    // d == 1 is the most recently pushed sample.
    float tap (int d) const  { return buffer[(size_t) ((writePos - d) & mask)]; }

    float tapLinear (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = tap (i), b = tap (i + 1);
        return a + f * (b - a);
    }

    // Cubic Hermite interpolation, d must be >= 2.
    float tapCubic (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

//==============================================================================
struct OnePole
{
    float a = 1.0f, z = 0.0f;

    void setCutoff (float hz, float sampleRate) { a = 1.0f - std::exp (-twoPi * hz / sampleRate); }
    float lowpass (float x)                     { z += a * (x - z); return z; }
    float highpass (float x)                    { return x - lowpass (x); }
    void reset()                                { z = 0.0f; }
};

//==============================================================================
// RBJ high-pass biquad (transposed direct form II).
struct HighPass
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setCutoff (float hz, float sampleRate)
    {
        const float w = twoPi * hz / sampleRate;
        const float cw = std::cos (w), alpha = std::sin (w) / (2.0f * 0.7071f);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f + cw) * 0.5f / a0;
        b1 = -(1.0f + cw) / a0;
        b2 = b0;
        a1 = -2.0f * cw / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0f; }
};

//==============================================================================
// Small deterministic random generator, so every space sounds the same each time.
struct Random
{
    uint32_t state;
    explicit Random (uint32_t seed) : state (seed) {}
    float next() { state = state * 1664525u + 1013904223u; return (float) (state >> 8) / 16777216.0f; }
};

//==============================================================================
// Granular octave shifter with three overlapping grains. Grain windows are
// power-complementary (sin), so the level stays steady on reverb-like signals
// instead of fluttering at the grain rate.
class PitchShifter
{
public:
    void prepare (int windowSamples, float startPhase)
    {
        window = (float) windowSamples;
        line.prepare (windowSamples + 8);
        phase = startPhase;
    }

    void setRatio (float r) { ratio = r; }
    void clear()            { line.clear(); }

    float process (float x)
    {
        line.push (x);

        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        float out = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            float p = phase + (float) k * (1.0f / 3.0f);
            if (p >= 1.0f)
                p -= 1.0f;
            out += std::sin (pi * p) * line.tapCubic (2.0f + p * window);
        }
        return out * 0.81649658f; // sqrt (2/3)
    }

private:
    DelayLine line;
    float window = 4096.0f, phase = 0.0f, ratio = 2.0f;
};

//==============================================================================
enum class Space { room = 0, hall = 1, cathedral = 2 };

struct SpaceSpec
{
    float lineMinMs, lineMaxMs;     // late reverb delay line spread (at size 50%)
    float diffusionMs;              // total time of the diffuser
    int diffusionSteps;
    float earlyStartMs, earlyEndMs; // early reflection window (at size 50%)
    float earlyLevel, earlyDecay;   // level of the reflections, level of the last one vs the first
    float earlyLowpassHz;
    float modDepthMs;               // late line modulation at Mod = 100%
    float crossoverHz;              // above this the tail decays faster (Tone)
    float highDecayScale;           // how much faster the highs die in this space
    uint32_t seed;
};

inline const SpaceSpec& specFor (Space s)
{
    static const SpaceSpec specs[] {
        // room
        { 7.0f, 31.0f, 22.0f, 3, 1.5f, 28.0f, 0.85f, 0.25f, 10000.0f, 0.25f, 4500.0f, 0.85f, 11u },
        // hall
        { 19.0f, 83.0f, 55.0f, 4, 6.0f, 70.0f, 0.55f, 0.35f, 8000.0f, 0.45f, 3200.0f, 0.65f, 23u },
        // cathedral
        { 41.0f, 167.0f, 110.0f, 5, 14.0f, 135.0f, 0.4f, 0.5f, 6000.0f, 0.7f, 2400.0f, 0.5f, 37u },
    };
    return specs[std::clamp ((int) s, 0, 2)];
}

//==============================================================================
struct Parameters
{
    Space space = Space::hall;
    float mix = 0.35f;          // 0..1
    float decaySeconds = 6.0f;  // RT60 of the low band
    float size = 0.5f;          // 0..1, scales the chosen space
    float preDelayMs = 20.0f;   // 0..500
    float shimmer = 0.5f;       // 0..1, how much shifted signal feeds back
    int octave = 1;             // -1, +1 or +2
    float tone = 0.6f;          // 0 = dark .. 1 = bright
    float lowCutHz = 120.0f;
    float modulation = 0.4f;    // 0..1
    bool freeze = false;
};

//==============================================================================
class Engine
{
public:
    static constexpr int numLines = 16;
    static constexpr int numDiffuseChannels = 8;
    static constexpr int maxDiffuseSteps = 5;
    static constexpr int numEarlyTaps = 12;

    void prepare (double sampleRateIn)
    {
        sampleRate = (float) sampleRateIn;

        const float maxLineMs = 170.0f * maxSizeScale;
        const int maxLine = (int) std::ceil ((maxLineMs + 1.0f) * 0.001f * sampleRate + 8.0f);
        for (auto& l : lines)
            l.prepare (maxLine);

        for (auto& step : diffuserLines)
            for (auto& l : step)
                l.prepare ((int) (0.07f * sampleRate) + 4);

        const int maxPre = (int) ((0.5f + 0.15f * maxSizeScale) * sampleRate) + 8;
        preDelayL.prepare (maxPre);
        preDelayR.prepare (maxPre);

        shifterL.prepare ((int) (0.083f * sampleRate), 0.0f);
        shifterR.prepare ((int) (0.071f * sampleRate), 0.5f);

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            lfoPhase[i] = std::fmod (0.618034f * (float) i, 1.0f);
            lfoPhase2[i] = std::fmod (0.414214f * (float) i + 0.3f, 1.0f);
            lfoInc[i] = (0.07f + 0.045f * (float) i) / sampleRate;
            lfoInc2[i] = (0.19f + 0.031f * (float) ((i * 7) % numLines)) / sampleRate;
        }

        smoothCoeff = 1.0f - std::exp (-1.0f / (0.03f * sampleRate));
        slowCoeff = 1.0f - std::exp (-1.0f / (0.3f * sampleRate));
        fadeStep = 1.0f / (0.03f * sampleRate);

        shimmerHP.setCutoff (180.0f, sampleRate);
        shimmerHP2.setCutoff (180.0f, sampleRate);
        shimmerLP.setCutoff (7500.0f, sampleRate);
        shimmerLP2.setCutoff (7500.0f, sampleRate);

        activeSpace = params.space;
        configureSpace();
        lastLowCut = -1.0f;
        computeTargets();
        cur = target;
        spaceFade = 1.0f;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& step : diffuserLines)
            for (auto& l : step)
                l.clear();

        preDelayL.clear();
        preDelayR.clear();
        shifterL.clear();
        shifterR.clear();
        crossover.fill (0.0f);
        earlyLPL.reset(); earlyLPR.reset();
        shimmerHP.reset(); shimmerHP2.reset(); shimmerLP.reset(); shimmerLP2.reset();
        outHPL.reset(); outHPR.reset();
        feedL = feedR = 0.0f;
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        computeTargets();
    }

    // In-place stereo processing: left/right hold the input and receive dry + wet.
    void process (float* left, float* right, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            updateSpaceFade();
            smoothAll();

            const float dryL = left[n], dryR = right[n];
            preDelayL.push (dryL);
            preDelayR.push (dryR);

            const float input = cur.input * spaceFade;
            const float pre = 1.0f + cur.preDelay;
            const float xL = preDelayL.tapLinear (pre) * input;
            const float xR = preDelayR.tapLinear (pre) * input;

            // Early reflections: a sparse pattern of taps, partly crossed between channels.
            float earlyL = 0.0f, earlyR = 0.0f;
            for (size_t i = 0; i < (size_t) numEarlyTaps; ++i)
            {
                const auto& srcL = (i % 3 == 2) ? preDelayR : preDelayL;
                const auto& srcR = (i % 3 == 2) ? preDelayL : preDelayR;
                earlyL += earlyGainL[i] * srcL.tapLinear (pre + earlyTimeL[i] * cur.sizeScale);
                earlyR += earlyGainR[i] * srcR.tapLinear (pre + earlyTimeR[i] * cur.sizeScale);
            }
            earlyL = earlyLPL.lowpass (earlyL) * input;
            earlyR = earlyLPR.lowpass (earlyR) * input;

            // Diffuser input: dry, some early reflections, and the shimmer feedback.
            std::array<float, numDiffuseChannels> d;
            const float inL = 0.5f * (xL + 0.5f * earlyL + feedL * spaceFade);
            const float inR = 0.5f * (xR + 0.5f * earlyR + feedR * spaceFade);
            for (size_t c = 0; c < (size_t) numDiffuseChannels; ++c)
                d[c] = (c & 1) == 0 ? inL : inR;

            diffuse (d);

            // Late reverb.
            std::array<float, numLines> y;
            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                lfoPhase2[i] += lfoInc2[i];
                if (lfoPhase2[i] >= 1.0f) lfoPhase2[i] -= 1.0f;

                // Two slow LFOs at unrelated rates give a wandering, non-periodic movement.
                const float wobble = 0.65f * std::sin (twoPi * lfoPhase[i]) + 0.35f * std::sin (twoPi * lfoPhase2[i]);
                float v = lines[i].tapCubic (cur.length[i] * cur.sizeScale + cur.modDepth * (1.0f + wobble));

                crossover[i] += crossoverCoeff * (v - crossover[i]);
                y[i] = cur.gainLow[i] * crossover[i] + cur.gainHigh[i] * (v - crossover[i]);
            }

            float lateL = 0.0f, lateR = 0.0f;
            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                lateL += outSignsL[i] * y[i];
                lateR += outSignsR[i] * y[i];
            }
            lateL *= 0.25f;
            lateR *= 0.25f;

            // Householder feedback matrix.
            float sum = 0.0f;
            for (auto v : y) sum += v;
            const float h = sum * (2.0f / numLines);

            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                const float in = d[i % numDiffuseChannels] * (i < numDiffuseChannels ? 0.7071f : -0.7071f);
                lines[i].push (softLimit (y[i] - h + in));
            }

            // Shimmer loop: shift the normalised late output and send it back round.
            const float sL = shifterL.process (lateL * cur.feedNorm);
            const float sR = shifterR.process (lateR * cur.feedNorm);
            feedL = softLimit (shimmerLP.lowpass (shimmerHP.process (sL)) * cur.shimmerGain);
            feedR = softLimit (shimmerLP2.lowpass (shimmerHP2.process (sR)) * cur.shimmerGain);

            const float wetL = outHPL.process (cur.earlyLevel * earlyL + cur.lateLevel * lateL) * spaceFade;
            const float wetR = outHPR.process (cur.earlyLevel * earlyR + cur.lateLevel * lateR) * spaceFade;

            left[n] = cur.dry * dryL + cur.wet * wetL;
            right[n] = cur.dry * dryR + cur.wet * wetR;
        }
    }

private:
    struct Smoothed
    {
        float dry = 1, wet = 0, input = 1, preDelay = 0, modDepth = 0, sizeScale = 1;
        float earlyLevel = 0, lateLevel = 0, feedNorm = 0, shimmerGain = 0;
        std::array<float, numLines> length {}, gainLow {}, gainHigh {};
    };

    static constexpr float maxSizeScale = 1.6f;
    static constexpr float lateOutputScale = 0.7f;
    static constexpr float tailCalibration = 18.8f; // measured: makes the late power gain ~0 dB

    static constexpr std::array<float, numLines> outSignsL { 1, -1, 1, 1, -1, 1, -1, -1, 1, 1, -1, 1, -1, -1, 1, -1 };
    static constexpr std::array<float, numLines> outSignsR { 1, 1, -1, 1, 1, -1, -1, 1, -1, 1, 1, -1, -1, 1, -1, -1 };

    static float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a < 2.0f)
            return x;
        return std::copysign (2.0f + std::tanh (a - 2.0f), x);
    }

    static float sizeScaleFor (float size) { return 0.4f + (maxSizeScale - 0.4f) * std::clamp (size, 0.0f, 1.0f); }

    void diffuse (std::array<float, numDiffuseChannels>& x)
    {
        for (int s = 0; s < diffuseSteps; ++s)
        {
            auto& step = diffuserLines[(size_t) s];
            std::array<float, numDiffuseChannels> t;

            for (size_t c = 0; c < (size_t) numDiffuseChannels; ++c)
            {
                step[c].push (x[c]);
                t[c] = step[c].tap (diffuserDelay[(size_t) s][c]);
            }

            // Shuffle channels and flip some polarities, then mix everything with everything.
            for (size_t c = 0; c < (size_t) numDiffuseChannels; ++c)
                x[c] = t[(c * 3 + (size_t) s) % numDiffuseChannels] * diffuserFlip[(size_t) s][c];

            hadamard8 (x);
        }
    }

    static void hadamard8 (std::array<float, numDiffuseChannels>& v)
    {
        for (size_t h = 1; h < (size_t) numDiffuseChannels; h <<= 1)
            for (size_t i = 0; i < (size_t) numDiffuseChannels; i += h << 1)
                for (size_t j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b;
                    v[j + h] = a - b;
                }

        for (auto& x : v)
            x *= 0.35355339059f; // 1 / sqrt(8)
    }

    // Lays out delay lengths, diffuser and early reflections for the active space.
    void configureSpace()
    {
        const auto& spec = specFor (activeSpace);
        Random rng (spec.seed);

        // Late lines spread exponentially between min and max, with a little jitter.
        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            const float t = ((float) i + 0.3f * (rng.next() - 0.5f)) / (float) (numLines - 1);
            baseLength[i] = spec.lineMinMs * std::pow (spec.lineMaxMs / spec.lineMinMs, std::clamp (t, 0.0f, 1.0f)) * 0.001f * sampleRate;
        }

        meanLengthSeconds = 0.0f;
        for (auto l : baseLength)
            meanLengthSeconds += l;
        meanLengthSeconds /= (float) numLines * sampleRate;

        // Diffuser: each step covers half the time of the one before.
        diffuseSteps = std::min (spec.diffusionSteps, maxDiffuseSteps);
        float range = spec.diffusionMs * 0.5f * 0.001f * sampleRate;
        for (size_t s = 0; s < (size_t) diffuseSteps; ++s)
        {
            for (size_t c = 0; c < (size_t) numDiffuseChannels; ++c)
            {
                const float slot = ((float) c + rng.next()) / (float) numDiffuseChannels;
                diffuserDelay[s][c] = 1 + (int) (range * slot);
                diffuserFlip[s][c] = rng.next() < 0.5f ? -1.0f : 1.0f;
            }
            range *= 0.5f;
        }

        // Early reflections: stratified random times, decaying gains, random polarity.
        crossoverCoeff = 1.0f - std::exp (-twoPi * spec.crossoverHz / sampleRate);
        earlyLPL.setCutoff (spec.earlyLowpassHz, sampleRate);
        earlyLPR.setCutoff (spec.earlyLowpassHz, sampleRate);

        auto layout = [&] (std::array<float, numEarlyTaps>& times, std::array<float, numEarlyTaps>& gains)
        {
            float energy = 0.0f;
            for (size_t i = 0; i < (size_t) numEarlyTaps; ++i)
            {
                const float t = ((float) i + rng.next()) / (float) numEarlyTaps;
                times[i] = (spec.earlyStartMs + (spec.earlyEndMs - spec.earlyStartMs) * t * t) * 0.001f * sampleRate;
                gains[i] = std::pow (spec.earlyDecay, t) * (0.6f + 0.4f * rng.next()) * (rng.next() < 0.5f ? -1.0f : 1.0f);
                energy += gains[i] * gains[i];
            }
            for (auto& g : gains)
                g /= std::sqrt (energy);
        };
        layout (earlyTimeL, earlyGainL);
        layout (earlyTimeR, earlyGainR);
    }

    void computeTargets()
    {
        const auto& p = params;
        const auto& spec = specFor (activeSpace);

        target.dry = std::cos (p.mix * 0.5f * pi);
        target.wet = std::sin (p.mix * 0.5f * pi);
        target.input = p.freeze ? 0.0f : 1.0f;
        target.preDelay = std::clamp (p.preDelayMs, 0.0f, 500.0f) * 0.001f * sampleRate;
        target.modDepth = std::clamp (p.modulation, 0.0f, 1.0f) * spec.modDepthMs * 0.001f * sampleRate;
        target.sizeScale = sizeScaleFor (p.size);

        const float rt60 = std::max (0.1f, p.decaySeconds);
        const float meanLen = meanLengthSeconds * target.sizeScale;

        // Energy in the tail grows with decay / line length. Normalise it so the
        // shimmer loop gain is the same everywhere, and the output only gets a
        // little louder for long decays.
        const float norm = tailCalibration * std::sqrt (meanLen / rt60);
        target.feedNorm = norm;
        target.lateLevel = lateOutputScale * norm * std::pow (rt60 / 6.0f, 0.1f);
        target.earlyLevel = spec.earlyLevel * 0.7f;
        // Each trip round the loop gains at most 0.62^2 (about -4 dB) of power, so a
        // full shimmer cascades up the octaves but always dies away in the end.
        target.shimmerGain = p.freeze ? 0.0f : 0.62f * std::sqrt (std::clamp (p.shimmer, 0.0f, 1.0f));

        const float hfRatio = (0.08f + 0.92f * std::pow (std::clamp (p.tone, 0.0f, 1.0f), 1.3f)) * spec.highDecayScale;

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            target.length[i] = baseLength[i];
            const float len = baseLength[i] * target.sizeScale;

            if (p.freeze)
            {
                target.gainLow[i] = 0.99999f;
                target.gainHigh[i] = 0.99999f;
            }
            else
            {
                target.gainLow[i] = std::pow (10.0f, -3.0f * len / (rt60 * sampleRate));
                target.gainHigh[i] = std::pow (10.0f, -3.0f * len / (rt60 * hfRatio * sampleRate));
            }
        }

        const float ratio = p.octave < 0 ? 0.5f : (p.octave >= 2 ? 4.0f : 2.0f);
        shifterL.setRatio (ratio);
        shifterR.setRatio (ratio);

        if (std::abs (p.lowCutHz - lastLowCut) > 1.0e-3f)
        {
            lastLowCut = p.lowCutHz;
            const float hz = std::clamp (p.lowCutHz, 10.0f, 0.45f * sampleRate);
            outHPL.setCutoff (hz, sampleRate);
            outHPR.setCutoff (hz, sampleRate);
        }
    }

    // Switching space fades the reverb out, swaps the layout, and fades back in.
    void updateSpaceFade()
    {
        if (params.space != activeSpace)
        {
            spaceFade -= fadeStep;
            if (spaceFade <= 0.0f)
            {
                spaceFade = 0.0f;
                activeSpace = params.space;
                configureSpace();
                computeTargets();
                cur.length = target.length;
                cur.gainLow = target.gainLow;
                cur.gainHigh = target.gainHigh;
                for (auto& l : lines) l.clear();
                for (auto& step : diffuserLines)
                    for (auto& l : step)
                        l.clear();
                shifterL.clear();
                shifterR.clear();
                feedL = feedR = 0.0f;
            }
        }
        else if (spaceFade < 1.0f)
        {
            spaceFade = std::min (1.0f, spaceFade + fadeStep);
        }
    }

    void smoothAll()
    {
        const float c = smoothCoeff, s = slowCoeff;
        cur.dry += c * (target.dry - cur.dry);
        cur.wet += c * (target.wet - cur.wet);
        cur.input += c * (target.input - cur.input);
        cur.preDelay += s * (target.preDelay - cur.preDelay);
        cur.modDepth += c * (target.modDepth - cur.modDepth);
        cur.sizeScale += s * (target.sizeScale - cur.sizeScale);
        cur.earlyLevel += c * (target.earlyLevel - cur.earlyLevel);
        cur.lateLevel += c * (target.lateLevel - cur.lateLevel);
        cur.feedNorm += c * (target.feedNorm - cur.feedNorm);
        cur.shimmerGain += c * (target.shimmerGain - cur.shimmerGain);

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            cur.gainLow[i] += c * (target.gainLow[i] - cur.gainLow[i]);
            cur.gainHigh[i] += c * (target.gainHigh[i] - cur.gainHigh[i]);
        }
    }

    float sampleRate = 48000.0f;
    Parameters params;
    Space activeSpace = Space::hall;
    Smoothed target, cur;
    float smoothCoeff = 0.001f, slowCoeff = 0.0001f, fadeStep = 0.001f, spaceFade = 1.0f;
    float crossoverCoeff = 0.3f, lastLowCut = -1.0f, meanLengthSeconds = 0.05f;
    float feedL = 0.0f, feedR = 0.0f;

    std::array<DelayLine, numLines> lines;
    std::array<float, numLines> baseLength {}, crossover {}, lfoPhase {}, lfoPhase2 {}, lfoInc {}, lfoInc2 {};

    int diffuseSteps = 4;
    std::array<std::array<DelayLine, numDiffuseChannels>, maxDiffuseSteps> diffuserLines;
    std::array<std::array<int, numDiffuseChannels>, maxDiffuseSteps> diffuserDelay {};
    std::array<std::array<float, numDiffuseChannels>, maxDiffuseSteps> diffuserFlip {};

    std::array<float, numEarlyTaps> earlyTimeL {}, earlyTimeR {}, earlyGainL {}, earlyGainR {};
    OnePole earlyLPL, earlyLPR;

    DelayLine preDelayL, preDelayR;
    PitchShifter shifterL, shifterR;
    HighPass shimmerHP, shimmerHP2, outHPL, outHPR;
    OnePole shimmerLP, shimmerLP2;
};

} // namespace shimmer
