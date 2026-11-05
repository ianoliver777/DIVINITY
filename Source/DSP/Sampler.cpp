#include "Sampler.h"
#include "../Parameters.h"

namespace divinity
{
namespace
{
    constexpr double kRate = 48000.0;
    constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

    struct Gen
    {
        juce::Random rng;
        explicit Gen (juce::int64 seed) : rng (seed) {}
        float noise() { return rng.nextFloat() * 2.0f - 1.0f; }
    };

    /** Simple RBJ band-pass / high-pass / low-pass for sample synthesis (not used in real time). */
    struct Biq
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (int type, double f, double q)
        {
            const double w = kTwoPi * juce::jlimit (10.0, kRate * 0.45, f) / kRate, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            double nb0, nb1, nb2;
            if (type == 0)      { nb0 = (1 - c) / 2; nb1 = 1 - c; nb2 = (1 - c) / 2; }        // LP
            else if (type == 1) { nb0 = (1 + c) / 2; nb1 = -(1 + c); nb2 = (1 + c) / 2; }     // HP
            else                { nb0 = al; nb1 = 0; nb2 = -al; }                             // BP
            const double a0 = 1 + al;
            b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
        }
        float tick (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    std::unique_ptr<SampleData> make (const char* name, double seconds)
    {
        auto s = std::make_unique<SampleData>();
        s->name = name;
        s->sampleRate = kRate;
        s->left.assign ((size_t) (seconds * kRate), 0.0f);
        return s;
    }

    void normalise (SampleData& s, float peak = 0.9f)
    {
        float m = 0.0f;
        for (float x : s.left) m = juce::jmax (m, std::abs (x));
        for (float x : s.right) m = juce::jmax (m, std::abs (x));
        if (m < 1.0e-9f) return;
        const float g = peak / m;
        for (auto& x : s.left) x *= g;
        for (auto& x : s.right) x *= g;
    }

    float expDecay (double t, double seconds) { return (float) std::exp (-t / juce::jmax (1.0e-4, seconds)); }

    //--------------------------------------------------------------------------
    std::unique_ptr<SampleData> hat (const char* name, double decay, double metal, juce::int64 seed)
    {
        auto s = make (name, decay * 6.0 + 0.05);
        Gen g (seed);
        static const double f[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };   // 808 hat oscillator set
        double ph[6] {};
        Biq bp, hp; bp.set (2, 9500.0, 0.9); hp.set (1, 7000.0, 0.7);
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            float sq = 0;
            for (int k = 0; k < 6; ++k) { ph[k] += f[k] * 1.83 / kRate; ph[k] -= std::floor (ph[k]); sq += ph[k] < 0.5 ? 1.0f : -1.0f; }
            const float x = (float) (metal * sq / 6.0) + (float) (1.0 - metal) * g.noise();
            s->left[(size_t) i] = hp.tick (bp.tick (x)) * expDecay (t, decay) * (t < 0.0006 ? (float) (t / 0.0006) : 1.0f);
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> clap()
    {
        auto s = make ("Clap", 0.6);
        Gen g (11);
        Biq bp; bp.set (2, 1250.0, 1.1);
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            float env = 0.0f;
            for (int b = 0; b < 3; ++b) { const double tb = t - b * 0.011; if (tb >= 0 && tb < 0.011) env = juce::jmax (env, expDecay (tb, 0.004)); }
            if (t >= 0.033) env = juce::jmax (env, 0.8f * expDecay (t - 0.033, 0.11));
            s->left[(size_t) i] = bp.tick (g.noise()) * env;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> snap()
    {
        auto s = make ("Snap", 0.25);
        Gen g (12);
        Biq bp; bp.set (2, 2600.0, 1.4);
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            s->left[(size_t) i] = bp.tick (g.noise()) * expDecay (t, 0.035) + (float) std::sin (kTwoPi * 1900 * t) * expDecay (t, 0.004) * 0.6f;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> rim()
    {
        auto s = make ("Rim", 0.2);
        Gen g (13);
        Biq hp; hp.set (1, 3000.0, 0.7);
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            s->left[(size_t) i] = (float) (std::sin (kTwoPi * 1700 * t) * 0.6 + std::sin (kTwoPi * 460 * t) * 0.8) * expDecay (t, 0.025)
                                + hp.tick (g.noise()) * expDecay (t, 0.006) * 0.5f;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> shaker()
    {
        auto s = make ("Shaker", 0.3);
        Gen g (14);
        Biq hp; hp.set (1, 6000.0, 0.8);
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            const float env = (float) (t < 0.015 ? t / 0.015 : std::exp (-(t - 0.015) / 0.07));
            s->left[(size_t) i] = hp.tick (g.noise()) * env;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> tick()
    {
        auto s = make ("Perc Tick", 0.15);
        double ph = 0;
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            ph += (1200.0 + 1800.0 * std::exp (-t / 0.006)) / kRate;
            s->left[(size_t) i] = (float) std::sin (kTwoPi * ph) * expDecay (t, 0.03);
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> pitchDrop (const char* name, double secs, double f0, double f1, double dropTime, double decay, double click)
    {
        auto s = make (name, secs);
        Gen g (21);
        double ph = 0;
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            ph += (f1 + (f0 - f1) * std::exp (-t / dropTime)) / kRate;
            float x = (float) std::tanh (1.4 * std::sin (kTwoPi * ph)) * expDecay (t, decay);
            x += g.noise() * expDecay (t, 0.003) * (float) click;
            s->left[(size_t) i] = x;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> impact()
    {
        auto s = make ("Impact Boom", 3.5);
        s->right.assign (s->left.size(), 0.0f);
        Gen g (31);
        Biq lpL, lpR;
        double ph = 0;
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            const double cut = 200.0 + 9000.0 * std::exp (-t / 0.25);
            if (i % 32 == 0) { lpL.set (0, cut, 0.7); lpR.set (0, cut * 1.03, 0.7); }
            ph += (48.0 + 60.0 * std::exp (-t / 0.08)) / kRate;
            const float boom = (float) std::tanh (2.0 * std::sin (kTwoPi * ph)) * expDecay (t, 0.9);
            const float envN = expDecay (t, 0.7);
            s->left[(size_t) i]  = boom + lpL.tick (g.noise()) * envN * 0.9f;
            s->right[(size_t) i] = boom + lpR.tick (g.noise()) * envN * 0.9f;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> reverseSwell()
    {
        auto s = make ("Reverse Swell", 2.5);
        s->right.assign (s->left.size(), 0.0f);
        Gen g (32);
        Biq bpL, bpR; bpL.set (2, 900.0, 0.6); bpR.set (2, 1100.0, 0.6);
        const int n = s->length();
        static const double chord[] = { 261.63, 311.13, 392.0, 523.25 };
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kRate, rev = (n - i) / kRate;     // time-reversed decay
            float tone = 0;
            for (double f : chord) tone += (float) std::sin (kTwoPi * f * t);
            const float env = (float) std::exp (-rev / 0.6);
            s->left[(size_t) i]  = (tone * 0.2f + bpL.tick (g.noise()) * 0.6f) * env;
            s->right[(size_t) i] = (tone * 0.2f + bpR.tick (g.noise()) * 0.6f) * env;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> riser()
    {
        auto s = make ("Noise Riser", 4.0);
        s->right.assign (s->left.size(), 0.0f);
        Gen g (33);
        Biq bpL, bpR;
        const int n = s->length();
        for (int i = 0; i < n; ++i)
        {
            const double x = (double) i / n;
            if (i % 32 == 0) { const double f = 300.0 * std::pow (30.0, x); bpL.set (2, f, 1.2); bpR.set (2, f * 1.05, 1.2); }
            const float env = (float) (x * x);
            s->left[(size_t) i]  = bpL.tick (g.noise()) * env;
            s->right[(size_t) i] = bpR.tick (g.noise()) * env;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> texture (const char* name, int kind, juce::int64 seed)
    {
        auto s = make (name, 4.0);
        s->right.assign (s->left.size(), 0.0f);
        Gen g (seed);
        Biq aL, aR, bL, bR;
        float pinkL = 0, pinkR = 0;
        const int n = s->length();
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kRate;
            float l = 0, r = 0;
            switch (kind)
            {
                case 0:   // vinyl crackle: dull hiss + sparse clicks
                {
                    if (i == 0) { aL.set (0, 2500, 0.7); aR.set (0, 2500, 0.7); }
                    l = aL.tick (g.noise()) * 0.05f; r = aR.tick (g.noise()) * 0.05f;
                    if (g.rng.nextFloat() < 0.0009f) { const float c = g.noise() * (0.4f + g.rng.nextFloat()); l += c; r += c * 0.7f; }
                    break;
                }
                case 1:   // tape hiss with slow wow
                {
                    if (i == 0) { aL.set (1, 1800, 0.7); aR.set (1, 1800, 0.7); }
                    pinkL = 0.97f * pinkL + 0.03f * g.noise(); pinkR = 0.97f * pinkR + 0.03f * g.noise();
                    const float wow = 0.85f + 0.15f * (float) std::sin (kTwoPi * 0.4 * t);
                    l = aL.tick (g.noise() * 0.6f + pinkL * 4.0f) * wow; r = aR.tick (g.noise() * 0.6f + pinkR * 4.0f) * wow;
                    break;
                }
                case 2:   // rain: soft wash + droplets
                {
                    if (i == 0) { aL.set (0, 4000, 0.7); aR.set (0, 4000, 0.7); bL.set (2, 3200, 4.0); bR.set (2, 2800, 4.0); }
                    l = aL.tick (g.noise()) * 0.25f; r = aR.tick (g.noise()) * 0.25f;
                    if (g.rng.nextFloat() < 0.004f) { l += bL.tick (g.noise() * 3.0f); r += bR.tick (g.noise() * 3.0f); }
                    else { l += bL.tick (0) ; r += bR.tick (0); }
                    break;
                }
                case 3:   // wind: moving band-pass, gusts
                {
                    if (i % 64 == 0)
                    {
                        const double f = 500.0 + 700.0 * (0.5 + 0.5 * std::sin (kTwoPi * 0.13 * t)) + 300.0 * std::sin (kTwoPi * 0.37 * t);
                        aL.set (2, f, 2.5); aR.set (2, f * 1.12, 2.5);
                    }
                    const float gust = 0.5f + 0.5f * (float) std::sin (kTwoPi * 0.21 * t + 1.0);
                    l = aL.tick (g.noise()) * gust; r = aR.tick (g.noise()) * gust;
                    break;
                }
                default:  // air breath
                {
                    if (i == 0) { aL.set (2, 3000, 0.8); aR.set (2, 3400, 0.8); }
                    const float sw = 0.6f + 0.4f * (float) std::sin (kTwoPi * 0.25 * t);
                    l = aL.tick (g.noise()) * sw; r = aR.tick (g.noise()) * sw;
                    break;
                }
            }
            s->left[(size_t) i] = l; s->right[(size_t) i] = r;
        }
        normalise (*s, kind == 0 ? 0.9f : 0.7f);
        return s;
    }

    /** Additive voice through vowel formants (tonal, root C4). */
    std::unique_ptr<SampleData> vocal (const char* name, const double (&formants)[3], double seconds, bool sustained, juce::int64 seed)
    {
        auto s = make (name, seconds);
        s->right.assign (s->left.size(), 0.0f);
        Gen g (seed);
        const double f0 = 261.63;
        const int voices = sustained ? 3 : 1;
        const double det[3] = { 0.0, 0.07, -0.08 };
        double ph[3][48] {};
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            const double vib = 1.0 + 0.004 * std::sin (kTwoPi * 5.2 * t) * juce::jmin (1.0, t * 2.0);
            float l = 0, r = 0;
            for (int v = 0; v < voices; ++v)
                for (int k = 1; k < 48; ++k)
                {
                    const double hz = f0 * k * vib * std::pow (2.0, det[v] / 12.0);
                    if (hz > 16000) break;
                    double amp = 0.02;
                    const double gains[3] = { 1.0, 0.55, 0.3 };
                    for (int fm = 0; fm < 3; ++fm) { const double d = (hz - formants[fm]) / (80.0 + fm * 50.0); amp += gains[fm] * std::exp (-0.5 * d * d); }
                    ph[v][k] += hz / kRate;
                    const float x = (float) (std::sin (kTwoPi * ph[v][k]) * amp / std::sqrt ((double) k));
                    l += x * (v == 2 ? 0.4f : 1.0f); r += x * (v == 1 ? 0.4f : 1.0f);
                }
            const float env = sustained ? (float) juce::jmin (1.0, t / 0.08) : (float) (juce::jmin (1.0, t / 0.01) * std::exp (-t / 0.25));
            const float br = g.noise() * 0.01f;
            s->left[(size_t) i] = (l + br) * env; s->right[(size_t) i] = (r + br) * env;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> partials (const char* name, std::initializer_list<std::pair<double, double>> ratiosDecays, double seconds, double f0)
    {
        auto s = make (name, seconds);
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            float x = 0;
            int k = 0;
            for (auto [ratio, decay] : ratiosDecays)
                x += (float) (std::sin (kTwoPi * f0 * ratio * t + k++) * std::exp (-t / decay) / std::sqrt (ratio));
            s->left[(size_t) i] = x * (t < 0.001 ? (float) (t / 0.001) : 1.0f);
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> metalHit()
    {
        auto s = make ("Metal Hit", 2.0);
        Gen g (41);
        double f[24], d[24];
        for (int k = 0; k < 24; ++k) { f[k] = 300.0 * std::pow (20.0, g.rng.nextDouble()); d[k] = 0.15 + 0.9 * g.rng.nextDouble(); }
        for (int i = 0; i < s->length(); ++i)
        {
            const double t = i / kRate;
            float x = g.noise() * expDecay (t, 0.01) * 0.5f;
            for (int k = 0; k < 24; ++k) x += (float) (std::sin (kTwoPi * f[k] * t) * std::exp (-t / d[k])) * 0.2f;
            s->left[(size_t) i] = x;
        }
        normalise (*s);
        return s;
    }

    std::unique_ptr<SampleData> pluck()
    {
        auto s = make ("Pluck Body", 2.0);
        Gen g (42);
        const int period = (int) std::round (kRate / 261.63);
        std::vector<float> line ((size_t) period);
        for (auto& x : line) x = g.noise();
        int idx = 0; float prev = 0;
        for (int i = 0; i < s->length(); ++i)
        {
            const float y = line[(size_t) idx];
            const float avg = 0.5f * (y + prev) * 0.996f;
            prev = y;
            line[(size_t) idx] = avg;
            idx = (idx + 1) % period;
            s->left[(size_t) i] = y;
        }
        normalise (*s);
        return s;
    }
}

//==============================================================================
FactorySamples::FactorySamples()
{
    static const double ah[3] = { 800, 1150, 2900 }, oh[3] = { 450, 800, 2830 }, ee[3] = { 300, 2300, 3000 };
    samples.push_back (hat ("Hat Closed", 0.035, 0.55, 1));
    samples.push_back (hat ("Hat Open", 0.32, 0.55, 2));
    samples.push_back (hat ("Hat Metal", 0.12, 0.85, 3));
    samples.push_back (clap());
    samples.push_back (snap());
    samples.push_back (rim());
    samples.push_back (shaker());
    samples.push_back (tick());
    samples.push_back (pitchDrop ("Kick Thump", 0.8, 160.0, 46.0, 0.035, 0.32, 0.6));
    samples.push_back (pitchDrop ("Sub Drop", 2.5, 110.0, 32.0, 0.6, 1.2, 0.0));
    samples.push_back (impact());
    samples.push_back (reverseSwell());
    samples.push_back (riser());
    samples.push_back (texture ("Vinyl Crackle", 0, 51));
    samples.push_back (texture ("Tape Hiss", 1, 52));
    samples.push_back (texture ("Rain", 2, 53));
    samples.push_back (texture ("Wind", 3, 54));
    samples.push_back (texture ("Air Breath", 4, 55));
    samples.push_back (vocal ("Choir Ahh", ah, 3.0, true, 61));
    samples.push_back (vocal ("Vox Chop Oh", oh, 0.7, false, 62));
    samples.push_back (vocal ("Vox Chop Ee", ee, 0.7, false, 63));
    samples.push_back (partials ("Glass Hit", { { 1.0, 1.2 }, { 2.76, 0.6 }, { 5.40, 0.35 }, { 8.93, 0.2 } }, 2.5, 523.25));
    samples.push_back (partials ("Bell Strike", { { 0.5, 2.5 }, { 1.0, 2.0 }, { 1.19, 1.4 }, { 1.5, 1.1 }, { 2.0, 0.9 }, { 2.52, 0.6 }, { 3.0, 0.5 }, { 4.2, 0.3 } }, 3.0, 261.63));
    samples.push_back (metalHit());
    samples.push_back (pluck());
    samples.push_back (partials ("Music Box", { { 1.0, 1.5 }, { 4.0, 0.4 }, { 9.8, 0.12 } }, 2.0, 523.25));
    for (auto& s : samples)
        s->rootNote = (s->name == "Glass Hit" || s->name == "Music Box") ? 72 : 60;
    jassert ((int) samples.size() == samplerSampleNames().size() - 1);
}

//==============================================================================
UserSampleSlot::~UserSampleSlot() { current.store (nullptr); }

juce::String UserSampleSlot::load (const juce::File& f)
{
    if (! formatsReady) { formats.registerBasicFormats(); formatsReady = true; }
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) { status = "Can't read " + f.getFileName(); return "\"" + f.getFileName() + "\" isn't a WAV, AIFF, FLAC or OGG file DIVINITY can read."; }
    const auto maxLen = (juce::int64) (reader->sampleRate * 120.0);
    if (reader->lengthInSamples <= 0) return "\"" + f.getFileName() + "\" is empty.";
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);

    juce::AudioBuffer<float> buf ((int) juce::jmin (2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);

    auto s = std::make_unique<SampleData>();
    s->name = f.getFileNameWithoutExtension();
    s->sampleRate = reader->sampleRate;
    s->rootNote = 60;
    s->left.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (buf.getNumChannels() > 1) s->right.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);

    auto* old = current.exchange (s.get(), std::memory_order_acq_rel);
    juce::ignoreUnused (old);
    if (owned != nullptr) retired.emplace_back (std::move (owned), juce::Time::getMillisecondCounter());
    owned = std::move (s);
    file = f;
    status = f.getFileName();
    return {};
}

void UserSampleSlot::clear()
{
    current.store (nullptr, std::memory_order_release);
    if (owned != nullptr) retired.emplace_back (std::move (owned), juce::Time::getMillisecondCounter());
    file = juce::File();
    status = {};
}

void UserSampleSlot::collectGarbage()
{
    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const auto& r) { return now - r.second > 2000; }), retired.end());
}

} // namespace divinity
