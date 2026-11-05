#include "Wavetables.h"
#include "../Parameters.h"
#include <complex>

namespace divinity
{
namespace
{
    using Spectrum = std::vector<std::complex<double>>;   // bins 0 .. kSize/2
    constexpr int N    = WavetableBank::kSize;
    constexpr int Half = N / 2;
    constexpr double kPi = juce::MathConstants<double>::pi;

    /** Sets harmonic k to a sine-phase partial of amplitude b. */
    void setSine (Spectrum& s, int k, double b)          { if (k > 0 && k <= Half) s[(size_t) k] = { 0.0, -b }; }
    void setPhased (Spectrum& s, int k, double b, double ph)
    {
        if (k > 0 && k <= Half) s[(size_t) k] = std::polar (b, ph - kPi * 0.5);
    }

    Spectrum spectrumFromTimeDomain (const std::vector<double>& wave)
    {
        juce::dsp::FFT fft (WavetableBank::kSizeOrder);
        std::vector<juce::dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
        for (int i = 0; i < N; ++i) in[(size_t) i] = { (float) wave[(size_t) i], 0.0f };
        fft.perform (in.data(), out.data(), false);
        Spectrum s ((size_t) Half + 1);
        for (int k = 1; k <= Half; ++k) s[(size_t) k] = { out[(size_t) k].real(), out[(size_t) k].imag() };
        s[0] = 0.0;   // remove DC
        return s;
    }

    // ---- Shape coefficient helpers ------------------------------------------
    double sawB (int k)      { return (2.0 / kPi) * ((k % 2) ? 1.0 : -1.0) / k; }
    double squareB (int k)   { return (k % 2) ? (4.0 / kPi) / k : 0.0; }
    double triangleB (int k)
    {
        if ((k % 2) == 0) return 0.0;
        const double sign = (((k - 1) / 2) % 2) ? -1.0 : 1.0;
        return (8.0 / (kPi * kPi)) * sign / (double) (k * k);
    }

    double gauss (double x, double centre, double width)
    {
        const double d = (x - centre) / width;
        return std::exp (-0.5 * d * d);
    }

    //--------------------------------------------------------------------------
    Spectrum makeBasicShapes (double t)
    {
        // sine -> triangle -> saw -> square (spectral crossfade between keyframes)
        Spectrum s ((size_t) Half + 1);
        const double seg = t * 3.0;
        const int idx = juce::jlimit (0, 2, (int) seg);
        const double f = seg - idx;
        auto key = [] (int which, int k)
        {
            switch (which)
            {
                case 0:  return k == 1 ? 1.0 : 0.0;
                case 1:  return triangleB (k);
                case 2:  return sawB (k);
                default: return squareB (k);
            }
        };
        for (int k = 1; k <= Half; ++k)
            setSine (s, k, key (idx, k) * (1.0 - f) + key (idx + 1, k) * f);
        return s;
    }

    Spectrum makeHarmonicSweep (double t)
    {
        // Harmonic count grows from 1 to ~96 with a soft spectral edge.
        Spectrum s ((size_t) Half + 1);
        const double edge = 1.0 + std::pow (t, 1.6) * 95.0;
        for (int k = 1; k <= Half; ++k)
        {
            const double roll = 1.0 / (1.0 + std::exp ((k - edge) * 0.6));
            setSine (s, k, roll / std::pow ((double) k, 0.85));
        }
        return s;
    }

    Spectrum makeFormantVox (double t)
    {
        // Saw-like source through three vowel formants, morphing A-E-I-O-U.
        static const double vowels[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                             { 450, 800, 2830 },  { 325, 700, 2530 } };
        const double seg = t * 4.0;
        const int idx = juce::jlimit (0, 3, (int) seg);
        const double f = seg - idx;
        const double f0 = 110.0;  // formant positions defined against a 110 Hz reference
        Spectrum s ((size_t) Half + 1);
        for (int k = 1; k <= Half; ++k)
        {
            const double hz = k * f0;
            double env = 0.04;
            const double gains[3] = { 1.0, 0.55, 0.3 };
            for (int fm = 0; fm < 3; ++fm)
            {
                const double centre = vowels[idx][fm] * (1.0 - f) + vowels[idx + 1][fm] * f;
                env += gains[fm] * gauss (hz, centre, 90.0 + fm * 40.0);
            }
            setSine (s, k, env / std::sqrt ((double) k));
        }
        return s;
    }

    Spectrum makePulseGrit (double t)
    {
        // Pulse width sweeps 50% -> 4% with a touch of step-quantisation grit later in the table.
        const double width = 0.5 - t * 0.46;
        const double steps = 64.0 - t * 48.0;
        std::vector<double> w ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const double ph = (double) i / N;
            double v = ph < width ? 1.0 : -1.0;
            const double saw = 2.0 * ph - 1.0;
            v = v * (1.0 - 0.3 * t) + std::round (saw * steps) / steps * 0.3 * t;
            w[(size_t) i] = v;
        }
        return spectrumFromTimeDomain (w);
    }

