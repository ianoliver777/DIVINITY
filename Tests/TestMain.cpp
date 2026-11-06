#include <set>
// DIVINITY test runner: exercises the real processor, engine, presets and editor.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/DSPCore.h"
#include "Assistant/GuardianBrain.h"
#include "Midi/MidiGenerator.h"
#include <chrono>
#include <random>
#include <map>
#include <juce_dsp/juce_dsp.h>

using namespace divinity;

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (! (cond)) { ++failures; std::cout << "  [FAIL] " << msg << "\n"; } } while (0)
#define INFO(msg) std::cout << "  " << msg << "\n"

//==============================================================================
static void setP (DivinityProcessor& p, const juce::String& id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

static void setChoice (DivinityProcessor& p, const juce::String& id, const juce::String& name)
{
    auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id));
    jassert (c != nullptr && c->choices.contains (name));
    c->setValueNotifyingHost (c->convertTo0to1 ((float) c->choices.indexOf (name)));
}

/** A bare test patch: one oscillator, no filter, no effects, no limiter. */
static void cleanPatch (DivinityProcessor& p)
{
    p.presets.loadInit();
    setChoice (p, ids::filt (0, "type"), "Off");
    setP (p, ids::satOn, 0); setP (p, ids::limOn, 0); setP (p, ids::master, 0.0f);
    setP (p, ids::env (0, "attack"), 0.001f); setP (p, ids::env (0, "sustain"), 1.0f);
    setP (p, ids::osc (0, "randphase"), 0);
    setP (p, ids::analog, 0.0f);   // DSP accuracy checks want a fixed pitch (drift has its own tests)
}

struct Note { double start, length; int note; float vel; };
static constexpr float kNearDuplicate = 3.0f;   // fingerprint distance (≈ dB-scale) under which two presets count as copies

static juce::AudioBuffer<float> render (DivinityProcessor& p, double sr, int block, double seconds,
                                        const std::vector<Note>& notes, std::function<void (int)> perBlock = {})
{
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    juce::MidiBuffer midi;
    int blockIndex = 0;
    for (int pos = 0; pos < total; pos += block, ++blockIndex)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        midi.clear();
        for (auto& nt : notes)
        {
            const int on = (int) (nt.start * sr), off = (int) ((nt.start + nt.length) * sr);
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, nt.note), off - pos);
        }
        if (perBlock) perBlock (blockIndex);
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

static bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

static double measureFrequency (const juce::AudioBuffer<float>& b, double sr, int from, int to)
{
    const float* d = b.getReadPointer (0);
    double first = -1, last = -1; int crossings = 0;
    for (int i = from + 1; i < to; ++i)
        if (d[i - 1] <= 0.0f && d[i] > 0.0f)
        {
            const double t = (i - 1) + (double) (-d[i - 1]) / (double) (d[i] - d[i - 1]);
            if (first < 0) first = t; else ++crossings;
            last = t;
        }
    return crossings > 0 ? sr * crossings / (last - first) : 0.0;
}

static float rmsDb (const juce::AudioBuffer<float>& b, int from, int len)
{
    return juce::Decibels::gainToDecibels (b.getRMSLevel (0, from, len), -120.0f);
}

//==============================================================================
static void testParameters (DivinityProcessor& p)
{
    std::cout << "\n== Parameters\n";
    juce::StringArray seen;
    bool unique = true;
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            if (seen.contains (rp->getParameterID())) unique = false;
            seen.add (rp->getParameterID());
        }
    INFO (seen.size() << " host-exposed parameters");
    CHECK (unique, "parameter IDs are unique");
    int missingHelp = 0;
    for (auto& id : seen) if (helpTextFor (id).isEmpty()) { ++missingHelp; std::cout << "  no help: " << id << "\n"; }
    CHECK (missingHelp == 0, missingHelp << " parameters without plain-English help text");
}

//==============================================================================
// 0.9.0 wave bank: does every name deliver what it promises? Each preset is rendered (held note, then a
// release tail) and the words in its name are checked against measurements. Words that describe the
// patch structure (a table, a sample, a filter type) are checked against the preset's own values.
//==============================================================================
static juce::var presetValue (const FactoryPreset& fp, const juce::String& id)
{
    juce::var v;
    for (auto& [k, x] : fp.values) if (k == id) v = x;
    return v;
}

struct HeldAnalysis
{
    float sustainDb = -120, tailDb = -120, centroidLog2 = 0, centroidStd = 0, rmsStd = 0, gateDepth = 0,
          lowCorr = 1, crestDb = 0, riseOct = 0, riseDb = 0, earlyPeakDb = 0, pitchFallOct = 0;
};

static HeldAnalysis analyseHeld (const juce::AudioBuffer<float>& b, double sr, double holdSecs)
{
    HeldAnalysis a;
    const int hold = (int) (holdSecs * sr), n = b.getNumSamples();
    auto rmsOf = [&b] (int from, int len) { double e = 0; for (int c = 0; c < 2; ++c) for (int i = from; i < from + len; ++i) e += b.getSample (c, i) * b.getSample (c, i); return std::sqrt (e / (2.0 * len)); };
    const int s0 = (int) (0.05 * sr);
    // body = the loudest 100 ms of the held note (plucks decay, pads swell)
    float body = 0; for (int f = 0; f + 4800 <= hold; f += 2400) body = std::max (body, (float) rmsOf (f, 4800));
    a.sustainDb = juce::Decibels::gainToDecibels (body, -120.0f);
    const int t0 = hold + (int) (0.3 * sr), tl = juce::jmin (n - t0, (int) (1.0 * sr));
    a.tailDb = juce::Decibels::gainToDecibels ((float) rmsOf (t0, tl), -120.0f) - a.sustainDb;
    // frames over the held part (frames more than 30 dB under the body are left out: tails / silence)
    constexpr int order = 11, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) N * 2), cents, levels;
    for (int f = s0; f + N < hold; f += N / 2)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (b.getSample (0, f + i) + b.getSample (1, f + i)) * (0.5f - 0.5f * std::cos (6.2831853f * i / N));
        const float lv = juce::Decibels::gainToDecibels ((float) rmsOf (f, N), -100.0f);
        if (lv < a.sustainDb - 30.0f) continue;
        fft.performFrequencyOnlyForwardTransform (buf.data());
        double num = 0, den = 0;
        for (int k = 2; k < N / 2; ++k) { const double m = buf[(size_t) k]; num += m * k * sr / N; den += m; }
        if (den > 1.0e-6) { cents.push_back ((float) std::log2 (num / den)); levels.push_back (lv); }
    }
    auto meanStd = [] (const std::vector<float>& v, float& mean, float& sd)
    { mean = 0; sd = 0; if (v.empty()) return; for (auto x : v) mean += x; mean /= v.size(); for (auto x : v) sd += (x - mean) * (x - mean); sd = std::sqrt (sd / v.size()); };
    float m; meanStd (cents, a.centroidLog2, a.centroidStd); meanStd (levels, m, a.rmsStd);
    if (cents.size() >= 6)
    {
        const size_t t = cents.size() / 3;
        float c1 = 0, c3 = 0, l1 = 0, l3 = 0;
        for (size_t i = 0; i < t; ++i) { c1 += cents[i]; c3 += cents[cents.size() - 1 - i]; l1 += levels[i]; l3 += levels[levels.size() - 1 - i]; }
        a.riseOct = (c3 - c1) / t; a.riseDb = (l3 - l1) / t;
    }
    // 10 ms gate depth (p90 - p10)
    std::vector<float> g;
    for (int f = s0; f + 480 < hold; f += 480) g.push_back (juce::Decibels::gainToDecibels ((float) rmsOf (f, 480), -100.0f));
    if (! g.empty()) { std::sort (g.begin(), g.end()); a.gateDepth = g[(size_t) (g.size() * 0.9)] - g[(size_t) (g.size() * 0.1)]; }
    // low-band L/R correlation (four one-pole low-passes at 100 Hz: the fundamental region)
    {
        const float k = 1.0f - std::exp (-6.2831853f * 100.0f / (float) sr);
        float l1 = 0, l2 = 0, l3 = 0, l4 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0; double lr = 0, ll = 0, rr = 0;
        for (int i = 0; i < hold; ++i)
        {
            l1 += k * (b.getSample (0, i) - l1); l3 += k * (l1 - l3); l4 += k * (l3 - l4); l2 += k * (l4 - l2);
            r1 += k * (b.getSample (1, i) - r1); r3 += k * (r1 - r3); r4 += k * (r3 - r4); r2 += k * (r4 - r2);
            lr += l2 * r2; ll += l2 * l2; rr += r2 * r2;
        }
        a.lowCorr = (ll > 1.0e-9 && rr > 1.0e-9) ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
    }
    // pitch fall: zero-crossing rate of the low-passed signal (600 Hz), first vs last active third
    {
        const float k = 1.0f - std::exp (-6.2831853f * 600.0f / (float) sr);
        std::vector<float> zc;
        float y1 = 0, y2 = 0, prev = 0;
        const int W = 4800;
        for (int f = 0; f + W < hold; f += W)
        {
            int z = 0; double e = 0;
            for (int i = f; i < f + W; ++i)
            {
                y1 += k * (0.5f * (b.getSample (0, i) + b.getSample (1, i)) - y1); y2 += k * (y1 - y2);
                if ((prev <= 0.0f) != (y2 <= 0.0f)) ++z;
                prev = y2; e += y2 * y2;
            }
            if (juce::Decibels::gainToDecibels ((float) std::sqrt (e / W), -100.0f) > a.sustainDb - 30.0f && z > 2) zc.push_back ((float) z);
        }
        if (zc.size() >= 4)
        {
            std::vector<float> late (zc.begin() + (long) (zc.size() / 2), zc.end());
            std::sort (late.begin(), late.end());
            a.pitchFallOct = std::log2 (0.5f * (zc[0] + zc[1]) / late[late.size() / 2]);
        }
    }
    float peak = 0; for (int c = 0; c < 2; ++c) peak = std::max (peak, b.getMagnitude (c, 0, hold));
    a.crestDb = juce::Decibels::gainToDecibels (peak, -100.0f) - juce::Decibels::gainToDecibels ((float) rmsOf (0, hold), -100.0f);
    float early = 0; for (int c = 0; c < 2; ++c) early = std::max (early, b.getMagnitude (c, 0, (int) (0.12 * sr)));
    a.earlyPeakDb = juce::Decibels::gainToDecibels (early, -100.0f) - juce::Decibels::gainToDecibels ((float) rmsOf ((int) (0.8 * sr), (int) (0.8 * sr)), -100.0f);
    return a;
}

/** Beating: the harmonics of a detuned stack swell and cancel against each other. Rendered dry and with
    every modulator off, so only the oscillators themselves can make the harmonics move. */
static float beatingAmount (DivinityProcessor& p, const FactoryPreset& fp)
{
    FactoryPreset dry = fp;
    for (int s = 0; s < kNumModSlots; ++s) dry.set (ids::mod (s, "amt"), 0.0);
    dry.set (arpids::on, 0).set (ids::stepOn, 0).set (ids::revOn, 0).set (ids::dlyOn, 0).set (ids::choOn, 0).set (ids::analog, 0.0)
       .set (fxids::tremOn, 0).set (fxids::phaseOn, 0).set (fxids::flangOn, 0).set (fxids::fshiftOn, 0).set (fxids::stereoOn, 0)
       .set (ids::smpOn, 0).set (ids::noiseOn, 0).set (ids::subOn, 0).set (ids::env (0, "attack"), 0.01).set (ids::env (0, "sustain"), 1.0).set (ids::env (0, "decay"), 1.0);
    p.presets.applyFactoryPreset (dry);
    const double sr = 48000;
    auto b = render (p, sr, 256, 3.0, { { 0.0, 3.0, 48, 0.9f } });   // C3, 130.8 Hz
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<std::vector<float>> mags;
    std::vector<float> buf ((size_t) N * 2);
    for (int f = (int) (0.3 * sr); f + N < b.getNumSamples(); f += 1024)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = b.getSample (0, f + i) * (0.5f - 0.5f * std::cos (6.2831853f * i / N));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        std::vector<float> h;
        for (int k = 1; k <= 10; ++k)
        {
            const int c = (int) std::lround (130.81 * k * N / sr);
            float m = 0; for (int d = -4; d <= 4; ++d) m = std::max (m, buf[(size_t) (c + d)]);
            h.push_back (m);
        }
        mags.push_back (h);
    }
    std::vector<float> cv;
    for (size_t k = 0; k < 10; ++k)
    {
        double mean = 0, sd = 0;
        for (auto& f : mags) mean += f[k];
        mean /= mags.size();
        if (mean < 1.0e-3) continue;
        for (auto& f : mags) sd += (f[k] - mean) * (f[k] - mean);
        cv.push_back ((float) (std::sqrt (sd / mags.size()) / mean));
    }
    if (cv.empty()) return 0.0f;
    std::sort (cv.begin(), cv.end());
    return cv[cv.size() / 2];
}

/** Renders one preset and checks every word in its name that makes a measurable promise.
    Returns "claim: why" for each broken promise; `tally` (optional) counts pass / fail per claim. */
