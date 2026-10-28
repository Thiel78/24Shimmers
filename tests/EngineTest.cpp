// Offline checks for the shimmer engine: decay time, loudness, echo density,
// that the shimmer really moves energy by an octave, freeze, stability and CPU.
// Exits non-zero if a check fails.

#include "../Source/ShimmerEngine.h"

#include <chrono>
#include <cstdio>
#include <random>

namespace
{
constexpr double sr = 48000.0;
int failures = 0;

const char* spaceName (shimmer::Space s)
{
    return s == shimmer::Space::room ? "room" : s == shimmer::Space::hall ? "hall" : "cathedral";
}

constexpr shimmer::Space allSpaces[] { shimmer::Space::room, shimmer::Space::hall, shimmer::Space::cathedral };

void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (! ok)
        ++failures;
}

float db (double x) { return (float) (20.0 * std::log10 (std::max (x, 1e-12))); }

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n) : l (n, 0.0f), r (n, 0.0f) {}
};

void run (shimmer::Engine& e, Stereo& s, int block = 256)
{
    for (size_t pos = 0; pos < s.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, s.l.size() - pos);
        e.process (s.l.data() + pos, s.r.data() + pos, n);
    }
}

double rms (const Stereo& s, size_t from, size_t to)
{
    double acc = 0;
    for (size_t i = from; i < to; ++i)
        acc += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i];
    return std::sqrt (acc / (2.0 * (double) (to - from)));
}

// Energy of a tone in a stretch of the left channel (Goertzel).
double toneLevel (const Stereo& s, size_t from, size_t to, double hz)
{
    const double k = 2.0 * std::cos (2.0 * 3.141592653589793 * hz / sr);
    double total = 0;
    const size_t win = 4800;
    for (size_t start = from; start + win <= to; start += win)
    {
        double s1 = 0, s2 = 0;
        for (size_t i = start; i < start + win; ++i)
        {
            const double v = s.l[i] + k * s1 - s2;
            s2 = s1;
            s1 = v;
        }
        total += s1 * s1 + s2 * s2 - k * s1 * s2;
    }
    return total;
}

// Normalised echo density (Abel & Huang): ~1 means the response is as dense
// as Gaussian noise, low values mean distinct echoes (heard as metallic/grainy).
double echoDensity (const std::vector<float>& h, size_t centre)
{
    const size_t half = (size_t) (0.01 * sr);
    double var = 0;
    for (size_t i = centre - half; i < centre + half; ++i)
        var += (double) h[i] * h[i];
    const double sd = std::sqrt (var / (double) (2 * half));
    size_t outside = 0;
    for (size_t i = centre - half; i < centre + half; ++i)
        outside += std::abs (h[i]) > sd ? 1 : 0;
    return (double) outside / (double) (2 * half) / 0.3173;
}

shimmer::Parameters wetOnly (shimmer::Space space = shimmer::Space::hall)
{
    shimmer::Parameters p;
    p.space = space;
    p.mix = 1.0f;
    return p;
}

void testDecay()
{
    std::puts ("Decay time (body of the tail)");

    for (auto space : allSpaces)
        for (float decay : { 2.0f, 8.0f })
        {
            shimmer::Engine e;
            auto p = wetOnly (space);
            p.shimmer = 0.0f;
            p.decaySeconds = decay;
            p.preDelayMs = 0.0f;
            e.setParameters (p);
            e.prepare (sr);

            Stereo s ((size_t) (sr * (decay + 1.5)));
            s.l[0] = s.r[0] = 1.0f;
            run (e, s);

            const size_t win = (size_t) (0.25 * sr);
            const size_t from = (size_t) (sr * 0.5), to = (size_t) (sr * (0.5 + decay * 0.6));
            const double slope = (db (rms (s, from, from + win)) - db (rms (s, to, to + win))) / ((double) (to - from) / sr);
            const double rt60 = 60.0 / slope;
            std::printf ("    %-9s decay %4.1f s -> RT60 %5.2f s\n", spaceName (space), decay, rt60);
            check (rt60 > decay * 0.6 && rt60 < decay * 1.4, "RT60 within 40% of setting");
        }
}

