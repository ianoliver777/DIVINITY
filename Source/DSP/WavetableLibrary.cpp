// DIVINITY 0.7.0 wavetable library: 82 tables generated in code.
//
// Every table is a 32-frame morph built from one of ~20 synthesis families
// (phase distortion, hard sync, folding, drive, PWM, FM, crushing, Chebyshev,
// ring mod, formants, combs, tilts, resonant peaks, registrations, tuned
// percussion partials, seeded spectral chaos). The family decides the *kind*
// of harmonic movement; the parameters give each table its own character.
// Nothing here is a filtered copy of another table.
#include "Wavetables.h"
#include <cmath>

namespace divinity
{
namespace
{
    constexpr int N = 2048, Half = 1024;
    constexpr double kPi = juce::MathConstants<double>::pi, kTwoPi = 2.0 * kPi;

    enum Fam
    {
        PD, PDReso, PDPulse, Sync, Fold, Drive, Pwm, PwmDouble, Fm, FmFeedback, Crush, Hold,
        Cheby, Ring, Morph, Formant, Comb, Tilt, Peak, TwinPeak, Registration, Partials,
        Stack, Chaos, FmFold, FormantDrive, CrushFm, SevenHeaven
    };

    struct Def { const char* name; Fam fam; double a, b, c; int wave; };

