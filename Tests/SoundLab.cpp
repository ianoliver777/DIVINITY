// DIVINITY Sound Lab: objective audio measurements of the real engine + FX.
// Used to compare engine versions (baseline vs overhaul) with numbers, not vibes.
//
//   DivinitySoundLab <outDir> [--wavs]
//
// Prints a metrics table and writes metrics.csv (+ every factory preset as a
// 16-bit WAV with --wavs) into <outDir>.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include <chrono>
#include <fstream>

using namespace divinity;

namespace
{
constexpr double kSr = 48000.0;
bool forceClassic = false;   // --classic: run every measurement on the Classic 1.0 engine (compatibility check)

void setP (DivinityProcessor& p, const juce::String& id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

void setChoice (DivinityProcessor& p, const juce::String& id, const juce::String& name)
{
    auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id));
    jassert (c != nullptr && c->choices.contains (name));
    c->setValueNotifyingHost (c->convertTo0to1 ((float) c->choices.indexOf (name)));
}

void cleanPatch (DivinityProcessor& p)
{
    p.presets.loadInit();
    setChoice (p, ids::filt (0, "type"), "Off");
    setChoice (p, ids::filt (1, "type"), "Off");
    setP (p, ids::satOn, 0); setP (p, ids::limOn, 0); setP (p, ids::master, 0.0f);
    setP (p, ids::env (0, "attack"), 0.001f); setP (p, ids::env (0, "sustain"), 1.0f);
    for (int m = 0; m < kNumMacros; ++m) setP (p, ids::macro (m), 0.0f);
    // dry DSP measurements need a fixed pitch: switch organic drift off where the engine has it
    if (p.apvts.getParameter ("analog") != nullptr) setP (p, "analog", 0.0f);
    if (forceClassic) setP (p, "engine_mode", 0.0f);
}

struct Note { double start, length; int note; float vel; };

juce::AudioBuffer<float> render (DivinityProcessor& p, double seconds, const std::vector<Note>& notes, int block = 256)
{
    p.setRateAndBufferSizeDetails (kSr, block);
    p.prepareToPlay (kSr, block);
    const int total = (int) (seconds * kSr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        midi.clear();
        for (auto& nt : notes)
        {
            const int on = (int) (nt.start * kSr), off = (int) ((nt.start + nt.length) * kSr);
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, nt.note), off - pos);
        }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

/** Power spectrum (Blackman-Harris window) of channel `ch` starting at `from`, 2^order points. */
std::vector<double> powerSpectrum (const juce::AudioBuffer<float>& b, int ch, int from, int order)
{
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) N * 2, 0.0f);
    const float* d = b.getReadPointer (ch);
    for (int i = 0; i < N; ++i)
    {
        const double x = (double) i / (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2 * juce::MathConstants<double>::pi * x) + 0.14128 * std::cos (4 * juce::MathConstants<double>::pi * x) - 0.01168 * std::cos (6 * juce::MathConstants<double>::pi * x);
        data[(size_t) i] = (from + i < b.getNumSamples() ? d[from + i] : 0.0f) * (float) w;
    }
    fft.performFrequencyOnlyForwardTransform (data.data());
    std::vector<double> p ((size_t) N / 2);
    for (int i = 0; i < N / 2; ++i) p[(size_t) i] = (double) data[(size_t) i] * data[(size_t) i];
    return p;
}

/** Energy below 16 kHz that is not within +-4 bins of a harmonic of f0, relative to the total below 16 kHz (dB). Lower = cleaner. */
double aliasDb (const juce::AudioBuffer<float>& b, double f0, int from)
{
    const int order = 16;
    auto ps = powerSpectrum (b, 0, from, order);
    const double binHz = kSr / (1 << order);
    double harm = 0, other = 0;
    for (size_t k = 2; k < ps.size(); ++k)
    {
        const double hz = (double) k * binHz;
        if (hz > 16000.0) break;   // audible band only: fold-back above 16 kHz is inaudible by design
        const double h = hz / f0;
        const double nearest = std::round (h);
        const bool isHarm = nearest >= 1.0 && std::abs (h - nearest) * f0 < 4.0 * binHz;
        (isHarm ? harm : other) += ps[k];
    }
    return 10.0 * std::log10 ((other + 1e-30) / (harm + other + 1e-30));
}