void testLoudness()
{
    std::puts ("Loudness (noise in, wet only)");
    std::mt19937 rng (1);
    std::normal_distribution<float> noise (0.0f, 0.1f);

    for (auto space : allSpaces)
        for (float decay : { 1.0f, 6.0f, 30.0f })
        {
            shimmer::Engine e;
            auto p = wetOnly (space);
            p.decaySeconds = decay;
            p.shimmer = 0.0f;
            e.setParameters (p);
            e.prepare (sr);

            Stereo s ((size_t) (sr * 8));
            for (size_t i = 0; i < s.l.size(); ++i) { s.l[i] = noise (rng); s.r[i] = noise (rng); }
            run (e, s);
            const float gain = db (rms (s, (size_t) (sr * 6), s.l.size()) / 0.1);
            std::printf ("    %-9s decay %4.1f s -> wet/dry %+5.1f dB\n", spaceName (space), decay, gain);
            check (gain > -15.0f && gain < 6.0f, "wet level in a sane range");
        }
}

void testDensity()
{
    std::puts ("Echo density of the impulse response (1.0 = smooth, noise-like)");

    for (auto space : allSpaces)
    {
        shimmer::Engine e;
        auto p = wetOnly (space);
        p.shimmer = 0.0f;
        p.preDelayMs = 0.0f;
        p.lowCutHz = 20.0f;
        e.setParameters (p);
        e.prepare (sr);

        Stereo s ((size_t) (sr * 1));
        s.l[0] = s.r[0] = 1.0f;
        run (e, s);

        const double d100 = echoDensity (s.l, (size_t) (0.1 * sr));
        const double d300 = echoDensity (s.l, (size_t) (0.3 * sr));
        std::printf ("    %-9s at 100 ms %.2f, at 300 ms %.2f\n", spaceName (space), d100, d300);
        check (d300 > 0.85, "dense by 300 ms");
    }
}

void testShimmerOctaves()
{
    std::puts ("Shimmer moves energy by the chosen octave");

    struct Case { int octave; double target; } cases[] { { 1, 880.0 }, { 2, 1760.0 }, { -1, 220.0 } };

    for (auto c : cases)
    {
        double ratio[2];
        for (int k = 0; k < 2; ++k)
        {
            shimmer::Engine e;
            auto p = wetOnly();
            p.shimmer = k == 0 ? 0.0f : 0.8f;
            p.octave = c.octave;
            p.decaySeconds = 10.0f;
            p.lowCutHz = 40.0f;
            e.setParameters (p);
            e.prepare (sr);

            Stereo s ((size_t) (sr * 5));
            for (size_t i = 0; i < (size_t) (sr * 0.5); ++i)
                s.l[i] = s.r[i] = 0.3f * (float) std::sin (2.0 * 3.141592653589793 * 440.0 * (double) i / sr);
            run (e, s);

            const double f0 = toneLevel (s, (size_t) (sr * 2.5), (size_t) (sr * 4.5), 440.0);
            const double f1 = toneLevel (s, (size_t) (sr * 2.5), (size_t) (sr * 4.5), c.target);
            ratio[k] = f1 / f0;
        }
        std::printf ("    octave %+d: %4.0f Hz vs 440 Hz in tail, off %+6.1f dB, on %+6.1f dB\n",
                     c.octave, c.target, 10.0 * std::log10 (ratio[0]), 10.0 * std::log10 (ratio[1]));
        check (ratio[1] > ratio[0] * 100.0, "shifted content at least 20 dB stronger with shimmer");
    }
}

void testShimmerBounded()
{
    std::puts ("Full shimmer stays bounded and keeps a tail");
    std::mt19937 rng (4);
    std::normal_distribution<float> noise (0.0f, 0.1f);

    for (int octave : { 1, 2, -1 })
        for (auto space : allSpaces)
        {
            shimmer::Engine e;
            auto p = wetOnly (space);
            p.shimmer = 1.0f;
            p.octave = octave;
            p.decaySeconds = 60.0f;
            p.tone = 1.0f;
            p.size = 1.0f;
            p.lowCutHz = 20.0f;
            e.setParameters (p);
            e.prepare (sr);

            Stereo s ((size_t) (sr * 30));
            for (size_t i = 0; i < (size_t) (sr * 4); ++i) { s.l[i] = noise (rng); s.r[i] = noise (rng); }
            run (e, s);

            const double during = rms (s, (size_t) (sr * 2), (size_t) (sr * 4));
            double peakLate = 0;
            for (int sec = 5; sec <= 28; sec += 2)
                peakLate = std::max (peakLate, rms (s, (size_t) (sr * sec), (size_t) (sr * (sec + 2))));
            const double end = rms (s, (size_t) (sr * 26), (size_t) (sr * 30));

            std::printf ("    octave %+d %-9s: while playing %5.1f dB, loudest later %5.1f dB, at 26-30 s %6.1f dB\n",
                         octave, spaceName (space), db (during), db (peakLate), db (end));
            check (peakLate < during * 2.0, "no runaway build-up (< +6 dB over the input phase)");
        }
}

