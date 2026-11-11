#include "FXChain.h"
#include "Spaces.h"

namespace divinity
{
using namespace fxids;

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;
    inline float clamp01 (float x) { return juce::jlimit (0.0f, 1.0f, x); }
    inline float logCosh (float x) { const float a = std::abs (x); return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f; }

    //==========================================================================
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] {}, z2[2] {};
        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
        float process (int ch, float x) noexcept
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = dsp::sanitise (b1 * x - a1 * y + z2[ch]);
            z2[ch] = dsp::sanitise (b2 * x - a2 * y);
            return y;
        }
        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
            a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
        }
        void lowShelf (double fs, double f, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                 (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
        }
        void highShelf (double fs, double f, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                 (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
        }
        void peak (double fs, double f, double gainDb, double q)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
    };

    struct OnePole
    {
        float z[2] {};
        void reset() { z[0] = z[1] = 0; }
        float lp (int ch, float x, float a) { z[ch] += a * (x - z[ch]); z[ch] = dsp::sanitise (z[ch]); return z[ch]; }
    };

    /** 1st-order ADAA state for two channels (shaper + antiderivative passed per call). */
    struct AdaaState
    {
        float prev[2] {};
        void reset() { prev[0] = prev[1] = 0; }
        template <typename F1, typename F2>
        float process (int ch, float x, F1 f, F2 af) noexcept
        {
            const float x0 = prev[ch];
            prev[ch] = x;
            const float d = x - x0;
            return std::abs (d) < 1.0e-4f ? f (0.5f * (x + x0)) : (af (x) - af (x0)) / d;
        }
    };

    //==========================================================================
    // Saturation (macros TEXTURE + SOUL + matrix can drive it even when off)
    //==========================================================================
    class SatModule : public FxModule
    {
    public:
        SatModule (juce::AudioProcessorValueTreeState& s, FXChain::Display& d)
            : FxModule (s, ids::satOn), disp (d), drive (p (ids::satDrive)), mix (p (ids::satMix)) {}
        void prepare (double sr, int) override { driveS.reset (sr, 0.03); mixS.reset (sr, 0.03); reset(); }
        void reset() override { adaa.reset(); driveS.setCurrentAndTargetValue (0); mixS.setCurrentAndTargetValue (0); }
        bool wantsToRun (const FxContext& c) const noexcept override { return isOn() || c.macros[2] > 0.001f || c.macros[4] > 0.001f || c.mods.satDrive > 0.001f; }
        void process (float* L, float* R, int n, const FxContext& c) noexcept override
        {
            const bool on = isOn();
            const float driveT = clamp01 ((on ? drive->load() : 0.0f) + c.macros[2] * 0.5f + c.macros[4] * 0.35f + c.mods.satDrive);
            driveS.setTargetValue (driveT);
            mixS.setTargetValue (on ? mix->load() : 1.0f);
            disp.sat = driveT;
            auto f = [] (float x) { return std::tanh (x); };
            auto F = [] (float x) { return logCosh (x); };
            for (int s = 0; s < n; ++s)
            {
                const float dr = driveS.getNextValue(), m = mixS.getNextValue();
                const float g = juce::Decibels::decibelsToGain (dr * 24.0f), mk = 1.0f / std::sqrt (g), bias = dr * 0.15f;
                const float yl = (adaa.process (0, L[s] * g + bias, f, F) - std::tanh (bias)) * mk;
                const float yr = (adaa.process (1, R[s] * g + bias, f, F) - std::tanh (bias)) * mk;
                L[s] += m * (yl - L[s]); R[s] += m * (yr - R[s]);
            }
        }
    private:
        FXChain::Display& disp;
        std::atomic<float> *drive, *mix;
        AdaaState adaa;
        juce::SmoothedValue<float> driveS, mixS;
    };

    //==========================================================================
    class DistModule : public FxModule
    {
    public:
        explicit DistModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, distOn), type (p (distType)), drive (p (distDrive)), tone (p (distTone)), mix (p (distMix)) {}
        void prepare (double sr, int) override { fs = sr; driveS.reset (sr, 0.02); mixS.reset (sr, 0.02); reset(); }
        void reset() override
        {
            prev[0] = prev[1] = 0; lp.reset(); dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
            driveS.setCurrentAndTargetValue (drive->load()); mixS.setCurrentAndTargetValue (mix->load());
        }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            const int t = juce::jlimit (0, 11, (int) std::lround (type->load()));
            driveS.setTargetValue (drive->load()); mixS.setTargetValue (mix->load());
            const float toneHz = juce::jmin ((float) (fs * 0.45), 500.0f * std::pow (2.0f, tone->load() * 5.3f));
            const float a = 1.0f - std::exp (-2.0f * kPi * toneHz / (float) fs);
            const float dcA = 1.0f - std::exp (-2.0f * kPi * 12.0f / (float) fs);
            float* ch[2] = { L, R };
            for (int s = 0; s < n; ++s)
            {
                const float dr = driveS.getNextValue(), m = mixS.getNextValue();
                const float g = juce::Decibels::decibelsToGain (dr * 36.0f), mk = 1.0f / std::sqrt (g);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = ch[c][s] * g;
                    const float x0 = prev[c]; prev[c] = x;
                    const float d = x - x0;
                    float y = std::abs (d) < 1.0e-4f ? shape (t, 0.5f * (x + x0)) : (anti (t, x) - anti (t, x0)) / d;
                    dcY[c] = dsp::sanitise (y - dcX[c] + (1.0f - dcA) * dcY[c]); dcX[c] = y;   // DC blocker
                    y = lp.lp (c, dcY[c], a) * mk;                                            // tone
                    ch[c][s] += m * (y - ch[c][s]);
                }
            }
        }
    private:
        static float shape (int t, float x)
        {
            switch (t)
            {
                case 0:  return juce::jlimit (-1.0f, 1.0f, x);
                case 1:  return std::sin (x * kPi * 0.5f);
                case 2:  return std::tanh (x + 0.3f) - std::tanh (0.3f);
                case 3:  return std::abs (x);
                case 4:  { const float ax = std::abs (x); return 1.5f * (ax <= 1.0f ? x - x * x * x / 3.0f : (x < 0 ? -2.0f / 3.0f : 2.0f / 3.0f)); }
                case 5:  return x / (1.0f + std::abs (x));                                         // fuzz
                case 6:  return x >= 0.0f ? std::tanh (x) : 0.4f * std::tanh (x / 0.4f);           // diode (asymmetric)
                case 7:  return (2.0f / kPi) * std::atan (x);                                      // overdrive
                case 8:  return std::tanh (x);                                                     // tape
                case 9:  return x > 0.0f ? x : 0.0f;                                               // half rectify
                case 10: return 0.5f * juce::jlimit (-1.0f, 1.0f, 2.0f * x);                       // crunch
                default: return (x < 0 ? -1.0f : 1.0f) * (1.0f - std::exp (-3.0f * std::abs (x))); // square fuzz
            }
        }
        static float anti (int t, float x)
        {
            switch (t)
            {
                case 0:  { const float ax = std::abs (x); return ax <= 1.0f ? 0.5f * x * x : ax - 0.5f; }
                case 1:  { const float k = kPi * 0.5f; return -std::cos (k * x) / k; }
                case 2:  return logCosh (x + 0.3f) - x * std::tanh (0.3f);
                case 3:  return 0.5f * x * std::abs (x);
                case 4:  { const float ax = std::abs (x); return 1.5f * (ax <= 1.0f ? 0.5f * x * x - x * x * x * x / 12.0f : (2.0f / 3.0f) * ax - 0.25f); }
                case 5:  { const float ax = std::abs (x); return ax - std::log1p (ax); }
                case 6:  return x >= 0.0f ? logCosh (x) : 0.16f * logCosh (x / 0.4f);
                case 7:  return (2.0f / kPi) * (x * std::atan (x) - 0.5f * std::log1p (x * x));
                case 8:  return logCosh (x);
                case 9:  return x > 0.0f ? 0.5f * x * x : 0.0f;
                case 10: { const float y = 2.0f * x, ay = std::abs (y); return 0.25f * (ay <= 1.0f ? 0.5f * y * y : ay - 0.5f); }
                default: { const float ax = std::abs (x); return ax + (std::exp (-3.0f * ax) - 1.0f) / 3.0f; }
            }
        }
        std::atomic<float> *type, *drive, *tone, *mix;
        double fs = 44100.0;
        float prev[2] {}, dcX[2] {}, dcY[2] {};
        OnePole lp;
        juce::SmoothedValue<float> driveS, mixS;
    };

    //==========================================================================
    class CrushModule : public FxModule
    {
    public:
        explicit CrushModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, crushOn), bits (p (crushBits)), down (p (crushDown)), mix (p (crushMix)) {}
        void prepare (double sr, int) override { mixS.reset (sr, 0.02); reset(); }
        void reset() override { counter = 0; held[0] = held[1] = 0; mixS.setCurrentAndTargetValue (mix->load()); }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            const float levels = std::pow (2.0f, juce::jlimit (2.0f, 16.0f, bits->load()) - 1.0f);
            const float dn = juce::jmax (1.0f, down->load());
            mixS.setTargetValue (mix->load());
            for (int s = 0; s < n; ++s)
            {
                counter += 1.0f;
                if (counter >= dn)
                {
                    counter -= dn;
                    held[0] = std::round (L[s] * levels) / levels;
                    held[1] = std::round (R[s] * levels) / levels;
                }
                const float m = mixS.getNextValue();
                L[s] += m * (held[0] - L[s]); R[s] += m * (held[1] - R[s]);
            }
        }
    private:
        std::atomic<float> *bits, *down, *mix;
        float counter = 0, held[2] {};
        juce::SmoothedValue<float> mixS;
    };

    //==========================================================================
    class EqModule : public FxModule
    {
    public:
        explicit EqModule (juce::AudioProcessorValueTreeState& s) : FxModule (s, eqOn)
        {
            for (auto* id : { &eqLowFreq, &eqLowGain, &eqMidFreq, &eqMidGain, &eqMidQ, &eqHighFreq, &eqHighGain }) prm.push_back (p (*id));
        }
        void prepare (double sr, int) override { fs = sr; cached.fill (-1.0e9f); reset(); }
        void reset() override { for (auto& b : band) b.reset(); }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            bool changed = false;
            for (size_t i = 0; i < prm.size(); ++i) { const float v = prm[i]->load(); if (v != cached[i]) { cached[i] = v; changed = true; } }
            if (changed)
            {
                const double ny = fs * 0.45;
                band[0].lowShelf (fs, juce::jmin ((double) cached[0], ny), cached[1]);
                band[1].peak (fs, juce::jmin ((double) cached[2], ny), cached[3], cached[4]);
                band[2].highShelf (fs, juce::jmin ((double) cached[5], ny), cached[6]);
            }
            for (int s = 0; s < n; ++s)
            {
                float l = L[s], r = R[s];
                for (auto& b : band) { l = b.process (0, l); r = b.process (1, r); }
                L[s] = l; R[s] = r;
            }
        }
    private:
        std::vector<std::atomic<float>*> prm;
        std::array<float, 7> cached {};
        Biquad band[3];
        double fs = 44100.0;
    };

    //==========================================================================
    class CompModule : public FxModule
    {
    public:
        explicit CompModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, compOn), thr (p (compThresh)), ratio (p (compRatio)), att (p (compAttack)), rel (p (compRelease)),
              makeup (p (compMakeup)), mix (p (compMix)) {}
        void prepare (double sr, int maxBlock) override
        {
            comp.prepare ({ sr, (juce::uint32) maxBlock, 2 });
            wet.setSize (2, maxBlock);
            mixS.reset (sr, 0.02); mkS.reset (sr, 0.02);
            reset();
        }
        void reset() override
        {
            comp.reset();
            mixS.setCurrentAndTargetValue (mix->load()); mkS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (makeup->load()));
        }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            comp.setThreshold (thr->load()); comp.setRatio (juce::jmax (1.0f, ratio->load()));
            comp.setAttack (att->load()); comp.setRelease (rel->load());
            mixS.setTargetValue (mix->load()); mkS.setTargetValue (juce::Decibels::decibelsToGain (makeup->load()));
            wet.copyFrom (0, 0, L, n); wet.copyFrom (1, 0, R, n);
            juce::dsp::AudioBlock<float> b (wet.getArrayOfWritePointers(), 2, 0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (b);
            comp.process (ctx);
            const float* wl = wet.getReadPointer (0); const float* wr = wet.getReadPointer (1);
            for (int s = 0; s < n; ++s)
            {
                const float m = mixS.getNextValue(), mk = mkS.getNextValue();
                L[s] += m * (wl[s] * mk - L[s]); R[s] += m * (wr[s] * mk - R[s]);
            }
        }
    private:
        std::atomic<float> *thr, *ratio, *att, *rel, *makeup, *mix;
        juce::dsp::Compressor<float> comp;
        juce::AudioBuffer<float> wet;
        juce::SmoothedValue<float> mixS, mkS;
    };

    //==========================================================================
    class MultibandModule : public FxModule
    {
    public:
        explicit MultibandModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, mbOn), depth (p (mbDepth)), time (p (mbTime)), out (p (mbOut)), lowX (p (mbLowX)), highX (p (mbHighX))
        {
            for (int b = 0; b < 3; ++b)
            {
                gain[b] = p (mbBand (b, "gain")); dthr[b] = p (mbBand (b, "dthr")); drat[b] = p (mbBand (b, "dratio"));
                uthr[b] = p (mbBand (b, "uthr")); uamt[b] = p (mbBand (b, "uamt"));
            }
        }
        void prepare (double sr, int maxBlock) override
        {
            fs = sr;
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
            for (auto* f : { &lr1, &lr2, &ap }) f->prepare (spec);
            curLo = lowX->load(); curHi = juce::jmax (curLo * 1.5f, highX->load());
            lr1.setCutoffFrequency (curLo); lr2.setCutoffFrequency (curHi); ap.setCutoffFrequency (curHi);
            ap.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
            depthS.reset (sr, 0.02); outS.reset (sr, 0.02);
            reset();
        }
        void reset() override
        {
            lr1.reset(); lr2.reset(); ap.reset();
            for (auto& e : env) e = 1.0e-5f;
            depthS.setCurrentAndTargetValue (depth->load()); outS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (out->load()));
        }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            const float t = time->load();
            const float attMs = 0.5f + t * 25.0f, relMs = 20.0f + t * 380.0f;
            const float aA = 1.0f - std::exp (-1.0f / (0.001f * attMs * (float) fs));
            const float aR = 1.0f - std::exp (-1.0f / (0.001f * relMs * (float) fs));
            depthS.setTargetValue (depth->load()); outS.setTargetValue (juce::Decibels::decibelsToGain (out->load()));
            const float lo = juce::jlimit (20.0f, (float) fs * 0.2f, lowX->load()), hi = juce::jlimit (lo * 1.5f, (float) fs * 0.45f, highX->load());
            if (lo != curLo) { curLo = lo; lr1.setCutoffFrequency (lo); }
            if (hi != curHi) { curHi = hi; lr2.setCutoffFrequency (hi); ap.setCutoffFrequency (hi); }
            float bGain[3], bDt[3], bDs[3], bUt[3], bUs[3];
            for (int b = 0; b < 3; ++b)
            {
                bGain[b] = juce::Decibels::decibelsToGain (gain[b]->load()); bDt[b] = dthr[b]->load();
                bDs[b] = 1.0f - 1.0f / juce::jmax (1.0f, drat[b]->load()); bUt[b] = uthr[b]->load(); bUs[b] = uamt[b]->load();
            }
            for (int s = 0; s < n; ++s)
            {
                float band[3][2];
                for (int c = 0; c < 2; ++c)
                {
                    const float x = c == 0 ? L[s] : R[s];
                    float low, rest, mid, high;
                    lr1.processSample (c, x, low, rest);
                    lr2.processSample (c, rest, mid, high);
                    low = ap.processSample (c, low);   // phase-align the low band with the mid/high split
                    band[0][c] = low; band[1][c] = mid; band[2][c] = high;
                }
                float sumL = 0, sumR = 0;
                for (int b = 0; b < 3; ++b)
                {
                    const float lvl = juce::jmax (std::abs (band[b][0]), std::abs (band[b][1]));
                    env[b] += (lvl > env[b] ? aA : aR) * (lvl - env[b]);
                    env[b] = juce::jmax (env[b], 1.0e-6f);
                    const float dbl = juce::Decibels::gainToDecibels (env[b]);
                    float gDb = 0.0f;
                    if (dbl > bDt[b]) gDb -= (dbl - bDt[b]) * bDs[b];                                                 // downward (default 4:1 over -24 dB)
                    else if (dbl < bUt[b] && dbl > -80.0f) gDb += juce::jmin (18.0f, (bUt[b] - dbl) * bUs[b]);         // upward (default ~3:1 under -40 dB)
                    const float g = juce::Decibels::decibelsToGain (gDb) * bGain[b];
                    sumL += band[b][0] * g; sumR += band[b][1] * g;
                }
                const float dpt = depthS.getNextValue(), og = outS.getNextValue();
                L[s] = (L[s] + dpt * (sumL - L[s])) * og;
                R[s] = (R[s] + dpt * (sumR - R[s])) * og;
            }
        }
    private:
        std::atomic<float> *depth, *time, *out, *lowX, *highX;
        std::atomic<float> *gain[3] {}, *dthr[3] {}, *drat[3] {}, *uthr[3] {}, *uamt[3] {};
        float curLo = 150.0f, curHi = 2500.0f;
        juce::dsp::LinkwitzRileyFilter<float> lr1, lr2, ap;
        float env[3] {};
        double fs = 44100.0;
        juce::SmoothedValue<float> depthS, outS;
    };

    //==========================================================================
    class TransientModule : public FxModule
    {
    public:
        explicit TransientModule (juce::AudioProcessorValueTreeState& s) : FxModule (s, transOn), att (p (transAttack)), sus (p (transSustain)) {}
        void prepare (double sr, int) override
        {
            auto coef = [sr] (float msv) { return 1.0f - std::exp (-1.0f / (0.001f * msv * (float) sr)); };
            fastA = coef (0.5f); fastR = coef (40.0f); slowA = coef (25.0f); slowR = coef (250.0f);
            reset();
        }
        void reset() override { fast = slow = 0.0f; gSm = 1.0f; }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            const float a = att->load(), su = sus->load();
            for (int s = 0; s < n; ++s)
            {
                const float lvl = juce::jmax (std::abs (L[s]), std::abs (R[s]));
                fast += (lvl > fast ? fastA : fastR) * (lvl - fast);
                slow += (lvl > slow ? slowA : slowR) * (lvl - slow);
                const float tr = juce::jlimit (0.0f, 1.0f, (fast - slow) / (fast + 1.0e-5f));
                const float body = juce::jlimit (0.0f, 1.0f, slow / (fast + 1.0e-5f));
                const float g = std::pow (2.0f, a * 2.0f * tr) * std::pow (2.0f, su * 1.5f * body * (1.0f - tr));
                gSm += 0.02f * (g - gSm);
                L[s] *= gSm; R[s] *= gSm;
            }
            fast = dsp::sanitise (fast); slow = dsp::sanitise (slow);
        }
    private:
        std::atomic<float> *att, *sus;
        float fastA = 0, fastR = 0, slowA = 0, slowR = 0, fast = 0, slow = 0, gSm = 1.0f;
    };

    //==========================================================================
    class ChorusModule : public FxModule
    {
    public:
        ChorusModule (juce::AudioProcessorValueTreeState& s, FXChain::Display& d)
            : FxModule (s, ids::choOn), disp (d), rate (p (ids::choRate)), depth (p (ids::choDepth)), mix (p (ids::choMix)) {}
        void prepare (double sr, int maxBlock) override { chorus.prepare ({ sr, (juce::uint32) maxBlock, 2 }); dim.prepare (sr); mixS.reset (sr, 0.03); reset(); }
        void reset() override { chorus.reset(); dim.reset(); mixS.setCurrentAndTargetValue (0); lastMix = 0.0f; }
        bool wantsToRun (const FxContext& c) const noexcept override { return isOn() || c.macros[1] > 0.001f; }
        void process (float* L, float* R, int n, const FxContext& c) noexcept override
        {
            const bool on = isOn();
            const float depthT = clamp01 (depth->load() + c.macros[1] * 0.5f + c.mods.chorusDepth);
            mixS.setTargetValue (clamp01 (on ? mix->load() : c.macros[1] * 0.6f));
            mixS.skip (n);
            disp.chorus = depthT;
            if (engine->load() > 0.5f)
            {
                // Divine 2.0: dimension chorus (true stereo; per-sample mix ramp)
                const float m1 = mixS.getCurrentValue();
                dim.process (L, R, n, rate->load(), depthT, lastMix, m1);
                lastMix = m1;
                return;
            }
            lastMix = mixS.getCurrentValue();
            chorus.setRate (rate->load()); chorus.setDepth (depthT); chorus.setCentreDelay (7.0f);
            chorus.setFeedback (0.0f); chorus.setMix (mixS.getCurrentValue());
            float* chans[2] = { L, R };
            juce::dsp::AudioBlock<float> b (chans, 2, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (b);
            chorus.process (ctx);
        }
    private:
        FXChain::Display& disp;
        std::atomic<float> *rate, *depth, *mix;
        std::atomic<float>* engine = p (ids::engineMode);
        juce::dsp::Chorus<float> chorus;
        dsp::DimensionChorus dim;
        juce::SmoothedValue<float> mixS;
        float lastMix = 0.0f;
    };

    //==========================================================================
    class FlangerModule : public FxModule
    {
    public:
        explicit FlangerModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, flangOn), rate (p (flangRate)), depth (p (flangDepth)), fb (p (flangFb)), mix (p (flangMix)) {}
        void prepare (double sr, int) override
        {
            fs = sr;
            for (auto& b : buf) b.assign ((size_t) (sr * 0.02) + 4, 0.0f);
            mixS.reset (sr, 0.02);
            reset();
        }
        void reset() override { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); w = 0; phase = 0; mixS.setCurrentAndTargetValue (mix->load()); }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            const int size = (int) buf[0].size();
            const float inc = rate->load() / (float) fs, dp = depth->load(), f = juce::jlimit (-0.95f, 0.95f, fb->load());
            mixS.setTargetValue (mix->load());
            float* ch[2] = { L, R };
            for (int s = 0; s < n; ++s)
            {
                const float m = mixS.getNextValue();
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = 0.5f + 0.5f * std::sin (2.0f * kPi * (phase + (c == 1 ? 0.25f : 0.0f)));
                    const float d = (float) fs * (0.0005f + dp * 0.0045f * lfo);
                    float rp = (float) w - d; if (rp < 0) rp += (float) size;
                    const int i0 = juce::jlimit (0, size - 1, (int) rp), i1 = (i0 + 1) % size;
                    const float fr = rp - (float) (int) rp;
                    const float del = buf[c][(size_t) i0] + fr * (buf[c][(size_t) i1] - buf[c][(size_t) i0]);
                    const float x = ch[c][s];
                    buf[c][(size_t) w] = dsp::sanitise (std::tanh (x + f * del));
                    ch[c][s] = x * (1.0f - 0.5f * m) + del * 0.5f * m;
                }
                w = (w + 1) % size;
                phase += inc; if (phase >= 1.0f) phase -= 1.0f;
            }
        }
    private:
        std::atomic<float> *rate, *depth, *fb, *mix;
        std::vector<float> buf[2];
        int w = 0; float phase = 0; double fs = 44100.0;
        juce::SmoothedValue<float> mixS;
    };

    //==========================================================================
    class PhaserModule : public FxModule
    {
    public:
        explicit PhaserModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, phaseOn), rate (p (phaseRate)), depth (p (phaseDepth)), fb (p (phaseFb)), centre (p (phaseCentre)), mix (p (phaseMix)) {}
        void prepare (double sr, int maxBlock) override { fs = sr; spec = { sr, (juce::uint32) maxBlock, 2 }; phaser.prepare (spec); reset(); }
        // juce::dsp::Phaser::reset() keeps its smoothed LFO depth / filter state: re-preparing with the same spec
        // (no reallocation at the same size) makes a restarted phaser sound the same every time
        void reset() override
        {
            // juce::dsp::Phaser's smoothers restart from their last *target* on reset(): park the targets first so a
            // restarted phaser always starts from the same state as a fresh one
            phaser.setDepth (0.0f); phaser.setFeedback (0.0f); phaser.setMix (0.0f);
            phaser.prepare (spec); phaser.reset();
        }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            phaser.setRate (rate->load()); phaser.setDepth (depth->load());
            phaser.setCentreFrequency (juce::jmin (centre->load(), (float) (fs * 0.4)));
            phaser.setFeedback (juce::jlimit (-0.95f, 0.95f, fb->load())); phaser.setMix (mix->load());
            float* chans[2] = { L, R };
            juce::dsp::AudioBlock<float> b (chans, 2, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (b);
            phaser.process (ctx);
        }
    private:
        std::atomic<float> *rate, *depth, *fb, *centre, *mix;
        juce::dsp::Phaser<float> phaser;
        juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
        double fs = 44100.0;
    };

    //==========================================================================
    class TremoloModule : public FxModule
    {
    public:
        explicit TremoloModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, tremOn), mode (p (tremMode)), shape (p (tremShape)), sync (p (tremSync)), rate (p (tremRate)),
              div (p (tremDiv)), depth (p (tremDepth)) {}
        void prepare (double sr, int) override { fs = sr; reset(); }
        void reset() override { phase = 0; sm = 1.0f; }
        void process (float* L, float* R, int n, const FxContext& c) noexcept override
        {
            const double hz = sync->load() > 0.5f ? (c.bpm / 60.0) / lfoDivisionInBeats ((int) std::lround (div->load())) : (double) rate->load();
            const float inc = (float) (hz / fs), dp = depth->load();
            const int sh = (int) std::lround (shape->load());
            const bool pan = std::lround (mode->load()) == 1;
            const float smA = 1.0f - std::exp (-1.0f / (0.002f * (float) fs));   // de-click square edges
            for (int s = 0; s < n; ++s)
            {
                const float u0 = sh == 0 ? 0.5f + 0.5f * std::cos (2.0f * kPi * phase)
                               : sh == 1 ? (phase < 0.5f ? 1.0f - 2.0f * phase : 2.0f * phase - 1.0f)
                                         : (phase < 0.5f ? 1.0f : 0.0f);
                sm += smA * (u0 - sm);
                if (pan)
                {
                    const float pp = (sm * 2.0f - 1.0f) * dp;
                    L[s] *= juce::jmin (1.0f, 1.0f - pp); R[s] *= juce::jmin (1.0f, 1.0f + pp);
                }
                else
                {
                    const float g = 1.0f - dp * (1.0f - sm);
                    L[s] *= g; R[s] *= g;
                }
                phase += inc; if (phase >= 1.0f) phase -= 1.0f;
            }
        }
    private:
        std::atomic<float> *mode, *shape, *sync, *rate, *div, *depth;
        double fs = 44100.0; float phase = 0, sm = 1.0f;
    };

    //==========================================================================
    class RingModule : public FxModule
    {
    public:
        explicit RingModule (juce::AudioProcessorValueTreeState& s) : FxModule (s, ringOn), freq (p (ringFreq)), mix (p (ringMix)) {}
        void prepare (double sr, int) override { fs = sr; freqS.reset (sr, 0.03); mixS.reset (sr, 0.02); reset(); }
        void reset() override { phase = 0; freqS.setCurrentAndTargetValue (freq->load()); mixS.setCurrentAndTargetValue (mix->load()); }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            freqS.setTargetValue (freq->load()); mixS.setTargetValue (mix->load());
            for (int s = 0; s < n; ++s)
            {
                const float car = std::sin (2.0f * kPi * phase), m = mixS.getNextValue();
                L[s] += m * (L[s] * car - L[s]); R[s] += m * (R[s] * car - R[s]);
                phase += freqS.getNextValue() / (float) fs; if (phase >= 1.0f) phase -= 1.0f;
            }
        }
    private:
        std::atomic<float> *freq, *mix;
        double fs = 44100.0; float phase = 0;
        juce::SmoothedValue<float> freqS, mixS;
    };

    //==========================================================================
    /** SSB frequency shifter: 90-degree IIR Hilbert pair (Niemitalo coefficients). */
    class FreqShiftModule : public FxModule
    {
    public:
        explicit FreqShiftModule (juce::AudioProcessorValueTreeState& s) : FxModule (s, fshiftOn), shift (p (fshiftShift)), mix (p (fshiftMix)) {}
        void prepare (double sr, int) override { fs = sr; mixS.reset (sr, 0.02); reset(); }
        void reset() override
        {
            for (auto& c : st) { for (auto& a : c.a) a = {}; for (auto& b : c.b) b = {}; c.delay = 0; }
            phase = 0; mixS.setCurrentAndTargetValue (mix->load());
        }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            static const float kA[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static const float kB[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            const float v = shift->load();
            const float hz = (v < 0 ? -1.0f : 1.0f) * std::pow (std::abs (v), 3.0f) * 2000.0f;
            mixS.setTargetValue (mix->load());
            float* ch[2] = { L, R };
            for (int s = 0; s < n; ++s)
            {
                const float co = std::cos (2.0f * kPi * phase), si = std::sin (2.0f * kPi * phase), m = mixS.getNextValue();
                for (int c = 0; c < 2; ++c)
                {
                    const float x = ch[c][s];
                    float i = x, q = x;
                    for (int k = 0; k < 4; ++k) { i = st[c].a[k].tick (i, kA[k]); q = st[c].b[k].tick (q, kB[k]); }
                    const float iDel = st[c].delay; st[c].delay = i;
                    const float y = iDel * co - q * si;
                    ch[c][s] = x + m * (y - x);
                }
                phase += hz / (float) fs; phase -= std::floor (phase);
            }
        }
    private:
        struct AP
        {
            float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            float tick (float x, float a) noexcept { const float y = a * a * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = dsp::sanitise (y); return y; }
        };
        struct Chan { AP a[4], b[4]; float delay = 0; } st[2];
        std::atomic<float> *shift, *mix;
        double fs = 44100.0; float phase = 0;
        juce::SmoothedValue<float> mixS;
    };

    //==========================================================================
    class StereoDelay
    {
    public:
        void prepare (double sr, int maxSeconds)
        {
            sampleRate = sr;
            const size_t size = (size_t) (sr * maxSeconds) + 4;
            bufL.assign (size, 0.0f); bufR.assign (size, 0.0f);
            time.reset (sr, 0.25);
            reset();
        }
        void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); writePos = 0; lpL = lpR = hpL = hpR = 0.0f; }
        void process (float* L, float* R, int n, float delaySeconds, float feedback, float mix, float tone, bool pingPong, bool v2 = false) noexcept
        {
            const float hpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 140.0f / (float) sampleRate);
            const int size = (int) bufL.size();
            if (size < 8) return;
            time.setTargetValue (juce::jlimit (0.001f, (float) (size - 4) / (float) sampleRate, delaySeconds));
            const float toneHz = 800.0f * std::pow (2.0f, tone * 4.5f);
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz / (float) sampleRate);
            auto read = [size] (const std::vector<float>& b, float pos)
            {
                if (pos < 0) pos += (float) size;
                const int i0 = juce::jlimit (0, size - 1, (int) pos), i1 = (i0 + 1) % size;
                const float f = pos - (float) (int) pos;
                return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
            };
            for (int s = 0; s < n; ++s)
            {
                const float d = time.getNextValue() * (float) sampleRate;
                const float rp = (float) writePos - d;
                const float outL = read (bufL, rp), outR = read (bufR, rp);
                lpL = dsp::sanitise (lpL + a * (outL - lpL)); lpR = dsp::sanitise (lpR + a * (outR - lpR));
                float fbL = lpL, fbR = lpR;
                if (v2)
                {
                    // Divine 2.0: repeats lose their low-end mud and saturate softly instead of piling up
                    hpL = dsp::sanitise (hpL + hpA * (lpL - hpL)); hpR = dsp::sanitise (hpR + hpA * (lpR - hpR));
                    fbL = dsp::fastTanh ((lpL - hpL) * 1.15f) / 1.15f;
                    fbR = dsp::fastTanh ((lpR - hpR) * 1.15f) / 1.15f;
                }
                const float inL = L[s], inR = R[s];
                if (pingPong) { bufL[(size_t) writePos] = 0.5f * (inL + inR) + feedback * fbR; bufR[(size_t) writePos] = feedback * fbL; }
                else          { bufL[(size_t) writePos] = inL + feedback * fbL; bufR[(size_t) writePos] = inR + feedback * fbR; }
                L[s] += mix * outL; R[s] += mix * outR;
                writePos = (writePos + 1) % size;
            }
        }
    private:
        double sampleRate = 44100.0;
        std::vector<float> bufL, bufR;
        int writePos = 0;
        juce::SmoothedValue<float> time;
        float lpL = 0, lpR = 0, hpL = 0, hpR = 0;
    };

    class DelayModule : public FxModule
    {
    public:
        DelayModule (juce::AudioProcessorValueTreeState& s, FXChain::Display& d)
            : FxModule (s, ids::dlyOn), disp (d), div (p (ids::dlyDiv)), fb (p (ids::dlyFeedback)), mix (p (ids::dlyMix)),
              ping (p (ids::dlyPingPong)), tone (p (ids::dlyTone)) {}
        void prepare (double sr, int) override { delay.prepare (sr, 4); mixS.reset (sr, 0.03); reset(); }
        void reset() override { delay.reset(); mixS.setCurrentAndTargetValue (0); }
        bool wantsToRun (const FxContext& c) const noexcept override { return isOn() || c.mods.delayMix > 0.001f; }
        void process (float* L, float* R, int n, const FxContext& c) noexcept override
        {
            const float mixT = clamp01 ((isOn() ? mix->load() : 0.0f) + c.mods.delayMix);
            disp.delay = mixT;
            mixS.setTargetValue (mixT); mixS.skip (n);
            const double beats = delayDivisionInBeats ((int) std::lround (div->load()));
            const float secs = (float) (beats * 60.0 / juce::jmax (20.0, c.bpm));
            delay.process (L, R, n, secs, fb->load(), mixS.getCurrentValue(), tone->load(), ping->load() > 0.5f, engine->load() > 0.5f);
        }
    private:
        FXChain::Display& disp;
        std::atomic<float> *div, *fb, *mix, *ping, *tone;
        std::atomic<float>* engine = p (ids::engineMode);
        StereoDelay delay;
        juce::SmoothedValue<float> mixS;
    };

    //==========================================================================
    class ReverbModule : public FxModule
    {
    public:
        ReverbModule (juce::AudioProcessorValueTreeState& s, FXChain::Display& d)
            : FxModule (s, ids::revOn), disp (d), size (p (ids::revSize)), damp (p (ids::revDamp)), width (p (ids::revWidth)), mix (p (ids::revMix)) {}
        void prepare (double sr, int maxBlock) override
        {
            reverb.prepare ({ sr, (juce::uint32) maxBlock, 2 }); divine.prepare (sr);
            wet.setSize (2, maxBlock); mixS.reset (sr, 0.03); sizeS.reset (sr, 0.1); preS.reset (sr, 0.15); reset();
        }
        void reset() override { reverb.reset(); divine.reset(); mixS.setCurrentAndTargetValue (0); sizeS.setCurrentAndTargetValue (size->load()); preS.setCurrentAndTargetValue (predelay->load()); }
        bool wantsToRun (const FxContext& c) const noexcept override { return isOn() || c.macros[3] > 0.001f || c.mods.reverbMix > 0.001f; }
        void process (float* L, float* R, int n, const FxContext& c) noexcept override
        {
            const float mixT = clamp01 ((isOn() ? mix->load() : 0.0f) + c.macros[3] * 0.5f + c.mods.reverbMix);
            disp.reverb = mixT;
            mixS.setTargetValue (mixT);
            if (engine->load() > 0.5f)
            {
                // Divine 2.0: FDN hall (smooth, dense, modulated tail; pre-delay + early reflections)
                sizeS.setTargetValue (clamp01 (size->load() + c.macros[3] * 0.2f + c.mods.reverbSize));
                preS.setTargetValue (predelay->load());
                sizeS.skip (n); preS.skip (n);
                divine.process (L, R, wet.getWritePointer (0), wet.getWritePointer (1), n, sizeS.getCurrentValue(), damp->load(),
                                clamp01 (width->load() + c.mods.width), preS.getCurrentValue());
                const float* wl = wet.getReadPointer (0); const float* wr = wet.getReadPointer (1);
                // wet gain tracks room size (calibrated with SoundLab: ~3 dB under the Classic
                // reverb at every size, so presets keep their space without drowning in it)
                const float g = kDivineWet * juce::Decibels::decibelsToGain (4.0f * (sizeS.getCurrentValue() - 0.6f));   // bigger rooms bloom louder, like the Classic reverb
                for (int s = 0; s < n; ++s)
                {
                    const float m = mixS.getNextValue(), dryG = 1.0f - 0.4f * m;
                    L[s] = L[s] * dryG + wl[s] * m * g;
                    R[s] = R[s] * dryG + wr[s] * m * g;
                }
                return;
            }
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize = clamp01 (size->load() + c.macros[3] * 0.2f + c.mods.reverbSize);
            rp.damping = damp->load();
            rp.width = clamp01 (width->load() + c.mods.width);
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
            reverb.setParameters (rp);
            wet.copyFrom (0, 0, L, n); wet.copyFrom (1, 0, R, n);
            juce::dsp::AudioBlock<float> b (wet.getArrayOfWritePointers(), 2, 0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (b);
            reverb.process (ctx);
            const float* wl = wet.getReadPointer (0); const float* wr = wet.getReadPointer (1);
            for (int s = 0; s < n; ++s)
            {
                const float m = mixS.getNextValue(), dryG = 1.0f - 0.4f * m;
                L[s] = L[s] * dryG + wl[s] * m * 0.45f;
                R[s] = R[s] * dryG + wr[s] * m * 0.45f;
            }
        }
    private:
        FXChain::Display& disp;
        std::atomic<float> *size, *damp, *width, *mix;
        std::atomic<float>* predelay = p (ids::revPredelay);
        std::atomic<float>* engine = p (ids::engineMode);
        juce::dsp::Reverb reverb;
        dsp::DivineReverb divine;
        juce::AudioBuffer<float> wet;
        juce::SmoothedValue<float> mixS, sizeS, preS;
        static constexpr float kDivineWet = 1.6f;   // calibrated with SoundLab: wet sits ~3 dB under the Classic reverb (less wash, same space)
    };

    //==========================================================================
    class StereoModule : public FxModule
    {
    public:
        explicit StereoModule (juce::AudioProcessorValueTreeState& s)
            : FxModule (s, stereoOn), width (p (stereoWidth)), pan (p (stereoPan)), mono (p (stereoMonoBass)), monoFreq (p (stereoMonoFreq)), gain (p (stereoGain)) {}
        void prepare (double sr, int maxBlock) override
        {
            split.prepare ({ sr, (juce::uint32) maxBlock, 2 });
            for (auto* sv : { &widthS, &panS, &gainS }) sv->reset (sr, 0.03);
            reset();
        }
        void reset() override
        {
            split.reset();
            widthS.setCurrentAndTargetValue (width->load()); panS.setCurrentAndTargetValue (pan->load());
            gainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gain->load()));
        }
        void process (float* L, float* R, int n, const FxContext&) noexcept override
        {
            widthS.setTargetValue (width->load()); panS.setTargetValue (pan->load());
            gainS.setTargetValue (juce::Decibels::decibelsToGain (gain->load()));
            const bool monoBass = mono->load() > 0.5f;
            split.setCutoffFrequency (monoFreq->load());
            for (int s = 0; s < n; ++s)
            {
                float l = L[s], r = R[s];
                if (monoBass)
                {
                    float ll, lh, rl, rh;
                    split.processSample (0, l, ll, lh);
                    split.processSample (1, r, rl, rh);
                    const float m = 0.5f * (ll + rl);
                    l = m + lh; r = m + rh;
                }
                const float w = widthS.getNextValue(), mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
                l = mid + side; r = mid - side;
                const float pp = panS.getNextValue(), g = gainS.getNextValue();
                L[s] = l * juce::jmin (1.0f, 1.0f - pp) * g;
                R[s] = r * juce::jmin (1.0f, 1.0f + pp) * g;
            }
        }
    private:
        std::atomic<float> *width, *pan, *mono, *monoFreq, *gain;
        juce::dsp::LinkwitzRileyFilter<float> split;
        juce::SmoothedValue<float> widthS, panS, gainS;
    };
}

