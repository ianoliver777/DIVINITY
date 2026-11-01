#include "SynthEngine.h"

namespace divinity
{
using namespace dsp;

//==============================================================================
EngineParamRefs::EngineParamRefs (juce::AudioProcessorValueTreeState& s)
{
    auto get = [&s] (const juce::String& id)
    {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };
    static const char* oscNames[16] = { "on", "engine", "table", "pos", "fold", "wave", "pw", "unison", "detune",
                                        "width", "phase", "randphase", "oct", "semi", "fine", "level" };
    for (int i = 0; i < kNumOscs; ++i)
        for (int k = 0; k < 16; ++k)
            osc[(size_t) i].p[k] = get (ids::osc (i, oscNames[k]));

    sub[0] = get (ids::subOn); sub[1] = get (ids::subMode); sub[2] = get (ids::subLevel);
    noise[0] = get (ids::noiseOn); noise[1] = get (ids::noiseType); noise[2] = get (ids::noiseLevel);

    static const char* fNames[6] = { "type", "cutoff", "res", "drive", "mix", "keytrack" };
    for (int i = 0; i < 2; ++i)
        for (int k = 0; k < 6; ++k)
            filt[(size_t) i][(size_t) k] = get (ids::filt (i, fNames[k]));
    routing = get (ids::filterRouting);

    static const char* eNames[5] = { "attack", "decay", "sustain", "release", "curve" };
    for (int i = 0; i < kNumEnvs; ++i)
        for (int k = 0; k < 5; ++k)
            env[(size_t) i][(size_t) k] = get (ids::env (i, eNames[k]));

    static const char* lNames[7] = { "shape", "rate", "sync", "div", "depth", "phase", "smooth" };
    for (int i = 0; i < kNumLfos; ++i)
        for (int k = 0; k < 7; ++k)
            lfo[(size_t) i][(size_t) k] = get (ids::lfo (i, lNames[k]));

    static const char* mNames[3] = { "src", "dst", "amt" };
    for (int i = 0; i < kNumModSlots; ++i)
        for (int k = 0; k < 3; ++k)
            mod[(size_t) i][(size_t) k] = get (ids::mod (i, mNames[k]));

    for (int i = 0; i < kNumMacros; ++i)
        macro[(size_t) i] = get (ids::macro (i));

    global[0] = get (ids::voices); global[1] = get (ids::bendRange); global[2] = get (ids::glide);
    global[3] = get (ids::voiceMode); global[4] = get (ids::masterTune);
}

void EngineParamRefs::read (EngineSnapshot& s) const noexcept
{
    auto f = [] (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    auto i = [] (std::atomic<float>* p) { return (int) std::lround (p->load (std::memory_order_relaxed)); };
    auto b = [] (std::atomic<float>* p) { return p->load (std::memory_order_relaxed) > 0.5f; };

    for (int k = 0; k < kNumOscs; ++k)
    {
        auto& o = s.osc[(size_t) k]; const auto& p = osc[(size_t) k].p;
        o.on = b (p[0]); o.engine = i (p[1]); o.table = i (p[2]); o.pos = f (p[3]); o.fold = f (p[4]);
        o.wave = i (p[5]); o.pw = f (p[6]); o.unison = juce::jlimit (1, kMaxUnison, i (p[7])); o.detune = f (p[8]);
        o.width = f (p[9]); o.phase = f (p[10]); o.randPhase = b (p[11]); o.oct = i (p[12]); o.semi = i (p[13]);
        o.fine = f (p[14]); o.level = f (p[15]);
    }
    s.subOn = b (sub[0]); s.subMode = i (sub[1]); s.subLevel = f (sub[2]);
    s.noiseOn = b (noise[0]); s.noiseType = i (noise[1]); s.noiseLevel = f (noise[2]);
    for (int k = 0; k < 2; ++k)
    {
        auto& fl = s.filt[(size_t) k]; const auto& p = filt[(size_t) k];
        fl.type = i (p[0]); fl.cutoff = f (p[1]); fl.res = f (p[2]); fl.drive = f (p[3]); fl.mix = f (p[4]); fl.keytrack = f (p[5]);
    }
    s.routing = i (routing);
    for (int k = 0; k < kNumEnvs; ++k)
    {
        auto& e = s.env[(size_t) k]; const auto& p = env[(size_t) k];
        e.a = f (p[0]); e.d = f (p[1]); e.s = f (p[2]); e.r = f (p[3]); e.curve = f (p[4]);
    }
    for (int k = 0; k < kNumLfos; ++k)
    {
        auto& l = s.lfo[(size_t) k]; const auto& p = lfo[(size_t) k];
        l.shape = i (p[0]); l.rate = f (p[1]); l.sync = b (p[2]); l.div = i (p[3]); l.depth = f (p[4]); l.phase = f (p[5]); l.smooth = f (p[6]);
    }
    for (int k = 0; k < kNumModSlots; ++k)
    {
        auto& m = s.mod[(size_t) k]; const auto& p = mod[(size_t) k];
        m.src = i (p[0]); m.dst = i (p[1]); m.amt = f (p[2]);
    }
    for (int k = 0; k < kNumMacros; ++k) s.macro[(size_t) k] = f (macro[(size_t) k]);
    s.maxVoices = juce::jlimit (1, kMaxVoices, i (global[0]));
    s.bendRange = i (global[1]); s.glideMs = f (global[2]); s.voiceMode = i (global[3]); s.masterTune = f (global[4]);
}

//==============================================================================
SynthEngine::SynthEngine (juce::AudioProcessorValueTreeState& apvts, const WavetableBank& b)
    : refs (apvts), bank (b)
{
    heldNotes.reserve (128);
    refs.read (snap);
}

void SynthEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    scratchL.assign ((size_t) juce::jmax (kControlBlock, maxBlockSize), 0.0f);
    scratchR.assign (scratchL.size(), 0.0f);
    darkNoiseCoeff = (float) (1.0 - std::exp (-kTwoPi * 400.0 / sr));

    const int combSize = (int) std::ceil (sr / 16.0) + 4;   // lowest comb pitch = 16 Hz
    int idx = 0;
    for (auto& v : voices)
    {
        for (auto& e : v.env) e.setSampleRate (sr);
        for (auto& f : v.comb) for (auto& c : f) c.allocate (combSize);
        v.rng.seed (0x1234567u + (uint32_t) idx * 7919u);
        v.noiseL.rng.seed (0xA5A5u + (uint32_t) idx * 31u);
        v.noiseR.rng.seed (0x5A5Au + (uint32_t) idx * 131u);
        ++idx;
    }
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices)
    {
        v.active = v.keyDown = v.sustained = false;
        for (auto& e : v.env) e.reset();
        for (auto& f : v.svf) for (auto& c : f) c.reset();
        for (auto& f : v.ladder) for (auto& c : f) c.reset();
        for (auto& f : v.comb) for (auto& c : f) c.reset();
        v.noiseL.reset(); v.noiseR.reset();
        v.rampsValid = false;
    }
    uint32_t seed = 0xC0FFEEu;
    for (auto& l : lfos) l.reset (seed++);
    lfoValues.fill (0.0f);
    lastGlobalDest.fill (0.0f);
    heldNotes.clear();
    sustainPedal = false;
    pitchBend = modWheel = channelPressure = 0.0f;
    lastNotePitch = -1.0;
    activeVoiceCount = 0;
}

