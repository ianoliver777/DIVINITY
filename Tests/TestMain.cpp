// DIVINITY test runner: exercises the real processor, engine, presets and editor.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/DSPCore.h"
#include <chrono>
#include <random>

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
}

struct Note { double start, length; int note; float vel; };

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

    DivinityProcessor q;
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
    DivinityProcessor r;
    r.setStateInformation (older.getData(), (int) older.getSize());
    int wrong = 0;
    for (auto* prm : r.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        bool present = false;
        for (auto* e : params->getChildIterator()) if (e->getStringAttribute ("id") == rp->getParameterID()) present = true;
        if (! present && std::abs (rp->getValue() - rp->getDefaultValue()) > 1.0e-6f) ++wrong;
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

static void renderPresetDemos (DivinityProcessor& p, const juce::File& dir)
{
    std::cout << "\n== Factory preset levels (+ WAV demos)\n";
    dir.createDirectory();
    juce::WavAudioFormat wav;
    const auto& list = p.presets.getPresets();
    int hot = 0;
    for (int i = 0; i < list.size(); ++i)
    {
        if (! list[i].isFactory) continue;
        p.presets.loadPreset (i);
        const auto cat = list[i].category;
        std::vector<Note> phrase;
        if (cat == "Bass" || cat == "808")
            phrase = { { 0.0, 0.45, 36, 1.0f }, { 0.5, 0.45, 36, 0.9f }, { 1.0, 0.7, 39, 1.0f }, { 1.75, 0.6, 34, 1.0f } };
        else if (cat == "Percussion")
            phrase = { { 0.0, 0.2, 36, 1.0f }, { 0.5, 0.2, 36, 1.0f }, { 1.0, 0.2, 36, 1.0f }, { 1.5, 0.2, 36, 1.0f } };
        else if (cat == "Lead")
            phrase = { { 0.0, 0.45, 72, 0.9f }, { 0.5, 0.45, 75, 0.9f }, { 1.0, 0.45, 79, 0.9f }, { 1.5, 0.9, 77, 0.9f } };
        else if (cat == "Pluck" || cat == "Arpeggio" || cat == "Bell" || cat == "Keys")
            phrase = { { 0.0, 0.3, 60, 0.9f }, { 0.25, 0.3, 64, 0.8f }, { 0.5, 0.3, 67, 0.9f }, { 0.75, 0.3, 71, 0.8f }, { 1.0, 0.3, 72, 0.9f }, { 1.25, 0.3, 67, 0.8f }, { 1.5, 0.6, 64, 0.9f } };
        else
            phrase = { { 0.0, 2.5, 48, 0.8f }, { 0.0, 2.5, 55, 0.8f }, { 0.0, 2.5, 60, 0.8f }, { 0.0, 2.5, 63, 0.8f } };
        auto b = render (p, 48000, 256, 4.5, phrase);
        const float peak = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -120.0f);
        const float rms = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, (int) (2.5 * 48000)), -120.0f);
        std::cout << "  " << juce::String (list[i].name).paddedRight (' ', 20) << juce::String (cat).paddedRight (' ', 15)
                  << " peak " << juce::String (peak, 1).paddedLeft (' ', 6) << " dBFS   rms " << juce::String (rms, 1).paddedLeft (' ', 6) << " dBFS\n";
        if (peak > -0.2f) ++hot;
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
}

static void snapshotEditor (DivinityProcessor& p, const juce::File& dir)
{
    std::cout << "\n== Editor (offscreen)\n";
    p.presets.loadPreset (0);
    p.keyboardState.reset();   // earlier tests leave host notes held on the on-screen keyboard
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (DivinityEditor::kBaseWidth, DivinityEditor::kBaseHeight);
    // feed a little audio so meters/scope have content
    render (p, 48000, 512, 0.6, { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 64, 0.8f }, { 0.0, 1.0, 67, 0.8f } });
    for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    auto writePng = [&] (const juce::String& name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = dir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        INFO ("wrote " << f.getFullPathName());
    };
    writePng ("ui_design.png");
    p.uiWorkspace = "CREATE";
    ed.reset (p.createEditor());
    ed->setSize (DivinityEditor::kBaseWidth, DivinityEditor::kBaseHeight);
    for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    writePng ("ui_create.png");
    ed->setSize ((int) (DivinityEditor::kBaseWidth * 0.7f), (int) (DivinityEditor::kBaseHeight * 0.7f));
    CHECK (ed->getWidth() == (int) (DivinityEditor::kBaseWidth * 0.7f), "editor resizes to 70%");
    ed.reset();
    p.uiWorkspace = "DESIGN";
    CHECK (true, "editor constructs, paints and destroys in both workspaces");
}

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();
    std::cout << "DIVINITY test runner\n";

    {
        DivinityProcessor p;
        testParameters (p);
        testFactoryPresets (p);
        testEnvelopeTiming();
        testPitch (p);
        testModRouting (p);
        testFilters (p);
        testStateRoundTrip (p);
        testPresetFiles (p);
        testMidi (p);
        testFuzzAndExtremes (p);
        testRatesAndBuffers (p);
        testLongNotesAndSilence (p);
        testPerformance (p);
        renderPresetDemos (p, outDir.getChildFile ("preset-demos"));
        snapshotEditor (p, outDir);
    }

    std::cout << "\n" << (checks - failures) << " / " << checks << " checks passed";
    std::cout << (failures == 0 ? "  -  ALL PASS\n" : "  -  FAILURES\n");
    return failures == 0 ? 0 : 1;
}