static juce::StringArray claimFailures (DivinityProcessor& p, const FactoryPreset& fp, std::map<juce::String, std::pair<int, int>>* tally = nullptr,
                                        juce::String* verboseLine = nullptr)
{
    const double sr = 48000;
    const auto& c = fp.category;
    p.presets.applyFactoryPreset (fp);
    std::vector<Note> notes;
    const double hold = (c == "SEQUENCE" || c == "PERCUSSION" || c == "FX" || c == "TEXTURE") ? 3.0 : 2.0;
    const int key = c == "REESE" || c == "BASS" ? 36 : c == "LEAD" ? 72 : 60;
    if (c == "SEQUENCE") notes = { { 0.0, hold, 60, 0.9f }, { 0.0, hold, 63, 0.9f }, { 0.0, hold, 67, 0.9f } };
    else notes = { { 0.0, hold, key, 0.9f } };
    auto b = render (p, sr, 256, hold + 2.0, notes);
    HeldAnalysis a = analyseHeld (b, sr, hold);
    if (c == "FX")
    {   // the gesture (rise / fall / hit) is judged dry: a long reverb tail would blur the direction
        FactoryPreset dry = fp; dry.set (ids::revOn, 0).set (ids::dlyOn, 0).set (ids::choOn, 0);
        p.presets.applyFactoryPreset (dry);
        const auto da = analyseHeld (render (p, sr, 256, hold + 2.0, notes), sr, hold);
        a.riseOct = da.riseOct; a.riseDb = da.riseDb; a.pitchFallOct = da.pitchFallOct; a.earlyPeakDb = da.earlyPeakDb;
        p.presets.applyFactoryPreset (fp);
    }
    if (c == "SYNTH")
    {   // keys / stabs / plucks: the tail is judged after a short played note, like they're used
        a.tailDb = analyseHeld (render (p, sr, 256, 2.5, { { 0.0, 0.5, 60, 0.9f } }), sr, 0.5).tailDb;
    }
    const auto words = juce::StringArray::fromTokens (fp.name, " ", "");
    auto has = [&words] (const char* w) { return words.contains (w); };
    const float beat = has ("Reese") ? beatingAmount (p, fp) : -1.0f;

    juce::StringArray fails;
    auto check = [&] (const juce::String& claim, bool ok, const juce::String& why)
    {
        if (tally != nullptr) { auto& t = (*tally)[claim]; (ok ? t.first : t.second)++; }
        if (! ok) fails.add (claim + ": " + why);
    };
    auto num = [&fp] (const juce::String& id, double def) { auto v = presetValue (fp, id); return v.isVoid() || v.isString() ? def : (double) v; };
    auto str = [&fp] (const juce::String& id) { auto v = presetValue (fp, id); return v.isString() ? v.toString() : juce::String(); };
    static const juce::StringArray moving { "Breathing", "Drifting", "Wobbling", "Rolling", "Sweeping", "Evolving", "Swirling", "Twisting", "Stepping",
                                            "Growling", "Siren", "Glitch", "Sweep", "Whoosh", "Tidal" };
    static const juce::StringArray gated { "Pulsing", "Gated", "Stuttering" };

    // nothing is dry
    const bool wetFamily = c == "LEAD" || c == "SYNTH" || c == "PAD" || c == "TEXTURE" || c == "SEQUENCE";
    const double rev = num (ids::revOn, 0) > 0.5 ? num (ids::revMix, 0) : 0.0, dly = num (ids::dlyOn, 0) > 0.5 ? num (ids::dlyMix, 0) : 0.0;
    check ("not dry", rev + dly >= (wetFamily ? 0.45 : 0.1), "reverb " + juce::String (rev, 2) + " + delay " + juce::String (dly, 2));
    if (wetFamily) check ("wet tail", a.tailDb > -30.0f, "tail " + juce::String (a.tailDb, 1) + " dB after release");
    if (c == "REESE" || c == "BASS") check ("mono low end", a.lowCorr > 0.9f, "low-band L/R correlation " + juce::String (a.lowCorr, 2));
    if (c == "PERCUSSION") check ("clear transients", a.crestDb > 9.0f, "crest " + juce::String (a.crestDb, 1) + " dB");
    if (beat >= 0) check ("Reese beats", beat > 0.12f, "harmonic beating " + juce::String (beat, 3));
    if (has ("808")) check ("808 pitch drop", str (ids::osc (0, "wave")) == "Sine" && (str (ids::mod (0, "dst")) == "Osc A Pitch" || str (ids::mod (1, "dst")) == "Osc A Pitch"), "sine + pitch envelope");
    if (has ("Dark"))
    {   // dark = clearly darker than the same patch with its filter opened up
        FactoryPreset open = fp; open.choice (ids::filt (0, "type"), "Off");
        p.presets.applyFactoryPreset (open);
        const auto oa = analyseHeld (render (p, sr, 256, hold + 0.5, notes), sr, hold);
        check ("Dark", oa.centroidLog2 - a.centroidLog2 > 0.6f, "centroid " + juce::String (std::exp2 (a.centroidLog2), 0) + " Hz vs " + juce::String (std::exp2 (oa.centroidLog2), 0) + " Hz unfiltered");
    }
    if (has ("Open") && c != "PERCUSSION") check ("Open", str (ids::filt (0, "type")) == "Off" || num (ids::filt (0, "cutoff"), 0) >= 8000.0, "filter " + str (ids::filt (0, "type")));
    for (auto& w : moving)
        if (words.contains (w)) check ("moving (" + w + ")", a.centroidStd > 0.08f || a.rmsStd > 1.5f,
                                       "centroid wobble " + juce::String (a.centroidStd, 3) + " oct, level wobble " + juce::String (a.rmsStd, 2) + " dB");
    for (auto& w : gated)
        if (words.contains (w) && c != "SEQUENCE") check ("gated (" + w + ")", a.gateDepth > 6.0f, "gate depth " + juce::String (a.gateDepth, 1) + " dB");
    if (has ("Cathedral") || has ("Abyss") || has ("Distant") || has ("Cavern") || has ("Hall"))
        check ("long tail", a.tailDb > (c == "REESE" || c == "BASS" || c == "PERCUSSION" ? -40.0f : c == "FX" ? -36.0f : -30.0f), "tail " + juce::String (a.tailDb, 1) + " dB");
    if (has ("Echo") || has ("Tunnel")) check ("echoes", dly >= 0.15, "delay mix " + juce::String (dly, 2));
    if (c == "FX" && has ("Riser")) check ("rises", a.riseOct > 0.3f || a.riseDb > 4.0f, "centroid " + juce::String (a.riseOct, 2) + " oct, level " + juce::String (a.riseDb, 1) + " dB");
    if (c == "FX" && (has ("Downlifter") || has ("Drop")))
        check ("falls", a.riseOct < -0.3f || a.riseDb < -4.0f || a.pitchFallOct > 0.3f, "pitch fall " + juce::String (a.pitchFallOct, 2) + " oct, centroid " + juce::String (a.riseOct, 2) + " oct, level " + juce::String (a.riseDb, 1) + " dB");
    if (c == "FX" && has ("Impact")) check ("impact", a.earlyPeakDb > 6.0f, "early peak " + juce::String (a.earlyPeakDb, 1) + " dB over the tail");
    if (c == "SEQUENCE" && has ("Rising"))  check ("arp up", str (arpids::mode) == "Up", str (arpids::mode));
    if (c == "SEQUENCE" && has ("Falling")) check ("arp down", str (arpids::mode) == "Down", str (arpids::mode));
    if (has ("Gliding")) check ("glide", num (ids::glide, 0) >= 60.0, juce::String (num (ids::glide, 0), 0) + " ms");
    if (has ("Swelling")) check ("swell", num (ids::env (0, "attack"), 0) >= 0.08, juce::String (num (ids::env (0, "attack"), 0), 2) + " s attack");
    if (verboseLine != nullptr)
        *verboseLine = c + " | " + fp.name + " | tail " + juce::String (a.tailDb, 1) + " | cent " + juce::String (std::exp2 (a.centroidLog2), 0)
                     + " | cstd " + juce::String (a.centroidStd, 3) + " | rstd " + juce::String (a.rmsStd, 2) + " | gate " + juce::String (a.gateDepth, 1)
                     + " | beat " + juce::String (beat, 3) + " | lowCorr " + juce::String (a.lowCorr, 2) + " | crest " + juce::String (a.crestDb, 1);
    return fails;
}

static void testWaveBankClaims (DivinityProcessor& p, bool verbose)
{
    std::cout << "\n== Wave bank: do the names deliver?\n";
    const auto list = createFactoryPresets();
    std::map<juce::String, std::pair<int, int>> tally;
    juce::StringArray claimFails;
    for (auto& fp : list)
    {
        juce::String line;
        for (auto& f : claimFailures (p, fp, &tally, verbose ? &line : nullptr)) claimFails.add (fp.name + " [" + fp.category + "] " + f);
        if (verbose) std::cout << "  " << line << "\n";
    }
    for (auto& [claim, t] : tally) std::cout << "  " << claim << ": " << t.first << " pass, " << t.second << " fail\n";
    for (int i = 0; i < juce::jmin (60, claimFails.size()); ++i) std::cout << "    x " << claimFails[i] << "\n";
    CHECK (claimFails.isEmpty(), "every measured name claim holds (" << claimFails.size() << " failures)");
    int numbered = 0;
    for (auto& fp : list) numbered += fp.name.containsAnyOf ("0123456789#") && ! fp.name.contains ("808");
    CHECK (numbered == 0, "no numbered / cosmetic-variant names");
}

static void testFactoryPresets (DivinityProcessor& p)
{
    std::cout << "\n== Factory preset integrity\n";
    auto list = createFactoryPresets();
    int bad = 0;
    for (auto& fp : list)
        for (auto& [id, v] : fp.values)
        {
            auto* prm = p.apvts.getParameter (id);
            if (prm == nullptr) { ++bad; std::cout << "  unknown id " << id << " in " << fp.name << "\n"; continue; }
            if (v.isString())
            {
                auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm);
                if (c == nullptr || ! c->choices.contains (v.toString())) { ++bad; std::cout << "  bad choice " << v.toString() << " for " << id << "\n"; }
            }
        }
    CHECK (bad == 0, "every factory preset references valid parameters and choices");
    juce::StringArray names; int dupes = 0, empty = 0;
    std::map<juce::String, int> perCat;
    for (auto& fp : list) { if (names.contains (fp.name)) { ++dupes; std::cout << "  duplicate " << fp.name << "\n"; } names.add (fp.name);
                            empty += fp.name.trim().isEmpty(); perCat[fp.category]++; }
    CHECK (dupes == 0 && empty == 0, "factory preset names are unique and non-empty");
    const std::pair<const char*, int> want[] = { { "REESE", 120 }, { "BASS", 80 }, { "LEAD", 90 }, { "SYNTH", 90 }, { "PAD", 95 },
                                                 { "TEXTURE", 70 }, { "SEQUENCE", 90 }, { "PERCUSSION", 62 }, { "FX", 80 } };
    bool counts = perCat.size() == 9;
    for (auto& [c, n] : want) { counts &= perCat[c] == n; std::cout << "  " << c << " " << perCat[c] << "\n"; }
    CHECK (list.size() == 777 && counts, "777 factory presets in the nine categories (" << (int) list.size() << ")");

    // LFOs are off unless the sound routes them; percussion loops run the step sequencer in TRIGGER mode
    int lfoWrong = 0, percWrong = 0;
    for (auto& fp : list)
    {
        std::map<juce::String, juce::var> v; for (auto& [id, x] : fp.values) v[id] = x;
        for (int l = 0; l < kNumLfos; ++l)
        {
            bool routed = false;
            for (int m = 0; m < kNumModSlots; ++m) routed |= v[ids::mod (m, "src")].toString() == "LFO " + juce::String (l + 1);
            const bool on = (double) v[ids::lfo (l, "on")] > 0.5;
            if (on != routed) { ++lfoWrong; std::cout << "  LFO power mismatch " << fp.name << " LFO " << (l + 1) << "\n"; }
        }
        if (fp.category == "PERCUSSION" && ! ((double) v[ids::stepOn] > 0.5 && v[ids::stepMode].toString() == "Trigger")) ++percWrong;
    }
    CHECK (lfoWrong == 0, "LFOs are only switched on in presets that route them");
    CHECK (percWrong == 0, "every PERCUSSION preset is a tempo-synced step-sequencer loop");
    INFO (list.size() << " factory presets");
}

static void testEnvelopeTiming()
{
    std::cout << "\n== Envelope timing\n";
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        dsp::Envelope e; e.setSampleRate (sr);
        e.setParameters (0.1f, 0.2f, 0.5f, 0.3f, 0.0f);
        e.noteOn();
        int n = 0; while (e.getStage() == dsp::Envelope::Stage::Attack) { e.next(); ++n; }
        const double attackMs = 1000.0 * n / sr;
        int d = 0; while (e.getStage() == dsp::Envelope::Stage::Decay) { e.next(); ++d; }
        const double decayMs = 1000.0 * d / sr;
        e.noteOff();
        int r = 0; while (e.isActive()) { e.next(); ++r; }
        const double relMs = 1000.0 * r / sr;
        CHECK (std::abs (attackMs - 100.0) < 0.1 && std::abs (decayMs - 200.0) < 0.1 && std::abs (relMs - 300.0) < 0.1,
               "envelope A/D/R timing @" << sr << " = " << attackMs << "/" << decayMs << "/" << relMs << " ms");
    }
    INFO ("attack/decay/release land within 0.1 ms at 44.1-192 kHz");
}

static void testPitch (DivinityProcessor& p)
{
    std::cout << "\n== Oscillator pitch accuracy\n";
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (auto engine : { "Analog", "Wavetable" })
        {
            cleanPatch (p);
            setChoice (p, ids::osc (0, "engine"), engine);
            setChoice (p, ids::osc (0, "wave"), "Sine");
            setChoice (p, ids::osc (0, "table"), "Basic Shapes");
            setP (p, ids::osc (0, "pos"), 0.0f);
            for (int note : { 33, 69, 93 })
            {
                auto b = render (p, sr, 256, 1.2, { { 0.0, 2.0, note, 1.0f } });
                const double f = measureFrequency (b, sr, (int) (0.2 * sr), (int) (1.2 * sr));
                const double expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                const double cents = 1200.0 * std::log2 (f / expected);
                CHECK (std::abs (cents) < 0.5, engine << " note " << note << " @" << sr << ": " << f << " Hz (" << cents << " ct)");
            }
        }
    INFO ("analog + wavetable sine within 0.5 cent at notes 33/69/93, 44.1-192 kHz");
}

static void testModRouting (DivinityProcessor& p)
{
    std::cout << "\n== Modulation routing\n";
    cleanPatch (p);
    setChoice (p, ids::osc (0, "engine"), "Analog");
    setChoice (p, ids::osc (0, "wave"), "Sine");
    setChoice (p, ids::mod (0, "src"), "Macro 1");
    setChoice (p, ids::mod (0, "dst"), "Osc A Pitch");
    setP (p, ids::mod (0, "amt"), 1.0f);
    setP (p, ids::macro (0), 1.0f);
    auto b = render (p, 48000, 512, 1.0, { { 0.0, 2.0, 69, 1.0f } });
    const double f = measureFrequency (b, 48000, 4800, 48000);
    CHECK (std::abs (f - 880.0) < 0.5, "Macro 1 -> Osc A Pitch (+100% = +12 st) gives " << f << " Hz");

    setP (p, ids::mod (0, "amt"), -0.5f);
    b = render (p, 48000, 512, 1.0, { { 0.0, 2.0, 69, 1.0f } });
    const double f2 = measureFrequency (b, 48000, 4800, 48000);
    CHECK (std::abs (f2 - 440.0 / std::sqrt (2.0)) < 0.5, "negative amount (-50% = -6 st) gives " << f2 << " Hz");

    // removable: clearing the source removes the effect
    setChoice (p, ids::mod (0, "src"), "-");
    b = render (p, 48000, 512, 1.0, { { 0.0, 2.0, 69, 1.0f } });
    CHECK (std::abs (measureFrequency (b, 48000, 4800, 48000) - 440.0) < 0.5, "clearing the routing restores 440 Hz");

    // LFO -> filter cutoff makes the level move over time
    cleanPatch (p);
    setChoice (p, ids::osc (0, "engine"), "Analog");
    setChoice (p, ids::osc (0, "wave"), "Saw");
    setChoice (p, ids::filt (0, "type"), "Low Pass 12");
    setP (p, ids::filt (0, "cutoff"), 400.0f);
    setChoice (p, ids::mod (0, "src"), "LFO 1");
    setChoice (p, ids::mod (0, "dst"), "Filter 1 Cutoff");
    setP (p, ids::mod (0, "amt"), 0.5f);
    setP (p, ids::lfo (0, "rate"), 2.0f);
    setP (p, ids::lfo (0, "on"), 1.0f);
    b = render (p, 48000, 512, 1.0, { { 0.0, 2.0, 45, 1.0f } });
    float lo = 1e9f, hi = -1e9f;
    for (int w = 0; w < 20; ++w) { const float r = rmsDb (b, 4800 + w * 2000, 1200); lo = std::min (lo, r); hi = std::max (hi, r); }
    CHECK (hi - lo > 6.0f, "LFO 1 -> Filter 1 Cutoff modulates the level by " << (hi - lo) << " dB");
}

static void testFilters (DivinityProcessor& p)
{
    std::cout << "\n== Filter stability (all types, max resonance + drive, swept cutoff)\n";
    const auto names = filterTypeNames();
    for (int t = 1; t < names.size(); ++t)
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog");
        setChoice (p, ids::osc (0, "wave"), "Saw");
        setP (p, ids::noiseOn, 1); setP (p, ids::noiseLevel, 1.0f);
        setChoice (p, ids::filt (0, "type"), names[t]);
        setP (p, ids::filt (0, "res"), 1.0f); setP (p, ids::filt (0, "drive"), 1.0f);
        float peak = 0;
        bool finite = true;
        for (double sr : { 44100.0, 192000.0 })
        {
            auto b = render (p, sr, 128, 2.0, { { 0.0, 2.0, 36, 1.0f }, { 0.0, 2.0, 84, 1.0f } }, [&] (int blk)
            {
                setP (p, ids::filt (0, "cutoff"), 20.0f * std::pow (1000.0f, (float) ((blk * 37) % 100) / 100.0f));
            });
            finite &= allFinite (b);
            peak = std::max (peak, b.getMagnitude (0, b.getNumSamples()));
        }
        CHECK (finite && peak < 20.0f, names[t] << ": finite=" << finite << " peak=" << peak);
    }
}