/** Fraction of energy above `hz` (dB re total). */
double bandEnergyAboveDb (const juce::AudioBuffer<float>& b, double hz, int from)
{
    auto ps = powerSpectrum (b, 0, from, 15);
    const double binHz = kSr / (1 << 15);
    double hi = 0, tot = 0;
    for (size_t k = 1; k < ps.size(); ++k) { tot += ps[k]; if ((double) k * binHz >= hz) hi += ps[k]; }
    return 10.0 * std::log10 ((hi + 1e-30) / (tot + 1e-30));
}

double correlation (const juce::AudioBuffer<float>& b, int from, int len)
{
    const float* l = b.getReadPointer (0); const float* r = b.getReadPointer (1);
    double lr = 0, ll = 0, rr = 0;
    for (int i = from; i < from + len && i < b.getNumSamples(); ++i) { lr += l[i] * r[i]; ll += l[i] * l[i]; rr += r[i] * r[i]; }
    return lr / std::sqrt (ll * rr + 1e-30);
}

double rmsDb (const juce::AudioBuffer<float>& b, int ch, int from, int len)
{
    return juce::Decibels::gainToDecibels ((double) b.getRMSLevel (ch, from, juce::jmin (len, b.getNumSamples() - from)), -150.0);
}

/** Mono-sum loss: RMS(L+R)/2 vs average channel RMS (0 dB = fully mono-compatible). */
double monoLossDb (const juce::AudioBuffer<float>& b, int from, int len)
{
    const float* l = b.getReadPointer (0); const float* r = b.getReadPointer (1);
    double m = 0, s = 0;
    for (int i = from; i < from + len; ++i) { const double mm = 0.5 * (l[i] + r[i]); m += mm * mm; s += 0.5 * (l[i] * l[i] + r[i] * r[i]); }
    return 10.0 * std::log10 ((m + 1e-30) / (s + 1e-30));
}

/** Short-term level fluctuation (std-dev of 10 ms RMS in dB) — a proxy for "movement / beating". */
double movementDb (const juce::AudioBuffer<float>& b, int from, int len)
{
    const int w = (int) (0.01 * kSr);
    std::vector<double> v;
    for (int i = from; i + w <= from + len; i += w) v.push_back (rmsDb (b, 0, i, w));
    double mean = 0; for (double x : v) mean += x; mean /= (double) v.size();
    double var = 0;  for (double x : v) var += (x - mean) * (x - mean);
    return std::sqrt (var / (double) v.size());
}

/** Reverb tail "metallic-ness": peak-to-mean ratio (dB) of the smoothed tail spectrum, 300 Hz - 8 kHz. Lower = smoother. */
double tailPeakinessDb (const juce::AudioBuffer<float>& b, int from)
{
    auto ps = powerSpectrum (b, 0, from, 15);
    const double binHz = kSr / (1 << 15);
    std::vector<double> db;
    for (size_t k = 1; k < ps.size(); ++k)
    {
        const double hz = (double) k * binHz;
        if (hz < 300.0 || hz > 8000.0) continue;
        db.push_back (10.0 * std::log10 (ps[k] + 1e-30));
    }
    // remove the broad slope with a 1/3-octave-ish moving average, then measure 95th percentile deviation
    const int win = 64;
    std::vector<double> dev;
    for (size_t i = (size_t) win; i + (size_t) win < db.size(); ++i)
    {
        double m = 0; for (int j = -win; j <= win; ++j) m += db[(size_t) ((int) i + j)];
        m /= (2 * win + 1);
        dev.push_back (db[i] - m);
    }
    std::sort (dev.begin(), dev.end());
    return dev.empty() ? 0.0 : dev[dev.size() * 99 / 100];
}