void testFreeze()
{
    std::puts ("Freeze holds the tail");
    shimmer::Engine e;
    auto p = wetOnly();
    p.decaySeconds = 3.0f;
    e.setParameters (p);
    e.prepare (sr);

    std::mt19937 rng (2);
    std::normal_distribution<float> noise (0.0f, 0.1f);

    Stereo a ((size_t) (sr * 2));
    for (size_t i = 0; i < a.l.size(); ++i) { a.l[i] = noise (rng); a.r[i] = noise (rng); }
    run (e, a);

    p.freeze = true;
    e.setParameters (p);

    Stereo b ((size_t) (sr * 20));
    run (e, b);

    const double early = rms (b, (size_t) (sr * 1), (size_t) (sr * 2));
    const double late = rms (b, (size_t) (sr * 18), (size_t) (sr * 20));
    std::printf ("    frozen level after 1 s %.1f dB, after 18 s %.1f dB\n", db (early), db (late));
    check (late > early * 0.5 && late < early * 2.0, "frozen tail stays within 6 dB over 17 s");
}

void testStress()
{
    std::puts ("Stress: extreme and randomly automated settings");
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    std::normal_distribution<float> noise (0.0f, 0.25f);

    float worst = 0.0f;
    bool finite = true;

    for (int trial = 0; trial < 12; ++trial)
    {
        shimmer::Engine e;
        e.prepare (trial % 2 == 0 ? 44100.0 : 96000.0);

        for (int seg = 0; seg < 30; ++seg)
        {
            shimmer::Parameters p;
            const bool extreme = trial < 4;
            p.space = allSpaces[rng() % 3];
            p.mix = uni (rng);
            p.decaySeconds = extreme ? 60.0f : 0.3f + 59.7f * uni (rng) * uni (rng);
            p.size = extreme ? (float) (trial & 1) : uni (rng);
            p.preDelayMs = 500.0f * uni (rng);
            p.shimmer = extreme ? 1.0f : uni (rng);
            p.octave = (int[]) { -1, 1, 2 }[rng() % 3];
            p.tone = extreme ? 1.0f : uni (rng);
            p.lowCutHz = 20.0f + 980.0f * uni (rng) * uni (rng);
            p.modulation = uni (rng);
            p.freeze = uni (rng) < 0.15f;
            e.setParameters (p);

            Stereo s ((size_t) (sr * 1.0));
            const bool loud = uni (rng) < 0.5f;
            for (size_t i = 0; i < s.l.size(); ++i)
            {
                s.l[i] = loud ? std::clamp (noise (rng), -1.0f, 1.0f) : 0.0f;
                s.r[i] = loud ? std::clamp (noise (rng), -1.0f, 1.0f) : 0.0f;
            }
            run (e, s, 1 + (int) (rng() % 1024));

            for (size_t i = 0; i < s.l.size(); ++i)
            {
                finite = finite && std::isfinite (s.l[i]) && std::isfinite (s.r[i]);
                worst = std::max ({ worst, std::abs (s.l[i]), std::abs (s.r[i]) });
            }
        }
    }

    std::printf ("    worst peak %.2f (%.1f dBFS)\n", worst, db (worst));
    check (finite, "no NaN / Inf");
    check (worst < 8.0f, "output stays bounded");
}

void testCpu()
{
    std::puts ("CPU");
    shimmer::Engine e;
    e.setParameters (wetOnly (shimmer::Space::cathedral));
    e.prepare (sr);
    Stereo s ((size_t) (sr * 20));
    s.l[0] = 1.0f;

    const auto t0 = std::chrono::steady_clock::now();
    run (e, s, 512);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("    20 s of stereo audio in %.3f s (%.1f%% of one core)\n", secs, 100.0 * secs / 20.0);
    check (secs < 4.0, "under 20% of a core");
}
} // namespace

int main()
{
    testDecay();
    testLoudness();
    testDensity();
    testShimmerOctaves();
    testShimmerBounded();
    testFreeze();
    testStress();
    testCpu();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
