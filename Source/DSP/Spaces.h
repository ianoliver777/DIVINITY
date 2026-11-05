#pragma once

#include "DSPCore.h"
#include <array>
#include <vector>

//==============================================================================
// Divine 2.0 space effects: the FDN reverb and the dimension chorus.
// Both are allocation-free after prepare() and fully deterministic.
//==============================================================================
namespace divinity::dsp
{
/** Cheap sine for LFOs (phase 0..1): parabolic approximation with one refinement, |err| < 0.1%. */
inline float lfoSin (float phase) noexcept
{
    float x = phase * 2.0f - 1.0f;                // -1..1  ->  maps to -pi..pi
    float y = 4.0f * x * (1.0f - std::abs (x));   // parabola
    y = 0.225f * (y * std::abs (y) - y) + y;      // refinement
    return -y;                                    // sin(2 pi phase)
}

/** Fractional delay line with 4-point Hermite interpolation. */
class DelayLine
{
public:
    void allocate (int maxSamples)
    {
        size = 1;
        while (size < maxSamples + 4) size <<= 1;
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        pos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    void push (float x) noexcept { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }

    /** Delay in samples (>= 1). */
    float read (float d) const noexcept
    {
        const float rp = (float) pos - d;
        const int i1 = (int) std::floor (rp);
        const float f = rp - (float) i1;
        const float xm1 = buf[(size_t) ((i1 - 1) & mask)], x0 = buf[(size_t) (i1 & mask)];
        const float x1 = buf[(size_t) ((i1 + 1) & mask)], x2 = buf[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    float readInt (int d) const noexcept { return buf[(size_t) ((pos - d) & mask)]; }

private:
    std::vector<float> buf;
    int size = 0, mask = 0, pos = 0;
};

//==============================================================================
/** Schroeder allpass used as an input diffuser. */
struct Allpass
{
    std::vector<float> buf; int idx = 0, len = 1; float g = 0.6f;
    void allocate (int n) { len = juce::jmax (1, n); buf.assign ((size_t) len, 0.0f); idx = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }
    float process (float x) noexcept
    {
        const float d = buf[(size_t) idx];
        const float v = x + g * d;
        buf[(size_t) idx] = sanitise (v);
        if (++idx >= len) idx = 0;
        return d - g * v;
    }
};

//==============================================================================
/**
    8-line feedback delay network reverb.
      input HP/LP -> pre-delay -> early reflections -> 4-stage diffusion
      -> 8 modulated delay lines, Householder feedback, per-line damping
      -> decorrelated stereo taps -> width.
    Decay time is set per line from the target RT60, so the tail decays smoothly
    and evenly (no ringing modes from mismatched feedback gains). Slow, independent
    delay modulation on every line smears resonant modes (no metallic ring).
*/
class DivineReverb
{
public:
    static constexpr int kLines = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double sc = sr / 48000.0;
        pre.allocate ((int) (0.35 * sr));
        static const int inLoop[kLines] = { 113, 137, 157, 179, 193, 211, 229, 251 };
        for (int i = 0; i < kLines; ++i) { loopAp[(size_t) i].allocate ((int) (inLoop[i] * sc)); loopAp[(size_t) i].g = 0.45f; }
        static const int apL[4] = { 151, 199, 383, 541 }, apR[4] = { 163, 211, 397, 557 };
        static const float apG[4] = { 0.70f, 0.68f, 0.62f, 0.60f };
        for (int i = 0; i < 4; ++i)
        {
            diffL[i].allocate ((int) (apL[i] * sc)); diffL[i].g = apG[i];
            diffR[i].allocate ((int) (apR[i] * sc)); diffR[i].g = apG[i];
        }
        for (int i = 0; i < kLines; ++i)
            lines[(size_t) i].allocate ((int) (kBase[i] * sc * 1.35 + 64 * sc));
        reset();
    }

    void reset()
    {
        pre.clear();
        for (auto& a : diffL) a.clear();
        for (auto& a : diffR) a.clear();
        for (auto& l : lines) l.clear();
        for (auto& a : loopAp) a.clear();
        damp.fill (0.0f);
        lpInL = lpInR = hpInL = hpInR = 0.0f;
        for (int i = 0; i < kLines; ++i) lfoPhase[(size_t) i] = (float) i / kLines;
    }

    /** Renders the 100%-wet signal into wetL / wetR. */
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n,
                  float size, float damping, float width, float predelayMs) noexcept
    {
        size = juce::jlimit (0.0f, 1.0f, size);
        const double sc = sr / 48000.0;
        const float lenScale = 0.55f + 0.75f * size;
        const float rt60 = 0.2f * std::pow (30.0f, size);                      // 0.2 s .. 6 s
        // Frequency-dependent decay (Jot): each line's one-pole low-pass is set so the top end
        // decays with RT60 * hfRatio, i.e. damping shortens the highs relative to the body
        // instead of eating a fixed amount of treble on every pass.
        const float hfRatio = juce::jlimit (0.08f, 1.0f, 1.0f - 0.9f * juce::jlimit (0.0f, 1.0f, damping));
        const float hpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 90.0f / (float) sr);
        const float bwA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 13000.0f / (float) sr);
        const float pd = juce::jlimit (1.0f, (float) (0.3 * sr), predelayMs * 0.001f * (float) sr);
        const float modDepth = (float) (22.0 * sc);   // deep, slow line modulation smooths low modes (lush, Valhalla-style)
        const float lfoInc = 1.0f / (float) sr;

