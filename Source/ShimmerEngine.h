// 24 Shimmers - shimmer reverb DSP engine.
//
// Plain C++ (no JUCE) so it can be unit-tested on its own.
//
// Signal flow:
//   input -> pre-delay -> input diffusers -> 8-line feedback delay network
//   Each line feedback path: modulated delay -> two-band decay (tone)
//   -> shimmer crossfade (two granular pitch shifters) -> Hadamard mix -> back in.
//   Because the pitch shifters sit inside the network, every trip round the
//   loop pushes part of the tail up by the chosen interval - the "shimmer".

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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
struct Allpass
{
    DelayLine line;
    int length = 1;
    float gain = 0.6f;

    void prepare (int len, float g) { length = std::max (1, len); gain = g; line.prepare (length + 2); }

    float process (float x)
    {
        const float delayed = line.tap (length);
        const float w = x + gain * delayed;
        line.push (w);
        return delayed - gain * w;
    }
};

//==============================================================================
// Two-tap granular pitch shifter. Taps sweep through a short window and are
// crossfaded with complementary sin^2 windows, so output amplitude never
// exceeds input amplitude (important: this runs inside a feedback loop).
class PitchShifter
{
public:
    void prepare (int windowSamples, float startPhase)
    {
        window = (float) windowSamples;
        line.prepare (windowSamples + 8);
        phase = startPhase;
    }

    void setSemitones (float semitones) { ratio = std::pow (2.0f, semitones / 12.0f); }
    void clear()                        { line.clear(); }

    float process (float x)
    {
        line.push (x);

        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        float phase2 = phase + 0.5f;
        if (phase2 >= 1.0f)
            phase2 -= 1.0f;

        const float s1 = std::sin (pi * phase);
        const float s2 = std::sin (pi * phase2);

        return s1 * s1 * line.tapCubic (2.0f + phase * window)
             + s2 * s2 * line.tapCubic (2.0f + phase2 * window);
    }

private:
    DelayLine line;
    float window = 2048.0f, phase = 0.0f, ratio = 2.0f;
};

//==============================================================================
struct Parameters
{
    float mix = 0.35f;          // 0..1
    float decaySeconds = 7.0f;  // RT60 of the low band
    float size = 0.7f;          // 0..1
    float preDelayMs = 20.0f;   // 0..500
    float shimmer = 0.5f;       // 0..1
    float shift1 = 12.0f;       // semitones
    float shift2 = 19.0f;       // semitones
    float blend = 0.3f;         // 0 = shift 1 only, 1 = shift 2 only
    float tone = 0.55f;         // 0 = dark .. 1 = bright
    float lowCutHz = 120.0f;
    float modulation = 0.35f;   // 0..1
    bool freeze = false;
};

//==============================================================================
class Engine
{
public:
    static constexpr int numLines = 8;

    void prepare (double sampleRateIn)
    {
        sampleRate = (float) sampleRateIn;

        const float maxLineMs = baseLengthsMs[numLines - 1] * maxSizeScale;
        const int maxLineSamples = (int) std::ceil (maxLineMs * 0.001f * sampleRate + maxModDepth() + 8.0f);

        for (int i = 0; i < numLines; ++i)
        {
            lines[(size_t) i].prepare (maxLineSamples);
            shiftA[(size_t) i].prepare ((int) (0.05f * sampleRate), (float) i / numLines);
            shiftB[(size_t) i].prepare ((int) (0.043f * sampleRate), std::fmod ((float) i / numLines + 0.37f, 1.0f));
            lfoPhase[(size_t) i] = std::fmod (0.137f * (float) i, 1.0f);
            lfoInc[(size_t) i] = lfoRatesHz[(size_t) i] / sampleRate;
        }

        preDelayL.prepare ((int) (0.5f * sampleRate) + 8);
        preDelayR.prepare ((int) (0.5f * sampleRate) + 8);

        for (int i = 0; i < 4; ++i)
        {
            diffuserL[(size_t) i].prepare ((int) (diffuserMsL[(size_t) i] * 0.001f * sampleRate), diffuserGains[(size_t) i]);
            diffuserR[(size_t) i].prepare ((int) (diffuserMsR[(size_t) i] * 0.001f * sampleRate), diffuserGains[(size_t) i]);
        }

        crossoverCoeff = 1.0f - std::exp (-twoPi * 3000.0f / sampleRate);
        smoothCoeff = 1.0f - std::exp (-1.0f / (0.03f * sampleRate));
        sizeSmoothCoeff = 1.0f - std::exp (-1.0f / (0.25f * sampleRate));

        lastLowCut = -1.0f;
        computeTargets();
        snapToTargets();
        reset();
    }

