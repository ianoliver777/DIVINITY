#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace divinity::dsp
{
constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

inline float noteToHz (double note) noexcept { return (float) (440.0 * std::pow (2.0, (note - 69.0) / 12.0)); }

/** Flushes denormals / non-finite values on filter states. */
inline float sanitise (float x) noexcept
{
    return (std::isfinite (x) && std::abs (x) > 1.0e-20f) ? x : 0.0f;
}

//==============================================================================
/** PolyBLEP residual for band-limited discontinuities (t = phase, dt = increment). */
inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt)       { t /= dt;               return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return t * t + t + t + 1.0; }
    return 0.0;
}

/** PolyBLAMP residual (integrated BLEP) for slope discontinuities. Scale by the slope change * dt. */
inline double polyBlamp (double t, double dt) noexcept
{
    if (t < dt)       { t = t / dt - 1.0;         return -1.0 / 3.0 * t * t * t; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt + 1.0; return  1.0 / 3.0 * t * t * t; }
    return 0.0;
}

/** Engine 2.0 analog waveforms: PolyBLEP saw/pulse + PolyBLAMP triangle (no corner aliasing). */
inline float analogSample2 (int wave, double phase, double inc, double pw) noexcept
{
    if (wave == 1)
    {
        double v = phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
        // slope flips by -8 at phase 0.5 and +8 at phase 0
        v += 8.0 * inc * polyBlamp (phase, inc);
        double t2 = phase + 0.5; if (t2 >= 1.0) t2 -= 1.0;
        v -= 8.0 * inc * polyBlamp (t2, inc);
        return (float) v;
    }
    if (wave == 0) return (float) std::sin (phase * kTwoPi);
    if (wave == 2) return (float) ((2.0 * phase - 1.0) - polyBlep (phase, inc));
    if (wave == 3) pw = 0.5;
    double v = phase < pw ? 1.0 : -1.0;
    v += polyBlep (phase, inc);
    double t2 = phase - pw; if (t2 < 0.0) t2 += 1.0;
    v -= polyBlep (t2, inc);
    return (float) (v - (2.0 * pw - 1.0));
}

/** Fast, smooth tanh approximation (exact enough for in-loop saturation, |err| < 2% in range). */
inline float fastTanh (float x) noexcept
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Wavefolder with first-order antiderivative anti-aliasing (ADAA).
    f(x)  = (1-a)x + a sin(g x pi/2)   (same curve as the Classic engine)
    F(x)  = (1-a)x^2/2 - a cos(g x pi/2) / (g pi/2)                         */
struct FoldAdaa
{
    static float f (float x, float a, float w) noexcept  { return (1.0f - a) * x + a * std::sin (w * x); }
    static float F (float x, float a, float w) noexcept  { return 0.5f * (1.0f - a) * x * x - a * std::cos (w * x) / w; }

    static float process (float x, float& prev, float a) noexcept
    {
        const float x0 = prev;
        prev = x;
        if (a < 1.0e-4f) return x;
        const float w = (1.0f + a * 5.0f) * juce::MathConstants<float>::halfPi;
        const float d = x - x0;
        if (std::abs (d) < 1.0e-3f) return f (0.5f * (x + x0), a, w);
        return (F (x, a, w) - F (x0, a, w)) / d;
    }
};

inline float analogSample (int wave, double phase, double inc, double pw) noexcept
{
    switch (wave)
    {
        case 0: return (float) std::sin (phase * kTwoPi);                       // sine
        case 1: return (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase); // triangle (1/k^2 — low alias)
        case 2: return (float) ((2.0 * phase - 1.0) - polyBlep (phase, inc));   // saw
        case 3: pw = 0.5; [[fallthrough]];
        default:                                                                // square / pulse
        {
            double v = phase < pw ? 1.0 : -1.0;
            v += polyBlep (phase, inc);
            double t2 = phase - pw; if (t2 < 0.0) t2 += 1.0;
            v -= polyBlep (t2, inc);
            // remove DC offset of asymmetric pulse
            return (float) (v - (2.0 * pw - 1.0));
        }
    }
}

//==============================================================================
/** Fast deterministic white noise (xorshift). */
struct Rng
{
    uint32_t state = 0x9E3779B9u;
    void seed (uint32_t s) noexcept { state = s ? s : 0x9E3779B9u; }
    uint32_t next() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
    float bipolar() noexcept  { return (float) ((int32_t) next()) * (1.0f / 2147483648.0f); }
    float unipolar() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
};

/** Slow smoothed random walk used for analog drift (evaluated at control rate). */
struct DriftGen
{
    float value = 0.0f, target = 0.0f, timer = 0.0f;
    void reset() noexcept { value = target = timer = 0.0f; }
    float advance (Rng& rng, float seconds, float meanPeriod) noexcept
    {
        timer -= seconds;
        if (timer <= 0.0f) { target = rng.bipolar(); timer = meanPeriod * (0.5f + rng.unipolar()); }
        value += (1.0f - std::exp (-seconds / (meanPeriod * 0.6f))) * (target - value);
        return value;
    }
};