        std::array<float, kLines> len, gain, pole;
        for (int i = 0; i < kLines; ++i)
        {
            len[(size_t) i] = (float) (kBase[i] * sc * lenScale);
            gain[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / ((float) sr * rt60));
            const float gHf = std::pow (10.0f, -3.0f * len[(size_t) i] / ((float) sr * rt60 * hfRatio));
            const float r = juce::jlimit (1.0e-4f, 1.0f, gHf / gain[(size_t) i]);   // Nyquist gain relative to DC
            pole[(size_t) i] = (1.0f - r) / (1.0f + r);
        }

        const float erGain = 0.30f * (1.0f - 0.45f * size);
        const float ws = juce::jlimit (0.0f, 1.0f, width);

        for (int s = 0; s < n; ++s)
        {
            // band-limit the input: no sub mud, no fizz feeding the tank
            float xl = inL[s], xr = inR[s];
            lpInL += bwA * (xl - lpInL); lpInR += bwA * (xr - lpInR);
            hpInL += hpA * (lpInL - hpInL); hpInR += hpA * (lpInR - hpInR);
            xl = lpInL - hpInL; xr = lpInR - hpInR;

            pre.push (0.5f * (xl + xr));
            const float pmid = pre.read (pd);

            // early reflections (decorrelated per side)
            float erL = 0.0f, erR = 0.0f;
            for (int t = 0; t < 6; ++t)
            {
                erL += kErGain[t] * pre.readInt ((int) (pd + kErL[t] * sc));
                erR += kErGain[t] * pre.readInt ((int) (pd + kErR[t] * sc));
            }

            float dl = pmid + erL * 0.5f, dr = pmid + erR * 0.5f;
            for (int i = 0; i < 4; ++i) { dl = diffL[i].process (dl); dr = diffR[i].process (dr); }

            // read the tank
            std::array<float, kLines> y;
            for (int i = 0; i < kLines; ++i)
            {
                float& ph = lfoPhase[(size_t) i];
                ph += kLfoHz[i] * lfoInc; if (ph >= 1.0f) ph -= 1.0f;
                const float m = lfoSin (ph) * modDepth;
                float v = loopAp[(size_t) i].process (lines[(size_t) i].read (len[(size_t) i] - (float) loopAp[(size_t) i].len + modDepth + m)) * gain[(size_t) i];
                damp[(size_t) i] = v + pole[(size_t) i] * (damp[(size_t) i] - v);   // y = (1-a) v + a y
                damp[(size_t) i] = sanitise (damp[(size_t) i]);
                y[(size_t) i] = damp[(size_t) i];
            }

            // Householder feedback (energy-preserving, maximally mixing)
            float sum = 0.0f;
            for (float v : y) sum += v;
            sum *= 2.0f / kLines;
            for (int i = 0; i < kLines; ++i)
                lines[(size_t) i].push (sanitise (y[(size_t) i] - sum + ((i & 1) ? dr : dl) * 0.6f));

            // same-sign taps: no low-frequency cancellation between correlated lines (full-bodied tail);
            // L and R come from disjoint line sets fed from decorrelated diffusers -> wide image
            float outL = (y[0] + y[2] + y[4] + y[6]) * 0.3f + erL * erGain;
            float outR = (y[1] + y[3] + y[5] + y[7]) * 0.3f + erR * erGain;

            // width (mid/side)
            const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * ws;
            wetL[s] = mid + side;
            wetR[s] = mid - side;
        }
    }

private:
    static constexpr double kBase[kLines] = { 1433, 1601, 1867, 2053, 2251, 2399, 2647, 2903 };
    static constexpr float kLfoHz[kLines] = { 0.31f, 0.47f, 0.23f, 0.59f, 0.37f, 0.71f, 0.29f, 0.53f };
    static constexpr double kErL[6] = { 337, 571, 829, 1147, 1523, 1987 };
    static constexpr double kErR[6] = { 397, 631, 947, 1259, 1721, 2113 };
    static constexpr float kErGain[6] = { 0.55f, -0.45f, 0.38f, -0.31f, 0.25f, -0.19f };