void SynthEngine::allNotesOff (bool immediate)
{
    heldNotes.clear();
    sustainPedal = false;
    for (auto& v : voices)
    {
        if (! v.active) continue;
        if (immediate) { v.active = false; for (auto& e : v.env) e.reset(); }
        else releaseVoice (v);
    }
}

//==============================================================================
void SynthEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TransportInfo& transport)
{
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float* tmpR = scratchR.data();   // reused when output is mono

    refs.read (snap);

    int pos = 0;
    auto renderUpTo = [&] (int end)
    {
        while (pos < end)
        {
            const int n = juce::jmin (kControlBlock, end - pos);
            if (R != nullptr) renderChunk (L + pos, R + pos, n, transport, pos);
            else { std::fill (tmpR, tmpR + n, 0.0f); renderChunk (L + pos, tmpR, n, transport, pos); }
            pos += n;
        }
    };

    for (const auto meta : midi)
    {
        renderUpTo (juce::jlimit (0, numSamples, meta.samplePosition));
        handleMidi (meta.getMessage());
    }
    renderUpTo (numSamples);

    int count = 0;
    for (auto& v : voices) count += v.active ? 1 : 0;
    activeVoiceCount = count;
    samplesElapsed += numSamples;
}

//==============================================================================
void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        pitchBend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        channelPressure = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
    {
        for (auto& v : voices)
            if (v.active && v.note == m.getNoteNumber())
                v.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float val = (float) m.getControllerValue() / 127.0f;
        if (cc == 1) modWheel = val;
        else if (cc == 64)
        {
            const bool down = val >= 0.5f;
            if (sustainPedal && ! down)
                for (auto& v : voices)
                    if (v.active && v.sustained && ! v.keyDown)
                        releaseVoice (v);
            sustainPedal = down;
        }
        else if (cc == 120) allNotesOff (true);    // all sound off
        else if (cc == 123) allNotesOff (false);   // all notes off
    }
    else if (m.isAllNotesOff()) allNotesOff (false);
    else if (m.isAllSoundOff()) allNotesOff (true);
}