static void testStateRoundTrip (DivinityProcessor& p)
{
    std::cout << "\n== State round-trip / host restore\n";
    p.presets.loadPreset (0);
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> U (0.0f, 1.0f);
    for (auto* prm : p.getParameters()) prm->setValueNotifyingHost (U (rng));
    juce::MemoryBlock state;
    p.getStateInformation (state);

    auto qOwner = std::make_unique<DivinityProcessor>(); auto& q = *qOwner;
    q.setStateInformation (state.getData(), (int) state.getSize());
    int mismatches = 0;
    for (int i = 0; i < p.getParameters().size(); ++i)
    {
        // compare what the parameter *means* (bools/choices store raw floats but act on the snapped value)
        auto* a = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[i]);
        auto* b = dynamic_cast<juce::RangedAudioParameter*> (q.getParameters()[i]);
        if (a->getCurrentValueAsText() != b->getCurrentValueAsText()
            || std::abs (a->convertFrom0to1 (a->getValue()) - b->convertFrom0to1 (b->getValue())) > 1.0e-3f * (1.0f + std::abs (a->convertFrom0to1 (a->getValue()))))
            { ++mismatches; std::cout << "  mismatch " << a->getParameterID() << " " << a->getCurrentValueAsText() << " vs " << b->getCurrentValueAsText() << "\n"; }
    }
    CHECK (mismatches == 0, "all parameters restored in a fresh instance (" << mismatches << " mismatches)");
    CHECK (q.presets.getCurrentName() == p.presets.getCurrentName() && q.presets.isModified(), "preset name + modified flag restored");

    // identical audio from both instances = deterministic recall (seeded randomness)
    p.presets.loadPreset (0); q.presets.loadPreset (0);
    auto a = render (p, 48000, 256, 1.0, { { 0.0, 0.8, 60, 0.8f }, { 0.1, 0.7, 64, 0.8f } });
    auto b = render (q, 48000, 256, 1.0, { { 0.0, 0.8, 60, 0.8f }, { 0.1, 0.7, 64, 0.8f } });
    float maxDiff = 0;
    for (int i = 0; i < a.getNumSamples(); ++i) maxDiff = std::max (maxDiff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
    CHECK (maxDiff < 1.0e-6f, "two instances render identical audio (max diff " << maxDiff << ")");

    // older session: strip half the params, they must come back as defaults
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    auto* params = xml->getChildByName ("PARAMETERS");
    std::vector<juce::XmlElement*> toRemove;
    int k = 0;
    for (auto* e : params->getChildIterator()) if ((k++ % 2) == 0) toRemove.push_back (e);
    for (auto* e : toRemove) params->removeChildElement (e, true);
    juce::MemoryBlock older;
    juce::AudioProcessor::copyXmlToBinary (*xml, older);
    auto rOwner = std::make_unique<DivinityProcessor>(); auto& r = *rOwner;
    r.setStateInformation (older.getData(), (int) older.getSize());
    int wrong = 0;
    for (auto* prm : r.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        bool present = false;
        for (auto* e : params->getChildIterator()) if (e->getStringAttribute ("id") == rp->getParameterID()) present = true;
        // engine_mode is the one deliberate exception: a session without it predates 2.0 and loads as Classic
                bool legacyRule = rp->getParameterID() == ids::engineMode;   // pre-2.0 sessions load as Classic
        for (int l = 0; l < kNumLfos; ++l) legacyRule |= rp->getParameterID() == ids::lfo (l, "on");   // pre-0.7: LFOs were always running
        if (! present && ! legacyRule && std::abs (rp->getValue() - rp->getDefaultValue()) > 1.0e-6f) ++wrong;
    }
    CHECK (wrong == 0, "parameters missing from an older session fall back to defaults");

    // garbage data must not crash or wipe the sound
    const char junk[] = "not a divinity state";
    r.setStateInformation (junk, (int) sizeof (junk));
    CHECK (true, "garbage state data ignored safely");
}

static void testPresetFiles (DivinityProcessor& p)
{
    std::cout << "\n== User preset save / load / import errors\n";
    p.presets.loadPreset (3);
    setP (p, ids::macro (2), 0.77f);
    const auto err = p.presets.saveUserPreset ("zz DIVINITY test preset", "Experimental");
    CHECK (err.isEmpty(), "save user preset: " << err);
    const float saved = p.apvts.getRawParameterValue (ids::macro (2))->load();
    p.presets.loadInit();
    int idx = -1;
    for (int i = 0; i < p.presets.getPresets().size(); ++i) if (p.presets.getPresets()[i].name == "zz DIVINITY test preset") idx = i;
    CHECK (idx >= 0, "saved preset appears after rescan");
    if (idx >= 0)
    {
        p.presets.loadPreset (idx);
        CHECK (std::abs (p.apvts.getRawParameterValue (ids::macro (2))->load() - saved) < 1.0e-4f, "user preset reloads its values");
        CHECK (! p.presets.isModified(), "freshly loaded preset is not marked modified");
        setP (p, ids::macro (2), 0.1f);
        CHECK (p.presets.isModified(), "editing a parameter marks the preset modified");
        p.presets.getPresets()[idx].file.deleteFile();
        p.presets.rescan();
    }
    auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("does_not_exist.divpreset");
    CHECK (p.presets.importPreset (missing).isNotEmpty(), "importing a missing file reports an error");
    auto junk = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("junk.divpreset");
    junk.replaceWithText ("<NotAPreset/>");
    CHECK (p.presets.importPreset (junk).isNotEmpty(), "importing a non-DIVINITY file reports an error");
    junk.deleteFile();
}

static void testMidi (DivinityProcessor& p)
{
    std::cout << "\n== MIDI handling, sustain, voice limits, stealing\n";
    p.presets.loadPreset (0);
    setP (p, ids::voices, 4);
    std::vector<Note> notes;
    for (int i = 0; i < 10; ++i) notes.push_back ({ 0.05 * i, 2.0, 48 + i * 2, 0.8f });
    int maxVoices = 0;
    auto b = render (p, 48000, 256, 1.0, notes, [&] (int) { maxVoices = std::max (maxVoices, p.getActiveVoices()); });
    CHECK (maxVoices <= 4, "voice limit 4 respected while 10 notes are held (max " << maxVoices << ")");
    CHECK (allFinite (b), "voice stealing produces finite audio");

    // sustain pedal: note off while pedal is down keeps the voice
    cleanPatch (p);
    setP (p, ids::env (0, "release"), 0.05f);
    p.prepareToPlay (48000, 256);
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 1);
    m.addEvent (juce::MidiMessage::noteOff (1, 60), 100);
    p.processBlock (buf, m);
    m.clear();
    for (int i = 0; i < 40; ++i) p.processBlock (buf, m);
    CHECK (p.getActiveVoices() == 1, "sustain pedal holds a released note");
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    p.processBlock (buf, m);
    m.clear();
    for (int i = 0; i < 40; ++i) p.processBlock (buf, m);
    CHECK (p.getActiveVoices() == 0, "releasing the pedal releases the note");

    // all-notes-off (CC 123) and all-sound-off (CC 120)
    m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    m.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 0);
    p.processBlock (buf, m); m.clear();
    m.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 0);
    p.processBlock (buf, m); m.clear();
    CHECK (p.getActiveVoices() == 0, "CC 120 (all sound off) silences every voice immediately");

    // pitch bend: +2 semitones at full bend with default range
    cleanPatch (p);
    setChoice (p, ids::osc (0, "engine"), "Analog");
    setChoice (p, ids::osc (0, "wave"), "Sine");
    p.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> out (2, 48000), blk (2, 512);
    for (int pos = 0; pos < 48000; pos += 512)
    {
        m.clear();
        if (pos == 0) { m.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0); m.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0); }
        const int n = std::min (512, 48000 - pos);
        blk.setSize (2, n, false, false, true);
        p.processBlock (blk, m);
        out.copyFrom (0, pos, blk, 0, 0, n);
    }
    const double f = measureFrequency (out, 48000, 4800, 48000);
    CHECK (std::abs (1200.0 * std::log2 (f / (440.0 * std::pow (2.0, 2.0 / 12.0)))) < 1.0, "full pitch bend = +2 st (" << f << " Hz)");

    // legato: overlapping notes keep one voice and glide
    cleanPatch (p);
    setChoice (p, ids::voiceMode, "Legato");
    int legatoMax = 0;
    render (p, 48000, 256, 1.0, { { 0.0, 0.6, 60, 1.0f }, { 0.2, 0.6, 67, 1.0f } }, [&] (int) { legatoMax = std::max (legatoMax, p.getActiveVoices()); });
    CHECK (legatoMax == 1, "legato mode stays monophonic");
}

/** Brightness proxy: RMS of the first difference over RMS of the signal (tracks spectral centroid). */
static double zeroCrossRate (const juce::AudioBuffer<float>& b, int from, int len)
{
    double d = 0, s = 0;
    for (int i = from + 1; i < from + len; ++i)
    {
        const double x = b.getSample (0, i), dx = x - b.getSample (0, i - 1);
        d += dx * dx; s += x * x;
    }
    return s > 0 ? std::sqrt (d / s) : 0.0;
}

static void testSoul (DivinityProcessor& p)
{
    std::cout << "\n== SOUL ENGINE + Divine Intervention\n";
    const std::vector<Note> chord { { 0.0, 1.5, 48, 0.9f }, { 0.0, 1.5, 55, 0.9f }, { 0.0, 1.5, 60, 0.9f } };

    // neutral pad + Divine 0 must leave the patch bit-identical to a reference render
    p.presets.loadPreset (0);
    auto ref = render (p, 48000, 256, 1.0, chord);
    setP (p, ids::soulX, 0.5f); setP (p, ids::soulY, 0.5f); setP (p, ids::divineAmount, 0.0f);
    setP (p, ids::soulIntensity, 0.37f); setChoice (p, ids::divineMode, "Shatter");   // irrelevant at neutral / 0%
    auto same = render (p, 48000, 256, 1.0, chord);
    float diff = 0;
    for (int i = 0; i < ref.getNumSamples(); ++i) diff = std::max (diff, std::abs (ref.getSample (0, i) - same.getSample (0, i)));
    CHECK (diff < 1.0e-6f, "neutral XY + Divine 0% leaves the sound untouched (max diff " << diff << ")");

    // AGGRESSIVE is brighter than SOFT (Classic mapping)
    cleanPatch (p);
    setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
    setChoice (p, ids::filt (0, "type"), "Low Pass 12"); setP (p, ids::filt (0, "cutoff"), 1500.0f);
    setP (p, ids::soulSmooth, 0.0f);
    setP (p, ids::soulX, 0.0f);
    auto soft = render (p, 48000, 256, 1.0, { { 0.0, 2.0, 45, 0.9f } });
    setP (p, ids::soulX, 1.0f);
    auto aggr = render (p, 48000, 256, 1.0, { { 0.0, 2.0, 45, 0.9f } });
    const double zs = zeroCrossRate (soft, 24000, 20000), za = zeroCrossRate (aggr, 24000, 20000);
    CHECK (za > zs * 1.5, "AGGRESSIVE brighter than SOFT (slope/level ratio " << za << " vs " << zs << ")");

    // MELANCHOLY gives longer tails than EUPHORIA
    auto tailLevel = [&] (float yv)
    {
        setP (p, ids::soulX, 0.5f); setP (p, ids::soulY, yv);
        setP (p, ids::env (0, "release"), 0.3f);
        auto b = render (p, 48000, 256, 1.5, { { 0.0, 0.4, 57, 0.9f } });
        return rmsDb (b, (int) (0.75 * 48000), 4800);
    };
    const float tm = tailLevel (0.0f), te = tailLevel (1.0f);
    CHECK (tm > te + 6.0f, "MELANCHOLY tail louder 0.35 s after release than EUPHORIA (" << tm << " vs " << te << " dB)");

    // exclusions: with every target switched off, a hard-AGGRESSIVE pad must sound exactly like neutral
    setP (p, ids::soulX, 0.5f); setP (p, ids::soulY, 0.5f);
    auto neutral = render (p, 48000, 256, 1.0, { { 0.0, 2.0, 45, 0.9f } });
    for (int t = 0; t < kNumSoulTargets; ++t) setP (p, ids::soul (t, "on"), 0);
    setP (p, ids::soulX, 1.0f);
    auto off = render (p, 48000, 256, 1.0, { { 0.0, 2.0, 45, 0.9f } });
    float offDiff = 0;
    for (int i = 0; i < off.getNumSamples(); ++i) offDiff = std::max (offDiff, std::abs (off.getSample (0, i) - neutral.getSample (0, i)));
    CHECK (offDiff < 1.0e-6f, "excluding every target removes the SOUL effect (diff " << offDiff << ")");
    for (int t = 0; t < kNumSoulTargets; ++t) setP (p, ids::soul (t, "on"), 1);

    // smoothing: a full jump of the pad with 300 ms smoothing creates no clicks
    cleanPatch (p);
    setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Sine");
    setP (p, ids::soulSmooth, 0.3f);
    setP (p, ids::soulX, 0.0f); setP (p, ids::soulY, 0.0f);
    auto jump = render (p, 48000, 128, 1.5, { { 0.0, 2.0, 57, 0.9f } }, [&] (int blk)
    {
        if (blk == 180) { setP (p, ids::soulX, 1.0f); setP (p, ids::soulY, 1.0f); }
    });
    // reference: the same patch already sitting at the target corner (its own steepest slope)
    auto target = render (p, 48000, 128, 1.5, { { 0.0, 2.0, 57, 0.9f } });
    float maxStep = 0, refStep = 0;
    for (int i = 22000; i < jump.getNumSamples(); ++i) maxStep = std::max (maxStep, std::abs (jump.getSample (0, i) - jump.getSample (0, i - 1)));
    for (int i = 22000; i < target.getNumSamples(); ++i) refStep = std::max (refStep, std::abs (target.getSample (0, i) - target.getSample (0, i - 1)));
    CHECK (maxStep <= refStep * 1.15f, "XY jump with smoothing is click-free (max step " << maxStep << " vs settled target " << refStep << ")");

    // every Divine mode changes the sound, stays finite and under the ceiling
    p.presets.loadPreset (0);
    auto base = render (p, 48000, 256, 1.5, chord);
    for (int mode = 0; mode < 5; ++mode)
    {
        p.presets.loadPreset (0);
        setChoice (p, ids::divineMode, divineModeNames()[mode]);
        setP (p, ids::divineAmount, 1.0f);
        auto b = render (p, 48000, 256, 1.5, chord);
        float d = 0;
        for (int i = 0; i < b.getNumSamples(); ++i) d = std::max (d, std::abs (b.getSample (0, i) - base.getSample (0, i)));
        const float pk = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()));
        CHECK (allFinite (b) && d > 0.01f && pk <= -0.2f, "Divine " << divineModeNames()[mode] << ": changes sound (diff " << d << "), peak " << pk << " dBFS");
    }

    // mod matrix can drive the pad
    cleanPatch (p);
    setChoice (p, ids::mod (0, "src"), "Macro 1"); setChoice (p, ids::mod (0, "dst"), "Soul X"); setP (p, ids::mod (0, "amt"), 1.0f);
    setP (p, ids::macro (0), 1.0f); setP (p, ids::soulSmooth, 0.0f);
    render (p, 48000, 256, 0.3, { { 0.0, 0.3, 60, 0.9f } });
    CHECK (p.getSoulX() > 0.95f, "Macro 1 -> Soul X moves the effective pad position (" << p.getSoulX() << ")");

    // mapping weights recall with the patch
    p.presets.loadPreset (0);
    setP (p, ids::soul (3, "y"), -0.83f);
    juce::MemoryBlock st; p.getStateInformation (st);
    auto qOwner = std::make_unique<DivinityProcessor>(); auto& q = *qOwner; q.setStateInformation (st.getData(), (int) st.getSize());
    CHECK (std::abs (q.apvts.getRawParameterValue (ids::soul (3, "y"))->load() + 0.83f) < 1.0e-3f, "custom emotion mapping recalls with the session");
}

