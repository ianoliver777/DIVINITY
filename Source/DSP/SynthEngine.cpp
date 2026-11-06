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
    static const char* oscNames[20] = { "on", "engine", "table", "pos", "fold", "wave", "pw", "unison", "detune",
                                        "width", "phase", "randphase", "oct", "semi", "fine", "level", "blend",
                                        "warp", "warp_src", "warp_amt" };
    for (int i = 0; i < kNumOscs; ++i)
        for (int k = 0; k < 20; ++k)
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

    static const char* lNames[8] = { "shape", "rate", "sync", "div", "depth", "phase", "smooth", "on" };
    for (int i = 0; i < kNumLfos; ++i)
        for (int k = 0; k < 8; ++k)
            lfo[(size_t) i][(size_t) k] = get (ids::lfo (i, lNames[k]));

    static const char* mNames[3] = { "src", "dst", "amt" };
    for (int i = 0; i < kNumModSlots; ++i)
        for (int k = 0; k < 3; ++k)
            mod[(size_t) i][(size_t) k] = get (ids::mod (i, mNames[k]));

    for (int i = 0; i < kNumMacros; ++i)
        macro[(size_t) i] = get (ids::macro (i));

    global[0] = get (ids::voices); global[1] = get (ids::bendRange); global[2] = get (ids::glide);
    global[3] = get (ids::voiceMode); global[4] = get (ids::masterTune);
    engineMode = get (ids::engineMode); analog = get (ids::analog);
    velSens = get (ids::velSens);
    rnd[0] = get (ids::rndRate); rnd[1] = get (ids::rndSmooth); rnd[2] = get (ids::rndSync); rnd[3] = get (ids::rndDiv);
    stepG[0] = get (ids::stepOn); stepG[1] = get (ids::stepMode); stepG[2] = get (ids::stepRate);
    stepG[3] = get (ids::stepLength); stepG[4] = get (ids::stepSwing); stepG[5] = get (ids::stepSmooth);
    for (int i = 0; i < kNumSteps; ++i) stepLvl[(size_t) i] = get (ids::step (i, "lvl"));
    const juce::String smpIds[11] = { ids::smpOn, ids::smpSample, ids::smpLevel, ids::smpTune, ids::smpFine, ids::smpStart,
                                      ids::smpLoop, ids::smpLoopStart, ids::smpLoopEnd, ids::smpReverse, ids::smpKeytrack };
    for (int i = 0; i < 11; ++i) smp[i] = get (smpIds[i]);
    mpeOn = get (ids::mpeOn); mpeBend = get (ids::mpeBend); quality = get (ids::quality);
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
        o.fine = f (p[14]); o.level = f (p[15]); o.blend = f (p[16]);
        o.warp = juce::jlimit (0, (int) OscWarp::Mirror, i (p[17])); o.warpSrc = juce::jlimit (0, kNumOscs - 1, i (p[18])); o.warpAmt = f (p[19]);
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
        l.on = b (p[7]);
    }
    for (int k = 0; k < kNumModSlots; ++k)
    {
        auto& m = s.mod[(size_t) k]; const auto& p = mod[(size_t) k];
        m.src = i (p[0]); m.dst = i (p[1]); m.amt = f (p[2]);
    }
    for (int k = 0; k < kNumMacros; ++k) s.macro[(size_t) k] = f (macro[(size_t) k]);
    s.maxVoices = juce::jlimit (1, kMaxVoices, i (global[0]));
    s.bendRange = i (global[1]); s.glideMs = f (global[2]); s.voiceMode = i (global[3]); s.masterTune = f (global[4]);
    s.v2 = i (engineMode) == (int) EngineMode::Divine2;
    s.analog = juce::jlimit (0.0f, 1.0f, f (analog));
    s.velSens = f (velSens);
    s.rndRate = f (rnd[0]); s.rndSmooth = f (rnd[1]); s.rndSync = b (rnd[2]); s.rndDiv = i (rnd[3]);
    s.stepOn = b (stepG[0]); s.stepMode = i (stepG[1]); s.stepRate = i (stepG[2]); s.stepLength = juce::jlimit (1, kNumSteps, i (stepG[3]));
    s.stepSwing = f (stepG[4]); s.stepSmooth = f (stepG[5]);
    for (int k = 0; k < kNumSteps; ++k) s.stepLvl[(size_t) k] = f (stepLvl[(size_t) k]);
    s.smpOn = b (smp[0]); s.smpSample = i (smp[1]); s.smpLevel = f (smp[2]); s.smpTune = i (smp[3]); s.smpFine = f (smp[4]);
    s.smpStart = f (smp[5]); s.smpLoop = b (smp[6]); s.smpLoopStart = f (smp[7]); s.smpLoopEnd = f (smp[8]);
    s.smpReverse = b (smp[9]); s.smpKeytrack = b (smp[10]);
    s.mpeOn = b (mpeOn); s.mpeBend = juce::jlimit (1, 96, i (mpeBend)); s.quality = juce::jlimit (0, 2, i (quality));
}