void SynthEngine::startVoice (Voice& v, int note, float velocity, bool fresh)
{
    v.active = true; v.keyDown = true; v.sustained = false;
    v.note = note; v.velocity = velocity; v.aftertouch = 0.0f;
    v.order = ++noteCounter;
    v.targetPitch = note;

    const bool glide = snap.glideMs > 0.5f && lastNotePitch >= 0.0;
    v.pitch = glide ? lastNotePitch : (double) note;

    if (fresh)
    {
        // Brand new voice: start everything from a clean state.
        for (auto& f : v.svf) for (auto& c : f) c.reset();
        for (auto& f : v.ladder) for (auto& c : f) c.reset();
        for (auto& f : v.comb) for (auto& c : f) c.reset();
        for (auto& e : v.env) e.reset();
        v.rampsValid = false;
        v.subPhase = 0.0;
        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& op = snap.osc[(size_t) o];
            for (int u = 0; u < kMaxUnison; ++u)
                v.phase[(size_t) o][(size_t) u] = op.randPhase ? (double) v.rng.unipolar() : (double) op.phase;
        }
    }
    // Stolen / retriggered voices keep their phase, filter and envelope levels: no clicks.

    for (int e = 0; e < kNumEnvs; ++e)
    {
        const auto& ep = snap.env[(size_t) e];
        v.env[(size_t) e].setParameters (ep.a, ep.d, ep.s, ep.r, ep.curve);
        v.env[(size_t) e].noteOn();
    }
    lastNotePitch = note;
}

void SynthEngine::releaseVoice (Voice& v)
{
    v.keyDown = false; v.sustained = false;
    for (auto& e : v.env) e.noteOff();
}

SynthEngine::Voice* SynthEngine::findVoiceToSteal()
{
    Voice* best = nullptr;
    // 1) quietest releasing voice
    for (int i = 0; i < snap.maxVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        if (v.active && v.env[0].isReleasing() && (best == nullptr || v.env[0].getLevel() < best->env[0].getLevel()))
            best = &v;
    }
    if (best != nullptr) return best;
    // 2) oldest voice
    for (int i = 0; i < snap.maxVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        if (best == nullptr || v.order < best->order) best = &v;
    }
    return best;
}