struct NoiseGen
{
    Rng rng;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;   // pink (Kellet)
    float hpPrev = 0, lp = 0;

    void reset() { b0 = b1 = b2 = b3 = b4 = b5 = b6 = hpPrev = lp = 0; }

    float next (int type, float lpCoeff) noexcept
    {
        const float w = rng.bipolar();
        switch (type)
        {
            case 1:
            {
                b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
                b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
                b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
                const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
                b6 = w * 0.115926f;
                return p * 0.11f;
            }
            case 2: { const float y = (w - hpPrev) * 0.5f; hpPrev = w; return y * 1.4f; }       // bright (differentiated)
            case 3: { lp += lpCoeff * (w - lp); return sanitise (lp) * 3.0f; }                    // dark (low-passed)
            default: return w * 0.6f;                                                             // white
        }
    }
};

//==============================================================================
/** ADSR with curve shaping. Retriggers from its current level (click-free voice stealing). */
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    /** c = overall curve; ac / dc / rc bend one segment on top of it (1.0: drawn on the envelope display). */
    void setParameters (float a, float d, float s, float r, float c, float ac = 0.0f, float dc = 0.0f, float rc = 0.0f) noexcept
    {
        attack = juce::jmax (0.0005f, a); decay = juce::jmax (0.001f, d);
        sustain = juce::jlimit (0.0f, 1.0f, s); release = juce::jmax (0.001f, r);
        curve = juce::jlimit (-1.0f, 1.0f, c);
        attackExp = std::pow (2.0f, juce::jlimit (-2.0f, 2.0f, curve + ac) * 2.0f);
        decayExp  = 3.0f * std::pow (2.0f, juce::jlimit (-2.0f, 2.0f, curve + dc) * 1.5f);
        releaseExp = 3.0f * std::pow (2.0f, juce::jlimit (-2.0f, 2.0f, curve + rc) * 1.5f);
    }

    void noteOn() noexcept   { startLevel = level; progress = 0.0; stage = Stage::Attack; }
    void noteOff() noexcept  { if (stage != Stage::Idle) { startLevel = level; progress = 0.0; stage = Stage::Release; } }
    void reset() noexcept    { stage = Stage::Idle; level = 0.0f; progress = 0.0; }
    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Idle: level = 0.0f; break;
            case Stage::Attack:
                progress += 1.0 / (attack * sampleRate);
                if (progress >= 1.0) { level = 1.0f; progress = 0.0; stage = Stage::Decay; }
                else level = startLevel + (1.0f - startLevel) * std::pow ((float) progress, attackExp);
                break;
            case Stage::Decay:
                progress += 1.0 / (decay * sampleRate);
                if (progress >= 1.0) { level = sustain; stage = Stage::Sustain; }
                else level = sustain + (1.0f - sustain) * std::pow (1.0f - (float) progress, decayExp);
                break;
            case Stage::Sustain: level = sustain; break;
            case Stage::Release:
                progress += 1.0 / (release * sampleRate);
                if (progress >= 1.0) { level = 0.0f; stage = Stage::Idle; }
                else level = startLevel * std::pow (1.0f - (float) progress, releaseExp);
                break;
        }
        return level;
    }

private:
    double sampleRate = 44100.0, progress = 0.0;
    float attack = 0.01f, decay = 0.1f, sustain = 1.0f, release = 0.1f, curve = 0.0f;
    float attackExp = 1.0f, decayExp = 3.0f, releaseExp = 3.0f, level = 0.0f, startLevel = 0.0f;
    Stage stage = Stage::Idle;
};

//==============================================================================
/** Global, optionally tempo-synced LFO, evaluated at control rate. */
class Lfo
{
public:
    void reset (uint32_t seed) { phase = 0.0; rng.seed (seed); held = rng.bipolar(); smoothed = 0.0f; }

    /** Advances by `seconds` and returns the bipolar output (-1..1). */
    float advance (int shape, double rateHz, double seconds, float smooth, double phaseOffset,
                   bool locked, double lockedPhase, const float* custom = nullptr, int customN = 0) noexcept
    {
        if (locked)
        {
            const double newPhase = lockedPhase - std::floor (lockedPhase);
            if (newPhase < phase - 0.5) held = rng.bipolar();
            phase = newPhase;
        }
        else
        {
            phase += rateHz * seconds;
            if (phase >= 1.0) { phase -= std::floor (phase); held = rng.bipolar(); }
        }

        double p = phase + phaseOffset; p -= std::floor (p);
        float raw = 0.0f;
        switch (shape)
        {
            case 0:  raw = (float) std::sin (p * kTwoPi); break;
            case 1:  raw = p < 0.5 ? 1.0f : -1.0f; break;
            case 2:  raw = (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); break;
            case 4:  raw = (float) (2.0 * p - 1.0); break;    // saw up
            case 5:  raw = (float) (1.0 - 2.0 * p); break;    // saw down
            case 6:  // custom (drawn): linear between the drawn points, wrapping
                if (custom != nullptr && customN > 1)
                {
                    const double x = p * customN; const int i0 = (int) x % customN; const float f = (float) (x - std::floor (x));
                    raw = custom[i0] + f * (custom[(i0 + 1) % customN] - custom[i0]);
                }
                break;
            default: raw = held; break;
        }
        // Smooth: one-pole slew, time constant up to ~0.25 of a cycle (min 2 ms for de-clicking).
        const double tc = juce::jmax (0.002, smooth * 0.25 / juce::jmax (0.01, rateHz));
        const float a = (float) (1.0 - std::exp (-seconds / tc));
        smoothed += a * (raw - smoothed);
        return smoothed;
    }