//==============================================================================
SynthEngine::SynthEngine (juce::AudioProcessorValueTreeState& apvts, const WavetableBank& b,
                          const FactorySamples& fs, const UserSampleSlot& us, const std::array<UserWavetableSlot, kNumOscs>& ut)
    : refs (apvts), soul (apvts), bank (b), factorySamples (fs), userSample (us), userTables (ut)
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
        v.driftRng.seed (0xD1F7u + (uint32_t) idx * 104729u);
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
    rndRng.seed (0x7A11D0u); rndPhase = 0.0; rndTarget = rndValue = rndPrevTarget = 0.0f;
    stepFreeBeats = 0.0; stepValue = 0.0f; stepIndexDisplay = -1;
    lfoValues.fill (0.0f);
    lastGlobalDest.fill (0.0f);
    heldNotes.clear();
    sustainPedal = false;
    soul.reset();
    perf = {};
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
    // MPE (lower zone): channel 1 is the master channel, 2-16 carry one note each with its own bend,
    // pressure and slide (CC74). Without MPE every channel behaves like channel 1.
    const bool mpeNote = snap.mpeOn && m.getChannel() >= 2;
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity(), m.getChannel());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber(), m.getChannel());
    else if (m.isPitchWheel())
    {
        const float bend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        if (mpeNote) { mpeChBend[(size_t) m.getChannel()] = bend; for (auto& v : voices) if (v.active && v.channel == m.getChannel()) v.mpeBend = bend * (float) snap.mpeBend; }
        else pitchBend = bend;
    }
    else if (m.isChannelPressure())
    {
        const float pr = (float) m.getChannelPressureValue() / 127.0f;
        if (mpeNote) { mpeChPressure[(size_t) m.getChannel()] = pr; for (auto& v : voices) if (v.active && v.channel == m.getChannel()) v.mpePressure = pr; }
        else channelPressure = pr;
    }
    else if (mpeNote && m.isController() && m.getControllerNumber() == 74)
    {
        mpeChSlide[(size_t) m.getChannel()] = (float) m.getControllerValue() / 127.0f;
        for (auto& v : voices) if (v.active && v.channel == m.getChannel()) v.mpeSlide = (float) m.getControllerValue() / 127.0f;
    }
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
    v.channel = currentChannel;
    // MPE: a member channel's bend / pressure / slide sent before the note applies to it straight away
    const bool mpeCh = snap.mpeOn && juce::isPositiveAndBelow (v.channel, 17) && v.channel >= 2;
    v.mpeBend = mpeCh ? mpeChBend[(size_t) v.channel] * (float) snap.mpeBend : 0.0f;
    v.mpePressure = mpeCh ? mpeChPressure[(size_t) v.channel] : 0.0f;
    v.mpeSlide = mpeCh ? mpeChSlide[(size_t) v.channel] : 0.0f;
    v.modPhase.fill (0.0);   // FM / sync modulators restart with the note: tight, repeatable attacks
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
        for (auto& o : v.syncBlep) o.fill (0.0f);
        for (auto& o : v.warpDcValid) o.fill (false);
        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& op = snap.osc[(size_t) o];
            // Osc D draws from its own generator so A-C keep exactly the random-phase sequence of v0.5 (Classic recall)
            auto& r = o < 3 ? v.rng : v.driftRng;
            for (int u = 0; u < kMaxUnison; ++u)
                v.phase[(size_t) o][(size_t) u] = op.randPhase ? (double) r.unipolar() : (double) op.phase;
        }
        for (auto& f : v.svf2) for (auto& c : f) c.reset();
        for (auto& f : v.formant) for (auto& ch : f) for (auto& c : ch) c.reset();
        if (snap.v2)
        {
            for (auto& o : v.foldPrev) o.fill (0.0f);
            // start each drift somewhere different so voices never move in lockstep
            for (auto& o : v.uniDrift) for (auto& d : o) { d.reset(); d.value = d.target = v.driftRng.bipolar(); }
            v.pitchDrift.reset(); v.pitchDrift.value = v.driftRng.bipolar();
            v.cutoffDrift.reset(); v.cutoffDrift.value = v.driftRng.bipolar();
        }
    }
    // Divine 2.0: every note is a slightly different "take" (like a real analog voice card)
    v.noteCents  = snap.v2 ? v.driftRng.bipolar() * snap.analog * 2.5f : 0.0f;
    v.noteCutOct = snap.v2 ? v.driftRng.bipolar() * snap.analog * 0.06f : 0.0f;
    // Stolen / retriggered voices keep their phase, filter and envelope levels: no clicks.
    v.ageSamples = 0;
    v.smpDone = false;
    v.smpPos = -1.0;   // set on the first rendered block (needs the current sample + start point)

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