void SynthEngine::noteOn (int note, float velocity)
{
    if (snap.voiceMode == (int) VoiceMode::Legato)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        if (heldNotes.size() < heldNotes.capacity()) heldNotes.push_back (note);

        for (size_t i = 1; i < voices.size(); ++i)          // mono: only voice 0 sounds
            if (voices[i].active && voices[i].keyDown) releaseVoice (voices[i]);

        auto& v = voices[0];
        if (v.active && ! v.env[0].isReleasing() && heldNotes.size() > 1)
        {
            // legato: change pitch without retriggering envelopes
            v.note = note; v.targetPitch = note; v.keyDown = true;
            if (snap.glideMs <= 0.5f) v.pitch = note;
            lastNotePitch = note;
        }
        else
            startVoice (v, note, velocity, ! v.active);
        return;
    }

    // Poly: retrigger a voice already holding this note
    for (auto& v : voices)
        if (v.active && v.note == note && (v.keyDown || v.sustained))
        {
            startVoice (v, note, velocity, false);
            return;
        }

    for (int i = 0; i < snap.maxVoices; ++i)
        if (! voices[(size_t) i].active)
        {
            startVoice (voices[(size_t) i], note, velocity, true);
            return;
        }

    if (auto* v = findVoiceToSteal())
        startVoice (*v, note, velocity, false);
}

void SynthEngine::noteOff (int note)
{
    if (snap.voiceMode == (int) VoiceMode::Legato)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        auto& v = voices[0];
        if (! v.active) return;
        if (! heldNotes.empty())
        {
            if (v.note == note)
            {
                v.note = heldNotes.back(); v.targetPitch = v.note;
                if (snap.glideMs <= 0.5f) v.pitch = v.note;
                lastNotePitch = v.note;
            }
            return;
        }
        if (sustainPedal) { v.keyDown = false; v.sustained = true; }
        else releaseVoice (v);
        return;
    }

    for (auto& v : voices)
        if (v.active && v.keyDown && v.note == note)
        {
            if (sustainPedal) { v.keyDown = false; v.sustained = true; }
            else releaseVoice (v);
        }
}

//==============================================================================
void SynthEngine::computeVoiceMods (const Voice& v, const float* globalSrc, float* dest) const noexcept
{
    float src[(size_t) ModSource::NumSources];
    std::copy (globalSrc, globalSrc + (size_t) ModSource::NumSources, src);
    src[(size_t) ModSource::Env1] = v.env[0].getLevel();
    src[(size_t) ModSource::Env2] = v.env[1].getLevel();
    src[(size_t) ModSource::Env3] = v.env[2].getLevel();
    src[(size_t) ModSource::Velocity] = v.velocity;
    src[(size_t) ModSource::NoteNumber] = (float) (v.note - 60) / 60.0f;
    src[(size_t) ModSource::Aftertouch] = juce::jmax (channelPressure, v.aftertouch);

    std::fill (dest, dest + (size_t) ModDest::NumDests, 0.0f);
    for (const auto& slot : snap.mod)
    {
        if (slot.src <= 0 || slot.dst <= 0 || slot.src >= (int) ModSource::NumSources || slot.dst >= (int) ModDest::NumDests)
            continue;
        dest[slot.dst] += src[slot.src] * slot.amt;
    }
}