/** Echo density: fraction of 1 ms frames in the early tail whose RMS is within 6 dB of the local 20 ms envelope (grainy tails score low). */
double echoDensity (const juce::AudioBuffer<float>& b, int from, int len)
{
    const float* d = b.getReadPointer (0);
    const int w = (int) (0.02 * kSr);
    int inside = 0, total = 0;
    for (int i = from; i + w < from + len; i += w / 2)
    {
        double s = 0; for (int k = 0; k < w; ++k) s += d[i + k] * d[i + k];
        const double sd = std::sqrt (s / w) + 1e-20;
        for (int k = 0; k < w; ++k) { ++total; if (std::abs (d[i + k]) > sd) ++inside; }
    }
    // Gaussian noise has ~31.7% of samples above 1 sigma; sparse/echoey tails have far fewer.
    return total ? (double) inside / total / 0.3173 : 0.0;
}

std::vector<std::pair<std::string, double>> metrics;
void report (const std::string& name, double v, const char* unit, const char* note)
{
    metrics.push_back ({ name, v });
    std::cout << "  " << juce::String (name).paddedRight (' ', 44) << juce::String (v, 2).paddedLeft (' ', 9) << " " << unit << "   " << note << "\n";
}
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("soundlab");
    bool writeWavs = false;
    for (int i = 2; i < argc; ++i) { writeWavs |= juce::String (argv[i]) == "--wavs"; forceClassic |= juce::String (argv[i]) == "--classic"; }
    outDir.createDirectory();
    DivinityProcessor p;
    const int settle = (int) (0.3 * kSr);

    std::cout << "\n== Oscillators (dry, effects off)\n";
    for (int note : { 84, 96, 103 })
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        auto b = render (p, 2.0, { { 0.0, 2.0, note, 1.0f } });
        report ("analog saw alias, note " + std::to_string (note), aliasDb (b, 440.0 * std::pow (2.0, (note - 69) / 12.0), settle), "dB", "lower = cleaner");
    }
    for (int note : { 84, 96 })
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Triangle");
        auto b = render (p, 2.0, { { 0.0, 2.0, note, 1.0f } });
        report ("analog triangle alias, note " + std::to_string (note), aliasDb (b, 440.0 * std::pow (2.0, (note - 69) / 12.0), settle), "dB", "lower = cleaner");
    }
    for (int note : { 60, 79, 90, 96 })
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Wavetable"); setChoice (p, ids::osc (0, "table"), "Basic Shapes");
        setP (p, ids::osc (0, "pos"), 0.6667f);   // saw keyframe
        auto b = render (p, 2.0, { { 0.0, 2.0, note, 1.0f } });
        const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        report ("WT saw air (>8k re total), note " + std::to_string (note), bandEnergyAboveDb (b, 8000.0, settle), "dB", "higher = brighter (ref: ideal saw)");
        report ("WT saw alias, note " + std::to_string (note), aliasDb (b, f0, settle), "dB", "lower = cleaner");
    }
    for (float fold : { 0.5f, 0.9f })
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Sine");
        setP (p, ids::osc (0, "fold"), fold);
        for (int note : { 88, 100 })
        {
            auto b = render (p, 2.0, { { 0.0, 2.0, note, 1.0f } });
            report ("wavefold " + juce::String (fold, 1).toStdString() + " alias, note " + std::to_string (note), aliasDb (b, 440.0 * std::pow (2.0, (note - 69) / 12.0), settle), "dB", "lower = cleaner");
        }
    }

    for (int note : { 90, 100 })
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Pulse");
        setP (p, ids::osc (0, "pw"), 0.2f);
        auto b = render (p, 2.0, { { 0.0, 2.0, note, 1.0f } });
        report ("analog pulse 20% alias, note " + std::to_string (note), aliasDb (b, 440.0 * std::pow (2.0, (note - 69) / 12.0), settle), "dB", "lower = cleaner");
    }
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        auto b = render (p, 2.0, { { 0.0, 2.0, 60, 1.0f } });
        report ("analog saw air (>8k re total), note 60", bandEnergyAboveDb (b, 8000.0, settle), "dB", "ideal saw: -17.0");
        report ("analog saw level, note 60", rmsDb (b, 0, settle, (int) kSr), "dBFS", "should match Classic");
    }

    std::cout << "\n== Unison (7 voices, analog saw, effects off)\n";
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        auto one = render (p, 3.0, { { 0.0, 3.0, 57, 1.0f } });
        setP (p, ids::osc (0, "unison"), 7); setP (p, ids::osc (0, "detune"), 0.45f); setP (p, ids::osc (0, "width"), 1.0f);
        if (p.apvts.getParameter ("analog") != nullptr) setP (p, "analog", 0.3f);   // the engine's default drift
        auto b = render (p, 3.0, { { 0.0, 3.0, 57, 1.0f } });
        const int len = (int) (2.5 * kSr);
        report ("unison L/R correlation", correlation (b, settle, len), "", "lower = wider (keep > 0)");
        report ("unison mono-sum loss", monoLossDb (b, settle, len), "dB", "closer to 0 = more mono-safe");
        report ("unison loudness vs single voice", rmsDb (b, 0, settle, len) - rmsDb (one, 0, settle, len), "dB", "near 0 = compensated");
        report ("unison movement (10 ms RMS std-dev)", movementDb (b, settle, len), "dB", "a little = alive");
    }

    {
        // pitch life: a single sine held for 6 s, frequency measured every 100 ms
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Sine");
        if (p.apvts.getParameter ("analog") != nullptr) setP (p, "analog", 0.3f);
        auto b = render (p, 6.0, { { 0.0, 6.0, 69, 1.0f } });
        std::vector<double> c;
        const float* d = b.getReadPointer (0);
        for (int w = (int) (0.2 * kSr); w + 4800 < b.getNumSamples(); w += 4800)
        {
            double first = -1, last = -1; int n = 0;
            for (int i = w + 1; i < w + 4800; ++i)
                if (d[i - 1] <= 0.0f && d[i] > 0.0f) { const double t = (i - 1) + (double) (-d[i - 1]) / (double) (d[i] - d[i - 1]); if (first < 0) first = t; else ++n; last = t; }
            if (n > 0) c.push_back (1200.0 * std::log2 (kSr * n / (last - first) / 440.0));
        }
        double mean = 0; for (double x : c) mean += x; mean /= (double) c.size();
        double var = 0; for (double x : c) var += (x - mean) * (x - mean);
        report ("pitch wander over a held note (std-dev)", std::sqrt (var / (double) c.size()), "cents", "0 = frozen; 0.5-2 = analog");
    }

    std::cout << "\n== Filter\n";
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        auto dry = render (p, 1.5, { { 0.0, 1.5, 45, 1.0f } });
        for (auto* type : { "MG Low 24", "Low Pass 12" })
        {
            setChoice (p, ids::filt (0, "type"), type); setP (p, ids::filt (0, "cutoff"), 1200.0f);
            for (float res : { 0.0f, 0.9f })
            {
                setP (p, ids::filt (0, "res"), res);
                auto b = render (p, 1.5, { { 0.0, 1.5, 45, 1.0f } });
                report (std::string (type) + " res " + juce::String (res, 1).toStdString() + " level vs dry", rmsDb (b, 0, settle, (int) kSr) - rmsDb (dry, 0, settle, (int) kSr), "dB", "resonance should not gut the level");
            }
            setP (p, ids::filt (0, "res"), 0.3f); setP (p, ids::filt (0, "drive"), 0.8f);
            auto b = render (p, 1.5, { { 0.0, 1.5, 45, 1.0f } });
            report (std::string (type) + " drive 0.8 level vs dry", rmsDb (b, 0, settle, (int) kSr) - rmsDb (dry, 0, settle, (int) kSr), "dB", "");
            setP (p, ids::filt (0, "drive"), 0.0f);
        }
    }

    std::cout << "\n== Reverb (impulse, 100% wet)\n";
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        setP (p, ids::env (0, "attack"), 0.0005f); setP (p, ids::env (0, "decay"), 0.004f); setP (p, ids::env (0, "sustain"), 0.0f); setP (p, ids::env (0, "release"), 0.004f);
        setP (p, ids::revOn, 1); setP (p, ids::revMix, 1.0f); setP (p, ids::revSize, 0.8f); setP (p, ids::revDamp, 0.3f);
        auto b = render (p, 4.0, { { 0.0, 0.01, 60, 1.0f } });
        report ("reverb tail spectral peakiness", tailPeakinessDb (b, (int) (0.4 * kSr)), "dB", "lower = less metallic");
        report ("reverb echo density (early tail)", echoDensity (b, (int) (0.05 * kSr), (int) (0.25 * kSr)), "", "1.0 = Gaussian-dense");
        report ("reverb tail L/R correlation", correlation (b, (int) (0.3 * kSr), (int) (1.5 * kSr)), "", "low = wide");
        const double e1 = rmsDb (b, 0, (int) (0.2 * kSr), (int) (0.1 * kSr)), e2 = rmsDb (b, 0, (int) (1.2 * kSr), (int) (0.1 * kSr));
        report ("reverb decay 0.2s->1.2s", e1 - e2, "dB", "size 0.8 -> long tail");
        // steady-state wet level against the dry note (keeps preset loudness when the algorithm changes)
        setP (p, ids::env (0, "sustain"), 1.0f); setP (p, ids::revMix, 0.0f);
        auto dry = render (p, 3.0, { { 0.0, 3.0, 57, 0.8f } });
        setP (p, ids::revMix, 1.0f);
        auto wet = render (p, 3.0, { { 0.0, 3.0, 57, 0.8f } });
        report ("reverb mix 100% level vs dry", rmsDb (wet, 0, (int) (1.5 * kSr), (int) kSr) - rmsDb (dry, 0, (int) (1.5 * kSr), (int) kSr), "dB", "match Classic");
        // wet-only gain across room sizes (pad chord, so it reflects how presets use it)
        for (float sz : { 0.2f, 0.5f, 0.7f, 0.85f, 0.95f, 1.0f })
        {
            setP (p, ids::revSize, sz);
            setP (p, ids::env (0, "attack"), 0.3f); setP (p, ids::env (0, "release"), 1.5f);
            setP (p, ids::revMix, 0.0f);
            auto d = render (p, 4.0, { { 0.0, 2.5, 48, 0.8f }, { 0.0, 2.5, 55, 0.8f }, { 0.0, 2.5, 63, 0.8f } });
            setP (p, ids::revMix, 1.0f);
            auto w = render (p, 4.0, { { 0.0, 2.5, 48, 0.8f }, { 0.0, 2.5, 55, 0.8f }, { 0.0, 2.5, 63, 0.8f } });
            // renders are deterministic, so wet-only = (mix 100%) - 0.6 * dry
            juce::AudioBuffer<float> wo (w);
            wo.addFrom (0, 0, d, 0, 0, d.getNumSamples(), -0.6f); wo.addFrom (1, 0, d, 1, 0, d.getNumSamples(), -0.6f);
            report ("reverb size " + juce::String (sz, 2).toStdString() + " wet-only level vs dry", rmsDb (wo, 0, 0, (int) (4.0 * kSr)) - rmsDb (d, 0, 0, (int) (4.0 * kSr)), "dB", "");
        }
    }

    std::cout << "\n== Chorus (mono saw in)\n";
    {
        cleanPatch (p);
        setChoice (p, ids::osc (0, "engine"), "Analog"); setChoice (p, ids::osc (0, "wave"), "Saw");
        setP (p, ids::choOn, 1); setP (p, ids::choMix, 0.5f); setP (p, ids::choDepth, 0.5f);
        auto b = render (p, 3.0, { { 0.0, 3.0, 57, 1.0f } });
        const int len = (int) (2.5 * kSr);
        report ("chorus L/R correlation", correlation (b, settle, len), "", "lower = wider");
        report ("chorus mono-sum loss", monoLossDb (b, settle, len), "dB", "closer to 0 = mono-safe");
    }

    std::cout << "\n== CPU\n";
    {
        p.presets.loadPreset (0);
        setP (p, ids::voices, 16);
        std::vector<Note> chord;
        for (int i = 0; i < 16; ++i) chord.push_back ({ 0.0, 4.0, 40 + i * 3, 0.7f });
        auto t0 = std::chrono::high_resolution_clock::now();
        render (p, 4.0, chord);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count();
        report ("preset 1, 16 voices", ms / 40.0, "% core", "");
        if (p.apvts.getParameter ("engine_mode") != nullptr)
        {
            for (int mode : { 0, 1 })
            {
                p.presets.loadPreset (0); setP (p, ids::voices, 16); setP (p, "engine_mode", (float) mode);
                for (int fxOff : { 0, 1 })
                {
                    if (fxOff) { setP (p, ids::revOn, 0); setP (p, ids::choOn, 0); setP (p, ids::dlyOn, 0); setP (p, ids::macro (3), 0); setP (p, ids::macro (1), 0); }
                    auto t1 = std::chrono::high_resolution_clock::now();
                    render (p, 4.0, chord);
                    const double ms2 = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t1).count();
                    report (std::string ("preset 1 engine ") + (mode ? "2.0" : "classic") + (fxOff ? " (space fx off)" : ""), ms2 / 40.0, "% core", "");
                }
            }
        }
    }

    std::cout << "\n== Factory presets\n";
    {
        juce::WavAudioFormat wav;
        auto wavDir = outDir.getChildFile ("presets");
        if (writeWavs) wavDir.createDirectory();
        const auto& list = p.presets.getPresets();
        std::ofstream csv (outDir.getChildFile ("presets.csv").getFullPathName().toStdString());
        csv << "index,name,category,peak_db,rms_db,corr,mono_loss_db,air_db\n";
        double corrSum = 0, airSum = 0; int n = 0;
        for (int i = 0; i < list.size(); ++i)
        {
            if (! list[i].isFactory) continue;
            p.presets.loadPreset (i);
            const auto cat = list[i].category;
            std::vector<Note> phrase;
            if (cat.contains ("Bass") || cat.contains ("808") || cat.contains ("Sub"))
                phrase = { { 0.0, 0.45, 36, 1.0f }, { 0.5, 0.45, 36, 0.7f }, { 1.0, 0.7, 39, 1.0f }, { 1.75, 0.6, 34, 0.9f } };
            else if (cat.contains ("Lead") || cat.contains ("Vox"))
                phrase = { { 0.0, 0.45, 72, 0.9f }, { 0.5, 0.45, 75, 0.6f }, { 1.0, 0.45, 79, 0.9f }, { 1.5, 0.9, 77, 1.0f } };
            else if (cat.contains ("Pluck") || cat.contains ("Bell") || cat == "Keys" || cat.contains ("Stab"))
                phrase = { { 0.0, 0.3, 60, 0.9f }, { 0.25, 0.3, 64, 0.6f }, { 0.5, 0.3, 67, 0.9f }, { 0.75, 0.3, 71, 0.7f }, { 1.0, 0.3, 72, 1.0f }, { 1.25, 0.3, 67, 0.6f }, { 1.5, 0.6, 64, 0.9f } };
            else
                phrase = { { 0.0, 2.5, 48, 0.8f }, { 0.0, 2.5, 55, 0.8f }, { 0.0, 2.5, 60, 0.8f }, { 0.0, 2.5, 63, 0.8f } };
            auto b = render (p, 4.5, phrase);
            const double peak = juce::Decibels::gainToDecibels ((double) b.getMagnitude (0, b.getNumSamples()), -150.0);
            const double rms = rmsDb (b, 0, 0, (int) (2.5 * kSr));
            const double corr = correlation (b, 0, (int) (2.5 * kSr));
            const double ml = monoLossDb (b, 0, (int) (2.5 * kSr));
            const double air = bandEnergyAboveDb (b, 6000.0, (int) (0.1 * kSr));
            corrSum += corr; airSum += air; ++n;
            csv << i << ",\"" << list[i].name << "\",\"" << cat << "\"," << peak << "," << rms << "," << corr << "," << ml << "," << air << "\n";
            if (writeWavs)
            {
                auto f = wavDir.getChildFile (juce::String (i + 1).paddedLeft ('0', 3) + " " + juce::File::createLegalFileName (list[i].name) + ".wav");
                f.deleteFile();
                if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
                    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), kSr, 2, 16, {}, 0)))
                    {
                        os.release();
                        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                    }
            }
        }
        report ("presets: mean L/R correlation", corrSum / n, "", "");
        report ("presets: mean air (>6k re total)", airSum / n, "dB", "");
    }

    std::ofstream m (outDir.getChildFile ("metrics.csv").getFullPathName().toStdString());
    for (auto& [k, v] : metrics) m << "\"" << k << "\"," << v << "\n";
    std::cout << "\nwrote " << outDir.getFullPathName() << "\n";
    return 0;
}