void SynthEngine::noteOn (int note, float velocity, int channel)
{
    currentChannel = channel;
    if (snap.voiceMode != (int) VoiceMode::Poly)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        if (heldNotes.size() < heldNotes.capacity()) heldNotes.push_back (note);

        for (size_t i = 1; i < voices.size(); ++i)          // mono: only voice 0 sounds
            if (voices[i].active && voices[i].keyDown) releaseVoice (voices[i]);

        auto& v = voices[0];
        if (snap.voiceMode == (int) VoiceMode::Legato && v.active && ! v.env[0].isReleasing() && heldNotes.size() > 1)
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
        if (v.active && v.note == note && (v.keyDown || v.sustained) && (! snap.mpeOn || v.channel == channel))
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

void SynthEngine::noteOff (int note, int channel)
{
    if (snap.voiceMode != (int) VoiceMode::Poly)
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
        if (v.active && v.keyDown && v.note == note && (! snap.mpeOn || v.channel == channel || channel <= 1))
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
    src[(size_t) ModSource::Env4] = v.env[3].getLevel();
    src[(size_t) ModSource::Velocity] = v.velocity;
    src[(size_t) ModSource::NoteNumber] = (float) (v.note - 60) / 60.0f;
    src[(size_t) ModSource::Aftertouch] = juce::jmax (channelPressure, v.aftertouch, v.mpePressure);
    // MPE sources fall back to ordinary pressure when MPE is off, so patches still respond to a normal keyboard
    src[(size_t) ModSource::MpePressure] = snap.mpeOn ? v.mpePressure : juce::jmax (channelPressure, v.aftertouch);
    src[(size_t) ModSource::MpeSlide] = v.mpeSlide;

    std::fill (dest, dest + (size_t) ModDest::NumDests, 0.0f);
    for (const auto& slot : snap.mod)
    {
        if (slot.src <= 0 || slot.dst <= 0 || slot.src >= (int) ModSource::NumSources || slot.dst >= (int) ModDest::NumDests)
            continue;
        dest[slot.dst] += src[slot.src] * slot.amt;
    }
    // never let a non-finite value reach pitch / index maths
    for (size_t d = 0; d < (size_t) ModDest::NumDests; ++d)
        if (! std::isfinite (dest[d])) dest[d] = 0.0f;
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
        if (! std::isfinite (rate)) rate = 1.0;
        rate = juce::jlimit (0.0, 2000.0, rate);
        float raw = lfos[(size_t) i].advance (lp.shape, rate, seconds, lp.smooth, lp.phase, locked && std::isfinite (ppqNow), ppqNow / beats);
        if (! std::isfinite (raw)) { lfos[(size_t) i].reset (0xC0FFEEu + (uint32_t) i); raw = 0.0f; }
        if (! lp.on) raw = 0.0f;   // LFO power (0.7): an LFO that's off moves nothing
        lfoValues[(size_t) i] = raw * lp.depth * (1.0f + 2.0f * movement) * juce::jmax (0.1f, 1.0f + perf.motion);
        lfoDisplay[(size_t) i].store (raw * lp.depth);
    }

    float globalSrc[(size_t) ModSource::NumSources] {};
    for (int i = 0; i < kNumLfos; ++i) globalSrc[(size_t) ModSource::Lfo1 + (size_t) i] = lfoValues[(size_t) i];
    globalSrc[(size_t) ModSource::ModWheel]   = modWheel;
    globalSrc[(size_t) ModSource::Aftertouch] = channelPressure;
    globalSrc[(size_t) ModSource::PitchBend]  = pitchBend;
    for (int i = 0; i < kNumMacros; ++i) globalSrc[(size_t) ModSource::Macro1 + (size_t) i] = snap.macro[(size_t) i];
    globalSrc[(size_t) ModSource::SoulX] = soul.getEffectiveX();
    globalSrc[(size_t) ModSource::SoulY] = soul.getEffectiveY();
    for (int i = 4; i < kNumMacros; ++i) globalSrc[(size_t) ModSource::Macro5 + (size_t) (i - 4)] = snap.macro[(size_t) i];

    // ---- RANDOM: sample-and-hold with adjustable glide (free rate or tempo-synced) ----
    {
        const double period = snap.rndSync ? lfoDivisionInBeats (snap.rndDiv) * 60.0 / juce::jmax (20.0, tr.bpm)
                                           : 1.0 / juce::jmax (0.01, (double) snap.rndRate);
        rndPhase += seconds / period;
        if (rndPhase >= 1.0) { rndPhase -= std::floor (rndPhase); rndPrevTarget = rndTarget; rndTarget = rndRng.bipolar(); }
        const float glide = snap.rndSmooth;
        // smooth = crossfade from the previous value over (smooth x period); 0 = hard steps
        const float t = glide < 1.0e-3f ? 1.0f : (float) juce::jlimit (0.0, 1.0, rndPhase / glide);
        const float shaped = t * t * (3.0f - 2.0f * t);
        rndValue = rndPrevTarget + (rndTarget - rndPrevTarget) * shaped;
        globalSrc[(size_t) ModSource::Random] = rndValue;
        rndDisplay.store (rndValue);
    }

    // ---- STEP (modulate mode): current step level as a 0..1 source, host-synced ----
    {
        float target = 0.0f;
        if (snap.stepOn)
        {
            const double stepBeats = stepRateBeats (snap.stepRate);
            double beat;
            if (tr.isPlaying && tr.hasPpq) beat = tr.ppqAtBlockStart + (double) sampleOffset / sampleRate * tr.bpm / 60.0;
            else { stepFreeBeats += seconds * juce::jmax (20.0, tr.bpm) / 60.0; beat = stepFreeBeats; }
            const double pair = 2.0 * stepBeats, inPair = beat - std::floor (beat / pair) * pair;
            const double swingPoint = stepBeats * (1.0 + snap.stepSwing * 0.66);
            long long k = (long long) std::floor (beat / pair) * 2 + (inPair >= swingPoint ? 1 : 0);
            const int idx = (int) (((k % snap.stepLength) + snap.stepLength) % snap.stepLength);
            target = snap.stepLvl[(size_t) idx];
            stepIndexDisplay.store (idx);
            const double stepSecs = stepBeats * 60.0 / juce::jmax (20.0, tr.bpm);
            const double tc = juce::jmax (0.001, (double) snap.stepSmooth * stepSecs * 0.5);
            stepValue += (float) (1.0 - std::exp (-seconds / tc)) * (target - stepValue);
        }
        else { stepValue = 0.0f; stepIndexDisplay.store (-1); }
        globalSrc[(size_t) ModSource::Step] = stepValue;
        stepDisplay.store (stepValue);
    }

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
    // SOUL ENGINE + Divine Intervention (the matrix can push the XY position and the Divine amount)
    perf = soul.compute (seconds, gdest[(size_t) ModDest::SoulX], gdest[(size_t) ModDest::SoulY], gdest[(size_t) ModDest::DivineAmount]);
    globalMods.satDrive    = gdest[(size_t) ModDest::SatDrive] + perf.drive;
    globalMods.chorusDepth = gdest[(size_t) ModDest::ChorusDepth] + perf.detune * 0.3f;
    globalMods.delayMix    = gdest[(size_t) ModDest::DelayMix] + perf.delay;
    globalMods.reverbMix   = gdest[(size_t) ModDest::ReverbMix] + perf.reverb;
    globalMods.reverbSize  = perf.reverbSize;
    globalMods.width       = perf.width;

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

    /** Divine 2.0 filter drive: same curve family, but loudness-compensated by the
        actual small-signal gain so turning drive up adds harmonics, not just volume. */
    inline float drive2 (float x, float amount) noexcept
    {
        if (amount < 1.0e-4f) return x;
        const float g = 1.0f + amount * 7.0f;
        return dsp::fastTanh (x * g) / (0.5f + 0.5f * std::sqrt (g));
    }

    // fixed, slightly irregular unison spacing: breaks up the perfectly periodic beating
    // (and the periodic centre cancellation) of evenly spaced detune
    constexpr float kUnisonIrregular[kMaxUnison] = { 1.0f, 0.93f, 1.06f, 0.97f, 1.03f, 0.91f, 1.08f, 0.96f };
}

//==============================================================================
// 0.8.0 WARP: SYNC / FM / AM / phase warps + imported tables
//==============================================================================
namespace
{
    inline double wrap01 (double p) noexcept { p -= std::floor (p); return p >= 1.0 ? 0.0 : p; }
    constexpr double kWarpTwoPi = 6.283185307179586;
    /** Mirror: 0 -> 1 -> 0 with a smooth turn-around (no kink, so no aliasing from the reversal). */
    inline double mirrorPhase (double p) noexcept { return 0.5 - 0.5 * std::cos (kWarpTwoPi * p); }
    inline float fmDepth (float a) noexcept { return a * a * 1.5f; }                  // cycles of phase deviation
    inline double syncRatio (float a) noexcept { return std::exp2 ((double) a * 4.0); } // slave up to +4 octaves
    /** Phase-warp modes. Every curve keeps the phase *speed* continuous across the cycle wrap (no kinks):
        a kink right on a saw's edge aliases badly, a smooth speed change doesn't.
        Bend +/-: speed 1 + c sin(2 pi p)  - squeezes the cycle into its first (+) or second (-) half.
        Phase Dist: speed 1 - c cos(2 pi p) - slow edges, fast middle (Casio-style PD, smoothed).
        PWM: the cycle plays in the first w of the period (smooth in and out), then holds. */
    inline double warpPhase (int mode, double p, float a) noexcept
    {
        const double c = 0.95 * a;
        switch (mode)
        {
            case (int) OscWarp::BendPlus:  return p + c * (1.0 - std::cos (kWarpTwoPi * p)) / kWarpTwoPi;
            case (int) OscWarp::BendMinus: return p - c * (1.0 - std::cos (kWarpTwoPi * p)) / kWarpTwoPi;
            case (int) OscWarp::Pwm:       { const double w = 1.0 - 0.94 * a; return p < w ? 0.5 - 0.5 * std::cos (3.141592653589793 * p / w) : 0.0; }
            case (int) OscWarp::PhaseDist: return p - c * std::sin (kWarpTwoPi * p) / kWarpTwoPi;
            default: return p;
        }
    }
    inline double warpSlope (int mode, float a) noexcept
    {
        switch (mode)
        {
            case (int) OscWarp::BendPlus: case (int) OscWarp::BendMinus: case (int) OscWarp::PhaseDist: return 1.0 + 0.95 * a;
            case (int) OscWarp::Pwm:       return 1.5707963 / (1.0 - 0.94 * a);
            case (int) OscWarp::Mirror:    return 3.14159265;
            case (int) OscWarp::Sync:      return syncRatio (a);
            default: return 1.0;
        }
    }
}