//==============================================================================
FXChain::FXChain (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    // catalogue order (see FxCatalog.cpp)
    modules.push_back (std::make_unique<SatModule> (s, display));
    modules.push_back (std::make_unique<DistModule> (s));
    modules.push_back (std::make_unique<CrushModule> (s));
    modules.push_back (std::make_unique<EqModule> (s));
    modules.push_back (std::make_unique<CompModule> (s));
    modules.push_back (std::make_unique<MultibandModule> (s));
    modules.push_back (std::make_unique<TransientModule> (s));
    modules.push_back (std::make_unique<ChorusModule> (s, display));
    modules.push_back (std::make_unique<FlangerModule> (s));
    modules.push_back (std::make_unique<PhaserModule> (s));
    modules.push_back (std::make_unique<TremoloModule> (s));
    modules.push_back (std::make_unique<RingModule> (s));
    modules.push_back (std::make_unique<FreqShiftModule> (s));
    modules.push_back (std::make_unique<DelayModule> (s, display));
    modules.push_back (std::make_unique<ReverbModule> (s, display));
    modules.push_back (std::make_unique<StereoModule> (s));
    jassert (modules.size() == fxCatalog().size());

    limOn = s.getRawParameterValue (ids::limOn);
    limCeiling = s.getRawParameterValue (ids::limCeiling);
    master = s.getRawParameterValue (ids::master);
    pan = s.getRawParameterValue (ids::masterPan);
    setOrder (defaultFxOrder());
}