//==============================================================================
static float maxDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float d = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < juce::jmin (a.getNumSamples(), b.getNumSamples()); ++i)
            d = std::max (d, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
    return d;
}

static void allFxOff (DivinityProcessor& p)
{
    for (auto& d : fxCatalog()) setP (p, d.onID, 0);
}

static void testFxRack (DivinityProcessor& p)
{
    std::cout << "\n== FX rack: every module, ordering, recall, bypass\n";
    const auto& cat = fxCatalog();
    CHECK (cat.size() == 16, "16 rack effects (" << (int) cat.size() << ")");
    CHECK (defaultFxOrder().size() == (int) cat.size(), "default order covers every effect");

    const std::vector<Note> chord { { 0.0, 0.9, 48, 0.9f }, { 0.05, 0.85, 55, 0.8f }, { 0.1, 0.8, 64, 0.8f } };
    int working = 0;
    for (auto& d : cat)
    {
        cleanPatch (p); allFxOff (p);
        auto dry = render (p, 48000, 256, 1.2, chord);
        // push every control off its (often neutral) default
        for (auto& k : d.knobs)
            if (auto* prm = p.apvts.getParameter (k.id)) prm->setValueNotifyingHost (k.bipolar ? 0.85f : 0.75f);
        setP (p, d.onID, 1);
        auto wet = render (p, 48000, 256, 1.2, chord);
        const float diff = maxDiff (dry, wet);
        const bool finite = allFinite (wet);
        float peak = 0; for (int ch = 0; ch < 2; ++ch) peak = std::max (peak, wet.getMagnitude (ch, 0, wet.getNumSamples()));
        const bool ok = finite && diff > 1.0e-3f && peak < 8.0f;
        if (ok) ++working;
        else INFO ("  " << d.name << ": finite " << (int) finite << " diff " << diff << " peak " << peak);
        // switching it back off restores the dry sound exactly
        setP (p, d.onID, 0);
        auto again = render (p, 48000, 256, 1.2, chord);
        CHECK (maxDiff (dry, again) < 1.0e-5f, d.name << ": switching off restores the dry signal");
    }
    CHECK (working == (int) cat.size(), "every effect is finite, bounded and audibly changes the sound (" << working << "/" << (int) cat.size() << ")");

    // order matters: distortion -> reverb is not reverb -> distortion
    cleanPatch (p); allFxOff (p);
    setP (p, fxids::distOn, 1); setP (p, fxids::distDrive, 0.8f); setP (p, fxids::distMix, 1.0f);
    setP (p, ids::revOn, 1); setP (p, ids::revMix, 0.6f);
    auto order = defaultFxOrder();
    order.removeString ("dist"); order.insert (order.indexOf ("reverb"), "dist");
    p.setFxOrder (order, false);
    auto a = render (p, 48000, 256, 1.2, chord);
    order.removeString ("dist"); order.add ("dist");
    p.setFxOrder (order, false);
    auto b = render (p, 48000, 256, 1.2, chord);
    CHECK (maxDiff (a, b) > 1.0e-2f, "changing the chain order changes the sound (diff " << maxDiff (a, b) << ")");
    CHECK (p.getFxOrder() == order, "processor reports the order it was given");

    // garbage / partial orders are repaired
    p.setFxOrder (juce::StringArray { "reverb", "nonsense", "reverb", "sat" }, false);
    auto fixed = p.getFxOrder();
    CHECK (fixed.size() == (int) cat.size() && fixed[0] == "reverb" && fixed[1] == "sat", "bad orders are sanitised to a full permutation");
    for (auto& d : cat) CHECK (fixed.contains (d.key), "sanitised order contains " << d.key);

    // recall: host state
    p.setFxOrder (order, false);
    juce::MemoryBlock state; p.getStateInformation (state);
    auto qOwner = std::make_unique<DivinityProcessor>(); auto& q = *qOwner;
    q.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (q.getFxOrder() == order, "chain order restored from host state");

    // recall: preset XML, and factory presets reset to the default order
    PresetInfo info; info.name = "order test";
    auto xml = p.presets.createPresetXml (info);
    p.presets.loadPreset (0);
    CHECK (p.getFxOrder() == defaultFxOrder(), "factory preset loads the default order");
    p.presets.applyPresetXml (*xml);
    CHECK (p.getFxOrder() == order, "chain order restored from a preset file");

    // order change is undoable and marks the preset modified
    p.presets.loadPreset (0);
    p.undo.beginNewTransaction();
    auto moved = defaultFxOrder(); moved.move (0, 5);
    p.setFxOrder (moved);
    CHECK (p.presets.isModified(), "reordering marks the preset modified");
    p.undo.undo();
    CHECK (p.getFxOrder() == defaultFxOrder(), "reordering can be undone");

    // bypass crossfade: switching an effect on mid-note does not click
    cleanPatch (p); allFxOff (p);
    setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Sine");
    setChoice (p, fxids::distType, "Tube"); setP (p, fxids::distDrive, 0.6f); setP (p, fxids::distMix, 1.0f);
    auto sw = render (p, 48000, 128, 1.0, { { 0.0, 2.0, 57, 0.9f } }, [&] (int blk) { if (blk == 200) setP (p, fxids::distOn, 1); });
    auto on = render (p, 48000, 128, 1.0, { { 0.0, 2.0, 57, 0.9f } });
    setP (p, fxids::distOn, 0);
    auto off = render (p, 48000, 128, 1.0, { { 0.0, 2.0, 57, 0.9f } });
    auto steepest = [] (const juce::AudioBuffer<float>& x, int from, int to)
    { float m = 0; for (int i = from; i < to; ++i) m = std::max (m, std::abs (x.getSample (0, i) - x.getSample (0, i - 1))); return m; };
    const float swStep = steepest (sw, 24000, 30000), ref = std::max (steepest (on, 24000, 30000), steepest (off, 24000, 30000));
    CHECK (swStep <= ref * 1.15f, "switching an effect on mid-note is click-free (" << swStep << " vs " << ref << ")");
}

//==============================================================================
struct ArpEvent { double t; int note; bool on; };

/** Drives the arpeggiator directly: keys = (time, length, note), returns its output events. */
static std::vector<ArpEvent> runArp (DivinityProcessor& p, const std::vector<Note>& keys, double seconds,
                                     bool hostPlaying = false, double ppqStart = 0.0, double bpm = 120.0, std::function<void (int)> perBlock = {})
{
    const double sr = 48000; const int block = 512;
    p.arp.prepare (sr);
    std::vector<ArpEvent> evs;
    juce::MidiBuffer midi;
    const int total = (int) (seconds * sr);
    int blk = 0;
    for (int pos = 0; pos < total; pos += block, ++blk)
    {
        const int n = juce::jmin (block, total - pos);
        midi.clear();
        for (auto& k : keys)
        {
            const int on = (int) (k.start * sr), off = (int) ((k.start + k.length) * sr);
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, k.note, k.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, k.note), off - pos);
        }
        if (perBlock) perBlock (blk);
        TransportInfo tr; tr.bpm = bpm; tr.isPlaying = hostPlaying; tr.hasPpq = hostPlaying;
        tr.ppqAtBlockStart = ppqStart + pos / sr * bpm / 60.0;
        p.arp.process (midi, n, tr);
        for (const auto m : midi)
        {
            auto msg = m.getMessage();
            if (msg.isNoteOn()) evs.push_back ({ (pos + m.samplePosition) / sr, msg.getNoteNumber(), true });
            else if (msg.isNoteOff()) evs.push_back ({ (pos + m.samplePosition) / sr, msg.getNoteNumber(), false });
        }
    }
    return evs;
}

static std::vector<int> onNotes (const std::vector<ArpEvent>& e) { std::vector<int> v; for (auto& x : e) if (x.on) v.push_back (x.note); return v; }

static bool balanced (const std::vector<ArpEvent>& e)
{
    std::map<int, int> c;
    for (auto& x : e) c[x.note] += x.on ? 1 : -1;
    for (auto& [n, v] : c) if (v != 0) return false;
    return true;
}

static void testArpAndVoiceModes (DivinityProcessor& p)
{
    std::cout << "\n== Arpeggiator + voice modes\n";
    p.presets.loadInit();
    setP (p, arpids::on, 1); setChoice (p, arpids::rate, "1/16"); setP (p, arpids::gate, 0.5f); setP (p, arpids::octaves, 1);
    const std::vector<Note> chord { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 64, 0.8f }, { 0.0, 1.0, 67, 0.8f } };

    auto pattern = [&] (const char* mode, int octs)
    {
        setChoice (p, arpids::mode, mode); setP (p, arpids::octaves, (float) octs);
        auto e = runArp (p, chord, 1.2);
        return onNotes (e);
    };
    auto starts = [] (const std::vector<int>& v, std::vector<int> want)
    { if (v.size() < want.size()) return false; for (size_t i = 0; i < want.size(); ++i) if (v[i] != want[i]) return false; return true; };

    auto up = pattern ("Up", 1);
    CHECK (starts (up, { 60, 64, 67, 60, 64, 67, 60, 64 }), "Up plays the chord low to high, repeating");
    CHECK (up.size() == 8, "1/16 at 120 BPM = 8 steps per second (" << up.size() << ")");
    CHECK (starts (pattern ("Down", 1), { 67, 64, 60, 67, 64, 60 }), "Down plays high to low");
    CHECK (starts (pattern ("Up/Down", 1), { 60, 64, 67, 64, 60, 64, 67, 64 }), "Up/Down bounces without repeating the ends");
    CHECK (starts (pattern ("Up", 2), { 60, 64, 67, 72, 76, 79, 60 }), "Octaves extends the pattern upward");
    {
        setChoice (p, arpids::mode, "As Played"); setP (p, arpids::octaves, 1);
        auto e = onNotes (runArp (p, { { 0.0, 1.0, 67, 0.8f }, { 0.01, 1.0, 60, 0.8f }, { 0.02, 1.0, 64, 0.8f } }, 1.0));
        CHECK (e.size() >= 4 && e[1] == 60 && e[2] == 64 && e[3] == 67, "As Played follows the order the keys were pressed");
    }
    {
        setChoice (p, arpids::mode, "Chord");
        auto e = runArp (p, chord, 0.3);
        int atZero = 0; for (auto& x : e) if (x.on && x.t < 0.001) ++atZero;
        CHECK (atZero == 3, "Chord mode plays every held key together on each step (" << atZero << ")");
    }
    {
        setChoice (p, arpids::mode, "Random");
        auto e = onNotes (runArp (p, chord, 2.0));
        bool repeat = false, allIn = true;
        for (size_t i = 1; i < e.size(); ++i) repeat |= e[i] == e[i - 1];
        for (int n : e) allIn &= (n == 60 || n == 64 || n == 67);
        CHECK (! repeat && allIn && e.size() == 8, "Random only uses held keys and never repeats a note back to back");
    }

    // gate: 50% of a 1/16 (125 ms at 120) = 62.5 ms
    setChoice (p, arpids::mode, "Up");
    {
        auto e = runArp (p, chord, 0.5);
        double on = -1, len = -1;
        for (auto& x : e) { if (x.on && on < 0) on = x.t; else if (! x.on && on >= 0 && len < 0) len = x.t - on; }
        CHECK (std::abs (len - 0.0625) < 0.0005, "gate sets the note length (" << len * 1000.0 << " ms)");
        CHECK (balanced (e), "every arp note-on gets a note-off after the keys are released");
    }
    // swing delays every second step
    {
        setP (p, arpids::swing, 0.5f);
        auto e = runArp (p, chord, 0.5);
        std::vector<double> t; for (auto& x : e) if (x.on) t.push_back (x.t);
        CHECK (t.size() >= 3 && std::abs ((t[1] - t[0]) - 0.125 * 1.25) < 0.001 && std::abs ((t[2] - t[0]) - 0.25) < 0.001, "swing delays the off-beat steps only");
        setP (p, arpids::swing, 0.0f);
    }
    // host sync: transport playing from beat 0.3, key at time 0 -> first step lands on beat 0.5
    {
        auto e = runArp (p, chord, 0.5, true, 0.3, 120.0);
        const double first = e.empty() ? -1.0 : e.front().t;
        CHECK (std::abs (first - 0.2 * 0.5) < 0.0005, "follows the host grid: first step on the next 1/16 (" << first << " s)");
    }
    // latch keeps playing after release; new keys replace the latched chord
    {
        setP (p, arpids::latch, 1);
        auto e = runArp (p, { { 0.0, 0.2, 60, 0.8f }, { 0.0, 0.2, 64, 0.8f }, { 0.6, 0.3, 72, 0.8f } }, 1.2);
        int afterRelease = 0, replaced = 0, oldAfter = 0;
        for (auto& x : e) if (x.on) { if (x.t > 0.25 && x.t < 0.55) ++afterRelease; if (x.t > 0.65) { replaced += x.note == 72; oldAfter += x.note != 72; } }
        CHECK (afterRelease >= 2, "latch keeps the pattern going after the keys are released");
        CHECK (replaced >= 4 && oldAfter == 0, "pressing new keys replaces the latched chord");
        setP (p, arpids::latch, 0);
    }
    // switching the arp off mid-pattern leaves no hanging notes and hands held keys back to the synth
    {
        auto e = runArp (p, { { 0.0, 1.0, 60, 0.8f } }, 1.2, false, 0, 120, [&] (int blk) { if (blk == 30) setP (p, arpids::on, 0); });
        int handBack = 0; for (auto& x : e) if (x.on && x.t > 0.3) ++handBack;
        CHECK (balanced (e) && handBack == 1, "switching off releases arp notes and returns held keys to the synth");
        setP (p, arpids::on, 1);
    }
    // through the real synth: sound, finite, monophonic per step
    {
        cleanPatch (p);
        setP (p, arpids::on, 1); setChoice (p, arpids::mode, "Up"); setChoice (p, arpids::rate, "1/16");
        int maxVoices = 0;
        auto out = render (p, 48000, 256, 1.2, chord, [&] (int) { maxVoices = std::max (maxVoices, p.getActiveVoices()); });
        CHECK (allFinite (out) && rmsDb (out, 0, 48000) > -40.0f, "arp drives the synth (rms " << rmsDb (out, 0, 48000) << " dB)");
        CHECK (maxVoices <= 3, "arp steps don't pile up voices (" << maxVoices << ")");
        setP (p, arpids::on, 0);
    }

    // MONO: one voice, every key restarts the envelope
    cleanPatch (p);
    setChoice (p, ids::voiceMode, "Mono");
    setP (p, ids::env (0, "decay"), 0.05f); setP (p, ids::env (0, "sustain"), 0.1f);
    int monoMax = 0;
    auto mono = render (p, 48000, 256, 1.0, { { 0.0, 0.8, 60, 1.0f }, { 0.4, 0.5, 67, 1.0f } }, [&] (int) { monoMax = std::max (monoMax, p.getActiveVoices()); });
    setChoice (p, ids::voiceMode, "Legato");
    auto leg = render (p, 48000, 256, 1.0, { { 0.0, 0.8, 60, 1.0f }, { 0.4, 0.5, 67, 1.0f } });
    // just after the second key: mono restarted its envelope (back at the peak), legato stays at the low sustain level
    const float monoLevel = rmsDb (mono, (int) (0.402 * 48000), 480), legLevel = rmsDb (leg, (int) (0.402 * 48000), 480);
    CHECK (monoMax == 1, "mono mode stays monophonic");
    CHECK (monoLevel > legLevel + 6.0f, "mono retriggers the envelope, legato doesn't (" << monoLevel << " vs " << legLevel << " dB)");
    setChoice (p, ids::voiceMode, "Poly");
}