void SynthEngine::renderChunk (float* outL, float* outR, int n, const TransportInfo& tr, int sampleOffset)
{
    refs.read (snap);
    const double seconds = (double) n / sampleRate;

    // ---- global LFOs (LFO-rate destinations use the previous block's values:
    //      this gives LFO->LFO modulation a one-block delay, so cycles are safe) ----
    const float movement = snap.macro[1];
    for (int i = 0; i < kNumLfos; ++i)
    {
        const auto& lp = snap.lfo[(size_t) i];
        const double beats = lfoDivisionInBeats (lp.div);
        double rate = lp.sync ? (tr.bpm / 60.0) / beats : (double) lp.rate;
        rate *= std::pow (2.0, (double) lastGlobalDest[(size_t) ModDest::Lfo1Rate + (size_t) i] * 3.0);
        const bool locked = lp.sync && tr.isPlaying && tr.hasPpq;
        const double ppqNow = tr.ppqAtBlockStart + ((double) (sampleOffset + n) / sampleRate) * tr.bpm / 60.0;
        const float raw = lfos[(size_t) i].advance (lp.shape, rate, seconds, lp.smooth, lp.phase, locked, ppqNow / beats);
        lfoValues[(size_t) i] = raw * lp.depth * (1.0f + 2.0f * movement);
        lfoDisplay[(size_t) i].store (raw * lp.depth);
    }

    float globalSrc[(size_t) ModSource::NumSources] {};
    for (int i = 0; i < kNumLfos; ++i) globalSrc[(size_t) ModSource::Lfo1 + (size_t) i] = lfoValues[(size_t) i];
    globalSrc[(size_t) ModSource::ModWheel]   = modWheel;
    globalSrc[(size_t) ModSource::Aftertouch] = channelPressure;
    globalSrc[(size_t) ModSource::PitchBend]  = pitchBend;
    for (int i = 0; i < kNumMacros; ++i) globalSrc[(size_t) ModSource::Macro1 + (size_t) i] = snap.macro[(size_t) i];

    // ---- global (post-voice) destinations follow the newest sounding voice ----
    const Voice* newest = nullptr;
    for (auto& v : voices)
        if (v.active && (newest == nullptr || v.order > newest->order)) newest = &v;

    float gdest[(size_t) ModDest::NumDests] {};
    if (newest != nullptr) computeVoiceMods (*newest, globalSrc, gdest);
    else
    {
        silentVoice.velocity = 0.0f; silentVoice.note = 60;
        computeVoiceMods (silentVoice, globalSrc, gdest);
    }
    std::copy (gdest, gdest + (size_t) ModDest::NumDests, lastGlobalDest.begin());
    globalMods.satDrive    = gdest[(size_t) ModDest::SatDrive];
    globalMods.chorusDepth = gdest[(size_t) ModDest::ChorusDepth];
    globalMods.delayMix    = gdest[(size_t) ModDest::DelayMix];
    globalMods.reverbMix   = gdest[(size_t) ModDest::ReverbMix];

    for (auto& v : voices)
        if (v.active)
            renderVoice (v, outL, outR, n, globalSrc);
}

//==============================================================================
namespace
{
    inline void panGains (float pan, float& l, float& r) noexcept
    {
        const float a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        l = juce::MathConstants<float>::sqrt2 * std::cos (a);
        r = juce::MathConstants<float>::sqrt2 * std::sin (a);
    }

    inline float fold (float x, float amount) noexcept
    {
        if (amount < 1.0e-4f) return x;
        const float g = 1.0f + amount * 5.0f;
        return (1.0f - amount) * x + amount * std::sin (x * g * juce::MathConstants<float>::halfPi);
    }

    inline float drive (float x, float amount) noexcept
    {
        if (amount < 1.0e-4f) return x;
        const float g = 1.0f + amount * 7.0f;
        return std::tanh (x * g) / (1.0f + amount * 1.5f);
    }
}