int SynthEngine::warpSourceOf (int o) const noexcept
{
    const int s = snap.osc[(size_t) o].warpSrc;
    return s == o ? (o == 0 ? 1 : 0) : s;   // an oscillator can't modulate itself: fall back to its neighbour
}

void SynthEngine::renderModulator (Voice& v, int o, double basePitch, const float* dest, int n) noexcept
{
    const auto& op = snap.osc[(size_t) o];
    const double pitch = basePitch + op.oct * 12 + op.semi + op.fine / 100.0 + dest[(size_t) oscModDest (0, o)] * 12.0;
    const double inc = juce::jmin (0.49, (double) noteToHz (pitch) / sampleRate);
    modInc[(size_t) o] = inc;
    const int lvl = WavetableBank::levelForIncrement (inc, 0.45);
    const bool wt = op.engine == (int) OscEngine::Wavetable;
    const float pos = juce::jlimit (0.0f, 1.0f, op.pos + dest[(size_t) oscModDest (1, o)]);
    const UserWavetable* ut = (wt && op.table >= bank.getNumTables()) ? userTables[(size_t) o].get() : nullptr;
    const int table = juce::jlimit (0, bank.getNumTables() - 1, op.table);
    const float pw = juce::jlimit (0.03f, 0.97f, op.pw + dest[(size_t) oscModDest (4, o)] * 0.45f);

    WavetableBank::FrameView F;
    if (wt)
    {
        if (ut != nullptr) F = ut->getStoredFrame ((int) std::lround (pos * (float) (ut->frames - 1)), lvl);
        else if (op.table >= bank.getNumTables()) F = bank.getStoredFrame (bank.getAnalogTable(), WavetableBank::kAnalogSine, lvl);
        else F = bank.getStoredFrame (table, (int) std::lround (pos * (float) (bank.framesFor (table) - 1)), lvl);
    }
    else
    {
        const int frame = op.wave == (int) AnalogWave::Sine ? WavetableBank::kAnalogSine
                        : op.wave == (int) AnalogWave::Triangle ? WavetableBank::kAnalogTriangle
                        : op.wave == (int) AnalogWave::Square ? WavetableBank::kAnalogSquare : WavetableBank::kAnalogSaw;
        F = bank.getStoredFrame (bank.getAnalogTable(), frame, lvl);
    }
    const bool pulse = ! wt && op.wave == (int) AnalogWave::Pulse;
    double ph = v.modPhase[(size_t) o];
    auto& out = modBuf[(size_t) o];
    auto& wraps = wrapBuf[(size_t) o];
    for (int s = 0; s < n; ++s)
    {
        out[(size_t) s] = pulse ? F.at (wrap01 (ph - pw)) - F.at (ph) : F.at (ph);
        const double next = ph + inc;
        if (next >= 1.0) { wraps[(size_t) s] = (float) ((1.0 - ph) / inc); ph = next - 1.0; }
        else { wraps[(size_t) s] = -1.0f; ph = next; }
    }
    v.modPhase[(size_t) o] = ph;
}