    // wave: 0 sine, 1 triangle, 2 saw, 3 square, 4 pulse 10%
    const Def defs[] = {
        { "Phase Warp",      PD,        0.0, 0, 0, 0 },
        { "Reso PD",         PDReso,    12, 0, 0, 0 },
        { "CZ Pulse",        PDPulse,   0, 0, 0, 0 },
        { "Sync Saw",        Sync,      8, 0, 0, 2 },
        { "Sync Square",     Sync,      6, 0, 0, 3 },
        { "Sync Sine",       Sync,      10, 0, 0, 0 },
        { "Sync Tri",        Sync,      7, 0, 0, 1 },
        { "Laser Sync",      Sync,      16, 0, 0, 2 },
        { "Sine Fold",       Fold,      8, 0.0, 0, 0 },
        { "Tri Fold",        Fold,      6, 0.0, 0, 1 },
        { "Saw Fold",        Fold,      5, 0.0, 0, 2 },
        { "Bias Fold",       Fold,      7, 0.35, 0, 0 },
        { "Tube Saw",        Drive,     8, 0.2, 0, 2 },
        { "Fuzz Tri",        Drive,     14, 0.4, 0, 1 },
        { "Overdrive Sine",  Drive,     16, 0.0, 0, 0 },
        { "Diode Clip",      Drive,     10, 0.55, 1, 0 },
        { "PWM Sweep",       Pwm,       0.5, 0.02, 0, 4 },
        { "PWM Wide",        Pwm,       0.5, 0.25, 0, 4 },
        { "PWM Stack",       PwmDouble, 0, 0, 0, 4 },
        { "Hollow Pulse",    PwmDouble, 1, 0, 0, 4 },
        { "Bell FM",         Fm,        3, 6, 0, 0 },
        { "Electric FM",     Fm,        1, 8, 0, 0 },
        { "Growl FM",        Fm,        2, 10, 0, 0 },
        { "Metal FM",        Fm,        7, 5, 0, 0 },
        { "Glass FM",        Fm,        5, 3, 0, 0 },
        { "Wood FM",         Fm,        4, 2.2, 0, 0 },
        { "Sub FM",          Fm,        0.5, 4, 0, 0 },
        { "Feedback FM",     FmFeedback, 1.6, 0, 0, 0 },
        { "8-Bit Crush",     Crush,     8, 2, 0, 2 },
        { "Crushed Sine",    Crush,     7, 1.5, 0, 0 },
        { "Lo-Fi Hold",      Hold,      64, 5, 0, 2 },
        { "Arcade Steps",    Hold,      32, 3, 0, 1 },
        { "Cheby Shine",     Cheby,     1, 0, 0, 0 },
        { "Cheby Even",      Cheby,     2, 0, 0, 0 },
        { "Ring Bells",      Ring,      1, 3, 9, 0 },
        { "Ring Hollow",     Ring,      2, 5, 11, 0 },
        { "Saw To Square",   Morph,     2, 3, 0, 0 },
        { "Tri To Saw",      Morph,     1, 2, 0, 0 },
        { "Sine To Pulse",   Morph,     0, 4, 0, 0 },
        { "Vowel Sweep",     Formant,   0, 220, 90, 0 },
        { "Ooh To Ahh",      Formant,   1, 220, 90, 0 },
        { "Choir Formant",   Formant,   2, 220, 160, 0 },
        { "Talk Box",        Formant,   3, 165, 70, 0 },
        { "Yoi",             Formant,   4, 130, 80, 0 },
        { "Robot Voice",     Formant,   5, 165, 90, 1 },
        { "Throat Singer",   Formant,   6, 110, 60, 0 },
        { "Comb Sweep",      Comb,      2, 12, 0.7, 0 },
        { "Comb Shimmer",    Comb,      3, 9, 0.35, 0 },
        { "Odd Harmonics",   Tilt,      2.0, 0.5, 1, 0 },
        { "Even Harmonics",  Tilt,      1.8, 0.6, 2, 0 },
        { "Dark To Bright",  Tilt,      3.0, 0.6, 0, 0 },
        { "Air Tilt",        Tilt,      0.9, -0.25, 0, 0 },
        { "Peak Sweep",      Peak,      1, 48, 0.12, 0 },
        { "Wide Peak",       Peak,      1, 32, 0.45, 0 },
        { "Twin Peaks",      TwinPeak,  0, 0, 0, 0 },
        { "Organ Jazz",      Registration, 0, 0, 0, 0 },
        { "Organ Gospel",    Registration, 1, 0, 0, 0 },
        { "Pipe Organ",      Registration, 2, 0, 0, 0 },
        { "Church Reed",     Registration, 3, 0, 0, 0 },
        { "Music Box",       Partials,  0, 0, 0, 0 },
        { "Kalimba",         Partials,  1, 0, 0, 0 },
        { "Mallet",          Partials,  2, 0, 0, 0 },
        { "E-Piano Tine",    Partials,  3, 0, 0, 0 },
        { "Clavi",           Partials,  4, 0, 0, 0 },
        { "Brass Swell",     Partials,  5, 0, 0, 0 },
        { "String Ensemble", Partials,  6, 0, 0, 0 },
        { "Breathy Flute",   Partials,  7, 0, 0, 0 },
        { "Clarinet",        Partials,  8, 0, 0, 0 },
        { "Nasal Reed",      Partials,  9, 0, 0, 0 },
        { "808 Body",        Partials,  10, 0, 0, 0 },
        { "Reese Core",      Stack,     0, 0, 0, 0 },
        { "Saw Stack",       Stack,     1, 0, 0, 0 },
        { "Square Stack",    Stack,     2, 0, 0, 0 },
        { "Neuro Growl",     FmFold,    2, 6, 4, 0 },
        { "Wobble Talk",     FormantDrive, 4, 0, 0, 0 },
        { "Digital Grit",    CrushFm,   3, 0, 0, 0 },
        { "Chaos I",         Chaos,     1, 0, 0, 0 },
        { "Chaos II",        Chaos,     2, 0, 0, 0 },
        { "Chaos III",       Chaos,     3, 0, 0, 0 },
        { "Chaos IV",        Chaos,     4, 0, 0, 0 },
        { "Crystal",         Partials,  11, 0, 0, 0 },
        { "Seventh Heaven",  SevenHeaven, 0, 0, 0, 0 },
    };
    constexpr int kCount = (int) (sizeof (defs) / sizeof (defs[0]));