    void reset()
    {
        for (int i = 0; i < numLines; ++i)
        {
            lines[(size_t) i].clear();
            shiftA[(size_t) i].clear();
            shiftB[(size_t) i].clear();
            crossover[(size_t) i] = 0.0f;
            shimmerLP[(size_t) i].reset();
            shimmerHP[(size_t) i].reset();
        }

        preDelayL.clear();
        preDelayR.clear();

        for (auto& d : diffuserL) d.line.clear();
        for (auto& d : diffuserR) d.line.clear();

        outHPL.reset();
        outHPR.reset();
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
            smoothAll();

            const float dryL = left[n], dryR = right[n];

            preDelayL.push (dryL);
            preDelayR.push (dryR);
            float xL = preDelayL.tapLinear (1.0f + cur.preDelay);
            float xR = preDelayR.tapLinear (1.0f + cur.preDelay);

            for (auto& d : diffuserL) xL = d.process (xL);
            for (auto& d : diffuserR) xR = d.process (xR);

            xL *= cur.input * inputScale;
            xR *= cur.input * inputScale;

            std::array<float, numLines> y;

            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] >= 1.0f)
                    lfoPhase[i] -= 1.0f;

                const float mod = cur.modDepth * (0.5f + 0.5f * std::sin (twoPi * lfoPhase[i]));
                float v = lines[i].tapCubic (cur.length[i] + mod);

                // Two-band decay: lows decay at RT60, highs faster depending on tone.
                crossover[i] += crossoverCoeff * (v - crossover[i]);
                v = cur.gainLow[i] * crossover[i] + cur.gainHigh[i] * (v - crossover[i]);

                // Shimmer: crossfade part of the line towards its pitch-shifted copy.
                float shifted = cur.blendA * shiftA[i].process (v) + cur.blendB * shiftB[i].process (v);
                shifted = shimmerHP[i].highpass (shimmerLP[i].lowpass (shifted));
                y[i] = cur.dryLine[i] * v + cur.shimmerLine[i] * shifted;
            }

            const float wetL = 0.5f * (y[0] - y[2] + y[4] - y[6]);
            const float wetR = 0.5f * (y[1] - y[3] + y[5] - y[7]);

            hadamard (y);

            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                const float in = (i & 1) == 0 ? xL : xR;
                lines[i].push (softLimit (y[i] + inputSigns[i] * in));
            }

            const float outL = outHPL.process (wetL);
            const float outR = outHPR.process (wetR);

            left[n] = cur.dry * dryL + cur.wet * outL;
            right[n] = cur.dry * dryR + cur.wet * outR;
        }
    }