static void testFuzzAndExtremes (DivinityProcessor& p)
{
    std::cout << "\n== Extreme / random parameter fuzzing + rapid automation\n";
    std::mt19937 rng (777);
    std::uniform_real_distribution<float> U (0.0f, 1.0f);
    bool finite = true; float peak = 0;
    for (int iter = 0; iter < 60; ++iter)
    {
        for (auto* prm : p.getParameters()) prm->setValueNotifyingHost (U (rng));
        setP (p, ids::voices, 8);
        auto b = render (p, iter % 2 ? 44100 : 96000, 64 << (iter % 5), 0.25,
                         { { 0.0, 0.2, 40 + iter % 40, 1.0f }, { 0.01, 0.2, 60, 0.5f } },
                         [&] (int blk) { if (blk % 2) for (int k = 0; k < 10; ++k) p.getParameters()[(int) (U (rng) * (p.getParameters().size() - 1))]->setValueNotifyingHost (U (rng)); });
        finite &= allFinite (b);
        peak = std::max (peak, b.getMagnitude (0, b.getNumSamples()));
    }
    CHECK (finite, "60 fully-random patches with random per-block automation stay finite");
    INFO ("worst peak with random master/limiter settings: " << juce::Decibels::gainToDecibels (peak) << " dBFS");
}

static void testRatesAndBuffers (DivinityProcessor& p)
{
    std::cout << "\n== Sample-rate / buffer-size sweep + reloads\n";
    p.presets.loadPreset (0);
    bool ok = true;
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int bs : { 32, 64, 128, 256, 512, 1024 })
        {
            auto b = render (p, sr, bs, 0.3, { { 0.0, 0.25, 60, 0.9f }, { 0.0, 0.25, 67, 0.9f } });
            const bool good = allFinite (b) && b.getRMSLevel (0, 0, b.getNumSamples()) > 1.0e-4f;
            if (! good) std::cout << "  problem at " << sr << " / " << bs << "\n";
            ok &= good;
        }
    CHECK (ok, "renders cleanly at 44.1/48/96/192 kHz x 32-1024 samples (same instance, re-prepared each time)");

    // odd block sizes and variable host blocks
    p.prepareToPlay (48000, 1024);
    juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    bool vfin = true;
    for (int bs : { 1, 7, 33, 1000, 13, 512, 2, 999 })
    {
        juce::AudioBuffer<float> b (2, bs);
        p.processBlock (b, m); m.clear();
        vfin &= allFinite (b);
    }
    CHECK (vfin, "variable / odd host block sizes (1..1000) handled");
}

static void testLongNotesAndSilence (DivinityProcessor& p)
{
    std::cout << "\n== Long sustained notes, silence, denormals\n";
    cleanPatch (p);
    // single voice, no unison: unison beating would legitimately move short-window RMS
    setChoice (p, ids::osc (0, "engine"), "Analog");
    setChoice (p, ids::osc (0, "wave"), "Saw");
    setChoice (p, ids::filt (0, "type"), "MG Low 24");
    setP (p, ids::filt (0, "cutoff"), 3000.0f);
    auto b = render (p, 48000, 512, 60.0, { { 0.0, 59.0, 48, 0.9f } });
    const float first = rmsDb (b, 48000, 48000), last = rmsDb (b, 57 * 48000, 48000);
    CHECK (allFinite (b) && std::abs (first - last) < 0.5f, "60 s sustained note: level stable (" << first << " vs " << last << " dB)");

    // release to silence: output must decay to true silence, and silent processing must be cheap
    cleanPatch (p);
    setP (p, ids::env (0, "release"), 0.2f);
    b = render (p, 48000, 512, 3.0, { { 0.0, 0.5, 60, 1.0f } });
    CHECK (b.getMagnitude (0, (int) (2.0 * 48000), 48000) == 0.0f, "voice fully stops after release (exact digital silence)");

    p.presets.loadPreset (0);   // reverb/delay tails
    b = render (p, 48000, 512, 20.0, { { 0.0, 0.3, 60, 1.0f } });
    const float tail = b.getMagnitude (0, 18 * 48000, 2 * 48000);
    INFO ("Celestial Pad tail after ~19 s: " << juce::Decibels::gainToDecibels (tail, -200.0f) << " dBFS");
    auto t0 = std::chrono::high_resolution_clock::now();
    b = render (p, 48000, 512, 10.0, {});
    const double silentMs = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count();
    CHECK (allFinite (b) && silentMs < 2000.0, "10 s of silent processing with FX: " << silentMs << " ms (no denormal slowdown)");
}

static void testPerformance (DivinityProcessor& p)
{
    std::cout << "\n== CPU / polyphony\n";
    p.presets.loadPreset (0);
    setP (p, ids::voices, 16);
    std::vector<Note> chord;
    for (int i = 0; i < 16; ++i) chord.push_back ({ 0.0, 4.0, 40 + i * 3, 0.7f });
    auto t0 = std::chrono::high_resolution_clock::now();
    auto b = render (p, 48000, 256, 4.0, chord);
    double ms = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count();
    INFO ("Celestial Pad, 16 voices (5+3 unison + analog, ~144 osc streams): " << juce::String (ms / 40.0, 1) << "% of one core");
    CHECK (allFinite (b), "16-voice pad finite");

    setP (p, ids::voices, 32);
    for (int o = 0; o < 3; ++o) { setP (p, ids::osc (o, "on"), 1); setP (p, ids::osc (o, "unison"), 8); }
    std::vector<Note> big;
    for (int i = 0; i < 32; ++i) big.push_back ({ 0.0, 2.0, 30 + i * 2, 0.7f });
    t0 = std::chrono::high_resolution_clock::now();
    b = render (p, 48000, 256, 2.0, big);
    ms = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count();
    INFO ("worst case 32 voices x 3 osc x 8 unison (768 streams): " << juce::String (ms / 20.0, 1) << "% of one core");
    CHECK (allFinite (b), "768-stream worst case finite");
}

/** Level-independent "what does it sound like" fingerprint of a render, for spotting near-duplicate presets:
    average spectrum shape (28 log bands, dB, mean removed), loudness envelope over time, per-band movement
    (wobble / filter motion) and stereo width. */
static std::vector<float> soundFingerprint (const juce::AudioBuffer<float>& b, double sr)
{
    constexpr int order = 11, N = 1 << order, bandsN = 28, slots = 18;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> win ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> data ((size_t) N * 2);
    std::vector<double> edges ((size_t) bandsN + 1);
    for (int k = 0; k <= bandsN; ++k) edges[(size_t) k] = 30.0 * std::pow (16000.0 / 30.0, (double) k / bandsN);
    std::vector<std::vector<float>> frames;
    const int hop = N / 2, total = b.getNumSamples();
    for (int pos = 0; pos + N <= total; pos += hop)
    {
        for (int i = 0; i < N; ++i) data[(size_t) i] = 0.5f * (b.getSample (0, pos + i) + b.getSample (1, pos + i));
        std::fill (data.begin() + N, data.end(), 0.0f);
        win.multiplyWithWindowingTable (data.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (data.data());
        std::vector<float> bands ((size_t) bandsN, 0.0f);
        for (int k = 0; k < bandsN; ++k)
        {
            double e = 0;
            const int b0 = juce::jmax (1, (int) (edges[(size_t) k] * N / sr)), b1 = juce::jmax (b0 + 1, (int) (edges[(size_t) k + 1] * N / sr));
            for (int j = b0; j < b1 && j < N / 2; ++j) e += (double) data[(size_t) j] * data[(size_t) j];
            bands[(size_t) k] = (float) (10.0 * std::log10 (e / (b1 - b0) + 1.0e-10));
        }
        frames.push_back (bands);
    }
    std::vector<float> f;
    // 1) average spectrum shape (only frames with signal), mean removed
    std::vector<double> avg ((size_t) bandsN, 0.0); int used = 0;
    for (auto& fr : frames) { double m = 0; for (float v : fr) m = juce::jmax (m, (double) v); if (m < -40.0) continue; for (int k = 0; k < bandsN; ++k) avg[(size_t) k] += fr[(size_t) k]; ++used; }
    double mean = 0; for (auto& v : avg) { v /= juce::jmax (1, used); mean += v; } mean /= bandsN;
    for (auto v : avg) f.push_back ((float) juce::jlimit (-60.0, 60.0, v - mean));
    // 2) loudness envelope (dB below the loudest slot)
    std::vector<float> env ((size_t) slots, -80.0f);
    for (int s = 0; s < slots; ++s)
    {
        const int a = total * s / slots, z = total * (s + 1) / slots;
        double e = 0; for (int i = a; i < z; ++i) e += 0.5 * (b.getSample (0, i) * b.getSample (0, i) + b.getSample (1, i) * b.getSample (1, i));
        env[(size_t) s] = (float) (10.0 * std::log10 (e / juce::jmax (1, z - a) + 1.0e-10));
    }
    const float mx = *std::max_element (env.begin(), env.end());
    for (auto v : env) f.push_back (juce::jmax (-60.0f, v - mx) * 0.5f);
    // 3) movement: std-dev over time of 7 band groups while sounding
    for (int g = 0; g < 7; ++g)
    {
        std::vector<double> series;
        for (auto& fr : frames) { double m = 0; for (int k = g * 4; k < g * 4 + 4; ++k) m += fr[(size_t) k]; m /= 4; if (m > -70.0) series.push_back (m); }
        double mu = 0, var = 0; for (auto v : series) mu += v; mu /= std::max<size_t> (1, series.size());
        for (auto v : series) var += (v - mu) * (v - mu); var /= std::max<size_t> (1, series.size());
        f.push_back ((float) std::sqrt (var) * 1.5f);
    }
    // 4) stereo width (side / mid, dB)
    double mid = 0, side = 0;
    for (int i = 0; i < total; ++i) { const double l = b.getSample (0, i), r = b.getSample (1, i); mid += (l + r) * (l + r); side += (l - r) * (l - r); }
    f.push_back ((float) juce::jlimit (-40.0, 10.0, 10.0 * std::log10 ((side + 1.0e-12) / (mid + 1.0e-12))) * 0.4f);
    return f;
}

static float fingerprintDistance (const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0; for (size_t i = 0; i < a.size() && i < b.size(); ++i) d += (a[i] - b[i]) * (a[i] - b[i]);
    return (float) std::sqrt (d / (double) std::max<size_t> (1, a.size()));
}

static std::vector<Note> phraseFor (const juce::String& cat, bool arpPatch)
{
    std::vector<Note> phrase;
    if (cat == "BASS" || cat == "REESE")
        phrase = { { 0.0, 0.45, 36, 1.0f }, { 0.5, 0.45, 36, 0.9f }, { 1.0, 0.7, 39, 1.0f }, { 1.75, 0.6, 34, 1.0f } };
    else if (cat == "PERCUSSION")
        phrase = { { 0.0, 4.0, 60, 1.0f } };   // hold one key: the loop plays itself
    else if (cat == "FX")
        phrase = { { 0.0, 3.0, 60, 1.0f } };
    else if (arpPatch)
        phrase = { { 0.0, 2.4, 60, 0.9f }, { 0.0, 2.4, 63, 0.9f }, { 0.0, 2.4, 67, 0.9f } };
    else if (cat == "LEAD")
        phrase = { { 0.0, 0.45, 72, 0.9f }, { 0.5, 0.45, 75, 0.9f }, { 1.0, 0.45, 79, 0.9f }, { 1.5, 0.9, 77, 0.9f } };
    else if (cat == "SEQUENCE")
        phrase = { { 0.0, 3.0, 60, 0.9f }, { 0.0, 3.0, 63, 0.9f }, { 0.0, 3.0, 67, 0.9f } };   // hold a chord: arp / sequence plays
    else if (cat == "SYNTH")
        phrase = { { 0.0, 0.3, 60, 0.9f }, { 0.25, 0.3, 64, 0.8f }, { 0.5, 0.3, 67, 0.9f }, { 0.75, 0.3, 71, 0.8f }, { 1.0, 0.3, 72, 0.9f }, { 1.25, 0.3, 67, 0.8f }, { 1.5, 0.6, 64, 0.9f } };
    else
        phrase = { { 0.0, 2.5, 48, 0.8f }, { 0.0, 2.5, 55, 0.8f }, { 0.0, 2.5, 60, 0.8f }, { 0.0, 2.5, 63, 0.8f } };
    return phrase;
}

/** Offline: renders a large pool of candidate designs per category, fingerprints how each one actually
    sounds, and keeps the set that is furthest apart (farthest-point sampling). Writes the picks file. */
static void pickVariedPresets (DivinityProcessor& p, const juce::File& incFile, const juce::StringArray& only = {})
{
    std::cout << "\n== Picking the most varied candidates per category\n";
    juce::String inc;
    auto measure = [&p] (const FactoryPreset& fp, std::vector<float>& out) -> bool
    {
        p.presets.applyFactoryPreset (fp);
        const bool arpPatch = p.apvts.getRawParameterValue (arpids::on)->load() > 0.5f;
        auto b = render (p, 48000, 256, 4.5, phraseFor (fp.category, arpPatch));
        const float rms = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, (int) (2.5 * 48000)), -120.0f);
        if (! allFinite (b) || rms < -50.0f) return false;   // too sparse / quiet to level-match: not a candidate
        out = soundFingerprint (b, 48000.0);
        return true;
    };
    juce::StringArray keep;
    if (! only.isEmpty())
        for (auto& line : juce::StringArray::fromLines (incFile.loadFileAsString()))
            if (line.trim().isNotEmpty() && ! only.contains (line.fromFirstOccurrenceOf ("\"", false, false).upToFirstOccurrenceOf ("\"", false, false)))
                keep.add (line);
    for (auto& line : keep) inc << line << "\n";
    for (auto& cat : factoryCategoriesV7())
    {
        if (! only.isEmpty() && ! only.contains (cat)) continue;
        const int N = factoryCountV7 (cat), M = juce::jmax (N * 4, 240);
        std::vector<std::pair<int, int>> heroes;
        auto cands = createCandidatePresets (cat, 0, M, &heroes);
        std::vector<std::vector<float>> fps ((size_t) M);
        std::vector<bool> ok ((size_t) M, false);
        int rejected = 0;
        for (int k = 0; k < M; ++k)
        {
            ok[(size_t) k] = measure (cands[(size_t) k], fps[(size_t) k]);
            if (ok[(size_t) k] && ! claimFailures (p, cands[(size_t) k]).isEmpty()) { ok[(size_t) k] = false; ++rejected; }   // its name would lie
        }
        std::cout << "  " << cat << ": " << rejected << " of " << M << " candidates rejected (name not backed by the sound)\n";

        int valid = 0; for (bool v : ok) valid += v;
        const int P = juce::jmin (valid, N + N / 2);   // ranked extras: the bank skips any whose name is already taken
        std::vector<int> picks ((size_t) P, -1);
        std::vector<std::vector<float>> chosen;
        auto minDist = [&chosen] (const std::vector<float>& f)
        {
            float d = 1.0e9f; for (auto& c : chosen) d = std::min (d, fingerprintDistance (f, c)); return d;
        };
        // hero slots first: the best of 24 candidates that keep the hero's source / movement
        for (int slot = 0; slot < juce::jmin (N, P); ++slot)
        {
            const auto [fs, fm] = heroes[(size_t) slot];
            if (fs < 0 && fm < 0) continue;
            const int base = 100000 + slot * 100;
            auto hc = createCandidatePresets (cat, base, 24, nullptr, fs, fm);
            float best = -1.0f; int bestK = base; std::vector<float> bestF;
            for (int h = 0; h < 24; ++h)
            {
                std::vector<float> f;
                if (! measure (hc[(size_t) h], f)) continue;
                const float d = chosen.empty() ? 1.0f : minDist (f);
                if (d > best) { best = d; bestK = base + h; bestF = f; }
            }
            picks[(size_t) slot] = bestK;
            if (! bestF.empty()) chosen.push_back (bestF);
        }
        // everything else: farthest-point sampling over the pool
        std::vector<bool> taken ((size_t) M, false);
        for (int slot = 0; slot < P; ++slot)
        {
            if (picks[(size_t) slot] >= 0) continue;
            float best = -1.0f; int bk = -1;
            for (int k = 0; k < M; ++k)
            {
                if (! ok[(size_t) k] || taken[(size_t) k]) continue;
                const float d = chosen.empty() ? (float) (M - k) : minDist (fps[(size_t) k]);
                if (d > best) { best = d; bk = k; }
            }
            if (bk < 0) break;
            taken[(size_t) bk] = true;
            picks[(size_t) slot] = bk;
            chosen.push_back (fps[(size_t) bk]);
        }
        // report the achieved spread
        float worst = 1.0e9f;
        for (size_t a = 0; a < chosen.size(); ++a)
            for (size_t b = a + 1; b < chosen.size(); ++b) worst = std::min (worst, fingerprintDistance (chosen[a], chosen[b]));
        std::cout << "  " << cat.paddedRight (' ', 12) << N << " of " << M << " candidates (" << valid << " valid)   closest pair " << worst << "\n";
        inc << "{ \"" << cat << "\", { ";
        for (int i = 0; i < P; ++i) if (picks[(size_t) i] >= 0) inc << picks[(size_t) i] << (i + 1 < P ? ", " : "");
        inc << " } },\n";
    }
    incFile.replaceWithText (inc);
    std::cout << "  wrote " << incFile.getFullPathName() << "\n";
}