FXChain::~FXChain() = default;

void FXChain::setOrder (const juce::StringArray& keys)
{
    const auto clean = sanitiseFxOrder (keys);
    const juce::SpinLock::ScopedLockType lock (orderLock);
    numPending = juce::jmin ((int) pendingOrder.size(), clean.size());
    for (int i = 0; i < numPending; ++i) pendingOrder[(size_t) i] = fxIndexForKey (clean[i]);
    orderDirty = true;
}

juce::StringArray FXChain::getOrder() const
{
    juce::StringArray out;
    const juce::SpinLock::ScopedLockType lock (orderLock);
    const auto& src = orderDirty ? pendingOrder : order;
    const int n = orderDirty ? numPending : numOrdered;
    for (int i = 0; i < n; ++i) out.add (fxCatalog()[(size_t) src[(size_t) i]].key);
    return out;
}

void FXChain::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = juce::jmax (32, maxBlockSize);
    for (auto& m : modules)
    {
        m->prepare (sr, maxBlock);
        m->enable.reset (sr, 0.02);
    }
    limiter.prepare ({ sr, (juce::uint32) maxBlock, 2 });
    dry.setSize (2, maxBlock);
    masterS.reset (sr, 0.03);
    for (auto* sv : { &widthS, &airS, &panS }) sv->reset (sr, 0.03);
    reset();
}