void SynthEngine::renderWarpedUnison (Voice& v, int o, int u, const EngineSnapshot::Osc& op, double& ph, double inc, int n, float invN,
                                      float gL, float gR, float level0, float levelT, float pos0, float posT, float fold0, float foldT,
                                      float warp0, float warpT, const UserWavetable* ut, float* mixL, float* mixR) noexcept
{
    const int mode = op.warp;
    const bool wt = op.engine == (int) OscEngine::Wavetable;
    const int table = juce::jlimit (0, bank.getNumTables() - 1, op.table);
    const int src = warpSourceOf (o);
    const float* mod = modBuf[(size_t) src].data();
    const float* wraps = wrapBuf[(size_t) src].data();
    const bool v2 = snap.v2;

    // mip level: the steepest phase slope this block (warps and sync speed the waveform up) + FM sidebands
    const float aMax = juce::jmax (warp0, warpT);
    double effInc = inc * juce::jmin (16.0, warpSlope (mode, aMax));
    if (mode == (int) OscWarp::FM) effInc = inc + fmDepth (aMax) * juce::MathConstants<double>::twoPi * modInc[(size_t) src];
    const int lvl = WavetableBank::levelForIncrement (juce::jmin (0.49, effInc), 0.6);

    const float pw = juce::jlimit (0.03f, 0.97f, op.pw);
    const int nf = ut != nullptr ? ut->frames : (wt ? bank.framesFor (table) : 1);
    int analogFrame = WavetableBank::kAnalogSaw;
    if (! wt)
        analogFrame = op.wave == (int) AnalogWave::Sine ? WavetableBank::kAnalogSine
                    : op.wave == (int) AnalogWave::Triangle ? WavetableBank::kAnalogTriangle
                    : op.wave == (int) AnalogWave::Square ? WavetableBank::kAnalogSquare : WavetableBank::kAnalogSaw;
    const auto A = bank.getStoredFrame (bank.getAnalogTable(), analogFrame, lvl);
    const bool pulse = ! wt && op.wave == (int) AnalogWave::Pulse;

    // per-sample frame interpolation state
    int f0 = 0, f1 = 0; float fr = 0.0f;
    WavetableBank::FrameView FA, FB;
    auto setFrames = [&] (float t)
    {
        if (! wt) return;
        const float fpos = juce::jlimit (0.0f, (float) (nf - 1), (pos0 + (posT - pos0) * t) * (float) (nf - 1));
        const int a = juce::jlimit (0, nf - 1, (int) fpos), b = juce::jmin (a + 1, nf - 1);
        fr = fpos - (float) a;
        if (a != f0 || b != f1 || FA.data == nullptr)
        {
            f0 = a; f1 = b;
            FA = ut != nullptr ? ut->getStoredFrame (a, lvl) : bank.getStoredFrame (table, a, lvl);
            FB = ut != nullptr ? ut->getStoredFrame (b, lvl) : bank.getStoredFrame (table, b, lvl);
        }
    };
    auto read = [&] (double p) noexcept -> float
    {
        if (wt) { const float a = FA.at (p); return a + fr * (FB.at (p) - a); }
        if (pulse) return 0.9f * (A.at (wrap01 (p - pw)) - A.at (p));
        return 0.9f * A.at (p);
    };

    float& fprev = v.foldPrev[(size_t) o][(size_t) u];
    float& blep = v.syncBlep[(size_t) o][(size_t) u];

    // Bend / PWM / phase-dist / sync / AM shift the waveform's average: track and remove it (≈6 Hz),
    // seeded with the cycle mean at note start so there's no thump.
    float& dc = v.warpDc[(size_t) o][(size_t) u];
    if (! v.warpDcValid[(size_t) o][(size_t) u])
    {
        v.warpDcValid[(size_t) o][(size_t) u] = true;
        setFrames (0.0f);
        double sum = 0.0;
        constexpr int K = 64;
        for (int k = 0; k < K; ++k)
        {
            const double q = (k + 0.5) / K;
            switch (mode)
            {
                case (int) OscWarp::Sync:   sum += read (wrap01 (q * syncRatio (warp0))); break;
                case (int) OscWarp::Mirror: { const float x0 = read (q); sum += x0 + warp0 * (read (mirrorPhase (q)) - x0); break; }
                case (int) OscWarp::FM: case (int) OscWarp::AM: case (int) OscWarp::Off: sum += read (q); break;
                default:                    sum += read (warpPhase (mode, q, warp0)); break;
            }
        }
        dc = (float) (sum / K);
        FA.data = nullptr;   // re-pick frames in the loop
    }
    const float dcCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 6.0f / (float) sampleRate);

    for (int s = 0; s < n; ++s)
    {
        const float t = (float) s * invN;
        setFrames (t);
        const float a = warp0 + (warpT - warp0) * t;
        float x;
        switch (mode)
        {
            case (int) OscWarp::FM:     x = read (wrap01 (ph + fmDepth (a) * mod[s])); break;
            case (int) OscWarp::AM:     x = read (ph) * (1.0f - a + a * mod[s]); break;
            case (int) OscWarp::Mirror: { const float x0 = read (ph); x = x0 + a * (read (mirrorPhase (ph)) - x0); break; }
            case (int) OscWarp::Sync:   x = read (ph); break;
            default:                    x = read (warpPhase (mode, ph, a)); break;
        }

        // advance (hard sync resets on the master's wrap, with a 2-point polyBLEP on the jump)
        if (mode == (int) OscWarp::Sync)
        {
            x += blep; blep = 0.0f;
            const double incS = juce::jmin (0.49, inc * syncRatio (a));
            const float f = wraps[s];
            if (f >= 0.0f)
            {
                const double pe = wrap01 (ph + (double) f * incS);
                const float d = read (0.0) - read (pe);
                x += d * 0.5f * (1.0f - f) * (1.0f - f);
                blep = -d * 0.5f * f * f;
                // polyBLAMP for the slope change at the reset (matters for saw-like tables: phase 0 sits on the edge)
                const float ds = (read (incS) - read (0.0)) - (read (pe) - read (wrap01 (pe - incS)));
                const float g = 1.0f - f;
                x += ds * g * g * g / 6.0f;
                blep += ds * f * f * f / 6.0f;
                ph = (1.0 - (double) f) * incS;
            }
            else { ph += incS; if (ph >= 1.0) ph -= 1.0; }
        }
        else { ph += inc; if (ph >= 1.0) ph -= 1.0; }

        dc += dcCoeff * (x - dc);
        x -= dc;

        const float fAmt = fold0 + (foldT - fold0) * t;
        x = v2 ? dsp::FoldAdaa::process (x, fprev, fAmt) : fold (x, fAmt);
        x *= level0 + (levelT - level0) * t;
        mixL[s] += x * gL; mixR[s] += x * gR;
    }
}