private:
    struct Smoothed
    {
        float dry = 1, wet = 0, input = 1, preDelay = 0, modDepth = 0;
        float blendA = 1, blendB = 0;
        std::array<float, numLines> length {}, gainLow {}, gainHigh {}, dryLine {}, shimmerLine {};
    };

    static constexpr float maxSizeScale = 1.5f;
    static constexpr float maxShimmerShare = 0.92f;
    static constexpr float inputScale = 0.5f;
    static constexpr float outputScale = 1.6f;

    static constexpr std::array<float, numLines> baseLengthsMs { 41.1f, 47.9f, 55.7f, 63.1f, 70.3f, 77.9f, 86.2f, 94.7f };
    static constexpr std::array<float, numLines> lfoRatesHz    { 0.13f, 0.21f, 0.29f, 0.37f, 0.43f, 0.53f, 0.61f, 0.71f };
    static constexpr std::array<float, numLines> inputSigns    { 1, 1, -1, 1, 1, -1, -1, -1 };
    static constexpr std::array<float, 4> diffuserMsL   { 4.77f, 3.59f, 12.73f, 9.31f };
    static constexpr std::array<float, 4> diffuserMsR   { 4.91f, 3.37f, 13.13f, 8.89f };
    static constexpr std::array<float, 4> diffuserGains { 0.65f, 0.65f, 0.55f, 0.55f };

    float maxModDepth() const { return 0.001f * sampleRate; }

    static float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a < 2.0f)
            return x;
        return std::copysign (2.0f + std::tanh (a - 2.0f), x);
    }

    static void hadamard (std::array<float, numLines>& v)
    {
        for (size_t h = 1; h < (size_t) numLines; h <<= 1)
            for (size_t i = 0; i < (size_t) numLines; i += h << 1)
                for (size_t j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b;
                    v[j + h] = a - b;
                }

        constexpr float norm = 0.35355339059f; // 1 / sqrt(8)
        for (auto& x : v)
            x *= norm;
    }

    void computeTargets()
    {
        const auto& p = params;

        target.dry = std::cos (p.mix * 0.5f * pi);
        // Longer decays build up more energy; pull the wet level down to compensate.
        target.wet = std::sin (p.mix * 0.5f * pi) * outputScale / std::pow (std::max (0.5f, p.decaySeconds), 0.25f);
        target.input = p.freeze ? 0.0f : 1.0f;
        target.preDelay = std::clamp (p.preDelayMs, 0.0f, 500.0f) * 0.001f * sampleRate;
        target.modDepth = std::clamp (p.modulation, 0.0f, 1.0f) * maxModDepth();


        const float blend = std::clamp (p.blend, 0.0f, 1.0f);
        target.blendA = std::cos (blend * 0.5f * pi);
        target.blendB = std::sin (blend * 0.5f * pi);

        const float sizeScale = 0.35f + (maxSizeScale - 0.35f) * std::clamp (p.size, 0.0f, 1.0f);
        const float rt60 = std::max (0.1f, p.decaySeconds);
        const float hfRatio = 0.08f + 0.92f * std::pow (std::clamp (p.tone, 0.0f, 1.0f), 1.5f);

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            const float len = baseLengthsMs[i] * sizeScale * 0.001f * sampleRate;
            target.length[i] = len;

            if (p.freeze)
            {
                target.gainLow[i] = 0.99999f;
                target.gainHigh[i] = 0.99999f;
                target.dryLine[i] = 1.0f;
                target.shimmerLine[i] = 0.0f;
            }
            else
            {
                const float gLow = std::pow (10.0f, -3.0f * len / (rt60 * sampleRate));
                const float gHigh = std::pow (10.0f, -3.0f * len / (rt60 * hfRatio * sampleRate));

                // Shimmer only diverts energy the tail would lose anyway on this pass
                // (1 - gLow^2), so the unshifted tail still decays at the set RT60 and
                // each octave up receives a share of the one below it.
                const float shifted = maxShimmerShare * std::clamp (p.shimmer, 0.0f, 1.0f) * (1.0f - gLow * gLow);
                const float keep = std::sqrt (1.0f - shifted);

                target.shimmerLine[i] = std::sqrt (shifted);
                target.dryLine[i] = keep;
                target.gainLow[i] = gLow / keep;
                target.gainHigh[i] = gHigh / keep;
            }
        }

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            shiftA[i].setSemitones (p.shift1);
            shiftB[i].setSemitones (p.shift2);
        }

        if (std::abs (p.lowCutHz - lastLowCut) > 1.0e-3f)
        {
            lastLowCut = p.lowCutHz;
            const float hz = std::clamp (p.lowCutHz, 10.0f, 0.45f * sampleRate);
            outHPL.setCutoff (hz, sampleRate);
            outHPR.setCutoff (hz, sampleRate);

            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                shimmerHP[i].setCutoff (std::max (hz, 60.0f), sampleRate);
                shimmerLP[i].setCutoff (std::min (10000.0f, 0.45f * sampleRate), sampleRate);
            }
        }
    }

    void snapToTargets() { cur = target; }

    void smoothAll()
    {
        const float c = smoothCoeff;
        cur.dry += c * (target.dry - cur.dry);
        cur.wet += c * (target.wet - cur.wet);
        cur.input += c * (target.input - cur.input);
        cur.preDelay += sizeSmoothCoeff * (target.preDelay - cur.preDelay);
        cur.modDepth += c * (target.modDepth - cur.modDepth);
        cur.blendA += c * (target.blendA - cur.blendA);
        cur.blendB += c * (target.blendB - cur.blendB);

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            cur.length[i] += sizeSmoothCoeff * (target.length[i] - cur.length[i]);
            cur.gainLow[i] += c * (target.gainLow[i] - cur.gainLow[i]);
            cur.gainHigh[i] += c * (target.gainHigh[i] - cur.gainHigh[i]);
            cur.dryLine[i] += c * (target.dryLine[i] - cur.dryLine[i]);
            cur.shimmerLine[i] += c * (target.shimmerLine[i] - cur.shimmerLine[i]);
        }
    }

    float sampleRate = 48000.0f;
    Parameters params;
    Smoothed target, cur;
    float smoothCoeff = 0.001f, sizeSmoothCoeff = 0.0001f, crossoverCoeff = 0.3f, lastLowCut = -1.0f;

    std::array<DelayLine, numLines> lines;
    std::array<PitchShifter, numLines> shiftA, shiftB;
    std::array<OnePole, numLines> shimmerLP, shimmerHP;
    std::array<float, numLines> crossover {}, lfoPhase {}, lfoInc {};

    DelayLine preDelayL, preDelayR;
    std::array<Allpass, 4> diffuserL, diffuserR;
    HighPass outHPL, outHPR;
};

} // namespace shimmer