    double getPhase() const noexcept { return phase; }

private:
    double phase = 0.0;
    float held = 0.0f, smoothed = 0.0f;
    Rng rng;
};

//==============================================================================
/** Zero-delay-feedback state variable filter (Zavalishin TPT), one channel. */
struct SvfState { float ic1 = 0, ic2 = 0; void reset() { ic1 = ic2 = 0; } };

/** 4-pole ZDF ladder, one channel. */
struct LadderState { float s[4] {}; void reset() { s[0] = s[1] = s[2] = s[3] = 0; } };

struct FilterCoeffs { float g = 0, k = 0, a1 = 0, a2 = 0, a3 = 0, G = 0, ladderK = 0; };

inline FilterCoeffs makeCoeffs (float cutoffHz, float res, double sr) noexcept
{
    FilterCoeffs c;
    const float fc = juce::jlimit (16.0f, (float) (sr * 0.49), cutoffHz);
    c.g  = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
    c.k  = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, res);
    c.a1 = 1.0f / (1.0f + c.g * (c.g + c.k));
    c.a2 = c.g * c.a1;
    c.a3 = c.g * c.a2;
    c.G  = c.g / (1.0f + c.g);
    c.ladderK = 3.95f * juce::jlimit (0.0f, 1.0f, res);
    return c;
}

/** Returns lp, bp, hp of one SVF tick. */
inline void svfTick (SvfState& st, const FilterCoeffs& c, float x, float& lp, float& bp, float& hp) noexcept
{
    const float v3 = x - st.ic2;
    const float v1 = c.a1 * st.ic1 + c.a2 * v3;
    const float v2 = st.ic2 + c.a2 * st.ic1 + c.a3 * v3;
    st.ic1 = sanitise (2.0f * v1 - st.ic1);
    st.ic2 = sanitise (2.0f * v2 - st.ic2);
    lp = v2; bp = v1; hp = x - c.k * v1 - v2;
}

/** Engine 2.0 SVF: the band-pass integrator is softly saturated, so resonance self-limits musically
    and the filter warms up when driven (headroom shrinks as `sat` grows). */
inline void svfTickNL (SvfState& st, const FilterCoeffs& c, float x, float sat, float& lp, float& bp, float& hp) noexcept
{
    const float v3 = x - st.ic2;
    float v1 = c.a1 * st.ic1 + c.a2 * v3;
    v1 = fastTanh (v1 * sat) / sat;
    const float v2 = st.ic2 + c.g * v1;
    st.ic1 = sanitise (2.0f * v1 - st.ic1);
    st.ic2 = sanitise (2.0f * v2 - st.ic2);
    lp = v2; bp = v1; hp = x - c.k * v1 - v2;
}

inline float ladderTick (LadderState& st, const FilterCoeffs& c, float x, float passbandComp = 0.3f) noexcept
{
    const float G = c.G, inv = 1.0f / (1.0f + c.g);
    const float S = G * G * G * st.s[0] * inv + G * G * st.s[1] * inv + G * st.s[2] * inv + st.s[3] * inv;
    const float G4 = G * G * G * G;
    float y4 = (G4 * x + S) / (1.0f + c.ladderK * G4);
    float u = x - c.ladderK * y4;
    u = std::tanh (u);                     // gentle nonlinearity keeps resonance musical and bounded
    for (int i = 0; i < 4; ++i)
    {
        const float v = (u - st.s[i]) * G;
        const float y = v + st.s[i];
        st.s[i] = sanitise (y + v);
        u = y;
    }
    return u * (1.0f + c.ladderK * passbandComp);  // passband compensation
}

/** Feedback comb tuned to the cutoff frequency. */
struct CombState
{
    std::vector<float> buffer;
    int writePos = 0;
    void allocate (int size) { buffer.assign ((size_t) juce::jmax (4, size), 0.0f); writePos = 0; }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; }

    float tick (float x, float delaySamples, float feedback) noexcept
    {
        const int size = (int) buffer.size();
        if (! std::isfinite (delaySamples)) delaySamples = 1.0f;
        delaySamples = juce::jlimit (1.0f, (float) (size - 2), delaySamples);
        float readPos = (float) writePos - delaySamples;
        if (readPos < 0) readPos += (float) size;
        const int i0 = juce::jlimit (0, size - 1, (int) readPos);
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - (float) i0;
        const float delayed = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
        const float y = x + feedback * delayed;
        buffer[(size_t) writePos] = sanitise (std::tanh (y));
        writePos = (writePos + 1) % size;
        return y * 0.6f;
    }
};

} // namespace divinity::dsp