    double sr = 48000.0;
    DelayLine pre;
    std::array<Allpass, 4> diffL, diffR;
    std::array<DelayLine, kLines> lines;
    std::array<Allpass, kLines> loopAp;   // in-loop diffusion: denser modes, smoother tail
    std::array<float, kLines> damp {}, lfoPhase {};
    float lpInL = 0, lpInR = 0, hpInL = 0, hpInR = 0;
};

//==============================================================================
/**
    Dimension-style stereo chorus: three modulated taps per side with spread base
    delays, decorrelated rates and phases. Because each side averages several
    different taps, the mono sum is smooth instead of comb-filtered.
*/
class DimensionChorus
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        dl.allocate ((int) (0.05 * sr)); dr.allocate ((int) (0.05 * sr));
        reset();
    }
    void reset()
    {
        dl.clear(); dr.clear();
        for (int i = 0; i < 3; ++i) { phL[(size_t) i] = (float) i / 3.0f; phR[(size_t) i] = (float) i / 3.0f + 0.5f; if (phR[(size_t) i] >= 1.0f) phR[(size_t) i] -= 1.0f; }
    }

    /** In place. mix = linear dry/wet (same law as the Classic chorus, so preset levels hold). */
    void process (float* L, float* R, int n, float rateHz, float depth, float mixStart, float mixEnd) noexcept
    {
        const float mixStep = (mixEnd - mixStart) / (float) juce::jmax (1, n);
        float mix = mixStart;
        const float msToS = (float) sr * 0.001f;
        const float modMs = 0.5f + 5.5f * juce::jlimit (0.0f, 1.0f, depth);
        const float inc = rateHz / (float) sr;
        constexpr float baseL[3] = { 6.1f, 9.7f, 13.3f }, baseR[3] = { 7.3f, 10.9f, 12.1f };
        constexpr float rateMul[3] = { 1.0f, 0.83f, 1.21f };
        for (int s = 0; s < n; ++s)
        {
            dl.push (L[s]); dr.push (R[s]);
            float wl = 0.0f, wr = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                float& a = phL[(size_t) i]; a += inc * rateMul[i]; if (a >= 1.0f) a -= 1.0f;
                float& b = phR[(size_t) i]; b += inc * rateMul[i] * 1.07f; if (b >= 1.0f) b -= 1.0f;
                const float ma = 0.5f + 0.5f * lfoSin (a);
                const float mb = 0.5f + 0.5f * lfoSin (b);
                wl += dl.read ((baseL[i] + modMs * ma) * msToS);
                wr += dr.read ((baseR[i] + modMs * mb) * msToS);
            }
            wl *= (1.0f / 3.0f); wr *= (1.0f / 3.0f);
            // a little cross-blend glues the image (dimension style) without collapsing it
            const float cl = 0.92f * wl + 0.08f * wr, cr = 0.92f * wr + 0.08f * wl;
            L[s] = L[s] * (1.0f - mix) + cl * mix;
            R[s] = R[s] * (1.0f - mix) + cr * mix;
            mix += mixStep;
        }
    }

private:
    double sr = 48000.0;
    DelayLine dl, dr;
    std::array<float, 3> phL {}, phR {};
};

} // namespace divinity::dsp