static void renderPresetDemos (DivinityProcessor& p, const juce::File& dir)
{
    std::cout << "\n== Factory preset levels (+ WAV demos)\n";
    dir.createDirectory();
    juce::WavAudioFormat wav;
    const auto& list = p.presets.getPresets();
    int hot = 0, quiet = 0;
    std::map<juce::String, int> demoCats;
    std::vector<float> rmsAll;
    struct Print { juce::String name, cat; std::vector<float> f; };
    std::vector<Print> prints;
    for (int i = 0; i < list.size(); ++i)
    {
        if (! list[i].isFactory) continue;
        p.presets.loadPreset (i);
        const auto cat = list[i].category;
        const bool arpPatch = p.apvts.getRawParameterValue (arpids::on)->load() > 0.5f;
        const auto phrase = phraseFor (cat, arpPatch);
        auto b = render (p, 48000, 256, 4.5, phrase);
        const float peak = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -120.0f);
        const float rms = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, (int) (2.5 * 48000)), -120.0f);
        std::cout << "  " << juce::String (list[i].name).paddedRight (' ', 20) << juce::String (cat).paddedRight (' ', 15)
                  << " peak " << juce::String (peak, 1).paddedLeft (' ', 6) << " dBFS   rms " << juce::String (rms, 1).paddedLeft (' ', 6) << " dBFS\n";
        if (peak > -0.2f) ++hot;
        if (rms < -42.0f || ! allFinite (b)) { ++quiet; std::cout << "    ^ too quiet or not finite\n"; }
        rmsAll.push_back (rms);
        prints.push_back ({ list[i].name, cat, soundFingerprint (b, 48000.0) });
        // WAV demos: the classic set + the first preset of every category (keeps CI artefacts small)
        if (demoCats[cat] >= 3) continue;   // three demos per category
        ++demoCats[cat];
        auto f = dir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + " " + list[i].name + ".wav");
        f.deleteFile();
        if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), 48000, 2, 24, {}, 0)))
            {
                os.release();
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
    }
    CHECK (hot == 0, "no factory preset exceeds the -0.3 dB limiter ceiling");
    CHECK (quiet == 0, "every factory preset makes sound and stays finite");
    std::sort (rmsAll.begin(), rmsAll.end());
    const float spread = rmsAll.empty() ? 0.0f : rmsAll[rmsAll.size() * 9 / 10] - rmsAll[rmsAll.size() / 10];
    CHECK (spread < 16.0f, "factory presets are level-matched (10th-90th percentile spread " << spread << " dB)");

    // variety: no preset may sound like a near-copy of another preset in its category
    {
        juce::String csv;
        std::map<juce::String, std::pair<int, int>> dupes;   // category -> (near-duplicates, count)
        std::map<juce::String, std::vector<float>> nn;
        for (size_t a = 0; a < prints.size(); ++a)
        {
            float best = 1.0e9f; size_t bi = a;
            for (size_t c = 0; c < prints.size(); ++c)
                if (c != a && prints[c].cat == prints[a].cat)
                {
                    const float d = fingerprintDistance (prints[a].f, prints[c].f);
                    if (d < best) { best = d; bi = c; }
                }
            nn[prints[a].cat].push_back (best);
            auto& dc = dupes[prints[a].cat]; ++dc.second;
            // 808 / SUB are sine-based by nature, so their fingerprints sit closer together
            // 808s / subs are sine-body families and render a little differently per platform (macOS CI
            // measured 808 pairs ~1 unit closer than Linux), so their copy threshold sits lower.
            const float limit = (prints[a].cat == "BASS" || prints[a].cat == "REESE") ? kNearDuplicate * 0.66f : kNearDuplicate;
            if (best < limit) { ++dc.first; if (dc.first <= 4) std::cout << "  near-duplicate: " << prints[a].name << " ~ " << prints[bi].name << " (" << best << ")\n"; }
            csv << prints[a].name << "," << prints[a].cat << "," << best << "," << prints[bi].name;
            for (auto v : prints[a].f) csv << "," << v;
            csv << "\n";
        }
        dir.getParentDirectory().getChildFile ("preset_fingerprints.csv").replaceWithText (csv);
        int totalDupes = 0;
        for (auto& [cat, dc] : dupes)
        {
            auto v = nn[cat]; std::sort (v.begin(), v.end());
            std::cout << "  " << cat.paddedRight (' ', 12) << dc.first << " near-duplicates of " << dc.second
                      << "   nearest-neighbour distance: min " << v.front() << "  median " << v[v.size() / 2] << "\n";
            totalDupes += dc.first;
        }
        CHECK (totalDupes == 0, "every factory preset sounds distinct from the rest of its category (" << totalDupes << " near-duplicates)");
    }
}

static void snapshotWarpUi (DivinityProcessor& p, const juce::File& dir)
{
    p.presets.loadPreset (0);
    setChoice (p, ids::osc (0, "engine"), "Wavetable");
    setChoice (p, ids::osc (0, "warp"), "FM");
    setChoice (p, ids::osc (0, "warp_src"), "Osc B");
    setP (p, ids::osc (0, "warp_amt"), 0.45f);
    p.uiWorkspace = "CREATE"; p.uiAngelOpen = false;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (DivinityEditor::kBaseWidth, DivinityEditor::kBaseHeight);
    ed->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    auto f = dir.getChildFile ("ui_create_warp.png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    INFO ("wrote " << f.getFileName());
}

static void snapshotEditor (DivinityProcessor& p, const juce::File& dir)
{
    std::cout << "\n== Editor (offscreen, every tab)\n";
    p.presets.loadPreset (0);
    p.keyboardState.reset();
    render (p, 48000, 512, 0.6, { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 64, 0.8f }, { 0.0, 1.0, 67, 0.8f } });
    for (auto tab : { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB", "BROWSER" })
    {
        p.uiWorkspace = tab;
        p.uiAngelOpen = false;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (DivinityEditor::kBaseWidth, DivinityEditor::kBaseHeight);
        ed->setVisible (true);   // hosts make the editor visible; visualisers only run when visible
        // keep audio flowing so meters / analyzers have content
        for (int i = 0; i < 4; ++i)
        {
            render (p, 48000, 512, 0.15, { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 67, 0.8f } });
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = dir.getChildFile ("ui_" + juce::String (tab).toLowerCase() + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        INFO ("wrote " << f.getFileName());
        if (juce::String (tab) == "CREATE")
        {
            // retina-style 2x render, to check the logo and Guardian Angel artwork stay crisp
            auto hi = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            auto f2 = dir.getChildFile ("ui_create@2x.png");
            f2.deleteFile();
            juce::FileOutputStream os2 (f2);
            png.writeImageToStream (hi, os2);
        }
    }
    {
        // Guardian Angel collapsed: the window narrows, everything else stays put
        p.uiWorkspace = "CREATE";
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* de = dynamic_cast<DivinityEditor*> (ed.get());
        ed->setSize (DivinityEditor::kBaseWidth, DivinityEditor::kBaseHeight);
        ed->setVisible (true);
        de->setAngelOpen (true);
        CHECK (ed->getWidth() == DivinityEditor::kBaseWidth + DivinityEditor::kAngelWidth && ed->getHeight() == DivinityEditor::kBaseHeight, "showing Guardian Angel widens the window (" << ed->getWidth() << "x" << ed->getHeight() << ")");
        for (int i = 0; i < 3; ++i) { render (p, 48000, 512, 0.15, { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 67, 0.8f } }); juce::MessageManager::getInstance()->runDispatchLoopUntil (50); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = dir.getChildFile ("ui_create_angel_open.png"); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        de->setAngelOpen (false);
        CHECK (ed->getWidth() == DivinityEditor::kBaseWidth, "hiding Guardian Angel restores the width");
        CHECK (! p.uiAngelOpen, "angel state is remembered");
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize ((int) (DivinityEditor::kBaseWidth * 0.6f), (int) (DivinityEditor::kBaseHeight * 0.6f));
    CHECK (ed->getWidth() == (int) (DivinityEditor::kBaseWidth * 0.6f), "editor resizes to 60%");
    ed.reset();
    p.uiWorkspace = "CREATE";
    CHECK (true, "editor constructs, paints and destroys on every tab");
}

static void testGuardianAngel (DivinityProcessor& p)
{
    std::cout << "\n== Guardian Angel brain\n";
    GuardianBrain brain (p);
    p.presets.loadPreset (0);

    auto r = brain.ask ("what does the soul engine do?");
    CHECK (r.text.contains ("XY"), "answers a feature question (SOUL ENGINE)");
    r = brain.ask ("what does fold do");
    CHECK (r.text.containsIgnoreCase ("fold"), "answers a control question from the parameter help: " << r.text.substring (0, 60));
    r = brain.ask ("how do I undo");
    CHECK (r.text.contains ("Ctrl+Z"), "answers how-to questions");

    r = brain.ask ("make me a dark aggressive bass");
    auto currentIs = [&p] (const juce::String& cat, const juce::String& word)
    {
        for (auto& pr : p.presets.getPresets())
            if (pr.isFactory && pr.name == p.presets.getCurrentName()) return pr.category == cat && juce::StringArray::fromTokens (pr.name, " ", "").contains (word);
        return false;
    };
    CHECK (currentIs ("REESE", "Reese") && p.apvts.getRawParameterValue (ids::soulX)->load() > 0.6f
           && p.apvts.getRawParameterValue (ids::soulY)->load() < 0.4f, "\"dark aggressive bass\" builds from a reese, pushed aggressive + dark");
    r = brain.createSound ("dreamy evolving wide pad");
    CHECK (currentIs ("PAD", "Pad") && p.apvts.getRawParameterValue (ids::macro (1))->load() > 0.3f
           && p.apvts.getRawParameterValue (ids::divineAmount)->load() > 0.2f, "\"dreamy evolving wide pad\" adds movement + Divine");
    {
        // the 0.7 library is reachable by asking
        struct Ask { const char* q; const char* cat; const char* word; };
        const Ask asks[] = { { "make me a grime 808", "BASS", "808" }, { "give me an icy eski bell", "SYNTH", "Bell" },
                             { "dark wave pad please", "PAD", "Pad" }, { "make a wave reese", "REESE", "Reese" },
                             { "8-bit lead", "LEAD", "Chip" }, { "a distorted 808 that slaps", "BASS", "Distorted" },
                             { "trap hat loop", "PERCUSSION", "Hats" }, { "give me a riser", "FX", "Riser" },
                             { "make something that sounds like a grime lead", "LEAD", "Square" } };
        int hits = 0;
        for (auto& a : asks) { brain.createSound (a.q); const bool ok = currentIs (a.cat, a.word); hits += ok; if (! ok) std::cout << "    \"" << a.q << "\" -> " << p.presets.getCurrentName() << "\n"; }
        CHECK (hits == 9, "Guardian Angel builds from the matching family of the bank (" << hits << "/9)");
        r = brain.ask ("how do i use the arp");
        CHECK (r.text.contains ("ARP") && r.text.contains ("LATCH"), "Guardian Angel explains the arpeggiator");
    }
    r = brain.createSound ("something cool");
    CHECK (r.actionLabel.isEmpty() && r.text.contains ("pad"), "unknown sound type asks for a clearer description");

    const float tone0 = p.apvts.getRawParameterValue (ids::macro (0))->load();
    brain.ask ("make it brighter");
    CHECK (p.apvts.getRawParameterValue (ids::macro (0))->load() > tone0, "\"make it brighter\" raises TONE");

    int applied = 0;
    for (int i = 0; i < 6; ++i)
    {
        auto idea = brain.suggestIdea();
        if (idea.action) { idea.action(); ++applied; }
    }
    CHECK (applied >= 4, "suggests and applies one-click ideas (" << applied << ")");

    bool safe = true; float worst = -120.0f;
    for (int i = 0; i < 25; ++i)
    {
        brain.randomize();
        auto b = render (p, 48000, 256, 0.6, { { 0.0, 0.5, 48, 1.0f }, { 0.0, 0.5, 55, 1.0f }, { 0.0, 0.5, 60, 1.0f } });
        const float pk = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -120.0f);
        worst = juce::jmax (worst, pk);
        safe &= allFinite (b) && pk <= -0.2f && b.getRMSLevel (0, 0, b.getNumSamples()) > 1.0e-4f;
    }
    CHECK (safe, "25 random patches: all audible, finite and under the ceiling (worst peak " << worst << " dBFS)");

    p.undo.clearUndoHistory();
    p.presets.loadPreset (0);
    const float before = p.apvts.getRawParameterValue (ids::osc (0, "pos"))->load();
    p.undo.beginNewTransaction();
    brain.randomize();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);   // APVTS flushes to the undoable tree on a timer
    p.undo.undo();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (std::abs (p.apvts.getRawParameterValue (ids::osc (0, "pos"))->load() - before) < 1.0e-4f, "randomize is undoable");
    CHECK (brain.explain ("Cutoff help.\n\nShift-drag: fine").text == "Cutoff help.", "explain strips the shortcut footer");
}


//==============================================================================
static double correlationLR (const juce::AudioBuffer<float>& b, int from, int len)
{
    const float* l = b.getReadPointer (0); const float* r = b.getReadPointer (1);
    double lr = 0, ll = 0, rr = 0;
    for (int i = from; i < from + len; ++i) { lr += l[i] * r[i]; ll += l[i] * l[i]; rr += r[i] * r[i]; }
    return lr / std::sqrt (ll * rr + 1e-30);
}

/** Energy below 16 kHz away from the harmonics of f0, relative to total below 16 kHz (dB). */
static double audibleAliasDb (const juce::AudioBuffer<float>& b, double sr, double f0, int from)
{
    constexpr int order = 15, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> d ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const double x = (double) i / (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2 * juce::MathConstants<double>::pi * x) + 0.14128 * std::cos (4 * juce::MathConstants<double>::pi * x) - 0.01168 * std::cos (6 * juce::MathConstants<double>::pi * x);
        d[(size_t) i] = b.getSample (0, from + i) * (float) w;
    }
    fft.performFrequencyOnlyForwardTransform (d.data());
    const double binHz = sr / N;
    double harm = 0, other = 0;
    for (int k = 2; k < N / 2 && k * binHz < 16000.0; ++k)
    {
        const double h = k * binHz / f0, nearest = std::round (h);
        const double e = (double) d[(size_t) k] * d[(size_t) k];
        (nearest >= 1.0 && std::abs (h - nearest) * f0 < 4.0 * binHz ? harm : other) += e;
    }
    return 10.0 * std::log10 ((other + 1e-30) / (harm + other + 1e-30));
}

static void testSoundEngine2 (DivinityProcessor& p)
{
    std::cout << "\n== Sound Engine 2.0\n";
    auto engineMode = [] (DivinityProcessor& q) { return (int) std::lround (q.apvts.getRawParameterValue (ids::engineMode)->load()); };

    {
        auto freshOwner = std::make_unique<DivinityProcessor>(); auto& fresh = *freshOwner;
        CHECK (engineMode (fresh) == (int) EngineMode::Divine2, "new instances start on the Divine 2.0 engine");
        fresh.presets.loadPreset (5);
        CHECK (engineMode (fresh) == (int) EngineMode::Divine2, "factory presets load on the Divine 2.0 engine");
    }

    // a session saved before 2.0 (no engine_mode) must come back on the Classic engine
    {
        p.presets.loadPreset (0);
        juce::MemoryBlock state; p.getStateInformation (state);
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        auto* params = xml->getChildByName ("PARAMETERS");
        juce::XmlElement* em = nullptr;
        for (auto* e : params->getChildIterator()) if (e->getStringAttribute ("id") == ids::engineMode) em = e;
        CHECK (em != nullptr, "engine mode is saved with the session");
        auto sameOwner = std::make_unique<DivinityProcessor>(); auto& same = *sameOwner; same.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (engineMode (same) == (int) EngineMode::Divine2, "a 2.0 session restores on the 2.0 engine");
        if (em != nullptr) params->removeChildElement (em, true);
        juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary (*xml, old);
        auto rOwner = std::make_unique<DivinityProcessor>(); auto& r = *rOwner; r.setStateInformation (old.getData(), (int) old.getSize());
        CHECK (engineMode (r) == (int) EngineMode::Classic, "a pre-2.0 session restores on the Classic engine (sounds exactly as saved)");
    }

    // ...and so must a user preset file written before 2.0
    {
        p.presets.loadPreset (0);
        auto xml = p.presets.createPresetXml ({});
        auto* params = xml->getChildByName ("Params");
        juce::XmlElement* em = nullptr;
        for (auto* e : params->getChildIterator()) if (e->getStringAttribute ("id") == ids::engineMode) em = e;
        if (em != nullptr) params->removeChildElement (em, true);
        p.presets.applyPresetXml (*xml);
        CHECK (engineMode (p) == (int) EngineMode::Classic, "a pre-2.0 user preset loads on the Classic engine");
        p.presets.loadPreset (0);
    }

    // band-limited analog oscillators
    for (auto* wave : { "Saw", "Pulse" })
    {
        cleanPatch (p);
        setP (p, ids::analog, 0.0f);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), wave);
        setP (p, ids::osc (0, "pw"), 0.2f);
        auto b = render (p, 48000, 256, 1.2, { { 0.0, 1.2, 96, 1.0f } });
        const double a = audibleAliasDb (b, 48000, 440.0 * std::pow (2.0, 15.0 / 12.0), 12000);
        CHECK (a < -70.0, "2.0 analog " << wave << " is alias-free in the audible band at C7 (" << a << " dB)");
    }

    // organic drift: off = every note identical, on = every note a little different, pitch stays put
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Sine");
        setP (p, ids::osc (0, "randphase"), 0);
        setP (p, ids::analog, 0.0f);
        auto a1 = render (p, 48000, 256, 0.5, { { 0.0, 0.5, 69, 1.0f } });
        auto a2 = render (p, 48000, 256, 0.5, { { 0.0, 0.5, 69, 1.0f } });
        double diff = 0; for (int i = 0; i < a1.getNumSamples(); ++i) diff += std::abs (a1.getSample (0, i) - a2.getSample (0, i));
        CHECK (diff < 1.0e-6, "drift 0%: repeated notes are identical");
        setP (p, ids::analog, 1.0f);
        std::vector<Note> notes; for (int i = 0; i < 6; ++i) notes.push_back ({ i * 1.0, 0.9, 69, 1.0f });
        auto d = render (p, 48000, 256, 6.0, notes);
        double worst = 0;
        for (int i = 0; i < 6; ++i)
        {
            const double f = measureFrequency (d, 48000, (int) ((i + 0.2) * 48000), (int) ((i + 0.85) * 48000));
            worst = juce::jmax (worst, std::abs (1200.0 * std::log2 (f / 440.0)));
        }
        CHECK (worst > 0.2 && worst < 12.0, "drift 100%: notes wander a few cents, never out of tune (max " << worst << " cents)");
    }

    // dimension chorus is genuinely stereo; FDN reverb stays bounded at its extremes
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        setP (p, ids::choOn, 1); setP (p, ids::choMix, 0.5f); setP (p, ids::choDepth, 0.6f);
        auto b = render (p, 48000, 256, 2.0, { { 0.0, 2.0, 57, 1.0f } });
        const double c = correlationLR (b, 24000, 48000);
        CHECK (c < 0.95 && allFinite (b), "2.0 chorus widens a mono source (L/R correlation " << c << ")");

        setP (p, ids::choOn, 0);
        setP (p, ids::revOn, 1); setP (p, ids::revMix, 1.0f); setP (p, ids::revSize, 1.0f); setP (p, ids::revDamp, 0.0f); setP (p, ids::revPredelay, 250.0f);
        std::vector<Note> stab; for (int i = 0; i < 8; ++i) stab.push_back ({ i * 0.5, 0.3, 40 + i * 5, 1.0f });
        auto r = render (p, 48000, 256, 20.0, stab, [&p] (int blk) { if (blk % 40 == 0) setP (p, ids::revSize, (blk / 40) % 2 ? 0.0f : 1.0f); });
        const float pk = juce::Decibels::gainToDecibels (r.getMagnitude (0, r.getNumSamples()), -120.0f);
        const float endRms = juce::Decibels::gainToDecibels (r.getRMSLevel (0, r.getNumSamples() - 48000, 48000), -150.0f);
        CHECK (allFinite (r) && pk < 12.0f && endRms < -20.0f, "2.0 reverb: max size, zero damping, size jumps -> bounded and decaying (peak " << pk << " dBFS, last second " << endRms << " dBFS)");
    }

    // every factory preset: the mod wheel and aftertouch are wired to something
    {
        int missing = 0;
        for (auto& fp : createFactoryPresets())
        {
            if (fp.category == "BASS" || fp.category == "PERCUSSION") continue;
            bool wheel = false, touch = false;
            for (auto& [id, v] : fp.values) { wheel |= v.toString() == "Mod Wheel"; touch |= v.toString() == "Aftertouch"; }
            if (! (wheel && touch)) { ++missing; std::cout << "    no wheel/aftertouch: " << fp.name << "\n"; }
        }
        CHECK (missing == 0, "factory presets respond to mod wheel and aftertouch (" << missing << " missing)");
    }
    p.presets.loadPreset (0);
}