    Spectrum makeGlassHalo (double t)
    {
        // Sparse bell-like partials whose energy climbs upward through the table.
        static const int partials[] = { 1, 2, 3, 5, 7, 9, 12, 15, 19, 24, 31, 38 };
        Spectrum s ((size_t) Half + 1);
        const double focus = 1.0 + t * 10.0;
        int n = 0;
        for (int k : partials)
        {
            const double amp = (n == 0 ? 0.9 : 0.0) + gauss ((double) n, focus, 2.2) / std::sqrt ((double) (n + 1));
            setPhased (s, k, amp, (n * 1.7) + t * 2.0);
            ++n;
        }
        return s;
    }

    Spectrum makeHollowOrgan (double t)
    {
        // Drawbar-style registrations crossfading through four presets.
        static const double regs[4][9] = {
            { 1.0, 0.0, 0.8, 0.0, 0.5, 0.0, 0.3, 0.0, 0.0 },
            { 1.0, 0.7, 0.6, 0.5, 0.0, 0.4, 0.0, 0.3, 0.0 },
            { 0.6, 1.0, 0.0, 0.8, 0.0, 0.6, 0.0, 0.5, 0.4 },
            { 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 } };
        static const int harm[9] = { 1, 2, 3, 4, 5, 6, 8, 10, 12 };
        const double seg = t * 3.0;
        const int idx = juce::jlimit (0, 2, (int) seg);
        const double f = seg - idx;
        Spectrum s ((size_t) Half + 1);
        for (int d = 0; d < 9; ++d)
            setSine (s, harm[d], (regs[idx][d] * (1.0 - f) + regs[idx + 1][d] * f) / std::sqrt ((double) harm[d]));
        return s;
    }

    /** Internal analog table: exact Fourier series of the Analog engine's waveforms
        (same phase and polarity as the PolyBLEP versions, amplitude +-1, no DC). */
    Spectrum makeAnalogSpectrum (int frame)
    {
        Spectrum s ((size_t) Half + 1);
        for (int k = 1; k <= Half; ++k)
        {
            switch (frame)
            {
                case 0:  if (k == 1) setSine (s, 1, 1.0); break;                                  // sine
                case 1:  if (k % 2) s[(size_t) k] = { -8.0 / (kPi * kPi) / ((double) k * k), 0.0 }; break; // triangle: -cos series
                case 3:  if (k % 2) setSine (s, k, 4.0 / (kPi * k)); break;                       // square: +1 then -1
                default: setSine (s, k, -2.0 / (kPi * k)); break;                                 // saw 2p-1
            }
        }
        return s;
    }

    Spectrum makeSpectrum (int table, double t)
    {
        switch (table)
        {
            case 0:  return makeBasicShapes (t);
            case 1:  return makeHarmonicSweep (t);
            case 2:  return makeFormantVox (t);
            case 3:  return makePulseGrit (t);
            case 4:  return makeGlassHalo (t);
            default: return makeHollowOrgan (t);
        }
    }
}

//==============================================================================
WavetableBank::WavetableBank()
{
    numTables = wavetableNames().size();
    data.assign ((size_t) (numTables + 1) * kFrames * kLevels * kStride, 0.0f);

    for (int t = 0; t <= numTables; ++t)   // + the internal analog table
        buildTable (t, numTables);
}

void WavetableBank::buildTable (int tableIndex, int numPublicTables)
{
    juce::dsp::FFT fft (kSizeOrder);
    std::vector<juce::dsp::Complex<float>> in ((size_t) N), out ((size_t) N);
    const bool analogTable = tableIndex == numPublicTables;
    float analogScale = 1.0f;   // inverse-FFT scale: makes a unit sine partial peak at exactly 1

    for (int frame = 0; frame < kFrames; ++frame)
    {
        const double t = (double) frame / (kFrames - 1);
        const auto spec = analogTable ? makeAnalogSpectrum (frame) : makeSpectrum (tableIndex, t);
        float normalise = 1.0f;

        for (int level = 0; level < kLevels; ++level)
        {
            const int maxHarm = harmonicsForLevel (level);
            std::fill (in.begin(), in.end(), juce::dsp::Complex<float> {});
            for (int k = 1; k <= juce::jmin (maxHarm, Half - 1); ++k)
            {
                const auto c = spec[(size_t) k];
                in[(size_t) k]     = { (float) c.real(),  (float) c.imag() };
                in[(size_t) (N - k)] = { (float) c.real(), -(float) c.imag() };
            }
            fft.perform (in.data(), out.data(), true);

            float* dest = data.data() + ((size_t) ((tableIndex * kFrames + frame) * kLevels + level)) * kStride;
            for (int i = 0; i < N; ++i) dest[i] = out[(size_t) i].real();

            if (level == 0)
            {
                float peak = 0.0f;
                for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (dest[i]));
                if (analogTable)
                {
                    // frame 0 is a unit sine: its peak is the FFT scale for the whole table
                    if (frame == 0) analogScale = peak > 1.0e-9f ? 1.0f / peak : 1.0f;
                    normalise = analogScale;
                }
                else
                    normalise = peak > 1.0e-9f ? 0.9f / peak : 0.0f;
            }
            for (int i = 0; i < N; ++i) dest[i] *= normalise;
            dest[N] = dest[0];
        }
    }
}

} // namespace divinity