void SynthEngine::renderVoice (Voice& v, float* outL, float* outR, int n, const float* globalSrc)
{
    float dest[(size_t) ModDest::NumDests];
    computeVoiceMods (v, globalSrc, dest);
    auto D = [&dest] (ModDest d) { return dest[(size_t) d]; };

    const float tone = snap.macro[0], texture = snap.macro[2], space = snap.macro[3];
    const float character = snap.macro[4], widthMacro = snap.macro[5], punch = snap.macro[6];

    // envelope edits apply to held notes immediately (sustain level, release time...)
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const auto& ep = snap.env[(size_t) e];
        const float am = e == 0 ? std::pow (2.0f, juce::jlimit (-6.0f, 6.0f, perf.attackLog2) - punch * 4.0f) : 1.0f;   // PUNCH: snappier attack
        const float rm = e == 0 ? std::pow (2.0f, juce::jlimit (-6.0f, 6.0f, perf.releaseLog2)) : 1.0f;
        v.env[(size_t) e].setParameters (juce::jlimit (0.0005f, 30.0f, ep.a * am), ep.d, ep.s, juce::jlimit (0.001f, 40.0f, ep.r * rm), ep.curve);
    }

    // SOUL instability: slow random per-voice pitch wander
    {
        const float secs = (float) n / (float) sampleRate;
        v.driftTimer -= secs;
        if (v.driftTimer <= 0.0f) { v.driftTarget = v.rng.bipolar(); v.driftTimer = 0.25f + 0.35f * v.rng.unipolar(); }
        v.drift += (1.0f - std::exp (-secs / 0.3f)) * (v.driftTarget - v.drift);
    }
    const float invN = 1.0f / (float) n;
    const float blockSecs = (float) n / (float) sampleRate;
    const bool v2 = snap.v2;
    const float analogAmt = v2 ? snap.analog : 0.0f;
    float pitchDriftCents = 0.0f, cutDriftOct = 0.0f;
    if (v2)
    {
        pitchDriftCents = v.pitchDrift.advance (v.driftRng, blockSecs, 1.7f) * analogAmt * 3.0f + v.noteCents;
        cutDriftOct     = v.cutoffDrift.advance (v.driftRng, blockSecs, 2.3f) * analogAmt * 0.12f + v.noteCutOct;
    }

    // ---- glide ----
    const double glideSec = snap.glideMs * 0.001;
    if (glideSec > 0.0005)
    {
        const double coef = 1.0 - std::exp (-(double) n / (sampleRate * glideSec / 4.0));
        v.pitch += (v.targetPitch - v.pitch) * coef;
    }
    else v.pitch = v.targetPitch;

    if (! std::isfinite (v.pitch)) v.pitch = v.targetPitch;
    const double basePitch = v.pitch + snap.masterTune / 100.0 + pitchBend * snap.bendRange + (double) v.mpeBend
                           + (double) (v.drift * juce::jlimit (0.0f, 100.0f, perf.driftCents)) / 100.0
                           + (double) pitchDriftCents / 100.0;

    float* mixL = scratchL.data();
    float* mixR = scratchR.data();
    std::fill (mixL, mixL + n, 0.0f);
    std::fill (mixR, mixR + n, 0.0f);

    // ---- modulators for SYNC / FM / AM (0.8.0) ----
    // Each source is rendered once per voice (centre pitch, pre-level) so it can be switched off and still modulate.
    {
        std::array<bool, kNumOscs> need {};
        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& op = snap.osc[(size_t) o];
            if (op.on && (op.warp == (int) OscWarp::Sync || op.warp == (int) OscWarp::FM || op.warp == (int) OscWarp::AM))
                need[(size_t) warpSourceOf (o)] = true;
        }
        for (int o = 0; o < kNumOscs; ++o)
            if (need[(size_t) o]) renderModulator (v, o, basePitch, dest, n);
    }

    // ---- oscillators ----
    for (int o = 0; o < kNumOscs; ++o)
    {
        const auto& op = snap.osc[(size_t) o];
        if (! op.on) { v.lastLevel[(size_t) o] = 0.0f; continue; }

        const float levelT  = juce::jlimit (0.0f, 1.0f, op.level + dest[(size_t) oscModDest (2, o)]);
        const float posT    = juce::jlimit (0.0f, 1.0f, op.pos + dest[(size_t) oscModDest (1, o)]);
        const float foldT   = juce::jlimit (0.0f, 1.0f, op.fold + dest[(size_t) oscModDest (3, o)] + texture * 0.4f + character * 0.15f + perf.fold);
        const float pw      = juce::jlimit (0.03f, 0.97f, op.pw + dest[(size_t) oscModDest (4, o)] * 0.45f);
        const float detune  = juce::jlimit (0.0f, 1.0f, op.detune + dest[(size_t) oscModDest (5, o)] + perf.detune);
        const float width   = juce::jlimit (0.0f, 1.0f, op.width + space * 0.3f + widthMacro * 0.5f + perf.width);
        const double oscPitch = basePitch + op.oct * 12 + op.semi + op.fine / 100.0
                              + dest[(size_t) oscModDest (0, o)] * 12.0;

        const float level0 = v.rampsValid ? v.lastLevel[(size_t) o] : levelT;
        const float pos0   = v.rampsValid ? v.lastPos[(size_t) o]   : posT;
        const float fold0  = v.rampsValid ? v.lastFold[(size_t) o]  : foldT;
        v.lastLevel[(size_t) o] = levelT; v.lastPos[(size_t) o] = posT; v.lastFold[(size_t) o] = foldT;
        if (level0 < 1.0e-5f && levelT < 1.0e-5f) continue;

        const int nU = snap.quality == (int) Quality::Eco ? juce::jmin (op.unison, 4) : op.unison;
        // Unison blend: outer voices at `blend` against the centre, loudness-normalised (blend 1 = v0.6 exactly)
        const float blend = juce::jlimit (0.0f, 1.0f, op.blend);
        float uniPow = 0.0f;
        for (int u = 0; u < nU; ++u)
        {
            const float sp = nU > 1 ? std::abs (((float) u / (float) (nU - 1)) * 2.0f - 1.0f) : 0.0f;
            const float w = 1.0f - (1.0f - blend) * sp;
            uniPow += w * w;
        }
        const float uniGain = blend >= 1.0f ? 1.0f / std::sqrt ((float) nU) : 1.0f / std::sqrt (juce::jmax (1.0e-6f, uniPow));
        const bool wt = op.engine == (int) OscEngine::Wavetable;
        const int table = juce::jlimit (0, bank.getNumTables() - 1, op.table);
        // 0.8.0: WARP modes and imported tables take the generic renderer; everything else stays on the
        // original paths (bit-identical output for every pre-0.8 patch)
        const UserWavetable* userTable = (wt && op.table >= bank.getNumTables()) ? userTables[(size_t) o].get() : nullptr;
        if (wt && op.table >= bank.getNumTables() && userTable == nullptr) continue;   // "User Wavetable" with nothing loaded: silent
        const float warpT = juce::jlimit (0.0f, 1.0f, op.warpAmt + dest[(size_t) oscModDest (6, o)]);
        const float warp0 = v.rampsValid ? v.lastWarp[(size_t) o] : warpT;
        v.lastWarp[(size_t) o] = warpT;
        const bool warped = op.warp != (int) OscWarp::Off || userTable != nullptr;

        for (int u = 0; u < nU; ++u)
        {
            const float spreadPos = nU > 1 ? ((float) u / (float) (nU - 1)) * 2.0f - 1.0f : 0.0f;
            double cents = spreadPos * std::pow (detune, 1.5f) * 50.0;
            if (v2)
            {
                cents *= kUnisonIrregular[u];
                // each unison voice wanders on its own: the stack breathes instead of phasing mechanically
                cents += v.uniDrift[(size_t) o][(size_t) u].advance (v.driftRng, blockSecs, 0.9f) * analogAmt * (nU > 1 ? 4.0f : 1.5f);
            }
            // alternate sides so neighbouring detune values don't stack on one side
            const float pan = (u % 2 == 0 ? 1.0f : -1.0f) * std::abs (spreadPos) * width;
            float gL, gR; panGains (pan, gL, gR);
            if (blend < 1.0f) { const float w = 1.0f - (1.0f - blend) * std::abs (spreadPos); gL *= w; gR *= w; }
            gL *= uniGain; gR *= uniGain;

            const double inc = juce::jmin (0.49, (double) noteToHz (oscPitch + cents / 100.0) / sampleRate);
            double ph = v.phase[(size_t) o][(size_t) u];

            if (warped)
            {
                renderWarpedUnison (v, o, u, op, ph, inc, n, invN, gL, gR, level0, levelT, pos0, posT, fold0, foldT, warp0, warpT, userTable, mixL, mixR);
            }
            else if (wt)
            {
                float& fprev = v.foldPrev[(size_t) o][(size_t) u];
                // Classic 1.0 reads the original float tables exactly as v0.5 did; Divine 2.0 (and any
                // library table, which Classic never had) reads the compact half-octave bank.
                const bool classicTables = ! v2 && table < WavetableBank::kNumOriginalTables;
                const int lvl = classicTables ? WavetableBank::classicLevelForIncrement (inc) : WavetableBank::levelForIncrement (inc, 0.6);
                const int nf = classicTables ? WavetableBank::kFrames : bank.framesFor (table);
                for (int s = 0; s < n; ++s)
                {
                    const float t = (float) s * invN;
                    const float fpos = juce::jlimit (0.0f, (float) (nf - 1), (pos0 + (posT - pos0) * t) * (float) (nf - 1));
                    const int f0 = juce::jlimit (0, nf - 1, (int) fpos);
                    const int f1 = juce::jmin (f0 + 1, nf - 1);
                    const float fr = fpos - (float) f0;
                    float a, b;
                    if (classicTables)
                    {
                        const float* A = bank.getClassicFrame (table, f0, lvl);
                        const float* B = bank.getClassicFrame (table, f1, lvl);
                        const double idx = ph * WavetableBank::kSize;
                        const int i0 = juce::jlimit (0, WavetableBank::kSize - 1, (int) idx);
                        const float frac = (float) (idx - i0);
                        a = A[i0] + frac * (A[i0 + 1] - A[i0]);
                        b = B[i0] + frac * (B[i0 + 1] - B[i0]);
                    }
                    else
                    {
                        a = bank.getStoredFrame (table, f0, lvl).at (ph);
                        b = bank.getStoredFrame (table, f1, lvl).at (ph);
                    }
                    const float fAmt = fold0 + (foldT - fold0) * t;
                    float x = v2 ? dsp::FoldAdaa::process (a + fr * (b - a), fprev, fAmt) : fold (a + fr * (b - a), fAmt);
                    x *= level0 + (levelT - level0) * t;
                    mixL[s] += x * gL; mixR[s] += x * gR;
                    ph += inc; if (ph >= 1.0) ph -= 1.0;
                }
            }
            else
            {
                float& fprev = v.foldPrev[(size_t) o][(size_t) u];
                if (v2 && op.wave != (int) AnalogWave::Sine)
                {
                    // Divine 2.0: read the exact Fourier-series waveform from the band-limited analog
                    // table (half-octave mips). Pulse = difference of two band-limited saws, so PWM
                    // is alias-free too. Same phase, polarity and level as the PolyBLEP originals.
                    const int lvl = WavetableBank::levelForIncrement (inc, 0.6);
                    const int at = bank.getAnalogTable();
                    const bool pulse = op.wave == (int) AnalogWave::Pulse;
                    const int frame = op.wave == (int) AnalogWave::Triangle ? WavetableBank::kAnalogTriangle
                                    : op.wave == (int) AnalogWave::Square   ? WavetableBank::kAnalogSquare
                                                                             : WavetableBank::kAnalogSaw;
                    const auto T = bank.getStoredFrame (at, frame, lvl);
                    auto look = [&T] (double phase) noexcept { return T.at (phase); };
                    for (int s = 0; s < n; ++s)
                    {
                        const float t = (float) s * invN;
                        float x;
                        if (pulse)
                        {
                            double p2 = ph - pw; if (p2 < 0.0) p2 += 1.0;
                            x = look (p2) - look (ph);
                        }
                        else x = look (ph);
                        x = dsp::FoldAdaa::process (x * 0.9f, fprev, fold0 + (foldT - fold0) * t);
                        x *= level0 + (levelT - level0) * t;
                        mixL[s] += x * gL; mixR[s] += x * gR;
                        ph += inc; if (ph >= 1.0) ph -= 1.0;
                    }
                }
                else
                for (int s = 0; s < n; ++s)
                {
                    const float t = (float) s * invN;
                    const float fAmt = fold0 + (foldT - fold0) * t;
                    float x;
                    if (v2)
                    {
                        x = (float) std::sin (ph * kTwoPi);
                        x = dsp::FoldAdaa::process (x * 0.9f, fprev, fAmt);
                    }
                    else
                    {
                        x = analogSample (op.wave, ph, inc, op.wave == (int) AnalogWave::Pulse ? pw : 0.5);
                        x = fold (x * 0.9f, fAmt);
                    }
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
                const float x = (v2 ? analogSample2 (aw, v.subPhase, inc, 0.5) : analogSample (aw, v.subPhase, inc, 0.5)) * 0.9f * (sub0 + (subT - sub0) * (float) s * invN);
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

    // ---- sampler ----
    {
        const float smT = snap.smpOn ? juce::jlimit (0.0f, 1.0f, snap.smpLevel + D (ModDest::SamplerLevel)) : 0.0f;
        const float sm0 = v.rampsValid ? v.lastSmp : smT;
        v.lastSmp = smT;
        const SampleData* sd = snap.smpSample >= factorySamples.size() ? userSample.get() : factorySamples.get (snap.smpSample);
        if ((sm0 > 1.0e-5f || smT > 1.0e-5f) && sd != nullptr && sd->length() > 4 && ! v.smpDone)
        {
            const int len = sd->length();
            const bool rev = snap.smpReverse;
            const double startFrac = juce::jlimit (0.0, 0.95, (double) snap.smpStart + D (ModDest::SamplerStart));
            if (v.smpPos < 0.0) v.smpPos = rev ? (len - 1) * (1.0 - startFrac) : (len - 1) * startFrac;
            const double semis = (snap.smpKeytrack ? (basePitch - sd->rootNote) : (basePitch - v.note) /* bend/glide/drift only */)
                               + snap.smpTune + snap.smpFine / 100.0 + D (ModDest::SamplerPitch) * 24.0;
            const double inc = std::pow (2.0, semis / 12.0) * sd->sampleRate / sampleRate * (rev ? -1.0 : 1.0);
            const double loopA = (len - 1) * juce::jlimit (0.0, 0.95, (double) snap.smpLoopStart);
            const double loopB = (len - 1) * juce::jlimit (loopA / (len - 1) + 0.01, 1.0, (double) snap.smpLoopEnd);
            const float* L = sd->left.data();
            const float* R = sd->right.empty() ? L : sd->right.data();
            auto read = [len] (const float* d, double pos)
            {
                const int i1 = (int) pos;
                const float f = (float) (pos - i1);
                auto at = [d, len] (int i) { return d[juce::jlimit (0, len - 1, i)]; };
                const float xm1 = at (i1 - 1), x0 = at (i1), x1 = at (i1 + 1), x2 = at (i1 + 2);
                const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                return ((c3 * f + c2) * f + c1) * f + x0;
            };
            for (int s = 0; s < n; ++s)
            {
                double& pos = v.smpPos;
                if (snap.smpLoop && v.keyDown)
                {
                    if (! rev && pos >= loopB) pos = loopA + std::fmod (pos - loopB, juce::jmax (1.0, loopB - loopA));
                    if (rev && pos <= loopA)   pos = loopB - std::fmod (loopA - pos, juce::jmax (1.0, loopB - loopA));
                }
                if (pos < 0.0 || pos >= len - 1) { v.smpDone = true; break; }
                const float g = (sm0 + (smT - sm0) * (float) s * invN) * 0.9f;
                mixL[s] += read (L, pos) * g;
                mixR[s] += read (R, pos) * g;
                pos += inc;
            }
        }
    }

    // ---- filters ----
    FilterCoeffs cEnd[2], cStart[2];
    FilterCoeffs formantC[2][3]; float formantGain[2][3] {};
    float drives[2], mixes[2], combDelay[2], combFb[2], svfSat[2];
    for (int f = 0; f < 2; ++f)
    {
        const auto& fp = snap.filt[(size_t) f];
        const auto cutDest = f == 0 ? ModDest::Filter1Cutoff : ModDest::Filter2Cutoff;
        const auto resDest = f == 0 ? ModDest::Filter1Res : ModDest::Filter2Res;
        const auto drvDest = f == 0 ? ModDest::Filter1Drive : ModDest::Filter2Drive;
        double octaves = D (cutDest) * 6.0 + tone * 4.0 + fp.keytrack * (v.note - 60) / 12.0 + perf.brightOct;
        if (v2) octaves += cutDriftOct + (v.velocity - 0.8f) * 0.6f;   // organic wander + velocity brightness
        const float cutoff = juce::jlimit (16.0f, (float) (sampleRate * 0.45), (float) (fp.cutoff * std::pow (2.0, octaves)));
        const float res = juce::jlimit (0.0f, 1.0f, fp.res + D (resDest));
        drives[f] = juce::jlimit (0.0f, 1.0f, fp.drive + D (drvDest) + juce::jmax (0.0f, perf.drive) * 0.4f + character * 0.5f);
        mixes[f]  = juce::jlimit (0.0f, 1.0f, fp.mix);
        svfSat[f] = 0.3f + drives[f] * 1.2f;   // v2 in-loop saturation: more drive = less headroom = more growl
        cEnd[f]   = makeCoeffs (cutoff, res, sampleRate);
        cStart[f] = v.rampsValid ? v.lastCoeffs[(size_t) f] : cEnd[f];
        v.lastCoeffs[(size_t) f] = cEnd[f];
        combDelay[f] = (float) (sampleRate / cutoff);
        combFb[f] = res * 0.97f;
        if (fp.type == (int) FilterType::Formant)
        {
            // cutoff sweeps the vowel A-E-I-O-U (200 Hz .. 5 kHz), resonance narrows the formants
            static const float vowels[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 300, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            static const float gains[3] = { 1.0f, 0.6f, 0.35f };
            const float x = juce::jlimit (0.0f, 1.0f, std::log2 (cutoff / 200.0f) / std::log2 (25.0f)) * 4.0f;
            const int a = juce::jmin (3, (int) x); const float fr = x - (float) a;
            for (int k = 0; k < 3; ++k)
            {
                const float hz = vowels[a][k] + (vowels[a + 1][k] - vowels[a][k]) * fr;
                formantC[f][k] = makeCoeffs (hz, 0.55f + 0.42f * res, sampleRate);
                formantGain[f][k] = gains[k] * formantC[f][k].k * 1.6f;
            }
        }
    }

    auto runFilter = [&] (int f, int ch, float x, const FilterCoeffs& c) -> float
    {
        const int type = snap.filt[(size_t) f].type;
        if (type == (int) FilterType::Off) return x;
        const float in = v2 ? drive2 (x, drives[f]) : drive (x, drives[f]);
        float y = in;
        if (type == (int) FilterType::Ladder24)
            y = ladderTick (v.ladder[(size_t) f][(size_t) ch], c, in, v2 ? 0.55f : 0.3f);
        else if (type == (int) FilterType::Comb)
            y = v.comb[(size_t) f][(size_t) ch].tick (in, combDelay[f], combFb[f]);
        else if (type == (int) FilterType::CombNeg)
            y = v.comb[(size_t) f][(size_t) ch].tick (in, combDelay[f], -combFb[f]);
        else if (type == (int) FilterType::Formant)
        {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                float lp, bp, hp;
                svfTick (v.formant[(size_t) f][(size_t) ch][(size_t) k], formantC[f][k], in, lp, bp, hp);
                sum += bp * formantGain[f][k];
            }
            y = sum;
        }
        else
        {
            float lp, bp, hp;
            if (v2) svfTickNL (v.svf[(size_t) f][(size_t) ch], c, in, svfSat[f], lp, bp, hp);
            else    svfTick (v.svf[(size_t) f][(size_t) ch], c, in, lp, bp, hp);
            switch (type)
            {
                case (int) FilterType::LowPass12:  y = lp; break;
                case (int) FilterType::HighPass12: y = hp; break;
                case (int) FilterType::BandPass:   y = bp * c.k; break;
                case (int) FilterType::Notch:      y = in - c.k * bp; break;
                case (int) FilterType::Peak:       y = lp - hp; break;
                case (int) FilterType::LowPass24:
                case (int) FilterType::HighPass24:
                {
                    // second, Butterworth-damped stage: 24 dB/oct with a single resonant peak
                    FilterCoeffs c2 = c; c2.k = 1.414f;
                    c2.a1 = 1.0f / (1.0f + c2.g * (c2.g + c2.k)); c2.a2 = c2.g * c2.a1; c2.a3 = c2.g * c2.a2;
                    float lp2, bp2, hp2;
                    const float stage1 = type == (int) FilterType::LowPass24 ? lp : hp;
                    svfTick (v.svf2[(size_t) f][(size_t) ch], c2, stage1, lp2, bp2, hp2);
                    y = type == (int) FilterType::LowPass24 ? lp2 : hp2;
                    break;
                }
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
    float velGain = v2 ? 0.22f + 0.78f * std::pow (v.velocity, 1.25f) : 0.3f + 0.7f * v.velocity;
    velGain = 1.0f - juce::jlimit (0.0f, 1.0f, snap.velSens) * (1.0f - velGain);   // VEL sensitivity (100% = as before)
    const float punchAmt = punch * 1.4f, punchDecay = (float) std::exp (-1.0 / (0.022 * sampleRate));
    float punchEnv = punchAmt > 0.0f ? std::pow (punchDecay, (float) v.ageSamples) : 0.0f;
    const bool f1On = snap.filt[0].type != 0, f2On = snap.filt[1].type != 0;
    const bool parallel = snap.routing == (int) FilterRouting::Parallel && f1On && f2On;

    for (int s = 0; s < n; ++s)
    {
        const float t = (float) s * invN;
        FilterCoeffs c[2];
        for (int f = 0; f < 2; ++f)
        {
            const int ft = snap.filt[(size_t) f].type;
            if (ft == 0 || ft == (int) FilterType::Comb || ft == (int) FilterType::CombNeg || ft == (int) FilterType::Formant) continue;
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
        float amp = v.env[0].next() * velGain * ampMod * 0.5f;
        if (punchAmt > 0.0f) { amp *= 1.0f + punchAmt * punchEnv; punchEnv *= punchDecay; }   // PUNCH transient
        const float env4 = v.env[3].next(); juce::ignoreUnused (env4);

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
    v.ageSamples = juce::jmin (v.ageSamples + n, 1 << 30);
    if (! v.env[0].isActive())
        v.active = false;
}

} // namespace divinity