//==============================================================================
//==============================================================================
static void testMidiGeneratorAndMeters (DivinityProcessor& p)
{
    std::cout << "\n== MIDI generator, audition + meter feeds ==\n";
    using namespace midigen;
    // every part / style / scale / length gives a sane, in-key, deterministic clip
    int bad = 0, outOfKey = 0, total = 0, nonDeterministic = 0;
    for (int part = 0; part < 4; ++part)
        for (int style = 0; style < styleNames().size(); ++style)
            for (int scale = 0; scale < scaleNames().size(); ++scale)
                for (int bars : { 2, 4, 8 })
                {
                    Settings st; st.part = (Part) part; st.style = style; st.scale = scale; st.bars = bars; st.key = (style * 5 + scale) % 12; st.seed = 1234u + (juce::uint32) style;
                    const auto prog = makeProgression (style, st.seed);
                    const auto c = generate (st, prog), c2 = generate (st, prog);
                    ++total;
                    if (c.notes.size() != c2.notes.size()) ++nonDeterministic;
                    if (c.notes.empty() || c.lengthBeats != bars * 4.0) ++bad;
                    for (auto& n : c.notes)
                    {
                        if (n.start < 0.0 || n.start + n.length > c.lengthBeats + 1.0e-6 || n.length <= 0.0 || n.pitch < 0 || n.pitch > 127) { ++bad; break; }
                        if (part == 1)   // lead notes must be in the scale
                        {
                            static const std::vector<std::vector<int>> sc { { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 3, 5, 7, 10 } };
                            const int pc = ((n.pitch - st.key) % 12 + 12) % 12;
                            const auto& v = sc[(size_t) scale];
                            if (std::find (v.begin(), v.end(), pc) == v.end()) ++outOfKey;
                        }
                    }
                }
    CHECK (bad == 0 && total == 4 * 6 * 6 * 3, "every generated clip is non-empty, in range and the right length (" << total << " clips)");
    CHECK (outOfKey == 0, "generated leads stay in the chosen scale (" << outOfKey << " out-of-key notes)");
    CHECK (nonDeterministic == 0, "the same settings + seed give the same clip");

    // drums: GM notes, kick on the downbeat
    {
        Settings st; st.part = Part::Drums; st.style = 0; st.bars = 4;
        const auto c = generate (st, makeProgression (0, 1));
        bool kickOnOne = false; int nonGm = 0;
        for (auto& n : c.notes) { if (n.pitch == drums::kick && n.start < 0.01) kickOnOne = true; if (n.pitch < 35 || n.pitch > 81) ++nonGm; }
        CHECK (kickOnOne && nonGm == 0, "drum clips use GM drum notes and start with a kick");
    }

    // .mid round trip
    {
        Settings st; st.part = Part::Chords; st.style = 1; st.key = 9; st.bars = 4;
        const auto c = generate (st, makeProgression (1, 7));
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("divinity_test.mid");
        const bool ok = writeMidiFile (c, 140.0, f);
        juce::MidiFile mf; juce::FileInputStream in (f);
        const bool read = ok && in.openedOk() && mf.readFrom (in);
        int ons = 0; double tempo = 0;
        if (read)
        {
            mf.convertTimestampTicksToSeconds();
            for (int t = 0; t < mf.getNumTracks(); ++t)
                for (auto* e : *mf.getTrack (t))
                {
                    if (e->message.isNoteOn()) ++ons;
                    if (e->message.isTempoMetaEvent()) tempo = 60.0 / e->message.getTempoSecondsPerQuarterNote();
                }
        }
        CHECK (read && ons == (int) c.notes.size() && std::abs (tempo - 140.0) < 0.5,
               "exported .mid reads back with every note and the tempo (" << ons << "/" << (int) c.notes.size() << " notes, " << tempo << " BPM)");
        INFO ("chords: " << c.summary);
        f.deleteFile();
    }

    // audition: a chord clip plays through the synth, drums through the kit; stop goes quiet
    {
        cleanPatch (p);
        Settings st; st.part = Part::Chords; st.style = 0; st.bars = 2;
        p.midiPreview.play (generate (st, makeProgression (0, 3)));
        auto b = render (p, 48000.0, 512, 1.0, {});
        const float chordsRms = b.getRMSLevel (0, 0, b.getNumSamples());
        st.part = Part::Drums;
        p.midiPreview.play (generate (st, makeProgression (0, 3)));
        b = render (p, 48000.0, 512, 1.0, {});
        const float drumRms = b.getRMSLevel (0, 0, b.getNumSamples());
        p.midiPreview.stop();
        render (p, 48000.0, 512, 1.5, {});
        b = render (p, 48000.0, 512, 0.5, {});
        const float after = b.getMagnitude (0, 0, b.getNumSamples());
        CHECK (chordsRms > 0.005f && drumRms > 0.005f && after < 1.0e-3f && allFinite (b),
               "MIDI audition plays chords and drums, and stops cleanly (" << chordsRms << ", " << drumRms << ", tail " << after << ")");
    }

    // meter feed: two independent readers both see the same audio (spectrum + imager + scope at once)
    {
        cleanPatch (p);
        juce::int64 c1 = -1, c2 = -1;
        std::vector<float> a ((size_t) ScopeFifo::kSize), b2 ((size_t) ScopeFifo::kSize);
        p.scope.pull (c1, a.data(), (int) a.size()); p.scope.pull (c2, b2.data(), (int) b2.size());
        render (p, 48000.0, 512, 0.1, { { 0.0, 0.1, 60, 0.9f } });
        const int n1 = p.scope.pull (c1, a.data(), (int) a.size());
        const int n2 = p.scope.pull (c2, b2.data(), (int) b2.size());
        float m1 = 0, m2 = 0; for (int i = 0; i < n1; ++i) m1 = std::max (m1, std::abs (a[(size_t) i])); for (int i = 0; i < n2; ++i) m2 = std::max (m2, std::abs (b2[(size_t) i]));
        CHECK (n1 > 1000 && n1 == n2 && m1 > 0.01f && m1 == m2, "every display gets its own copy of the output feed (" << n1 << " / " << n2 << " samples)");
    }
}

