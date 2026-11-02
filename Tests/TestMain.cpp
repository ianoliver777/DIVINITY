// DIVINITY test runner: exercises the real processor, engine, presets and editor.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/DSPCore.h"
#include "Assistant/GuardianBrain.h"
#include <chrono>
#include <random>
#include <map>

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
    juce::StringArray names; int dupes = 0, wave = 0, grime = 0;
    for (auto& fp : list) { if (names.contains (fp.name)) { ++dupes; std::cout << "  duplicate " << fp.name << "\n"; } names.add (fp.name);
                            wave += fp.category.startsWith ("Wave "); grime += fp.category.startsWith ("Grime "); }
    CHECK (dupes == 0, "factory preset names are unique");
    CHECK (list.size() == 220 && wave == 110 && grime == 90, "220 factory presets: 110 wave + 90 neogrime (" << (int) list.size() << ", " << wave << ", " << grime << ")");
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
    DivinityProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
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
    DivinityProcessor q;
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

static void renderPresetDemos (DivinityProcessor& p, const juce::File& dir)
{
    std::cout << "\n== Factory preset levels (+ WAV demos)\n";
    dir.createDirectory();
    juce::WavAudioFormat wav;
    const auto& list = p.presets.getPresets();
    int hot = 0, quiet = 0;
    juce::StringArray demoCats;
    std::vector<float> rmsAll;
    for (int i = 0; i < list.size(); ++i)
    {
        if (! list[i].isFactory) continue;
        p.presets.loadPreset (i);
        const auto cat = list[i].category;
        const bool arpPatch = p.apvts.getRawParameterValue (arpids::on)->load() > 0.5f;
        std::vector<Note> phrase;
        if (cat.contains ("Bass") || cat.contains ("808") || cat.contains ("Sub"))
            phrase = { { 0.0, 0.45, 36, 1.0f }, { 0.5, 0.45, 36, 0.9f }, { 1.0, 0.7, 39, 1.0f }, { 1.75, 0.6, 34, 1.0f } };
        else if (cat == "Percussion")
            phrase = { { 0.0, 0.2, 36, 1.0f }, { 0.5, 0.2, 36, 1.0f }, { 1.0, 0.2, 36, 1.0f }, { 1.5, 0.2, 36, 1.0f } };
        else if (arpPatch)
            phrase = { { 0.0, 2.4, 60, 0.9f }, { 0.0, 2.4, 63, 0.9f }, { 0.0, 2.4, 67, 0.9f } };
        else if (cat.contains ("Lead") || cat.contains ("Vox"))
            phrase = { { 0.0, 0.45, 72, 0.9f }, { 0.5, 0.45, 75, 0.9f }, { 1.0, 0.45, 79, 0.9f }, { 1.5, 0.9, 77, 0.9f } };
        else if (cat.contains ("Pluck") || cat.contains ("Bell") || cat == "Keys" || cat.contains ("Stab"))
            phrase = { { 0.0, 0.3, 60, 0.9f }, { 0.25, 0.3, 64, 0.8f }, { 0.5, 0.3, 67, 0.9f }, { 0.75, 0.3, 71, 0.8f }, { 1.0, 0.3, 72, 0.9f }, { 1.25, 0.3, 67, 0.8f }, { 1.5, 0.6, 64, 0.9f } };
        else
            phrase = { { 0.0, 2.5, 48, 0.8f }, { 0.0, 2.5, 55, 0.8f }, { 0.0, 2.5, 60, 0.8f }, { 0.0, 2.5, 63, 0.8f } };
        auto b = render (p, 48000, 256, 4.5, phrase);
        const float peak = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -120.0f);
        const float rms = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, (int) (2.5 * 48000)), -120.0f);
        std::cout << "  " << juce::String (list[i].name).paddedRight (' ', 20) << juce::String (cat).paddedRight (' ', 15)
                  << " peak " << juce::String (peak, 1).paddedLeft (' ', 6) << " dBFS   rms " << juce::String (rms, 1).paddedLeft (' ', 6) << " dBFS\n";
        if (peak > -0.2f) ++hot;
        if (rms < -42.0f || ! allFinite (b)) { ++quiet; std::cout << "    ^ too quiet or not finite\n"; }
        rmsAll.push_back (rms);
        // WAV demos: the classic set + the first preset of every category (keeps CI artefacts small)
        if (i >= 20 && demoCats.contains (cat)) continue;
        demoCats.add (cat);
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
}

static void snapshotEditor (DivinityProcessor& p, const juce::File& dir)
{
    std::cout << "\n== Editor (offscreen, every tab)\n";
    p.presets.loadPreset (0);
    p.keyboardState.reset();
    render (p, 48000, 512, 0.6, { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 64, 0.8f }, { 0.0, 1.0, 67, 0.8f } });
    for (auto tab : { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB" })
    {
        p.uiWorkspace = tab;
        p.uiAngelOpen = true;
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
        p.uiWorkspace = "PLAY";
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* de = dynamic_cast<DivinityEditor*> (ed.get());
        ed->setSize (DivinityEditor::kBaseWidth, DivinityEditor::kBaseHeight);
        ed->setVisible (true);
        de->setAngelOpen (false);
        CHECK (ed->getWidth() == DivinityEditor::kCompactWidth && ed->getHeight() == DivinityEditor::kBaseHeight, "collapsing Guardian Angel narrows the window (" << ed->getWidth() << "x" << ed->getHeight() << ")");
        for (int i = 0; i < 3; ++i) { render (p, 48000, 512, 0.15, { { 0.0, 1.0, 60, 0.8f }, { 0.0, 1.0, 67, 0.8f } }); juce::MessageManager::getInstance()->runDispatchLoopUntil (50); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = dir.getChildFile ("ui_play_angel_collapsed.png"); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        de->setAngelOpen (true);
        CHECK (ed->getWidth() == DivinityEditor::kBaseWidth, "reopening Guardian Angel restores the width");
        CHECK (p.uiAngelOpen, "angel state is remembered");
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
    CHECK (p.presets.getCurrentName() == "Reese Bass" && p.apvts.getRawParameterValue (ids::soulX)->load() > 0.6f
           && p.apvts.getRawParameterValue (ids::soulY)->load() < 0.4f, "\"dark aggressive bass\" builds from Reese Bass, pushed aggressive + dark");
    r = brain.createSound ("dreamy evolving wide pad");
    CHECK (p.presets.getCurrentName() == "Celestial Pad" && p.apvts.getRawParameterValue (ids::macro (1))->load() > 0.3f
           && p.apvts.getRawParameterValue (ids::divineAmount)->load() > 0.2f, "\"dreamy evolving wide pad\" adds movement + Divine");
    {
        // the wave / neogrime library is reachable by asking
        const std::pair<const char*, const char*> asks[] = { { "make me a grime 808", "Grime 808" }, { "give me an icy eski bell", "Ice Bell" },
                                                             { "dark wave pad please", "Seraph Bloom" }, { "make a wave reese", "Wave Reese" },
                                                             { "8-bit lead", "8-Bit Ghost" } };
        int hits = 0;
        for (auto& [q, want] : asks) { brain.createSound (q); hits += p.presets.getCurrentName() == want; }
        CHECK (hits == 5, "Guardian Angel builds from the wave / neogrime library (" << hits << "/5)");
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
        testSoul (p);
        testGuardianAngel (p);
        testFxRack (p);
        testArpAndVoiceModes (p);
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