void SynthEngine::renderVoice (Voice& v, float* outL, float* outR, int n, const float* globalSrc)
{
    float dest[(size_t) ModDest::NumDests];
    computeVoiceMods (v, globalSrc, dest);
    auto D = [&dest] (ModDest d) { return dest[(size_t) d]; };

    const float tone = snap.macro[0], texture = snap.macro[2], space = snap.macro[3];

    // envelope edits apply to held notes immediately (sustain level, release time...)
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const auto& ep = snap.env[(size_t) e];
        v.env[(size_t) e].setParameters (ep.a, ep.d, ep.s, ep.r, ep.curve);
    }
    const float invN = 1.0f / (float) n;

    // ---- glide ----
    const double glideSec = snap.glideMs * 0.001;
    if (glideSec > 0.0005)
    {
        const double coef = 1.0 - std::exp (-(double) n / (sampleRate * glideSec / 4.0));
        v.pitch += (v.targetPitch - v.pitch) * coef;
    }
    else v.pitch = v.targetPitch;

    const double basePitch = v.pitch + snap.masterTune / 100.0 + pitchBend * snap.bendRange;

    float* mixL = scratchL.data();
    float* mixR = scratchR.data();
    std::fill (mixL, mixL + n, 0.0f);
    std::fill (mixR, mixR + n, 0.0f);

    // ---- oscillators ----
    for (int o = 0; o < kNumOscs; ++o)
    {
        const auto& op = snap.osc[(size_t) o];
        if (! op.on) { v.lastLevel[(size_t) o] = 0.0f; continue; }

        const float levelT  = juce::jlimit (0.0f, 1.0f, op.level + dest[(size_t) ModDest::OscALevel + (size_t) o]);
        const float posT    = juce::jlimit (0.0f, 1.0f, op.pos + dest[(size_t) ModDest::OscAPos + (size_t) o]);
        const float foldT   = juce::jlimit (0.0f, 1.0f, op.fold + dest[(size_t) ModDest::OscAFold + (size_t) o] + texture * 0.4f);
        const float pw      = juce::jlimit (0.03f, 0.97f, op.pw + dest[(size_t) ModDest::OscAPwm + (size_t) o] * 0.45f);
        const float detune  = juce::jlimit (0.0f, 1.0f, op.detune + dest[(size_t) ModDest::OscADetune + (size_t) o]);
        const float width   = juce::jlimit (0.0f, 1.0f, op.width + space * 0.3f);
        const double oscPitch = basePitch + op.oct * 12 + op.semi + op.fine / 100.0
                              + dest[(size_t) ModDest::OscAPitch + (size_t) o] * 12.0;

        const float level0 = v.rampsValid ? v.lastLevel[(size_t) o] : levelT;
        const float pos0   = v.rampsValid ? v.lastPos[(size_t) o]   : posT;
        const float fold0  = v.rampsValid ? v.lastFold[(size_t) o]  : foldT;
        v.lastLevel[(size_t) o] = levelT; v.lastPos[(size_t) o] = posT; v.lastFold[(size_t) o] = foldT;
        if (level0 < 1.0e-5f && levelT < 1.0e-5f) continue;

        const int nU = op.unison;
        const float uniGain = 1.0f / std::sqrt ((float) nU);
        const bool wt = op.engine == (int) OscEngine::Wavetable;
        const int table = juce::jlimit (0, bank.getNumTables() - 1, op.table);

        for (int u = 0; u < nU; ++u)
        {
            const float spreadPos = nU > 1 ? ((float) u / (float) (nU - 1)) * 2.0f - 1.0f : 0.0f;
            const double cents = spreadPos * std::pow (detune, 1.5f) * 50.0;
            // alternate sides so neighbouring detune values don't stack on one side
            const float pan = (u % 2 == 0 ? 1.0f : -1.0f) * std::abs (spreadPos) * width;
            float gL, gR; panGains (pan, gL, gR);
            gL *= uniGain; gR *= uniGain;

            const double inc = juce::jmin (0.49, (double) noteToHz (oscPitch + cents / 100.0) / sampleRate);
            double ph = v.phase[(size_t) o][(size_t) u];

            if (wt)
            {
                const int lvl = WavetableBank::levelForIncrement (inc);
                for (int s = 0; s < n; ++s)
                {
                    const float t = (float) s * invN;
                    const float fpos = (pos0 + (posT - pos0) * t) * (WavetableBank::kFrames - 1);
                    const int f0 = juce::jmin ((int) fpos, WavetableBank::kFrames - 1);
                    const int f1 = juce::jmin (f0 + 1, WavetableBank::kFrames - 1);
                    const float fr = fpos - (float) f0;
                    const float* A = bank.getFrame (table, f0, lvl);
                    const float* B = bank.getFrame (table, f1, lvl);
                    const double idx = ph * WavetableBank::kSize;
                    const int i0 = (int) idx;
                    const float frac = (float) (idx - i0);
                    const float a = A[i0] + frac * (A[i0 + 1] - A[i0]);
                    const float b = B[i0] + frac * (B[i0 + 1] - B[i0]);
                    float x = fold (a + fr * (b - a), fold0 + (foldT - fold0) * t);
                    x *= level0 + (levelT - level0) * t;
                    mixL[s] += x * gL; mixR[s] += x * gR;
                    ph += inc; if (ph >= 1.0) ph -= 1.0;
                }
            }
            else
            {
                for (int s = 0; s < n; ++s)
                {
                    const float t = (float) s * invN;
                    float x = analogSample (op.wave, ph, inc, op.wave == (int) AnalogWave::Pulse ? pw : 0.5);
                    x = fold (x * 0.9f, fold0 + (foldT - fold0) * t);
                    x *= level0 + (levelT - level0) * t;
                    mixL[s] += x * gL; mixR[s] += x * gR;
                    ph += inc; if (ph >= 1.0) ph -= 1.0;
                }
            }
            v.phase[(size_t) o][(size_t) u] = ph;
        }
    }

    // ---- sub ----
    {
        const float subT = snap.subOn ? juce::jlimit (0.0f, 1.0f, snap.subLevel + D (ModDest::SubLevel)) : 0.0f;
        const float sub0 = v.rampsValid ? v.lastSub : subT;
        v.lastSub = subT;
        if (sub0 > 1.0e-5f || subT > 1.0e-5f)
        {
            const int wave = snap.subMode / 2;                  // 0 sine, 1 triangle, 2 square
            const double octs = (snap.subMode % 2 == 0) ? -12.0 : -24.0;
            const double inc = (double) noteToHz (basePitch + octs) / sampleRate;
            const int aw = wave == 0 ? 0 : (wave == 1 ? 1 : 3);
            for (int s = 0; s < n; ++s)
            {
                const float x = analogSample (aw, v.subPhase, inc, 0.5) * 0.9f * (sub0 + (subT - sub0) * (float) s * invN);
                mixL[s] += x; mixR[s] += x;
                v.subPhase += inc; if (v.subPhase >= 1.0) v.subPhase -= 1.0;
            }
        }
    }

    // ---- noise ----
    {
        const float nzT = snap.noiseOn ? juce::jlimit (0.0f, 1.0f, snap.noiseLevel + D (ModDest::NoiseLevel)) : 0.0f;
        const float nz0 = v.rampsValid ? v.lastNoise : nzT;
        v.lastNoise = nzT;
        if (nz0 > 1.0e-5f || nzT > 1.0e-5f)
            for (int s = 0; s < n; ++s)
            {
                const float g = nz0 + (nzT - nz0) * (float) s * invN;
                mixL[s] += v.noiseL.next (snap.noiseType, darkNoiseCoeff) * g;
                mixR[s] += v.noiseR.next (snap.noiseType, darkNoiseCoeff) * g;
            }
    }

    // ---- filters ----
    FilterCoeffs cEnd[2], cStart[2];
    float drives[2], mixes[2], combDelay[2], combFb[2];
    for (int f = 0; f < 2; ++f)
    {
        const auto& fp = snap.filt[(size_t) f];
        const auto cutDest = f == 0 ? ModDest::Filter1Cutoff : ModDest::Filter2Cutoff;
        const auto resDest = f == 0 ? ModDest::Filter1Res : ModDest::Filter2Res;
        const auto drvDest = f == 0 ? ModDest::Filter1Drive : ModDest::Filter2Drive;
        const double octaves = D (cutDest) * 6.0 + tone * 4.0 + fp.keytrack * (v.note - 60) / 12.0;
        const float cutoff = juce::jlimit (16.0f, (float) (sampleRate * 0.45), (float) (fp.cutoff * std::pow (2.0, octaves)));
        const float res = juce::jlimit (0.0f, 1.0f, fp.res + D (resDest));
        drives[f] = juce::jlimit (0.0f, 1.0f, fp.drive + D (drvDest));
        mixes[f]  = juce::jlimit (0.0f, 1.0f, fp.mix);
        cEnd[f]   = makeCoeffs (cutoff, res, sampleRate);
        cStart[f] = v.rampsValid ? v.lastCoeffs[(size_t) f] : cEnd[f];
        v.lastCoeffs[(size_t) f] = cEnd[f];
        combDelay[f] = (float) (sampleRate / cutoff);
        combFb[f] = res * 0.97f;
    }

    auto runFilter = [&] (int f, int ch, float x, const FilterCoeffs& c) -> float
    {
        const int type = snap.filt[(size_t) f].type;
        if (type == (int) FilterType::Off) return x;
        const float in = drive (x, drives[f]);
        float y = in;
        if (type == (int) FilterType::Ladder24)
            y = ladderTick (v.ladder[(size_t) f][(size_t) ch], c, in);
        else if (type == (int) FilterType::Comb)
            y = v.comb[(size_t) f][(size_t) ch].tick (in, combDelay[f], combFb[f]);
        else
        {
            float lp, bp, hp;
            svfTick (v.svf[(size_t) f][(size_t) ch], c, in, lp, bp, hp);
            switch (type)
            {
                case (int) FilterType::LowPass12:  y = lp; break;
                case (int) FilterType::HighPass12: y = hp; break;
                case (int) FilterType::BandPass:   y = bp * c.k; break;
                case (int) FilterType::Notch:      y = in - c.k * bp; break;
                case (int) FilterType::Peak:       y = lp - hp; break;
                default: break;
            }
        }
        return x + mixes[f] * (y - x);
    };

    // ---- amp / pan ----
    float panL, panR; panGains (D (ModDest::Pan), panL, panR);
    const float panL0 = v.rampsValid ? v.lastPanL : panL;
    const float panR0 = v.rampsValid ? v.lastPanR : panR;
    v.lastPanL = panL; v.lastPanR = panR;
    const float ampMod = juce::jlimit (0.0f, 2.0f, 1.0f + D (ModDest::Amp));
    const float velGain = 0.3f + 0.7f * v.velocity;
    const bool f1On = snap.filt[0].type != 0, f2On = snap.filt[1].type != 0;
    const bool parallel = snap.routing == (int) FilterRouting::Parallel && f1On && f2On;

    for (int s = 0; s < n; ++s)
    {
        const float t = (float) s * invN;
        FilterCoeffs c[2];
        for (int f = 0; f < 2; ++f)
        {
            if (snap.filt[(size_t) f].type == 0 || snap.filt[(size_t) f].type == (int) FilterType::Comb) continue;
            auto& cc = c[f];
            cc.g = cStart[f].g + (cEnd[f].g - cStart[f].g) * t;
            cc.k = cStart[f].k + (cEnd[f].k - cStart[f].k) * t;
            cc.ladderK = cStart[f].ladderK + (cEnd[f].ladderK - cStart[f].ladderK) * t;
            cc.a1 = 1.0f / (1.0f + cc.g * (cc.g + cc.k));
            cc.a2 = cc.g * cc.a1;
            cc.a3 = cc.g * cc.a2;
            cc.G  = cc.g / (1.0f + cc.g);
        }

        const float env2 = v.env[1].next(); juce::ignoreUnused (env2);
        const float env3 = v.env[2].next(); juce::ignoreUnused (env3);
        const float amp = v.env[0].next() * velGain * ampMod * 0.5f;

        float l = mixL[s], r = mixR[s];
        if (parallel)
        {
            l = 0.5f * (runFilter (0, 0, l, c[0]) + runFilter (1, 0, l, c[1]));
            r = 0.5f * (runFilter (0, 1, r, c[0]) + runFilter (1, 1, r, c[1]));
        }
        else
        {
            l = runFilter (1, 0, runFilter (0, 0, l, c[0]), c[1]);
            r = runFilter (1, 1, runFilter (0, 1, r, c[0]), c[1]);
        }

        const float pl = panL0 + (panL - panL0) * t;
        const float pr = panR0 + (panR - panR0) * t;
        outL[s] += l * amp * pl;
        outR[s] += r * amp * pr;
    }

    v.rampsValid = true;
    if (! v.env[0].isActive())
        v.active = false;
}

} // namespace divinity