    //--------------------------------------------------------------------------
    WaveSpectrum empty() { return WaveSpectrum ((size_t) Half + 1); }
    void setSine (WaveSpectrum& s, int k, double b) { if (k > 0 && k <= Half) s[(size_t) k] = { 0.0, -b }; }
    void addSine (WaveSpectrum& s, int k, double b) { if (k > 0 && k <= Half) s[(size_t) k] += std::complex<double> (0.0, -b); }

    WaveSpectrum fromWave (const std::vector<double>& w)
    {
        juce::dsp::FFT fft (11);
        std::vector<juce::dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
        double mean = 0; for (double x : w) mean += x; mean /= N;
        for (int i = 0; i < N; ++i) in[(size_t) i] = { (float) (w[(size_t) i] - mean), 0.0f };
        fft.perform (in.data(), out.data(), false);
        WaveSpectrum s ((size_t) Half + 1);
        for (int k = 1; k <= Half; ++k) s[(size_t) k] = { out[(size_t) k].real(), out[(size_t) k].imag() };
        return s;
    }

    template <typename F> WaveSpectrum sampled (F f)
    {
        std::vector<double> w ((size_t) N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = f ((double) i / N);
        return fromWave (w);
    }

    /** Naive (not band-limited) basic waves; the mipmaps band-limit the result. */
    double basic (int wave, double p)
    {
        p -= std::floor (p);
        switch (wave)
        {
            case 0:  return std::sin (kTwoPi * p);
            case 1:  return p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p;
            case 2:  return 2.0 * p - 1.0;
            case 3:  return p < 0.5 ? 1.0 : -1.0;
            default: return p < 0.1 ? 1.0 : -0.11;
        }
    }

    double gauss (double x, double c, double w) { const double d = (x - c) / w; return std::exp (-0.5 * d * d); }

    const double kVowels[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 300, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
    // vowel paths (indices into kVowels: A E I O U)
    const std::vector<int> kPaths[7] = { { 0, 1, 2, 3, 4 }, { 4, 3, 0 }, { 0, 3 }, { 0, 2, 0 }, { 4, 2 }, { 2, 0, 4 }, { 3, 0 } };

    WaveSpectrum formantSpectrum (int path, double t, double f0, double bw, bool oddOnly, double drive = 0.0)
    {
        const auto& pth = kPaths[(size_t) juce::jlimit (0, 6, path)];
        const double seg = t * (double) (pth.size() - 1);
        const int i = juce::jlimit (0, (int) pth.size() - 2, (int) seg);
        const double f = seg - i;
        WaveSpectrum s = empty();
        for (int k = 1; k <= Half; ++k)
        {
            if (oddOnly && (k % 2) == 0) continue;
            const double hz = k * f0;
            double env = 0.03;
            const double g[3] = { 1.0, 0.55, 0.3 };
            for (int fm = 0; fm < 3; ++fm)
            {
                const double centre = kVowels[pth[(size_t) i]][fm] * (1.0 - f) + kVowels[pth[(size_t) i + 1]][fm] * f;
                env += g[fm] * gauss (hz, centre, bw + fm * 45.0);
            }
            setSine (s, k, env / std::pow ((double) k, 0.4));
        }
        if (drive <= 0.0) return s;
        // drive: render, saturate, re-analyse
        std::vector<double> w ((size_t) N);
        juce::dsp::FFT fft (11);
        std::vector<juce::dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
        for (int k = 1; k < Half; ++k) { in[(size_t) k] = { (float) s[(size_t) k].real(), (float) s[(size_t) k].imag() }; in[(size_t) (N - k)] = std::conj (in[(size_t) k]); }
        fft.perform (in.data(), out.data(), true);
        double peak = 1e-9; for (auto& c : out) peak = juce::jmax (peak, (double) std::abs (c.real()));
        const double g = 1.0 + drive * t * 6.0;
        for (int n = 0; n < N; ++n) w[(size_t) n] = std::tanh (g * out[(size_t) n].real() / peak);
        return fromWave (w);
    }

    juce::Random seeded (int seed) { return juce::Random ((juce::int64) seed * 7919 + 777); }

    WaveSpectrum chaosSpectrum (int seed, double t)
    {
        // morph through four seeded random spectra (with 1/k^0.8 shaping so they stay musical)
        WaveSpectrum s = empty();
        const double seg = t * 3.0;
        const int a = juce::jlimit (0, 2, (int) seg);
        const double f = seg - a;
        for (int key = 0; key < 2; ++key)
        {
            auto r = seeded (seed * 10 + a + key);
            const double w = key == 0 ? 1.0 - f : f;
            const int maxK = 24 + r.nextInt (180);
            for (int k = 1; k <= maxK; ++k)
            {
                const double amp = (k == 1 ? 1.0 : r.nextDouble() * r.nextDouble()) / std::pow ((double) k, 0.8);
                const double ph = r.nextDouble() * kTwoPi;
                s[(size_t) k] += std::polar (amp * w, ph);
            }
        }
        return s;
    }

    WaveSpectrum partialsSpectrum (int kind, double t)
    {
        WaveSpectrum s = empty();
        auto put = [&] (int k, double a) { addSine (s, k, a); };
        switch (kind)
        {
            case 0:   // music box: fundamental + tines at 4 and 10, tines fade across the table
                put (1, 1.0); put (4, 0.6 * (1.0 - t * 0.8)); put (10, 0.35 * (1.0 - t)); put (3, 0.15 * t); break;
            case 1:   // kalimba: odd-ish tines, softer and woodier through the table
                put (1, 1.0); put (5, 0.5 * (1.0 - 0.7 * t)); put (9, 0.25 * (1.0 - t)); put (2, 0.2 * t); break;
            case 2:   // mallet
                for (int k : { 1, 4, 10, 20 }) put (k, std::pow (0.45 + 0.35 * (1.0 - t), std::log2 ((double) k) * 1.5)); break;
            case 3:   // e-piano tine: bell partials fade into a warm sine
                put (1, 1.0); put (2, 0.3); put (7, 0.35 * (1.0 - t)); put (14, 0.2 * (1.0 - t)); put (3, 0.12 * t); break;
            case 4:   // clavi: narrow pulse, brightness fades
                for (int k = 1; k <= 200; ++k) put (k, std::sin (kPi * k * 0.08) / k * std::exp (-k * (0.01 + 0.05 * t))); break;
            case 5:   // brass swell: saw spectrum whose top end opens across the table, formant near 1.2 kHz
                for (int k = 1; k <= 120; ++k) put (k, 1.0 / k * std::exp (-k / (3.0 + 30.0 * t)) * (1.0 + 0.8 * gauss (k * 220.0, 1200.0, 500.0))); break;
            case 6:   // string ensemble: saw with gentle tilt and alternating-harmonic shimmer
                for (int k = 1; k <= 160; ++k) put (k, 1.0 / std::pow ((double) k, 1.0 + 0.4 * (1.0 - t)) * (1.0 + 0.25 * std::sin (k * (1.3 + t)))); break;
            case 7:   // breathy flute: fundamental with weak overtones that grow
                put (1, 1.0); put (2, 0.15 + 0.2 * t); put (3, 0.08 + 0.15 * t); put (4, 0.04 * t); put (5, 0.03 * t); break;
            case 8:   // clarinet: odd harmonics, darker -> reedier
                for (int k = 1; k <= 60; k += 2) put (k, 1.0 / std::pow ((double) k, 1.3 - 0.5 * t)); break;
            case 9:   // nasal reed: resonance moving through harmonics 3 .. 9
                for (int k = 1; k <= 80; ++k) put (k, 1.0 / std::sqrt ((double) k) * (0.15 + gauss (k, 3.0 + 6.0 * t, 1.5))); break;
            case 10:  // 808 body: sine growing a little 2nd/3rd harmonic grit
                put (1, 1.0); put (2, 0.25 * t); put (3, 0.18 * t * t); put (5, 0.06 * t * t); break;
            default:  // crystal: sparse high partials climbing
                put (1, 0.8);
                for (int k : { 6, 11, 17, 23, 31, 41, 53 }) put (k, 0.4 * gauss (k, 6.0 + 50.0 * t, 10.0)); break;
        }
        return s;
    }

    WaveSpectrum registration (int set, double t)
    {
        static const double regs[4][2][9] = {
            { { 1, 0.8, 0.8, 0, 0, 0, 0, 0, 0 },    { 1, 0.8, 0.8, 0.6, 0.4, 0.5, 0, 0.3, 0.4 } },     // jazz: 888 -> full
            { { 0.6, 1, 1, 0.7, 0, 0.4, 0, 0, 0 },  { 1, 1, 1, 1, 0.6, 0.8, 0.5, 0.6, 0.7 } },         // gospel
            { { 1, 0, 0.5, 0, 0, 0, 0, 0, 0 },     { 1, 0.6, 0.8, 0.5, 0.3, 0.6, 0.2, 0.5, 0.4 } },   // pipe: flue -> plenum
            { { 0.7, 0.3, 0.9, 0.2, 0.7, 0.2, 0.5, 0.1, 0.4 }, { 1, 0.2, 1, 0.2, 1, 0.2, 0.9, 0.1, 0.8 } } }; // reed
        static const int harm[9] = { 1, 2, 3, 4, 5, 6, 8, 10, 12 };
        WaveSpectrum s = empty();
        for (int d = 0; d < 9; ++d)
            addSine (s, harm[d], (regs[set][0][d] * (1.0 - t) + regs[set][1][d] * t) / std::sqrt ((double) harm[d]));
        if (set == 3) for (int k = 13; k < 40; k += 2) addSine (s, k, 0.12 * t / std::sqrt ((double) k));
        return s;
    }

    WaveSpectrum stack (int kind, double t)
    {
        WaveSpectrum s = empty();
        for (int k = 1; k <= Half; ++k)
        {
            double a = 0;
            switch (kind)
            {
                case 0:  a = 1.0 / k + (k % 2 ? 0.7 * t / k : 0.0) + (k % 3 == 0 ? 0.3 / k : 0.0); break;   // reese core: saw + square + fifths
                case 1:  a = 1.0 / k + ((k % 2) == 0 ? 0.6 * t * 2.0 / k : 0.0) + (k % 3 == 0 ? 0.5 * t * 3.0 / k : 0.0); break; // saw + octave + 12th
                default: a = (k % 2 ? 1.0 / k : 0.0) + ((k % 2) == 0 && (k / 2) % 2 ? 0.6 * t * 2.0 / k : 0.0); break;          // squares stacked
            }
            setSine (s, k, a);
        }
        return s;
    }

    WaveSpectrum make (const Def& d, double t)
    {
        switch (d.fam)
        {
            case PD:
                return sampled ([t] (double p)
                {
                    const double dd = 0.5 - 0.48 * t;   // warp point: 0.5 = sine .. 0.02 = saw-like
                    const double w = p < dd ? p * 0.5 / dd : 0.5 + (p - dd) * 0.5 / (1.0 - dd);
                    return -std::cos (kTwoPi * w);
                });
            case PDReso:
                return sampled ([t, &d] (double p)
                {
                    const double k = 1.0 + t * (d.a - 1.0);     // resonance harmonic sweeps up
                    return std::cos (kTwoPi * p * k) * (1.0 - p);   // CZ resonant saw window
                });
            case PDPulse:
                return sampled ([t] (double p)
                {
                    const double dd = 0.25 - 0.22 * t;
                    double w;
                    if (p < dd) w = p * 0.5 / dd; else if (p < 0.5) w = 0.5; else if (p < 0.5 + dd) w = 0.5 + (p - 0.5) * 0.5 / dd; else w = 1.0;
                    return -std::cos (kTwoPi * w);
                });
            case Sync:
                return sampled ([t, &d] (double p) { const double r = 1.0 + t * (d.a - 1.0); return basic (d.wave, p * r); });
            case Fold:
                return sampled ([t, &d] (double p)
                {
                    const double g = 1.0 + t * d.a;
                    const double x = basic (d.wave, p) * 0.9 + d.b * t;
                    return std::sin (kPi * 0.5 * g * x);
                });
            case Drive:
                return sampled ([t, &d] (double p)
                {
                    const double g = 1.0 + t * d.a;
                    const double x = basic (d.wave, p) + d.b;
                    if (d.c > 0.5) return juce::jlimit (-1.0, 1.0 - 0.5 * d.b, g * x) - juce::jlimit (-1.0, 1.0 - 0.5 * d.b, g * d.b);   // hard diode-ish
                    return std::tanh (g * x) - std::tanh (g * d.b);
                });
            case Pwm:
                return sampled ([t, &d] (double p) { const double w = d.a + (d.b - d.a) * t; return p < w ? 1.0 : -1.0; });
            case PwmDouble:
                return sampled ([t, &d] (double p)
                {
                    const double w1 = 0.5 - 0.4 * t, w2 = 0.25 + 0.2 * t;
                    const double a = p < w1 ? 1.0 : -1.0;
                    const double b = std::fmod (p * 2.0, 1.0) < w2 ? 1.0 : -1.0;
                    return d.a > 0.5 ? a - b * 0.8 : 0.6 * a + 0.4 * b;
                });
            case Fm:
                return sampled ([t, &d] (double p) { return std::sin (kTwoPi * p + d.b * t * std::sin (kTwoPi * d.a * p)); });
            case FmFeedback:
            {
                std::vector<double> w ((size_t) N);
                double y = 0.0;
                for (int pass = 0; pass < 3; ++pass)   // settle the feedback loop over a few cycles
                    for (int i = 0; i < N; ++i)
                    {
                        const double p = (double) i / N;
                        y = std::sin (kTwoPi * p + d.a * t * y);
                        w[(size_t) i] = y;
                    }
                return fromWave (w);
            }
            case Crush:
                return sampled ([t, &d] (double p)
                {
                    const double bits = d.a - (d.a - d.b) * t, steps = std::pow (2.0, bits - 1.0);
                    return std::round (basic (d.wave, p) * steps) / steps;
                });
            case Hold:
                return sampled ([t, &d] (double p)
                {
                    const double steps = d.a - (d.a - d.b) * t;
                    return basic (d.wave, std::floor (p * steps) / steps);
                });
            case Cheby:
                return sampled ([t, &d] (double p)
                {
                    const double x = std::sin (kTwoPi * p);
                    double y = 0.0, tPrev = 1.0, tCur = x;
                    const int maxOrder = 2 + (int) std::round (t * 10.0);
                    for (int n = 1; n <= maxOrder; ++n)
                    {
                        const bool use = d.a < 1.5 ? (n % 2 == 1) : (n == 1 || n % 2 == 0);
                        if (use) y += tCur / n;
                        const double tNext = 2.0 * x * tCur - tPrev; tPrev = tCur; tCur = tNext;
                    }
                    return y;
                });
            case Ring:
                return sampled ([t, &d] (double p)
                {
                    const double r = d.b + (d.c - d.b) * t;
                    return 0.5 * std::sin (kTwoPi * p * d.a) + std::sin (kTwoPi * p * d.a) * std::sin (kTwoPi * p * std::round (r));
                });
            case Morph:
                return sampled ([t, &d] (double p)
                {
                    const double warp = p + 0.08 * t * (1.0 - t) * std::sin (kTwoPi * p);   // a little phase bend mid-morph
                    return basic ((int) d.a, warp) * (1.0 - t) + basic ((int) d.b, warp) * t;
                });
            case Formant:      return formantSpectrum ((int) d.a, t, d.b, d.c, d.wave == 1);
            case FormantDrive: return formantSpectrum ((int) d.a, t, 150.0, 80.0, false, 1.0);
            case Comb:
            {
                WaveSpectrum s = empty();
                const double spacing = d.a + (d.b - d.a) * t;
                for (int k = 1; k <= Half; ++k)
                    setSine (s, k, std::pow ((double) k, -d.c) * std::abs (std::cos (kPi * k / spacing)));
                return s;
            }
            case Tilt:
            {
                WaveSpectrum s = empty();
                const double slope = d.a + (d.b - d.a) * t;
                for (int k = 1; k <= Half; ++k)
                {
                    if ((int) d.c == 1 && k % 2 == 0) continue;
                    double a = std::pow ((double) k, -slope);
                    if ((int) d.c == 2) a *= (k % 2 == 0 ? 1.0 : 0.35);
                    if (slope < 0) a *= std::exp (-k / 300.0);   // rising tilt needs a soft top
                    setSine (s, k, a);
                }
                return s;
            }
            case Peak:
            {
                WaveSpectrum s = empty();
                const double centre = std::log2 (2.0) + t * (std::log2 (d.b) - 1.0);
                for (int k = 1; k <= Half; ++k)
                    setSine (s, k, (0.15 / k) + gauss (std::log2 ((double) k), centre, d.c * 3.0) / std::sqrt ((double) k));
                return s;
            }
            case TwinPeak:
            {
                WaveSpectrum s = empty();
                for (int k = 1; k <= Half; ++k)
                {
                    const double lk = std::log2 ((double) k);
                    setSine (s, k, 0.1 / k + (gauss (lk, 3.0 - 1.5 * t, 0.3) + gauss (lk, 3.5 + 2.5 * t, 0.3)) / std::sqrt ((double) k));
                }
                return s;
            }
            case Registration: return registration ((int) d.a, t);
            case Partials:     return partialsSpectrum ((int) d.a, t);
            case Stack:        return stack ((int) d.a, t);
            case Chaos:        return chaosSpectrum ((int) d.a, t);
            case FmFold:
                return sampled ([t, &d] (double p)
                {
                    const double fm = std::sin (kTwoPi * p + d.b * t * std::sin (kTwoPi * d.a * p));
                    return std::sin (kPi * 0.5 * (1.0 + d.c * t) * fm);
                });
            case CrushFm:
                return sampled ([t, &d] (double p)
                {
                    const double fm = std::sin (kTwoPi * p + 4.0 * t * std::sin (kTwoPi * d.a * p));
                    const double steps = std::pow (2.0, 7.0 - 5.0 * t);
                    return std::round (fm * steps) / steps;
                });
            case SevenHeaven:
            {
                // harmonics in sevens, a little nod to TRIPLE SEVEN: 1, 7, 14, 21 ... bloom across the table
                WaveSpectrum s = empty();
                addSine (s, 1, 1.0); addSine (s, 2, 0.35); addSine (s, 3, 0.2);
                for (int k = 7; k <= 777 && k <= Half; k += 7) addSine (s, k, 0.6 * t / std::sqrt ((double) k) * std::exp (-k / (40.0 + 300.0 * t)));
                return s;
            }
        }
        return empty();
    }
}

int libraryTableCount() { return kCount; }
juce::String libraryTableName (int i) { return juce::isPositiveAndBelow (i, kCount) ? juce::String (defs[i].name) : juce::String(); }
WaveSpectrum makeLibrarySpectrum (int i, double t)
{
    if (! juce::isPositiveAndBelow (i, kCount)) return WaveSpectrum ((size_t) Half + 1);
    auto s = make (defs[i], t);
    s[0] = 0.0;
    return s;
}

} // namespace divinity