void FXChain::reset()
{
    for (auto& m : modules) { m->reset(); m->enable.setCurrentAndTargetValue (0.0f); m->running = false; }
    limiter.reset();
    masterS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (master->load(), -60.0f));
    widthS.setCurrentAndTargetValue (0.0f); airS.setCurrentAndTargetValue (0.0f); panS.setCurrentAndTargetValue (pan->load());
    airL = airR = sideLo = 0.0f;
}

void FXChain::process (juce::AudioBuffer<float>& buffer, double bpm, const SynthEngine::GlobalMods& mods,
                       const std::array<float, kNumMacros>& macros) noexcept
{
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || total == 0) return;

    {   // pick up a new chain order without ever blocking the audio thread
        const juce::SpinLock::ScopedTryLockType lock (orderLock);
        if (lock.isLocked() && orderDirty)
        {
            order = pendingOrder; numOrdered = numPending; orderDirty = false;
        }
    }

    FxContext ctx;
    ctx.sampleRate = sampleRate; ctx.bpm = bpm; ctx.mods = mods; ctx.macros = macros;
    masterS.setTargetValue (juce::Decibels::decibelsToGain (master->load(), -60.0f));

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getWritePointer (1, start);

        for (int oi = 0; oi < numOrdered; ++oi)
        {
            auto& m = *modules[(size_t) order[(size_t) oi]];
            const bool want = m.wantsToRun (ctx);
            m.enable.setTargetValue (want ? 1.0f : 0.0f);
            if (! want && ! m.enable.isSmoothing() && m.enable.getCurrentValue() <= 0.0f)
            {
                if (m.running) { m.reset(); m.running = false; }   // free CPU and clear tails while off
                continue;
            }
            if (! m.running) { m.reset(); m.running = true; }

            if (m.enable.isSmoothing() || m.enable.getCurrentValue() < 1.0f)
            {
                // click-free bypass: crossfade between the dry input and the module output
                dry.copyFrom (0, 0, L, n); dry.copyFrom (1, 0, R, n);
                m.process (L, R, n, ctx);
                const float* dl = dry.getReadPointer (0); const float* dr = dry.getReadPointer (1);
                for (int s = 0; s < n; ++s)
                {
                    const float g = m.enable.getNextValue();
                    L[s] = dl[s] + g * (L[s] - dl[s]);
                    R[s] = dr[s] + g * (R[s] - dr[s]);
                }
            }
            else
                m.process (L, R, n, ctx);
        }

        // WIDTH / AIR macros + master pan: a final polish stage before the master gain.
        // Width only widens above ~180 Hz (the low end stays mono); AIR is a gentle high shelf.
        {
            widthS.setTargetValue (macros[5]); airS.setTargetValue (macros[7]); panS.setTargetValue (pan->load());
            if (widthS.isSmoothing() || airS.isSmoothing() || panS.isSmoothing()
                || widthS.getCurrentValue() > 1.0e-4f || airS.getCurrentValue() > 1.0e-4f || std::abs (panS.getCurrentValue()) > 1.0e-4f)
            {
                const float aLo = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 180.0f / (float) sampleRate);
                const float aHi = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 7000.0f / (float) sampleRate);
                for (int s = 0; s < n; ++s)
                {
                    const float w = widthS.getNextValue(), air = airS.getNextValue(), pn = panS.getNextValue();
                    float l = L[s], r = R[s];
                    // AIR: add the band above ~7 kHz back in (shelf up to +7 dB)
                    airL += aHi * (l - airL); airR += aHi * (r - airR);
                    l += (l - airL) * air * 1.25f; r += (r - airR) * air * 1.25f;
                    // WIDTH: scale the side signal of everything above the crossover
                    sideLo += aLo * (0.5f * (l - r) - sideLo);
                    const float side = 0.5f * (l - r), mid = 0.5f * (l + r);
                    const float sideOut = sideLo + (side - sideLo) * (1.0f + w * 1.3f);
                    l = mid + sideOut; r = mid - sideOut;
                    // PAN: constant-power balance
                    const float a = (pn + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                    L[s] = l * juce::MathConstants<float>::sqrt2 * std::cos (a);
                    R[s] = r * juce::MathConstants<float>::sqrt2 * std::sin (a);
                }
                airL = dsp::sanitise (airL); airR = dsp::sanitise (airR); sideLo = dsp::sanitise (sideLo);
            }
        }

        // master + limiter (always last)
        for (int s = 0; s < n; ++s) { const float g = masterS.getNextValue(); L[s] *= g; R[s] *= g; }
        const bool limiterOn = limOn->load() > 0.5f;
        if (limiterOn)
        {
            if (! limiterWasOn) limiter.reset();
            limiter.setThreshold (limCeiling->load());
            limiter.setRelease (80.0f);
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctxLim (block);
            limiter.process (ctxLim);
            // juce::dsp::Limiter can overshoot its threshold by a fraction of a dB on very fast
            // transients; a final hard ceiling makes "never above the ceiling" literally true.
            const float ceil = juce::Decibels::decibelsToGain (limCeiling->load());
            for (int s = 0; s < n; ++s) { L[s] = juce::jlimit (-ceil, ceil, L[s]); R[s] = juce::jlimit (-ceil, ceil, R[s]); }
        }
        limiterWasOn = limiterOn;

        for (int s = 0; s < n; ++s)
            if (! std::isfinite (L[s]) || ! std::isfinite (R[s]))
            {
                buffer.clear (start, n);
                reset();
                break;
            }
    }
}

} // namespace divinity