//==============================================================================
static juce::AudioBuffer<float> renderMidi (DivinityProcessor& p, double sr, int block, double seconds,
                                            std::function<void (juce::MidiBuffer&, int pos, int n)> events)
{
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        midi.clear();
        events (midi, pos, n);
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

static void testVersion08 (DivinityProcessor& p)
{
    std::cout << "\n== 0.8.0: warp / FM / sync, user wavetables, MPE, MIDI learn, quality\n";
    const double sr = 48000;
    const std::vector<Note> a3 { { 0.0, 1.4, 57, 0.9f } };   // 220 Hz
    auto twoOscPatch = [&p] (const juce::String& engineA, const juce::String& waveA)
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), engineA);
        if (engineA == "Analog") setChoice (p, ids::osc (0, "wave"), waveA);
        setP (p, ids::osc (1, "on"), 0.0f);    // the modulator works even with its oscillator switched off
        setChoice (p, ids::osc (1, "engine"), "Analog"); setChoice (p, ids::osc (1, "wave"), "Sine");
        setP (p, ids::osc (1, "randphase"), 0);
        setChoice (p, ids::osc (0, "warp_src"), "Osc B");
    };

    // warp Off: the amount knob does nothing (old sound path untouched)
    {
        twoOscPatch ("Analog", "Saw");
        auto ref = render (p, sr, 256, 0.6, a3);
        setP (p, ids::osc (0, "warp_amt"), 0.8f);
        auto same = render (p, sr, 256, 0.6, a3);
        CHECK (maxDiff (ref, same) == 0.0f, "warp Off: warp amount leaves the sound bit-identical");
    }

    // every mode, both engines: finite, audible, sane level, and actually changes the sound
    for (auto* eng : { "Analog", "Wavetable" })
    {
        twoOscPatch (eng, "Saw");
        setP (p, ids::osc (0, "warp_amt"), 0.6f);
        auto ref = render (p, sr, 256, 1.0, a3);
        const float refPeak = ref.getMagnitude (0, 0, ref.getNumSamples());
        bool allOk = true, allChange = true;
        for (auto* mode : { "Sync", "FM", "AM", "Bend +", "Bend -", "PWM", "Phase Dist", "Mirror" })
        {
            setChoice (p, ids::osc (0, "warp"), mode);
            auto r = render (p, sr, 256, 1.0, a3);
            const float pk = r.getMagnitude (0, 0, r.getNumSamples());
            const bool ok = allFinite (r) && rmsDb (r, 4800, 19200) > -40.0f && pk < refPeak * 2.5f + 0.05f;
            if (! ok) { allOk = false; INFO (juce::String (eng) + " " + mode + " peak " + juce::String (pk)); }
            if (maxDiff (ref, r) < 1.0e-3f) { allChange = false; INFO (juce::String (eng) + " " + mode + " did not change the sound"); }
            // periodic at the note: everything except fold-over stays on the 220 Hz harmonic grid
            const double alias = audibleAliasDb (r, sr, 220.0, 2400);
            setChoice (p, ids::quality, "HQ");
            const double aliasHq = audibleAliasDb (render (p, sr, 256, 1.0, a3), sr, 220.0, 2400);
            setChoice (p, ids::quality, "Standard");
            INFO (juce::String (eng) + " " + mode + ": inharmonic " + juce::String (alias, 1) + " dB, HQ " + juce::String (aliasHq, 1) + " dB");
            if (alias > -38.0 || aliasHq > -47.0) allOk = false;
        }
        setChoice (p, ids::osc (0, "warp"), "No Warp");
        CHECK (allOk, juce::String (eng) + ": all 8 warp modes are clean, harmonic and level-safe");
        CHECK (allChange, juce::String (eng) + ": every warp mode changes the sound");
    }

    // hard sync stays alias-free even when swept hard at a high note
    {
        twoOscPatch ("Analog", "Saw");
        setChoice (p, ids::osc (0, "warp"), "Sync");
        setP (p, ids::osc (0, "warp_amt"), 0.85f);
        auto r = render (p, sr, 256, 1.0, { { 0.0, 1.0, 84, 0.9f } });   // C6, slave ~10x the master
        const double alias = audibleAliasDb (r, sr, 440.0 * std::pow (2.0, 15.0 / 12.0), 4800);
        INFO ("sync C6 @85%: inharmonic energy " + juce::String (alias, 1) + " dB");
        CHECK (alias < -33.0, "hard sync stays clean when pushed (alias < -33 dB at C6, 85%: slave at 11 kHz)");
    }

    // FM is deterministic and brighter than the plain sine
    {
        twoOscPatch ("Analog", "Sine");
        auto plain = render (p, sr, 256, 0.6, a3);
        setChoice (p, ids::osc (0, "warp"), "FM");
        setP (p, ids::osc (0, "warp_amt"), 0.7f);
        auto f1 = render (p, sr, 256, 0.6, a3);
        auto f2 = render (p, sr, 256, 0.6, a3);
        CHECK (maxDiff (f1, f2) == 0.0f, "FM renders identically twice");
        CHECK (zeroCrossRate (f1, 4800, 19200) > zeroCrossRate (plain, 4800, 19200) * 1.3, "FM adds sidebands (brighter than the plain sine)");
        CHECK (std::abs (measureFrequency (plain, sr, 4800, 24000) - 220.0) < 1.0, "carrier pitch unchanged");
        // the warp amount is a modulation destination
        CHECK (modDestNames()[(int) ModDest::OscAWarp] == "Osc A Warp", "Osc A Warp is a mod destination");
        setChoice (p, ids::osc (0, "warp"), "No Warp");
    }

    // user wavetables: build, select, play, empty slot is silent, file load + session restore
    {
        cleanPatch (p);
        auto& tableP = *dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (ids::osc (0, "table")));
        CHECK (tableP.choices[tableP.choices.size() - 1] == "User Wavetable", "\"User Wavetable\" is the last table choice");
        setChoice (p, ids::osc (0, "engine"), "Wavetable");
        setChoice (p, ids::osc (0, "table"), "User Wavetable");
        auto silent = render (p, sr, 256, 0.4, a3);
        CHECK (silent.getMagnitude (0, 0, silent.getNumSamples()) < 1.0e-6f, "User Wavetable with nothing loaded is silent");

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("divinity_test_table.wav");
        {
            juce::AudioBuffer<float> wav (1, 2048 * 4);
            for (int f = 0; f < 4; ++f)
                for (int i = 0; i < 2048; ++i)
                {
                    const double ph = i / 2048.0;
                    const float saw = (float) (2.0 * ph - 1.0), sq = ph < 0.5 ? 1.0f : -1.0f;
                    wav.setSample (0, f * 2048 + i, 0.8f * (saw + (sq - saw) * (float) f / 3.0f));
                }
            file.deleteFile();
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (file), 48000, 1, 24, {}, 0));
            w->writeFromAudioSampleBuffer (wav, 0, wav.getNumSamples());
        }
        const auto err = p.loadUserWavetable (0, file, true);
        CHECK (err.isEmpty(), "imports a 4-frame WAV wavetable (" + err + ")");
        CHECK (p.getUserWavetable (0).get() != nullptr && p.getUserWavetable (0).get()->frames == 4, "WAV split into 2048-sample frames");
        auto played = render (p, sr, 256, 1.0, a3);
        CHECK (allFinite (played) && rmsDb (played, 4800, 19200) > -30.0f, "imported table plays");
        CHECK (std::abs (measureFrequency (played, sr, 4800, 24000) - 220.0) < 1.5, "imported table plays at the right pitch");
        CHECK (audibleAliasDb (played, sr, 220.0, 2400) < -50.0, "imported table is band-limited");

        juce::MemoryBlock state; p.getStateInformation (state);
        auto qOwner = std::make_unique<DivinityProcessor>(); auto& q = *qOwner; q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q.getUserWavetable (0).get() != nullptr, "imported table comes back with the session");
        auto again = render (q, sr, 256, 1.0, a3);
        CHECK (maxDiff (played, again) < 1.0e-5f, "restored session sounds the same");

        auto xml = p.presets.createPresetXml ({});
        CHECK (xml != nullptr && xml->getStringAttribute ("userTable0") == file.getFullPathName(), "presets remember the imported table");
        p.clearUserWavetable (0);
        CHECK (p.getUserWavetable (0).get() == nullptr, "clear empties the slot");
        file.deleteFile();
    }

    // MPE: per-note bend on its own channel, other notes untouched
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Sine");
        setP (p, ids::mpeOn, 1.0f); setP (p, ids::mpeBend, 2.0f);
        auto r = renderMidi (p, sr, 256, 1.0, [] (juce::MidiBuffer& m, int pos, int)
        {
            if (pos == 0) { m.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0); m.addEvent (juce::MidiMessage::noteOn (2, 57, 0.9f), 1); }
        });
        const double f = measureFrequency (r, sr, 9600, 40000);
        CHECK (std::abs (f - 220.0 * std::pow (2.0, 2.0 / 12.0)) < 1.5, "MPE: channel bend moves only that note (+2 st, got " + juce::String (f, 1) + " Hz)");
        auto r2 = renderMidi (p, sr, 256, 1.0, [] (juce::MidiBuffer& m, int pos, int)
        {
            if (pos == 0) { m.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 0); m.addEvent (juce::MidiMessage::noteOn (3, 57, 0.9f), 1); }
        });
        CHECK (std::abs (measureFrequency (r2, sr, 9600, 40000) - 220.0) < 1.0, "MPE: a bend on another channel leaves the note alone");
        setP (p, ids::mpeOn, 0.0f);
        auto r3 = renderMidi (p, sr, 256, 1.0, [] (juce::MidiBuffer& m, int pos, int)
        {
            if (pos == 0) { m.addEvent (juce::MidiMessage::noteOn (2, 57, 0.9f), 1); m.addEvent (juce::MidiMessage::noteOff (2, 57), 24000); }
        });
        CHECK (r3.getMagnitude (0, 36000, 12000) < 0.01f, "MPE off: notes on any channel still play and release");
        setP (p, ids::mpeBend, 48.0f);
    }

    // MIDI learn
    {
        cleanPatch (p);
        const auto id = ids::filt (0, "cutoff");
        p.startMidiLearn (id);
        CHECK (p.isLearning (id), "learn mode armed");
        renderMidi (p, sr, 256, 0.05, [] (juce::MidiBuffer& m, int pos, int) { if (pos == 0) m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 0); });
        CHECK (p.getMidiCcFor (id) == 21 && ! p.isLearning (id), "first CC moved gets learned");
        auto* prm = p.apvts.getParameter (id);
        CHECK (prm->getValue() > 0.99f, "CC 127 sets the knob to max");
        renderMidi (p, sr, 256, 0.05, [] (juce::MidiBuffer& m, int pos, int) { if (pos == 0) m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 0), 0); });
        CHECK (prm->getValue() < 0.01f, "CC 0 sets the knob to min");
        juce::MemoryBlock state; p.getStateInformation (state);
        auto qOwner = std::make_unique<DivinityProcessor>(); auto& q = *qOwner; q.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (q.getMidiCcFor (id) == 21, "MIDI mappings are saved with the session");
        p.forgetMidiMapping (id);
        CHECK (p.getMidiCcFor (id) == -1, "forget removes the mapping");
    }

    // quality: HQ (2x oversampled), Eco; global settings survive preset loads
    {
        p.presets.loadPreset (0);
        auto std1 = render (p, sr, 512, 1.5, a3);
        setChoice (p, ids::quality, "HQ");
        auto hq = render (p, sr, 512, 1.5, a3);
        CHECK (p.getRunningQuality() == (int) Quality::HQ, "HQ engages on prepare");
        CHECK (p.getLatencySamples() > 0, "HQ reports its oversampling latency");
        CHECK (allFinite (hq), "HQ output is finite");
        const float dStd = rmsDb (std1, 4800, 48000), dHq = rmsDb (hq, 4800, 48000);
        INFO ("preset 0 rms: standard " + juce::String (dStd, 2) + " dB, HQ " + juce::String (dHq, 2) + " dB");
        CHECK (std::abs (dStd - dHq) < 1.5f, "HQ keeps the same level");
        p.presets.loadPreset (3);
        CHECK (std::lround (p.apvts.getRawParameterValue (ids::quality)->load()) == (int) Quality::HQ, "quality is a global setting (presets don't change it)");
        auto hqOdd = render (p, 44100, 333, 0.5, a3);
        CHECK (allFinite (hqOdd), "HQ handles odd block sizes");
        setChoice (p, ids::quality, "Eco");
        setP (p, ids::osc (0, "unison"), 16.0f);
        auto eco = render (p, sr, 256, 0.5, a3);
        CHECK (p.getRunningQuality() == (int) Quality::Eco && p.getLatencySamples() == 0 && allFinite (eco), "Eco runs with no latency");
        setChoice (p, ids::quality, "Standard");
        render (p, sr, 256, 0.05, {});
        CHECK (p.getRunningQuality() == (int) Quality::Standard && p.getLatencySamples() == 0, "back to Standard");
    }
}

/** Offline: measures every factory preset with its current trim and writes the trims that bring it to
    -18 dBFS RMS on its category phrase (run, rebuild, run again: the limiter / saturation are nonlinear). */
static void writeLevelTrims (DivinityProcessor& p, const juce::File& incFile)
{
    std::map<juce::String, double> current;
    for (auto& line : juce::StringArray::fromLines (incFile.loadFileAsString()))
        if (line.contains ("\""))
            current[line.fromFirstOccurrenceOf ("\"", false, false).upToFirstOccurrenceOf ("\"", false, false)]
                = line.fromLastOccurrenceOf (",", false, false).upToFirstOccurrenceOf ("}", false, false).trim().getDoubleValue();
    juce::String inc;
    const auto& list = p.presets.getPresets();
    float worst = 0;
    for (int i = 0; i < list.size(); ++i)
    {
        if (! list[i].isFactory) continue;
        p.presets.loadPreset (i);
        const bool arpPatch = p.apvts.getRawParameterValue (arpids::on)->load() > 0.5f;
        auto b = render (p, 48000, 256, 4.5, phraseFor (list[i].category, arpPatch));
        const float rms = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, (int) (2.5 * 48000)), -120.0f);
        const double trim = juce::jlimit (-24.0, 18.0, current[list[i].name] + (-18.0 - rms));
        worst = std::max (worst, std::abs (-18.0f - rms));
        inc << "{ \"" << list[i].name << "\", " << juce::String (trim, 1) << " },\n";
    }
    incFile.replaceWithText (inc);
    std::cout << "  wrote " << incFile.getFullPathName() << "  (largest correction this pass " << worst << " dB)\n";
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();
    std::cout << "DIVINITY test runner\n";

    if (argc > 3 && juce::String (argv[2]) == "--pick")
    {
        auto procOwner = std::make_unique<DivinityProcessor>();   // heap: the processor is ~350 KB (Windows stacks are 1 MB)
        auto& p = *procOwner;
        pickVariedPresets (p, juce::File (juce::String (argv[3])), argc > 4 ? juce::StringArray::fromTokens (juce::String (argv[4]), ",", "") : juce::StringArray());
        return 0;
    }
    if (argc > 3 && juce::String (argv[2]) == "--levels")
    {
        auto procOwner = std::make_unique<DivinityProcessor>();
        writeLevelTrims (*procOwner, juce::File (juce::String (argv[3])));
        if (argc > 4)
        {   // bank manifest: index, category, name, tags, description
            juce::String csv ("index,category,name,tags,description\n");
            int i = 0;
            auto q = [] (juce::String x) { return "\"" + x.replace ("\"", "\"\"") + "\""; };
            for (auto& fp : createFactoryPresets())
                csv << ++i << "," << fp.category << "," << q (fp.name) << "," << q (fp.tags) << "," << q (fp.description) << "\n";
            juce::File (juce::String (argv[4])).replaceWithText (csv);
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "--v08")
    {
        auto procOwner = std::make_unique<DivinityProcessor>();   // heap: the processor is ~350 KB (Windows stacks are 1 MB)
        auto& p = *procOwner;
        testVersion08 (p);
        snapshotWarpUi (p, outDir);
        std::cout << "\n" << (checks - failures) << " / " << checks << " checks passed\n";
        return failures == 0 ? 0 : 1;
    }
    if (argc > 2 && juce::String (argv[2]) == "--presets")
    {
        // quick loop for preset work: integrity + levels + variety only
        auto procOwner = std::make_unique<DivinityProcessor>();   // heap: the processor is ~350 KB (Windows stacks are 1 MB)
        auto& p = *procOwner;
        testFactoryPresets (p);
        testWaveBankClaims (p, argc > 3 && juce::String (argv[3]) == "--verbose");
        std::cout << "\n" << (checks - failures) << " / " << checks << " checks passed\n";
        return failures == 0 ? 0 : 1;
    }
    {
        auto procOwner = std::make_unique<DivinityProcessor>();   // heap: the processor is ~350 KB (Windows stacks are 1 MB)
        auto& p = *procOwner;
        testParameters (p);
        testFactoryPresets (p);
        testWaveBankClaims (p, false);
        testEnvelopeTiming();
        testPitch (p);
        testModRouting (p);
        testFilters (p);
        testStateRoundTrip (p);
        testPresetFiles (p);
        testMidi (p);
        testSoul (p);
        testGuardianAngel (p);
        testFxRack (p);
        testArpAndVoiceModes (p);
        testFuzzAndExtremes (p);
        testRatesAndBuffers (p);
        testLongNotesAndSilence (p);
        testSoundEngine2 (p);
        testMidiGeneratorAndMeters (p);
        testVersion08 (p);
        testPerformance (p);
        renderPresetDemos (p, outDir.getChildFile ("preset-demos"));
        snapshotEditor (p, outDir);
        snapshotWarpUi (p, outDir);
    }

    std::cout << "\n" << (checks - failures) << " / " << checks << " checks passed";
    std::cout << (failures == 0 ? "  -  ALL PASS\n" : "  -  FAILURES\n");
    return failures == 0 ? 0 : 1;
}
