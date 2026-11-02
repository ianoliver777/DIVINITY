#include "Arpeggiator.h"
#include <map>

namespace divinity
{
//==============================================================================
const juce::StringArray& arpModeNames()
{
    static const juce::StringArray n { "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord" };
    return n;
}

const juce::StringArray& arpRateNames()
{
    static const juce::StringArray n { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                       "1/4T", "1/8T", "1/16T", "1/4D", "1/8D", "1/16D" };
    return n;
}

double arpRateBeats (int i)
{
    static const double beats[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                  2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75, 0.375 };
    return beats[juce::jlimit (0, 11, i)];
}

void addArpParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace arpids;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
    auto pct = juce::AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
                   .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; });
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("arp", "Arpeggiator", " | ");
    g->addChild (std::make_unique<juce::AudioParameterBool> (pid (on), "Arp On", false));
    g->addChild (std::make_unique<juce::AudioParameterChoice> (pid (mode), "Arp Mode", arpModeNames(), 0));
    g->addChild (std::make_unique<juce::AudioParameterChoice> (pid (rate), "Arp Rate", arpRateNames(), 4));
    g->addChild (std::make_unique<juce::AudioParameterInt> (pid (octaves), "Arp Octaves", 1, 4, 1));
    g->addChild (std::make_unique<juce::AudioParameterFloat> (pid (gate), "Arp Gate", juce::NormalisableRange<float> (0.05f, 1.0f), 0.6f, pct));
    g->addChild (std::make_unique<juce::AudioParameterFloat> (pid (swing), "Arp Swing", juce::NormalisableRange<float> (0.0f, 0.75f), 0.0f, pct));
    g->addChild (std::make_unique<juce::AudioParameterBool> (pid (latch), "Arp Latch", false));
    layout.add (std::move (g));
}

juce::String arpHelpText (const juce::String& id)
{
    using namespace arpids;
    static const std::map<juce::String, const char*> help {
        { on, "Arpeggiator: plays the keys you hold as a stepped pattern, locked to your project tempo." },
        { mode, "Arp Mode: Up, Down, Up/Down, Down/Up, Random, As Played (the order you pressed the keys) or Chord (all keys together, rhythmically)." },
        { rate, "Arp Rate: step length. T = triplet, D = dotted." },
        { octaves, "Arp Octaves: repeats the pattern across 1 to 4 octaves." },
        { gate, "Arp Gate: how long each step holds. Short = staccato, 100% = tied." },
        { swing, "Arp Swing: delays every second step for a shuffled groove." },
        { latch, "Arp Latch: the pattern keeps playing after you let go. Press new keys to replace it." },
    };
    auto it = help.find (id);
    return it != help.end() ? juce::String (it->second) : juce::String();
}

//==============================================================================
Arpeggiator::Arpeggiator (juce::AudioProcessorValueTreeState& s)
{
    using namespace arpids;
    pOn = s.getRawParameterValue (on); pMode = s.getRawParameterValue (mode); pRate = s.getRawParameterValue (rate);
    pOct = s.getRawParameterValue (octaves); pGate = s.getRawParameterValue (gate);
    pSwing = s.getRawParameterValue (swing); pLatch = s.getRawParameterValue (latch);
    out.ensureSize (8192);
}

void Arpeggiator::prepare (double sr) { sampleRate = sr; reset(); }

void Arpeggiator::reset()
{
    numPhysical = numPattern = numSounding = 0;
    freePos = 0.0; offAtBeat = -1.0; lastStep = LLONG_MIN; stepCounter = 0; lastRandom = -1;
    displayNote = -1; displayCount = 0;
}

void Arpeggiator::noteDown (int note, float vel)
{
    for (int i = 0; i < numPhysical; ++i) if (physical[(size_t) i].note == note) { physical[(size_t) i].vel = vel; goto pattern_add; }
    if (numPhysical < kMaxHeld) physical[(size_t) numPhysical++] = { note, vel };
pattern_add:
    for (int i = 0; i < numPattern; ++i) if (pattern[(size_t) i].note == note) { pattern[(size_t) i].vel = vel; return; }
    if (numPattern < kMaxHeld) pattern[(size_t) numPattern++] = { note, vel };
}

void Arpeggiator::removeFromPattern (int note)
{
    for (int i = 0; i < numPattern; ++i)
        if (pattern[(size_t) i].note == note)
        {
            for (int k = i; k < numPattern - 1; ++k) pattern[(size_t) k] = pattern[(size_t) k + 1];
            --numPattern;
            return;
        }
}

void Arpeggiator::noteUp (int note)
{
    for (int i = 0; i < numPhysical; ++i)
        if (physical[(size_t) i].note == note)
        {
            for (int k = i; k < numPhysical - 1; ++k) physical[(size_t) k] = physical[(size_t) k + 1];
            --numPhysical;
            break;
        }
    if (pLatch->load() < 0.5f) removeFromPattern (note);
}

int Arpeggiator::buildSequence (int* notes, float* vels) const
{
    const int mode = juce::roundToInt (pMode->load());
    const int octs = juce::jlimit (1, 4, juce::roundToInt (pOct->load()));
    std::array<Held, kMaxHeld> base {};
    std::copy (pattern.begin(), pattern.begin() + numPattern, base.begin());
    if (mode != 5)   // everything except "As Played" works on pitch order
        std::sort (base.begin(), base.begin() + numPattern, [] (const Held& a, const Held& b) { return a.note < b.note; });

    int n = 0;
    for (int o = 0; o < octs; ++o)
        for (int i = 0; i < numPattern; ++i)
        {
            const int nt = base[(size_t) i].note + 12 * o;
            if (nt > 127) continue;
            notes[n] = nt; vels[n] = base[(size_t) i].vel; ++n;
        }
    if (n == 0) return 0;

    auto reverse = [&] { std::reverse (notes, notes + n); std::reverse (vels, vels + n); };
    if (mode == 1 || mode == 3) reverse();                     // Down, Down/Up start at the top
    if ((mode == 2 || mode == 3) && n > 2)                     // bounce without repeating the end points
    {
        const int m = n;
        for (int i = m - 2; i >= 1; --i) { notes[n] = notes[i]; vels[n] = vels[i]; ++n; }
    }
    return n;
}

void Arpeggiator::releaseSounding (juce::MidiBuffer& o, int sample)
{
    for (int i = 0; i < numSounding; ++i) o.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) i]), sample);
    numSounding = 0;
    displayNote = -1;
}

void Arpeggiator::fireStep (juce::MidiBuffer& o, int sample)
{
    releaseSounding (o, sample);
    if (numPattern == 0) return;
    const int mode = juce::roundToInt (pMode->load());
    const int octs = juce::jlimit (1, 4, juce::roundToInt (pOct->load()));

    if (mode == 6)   // Chord: every key at once, climbing through the octaves
    {
        const int oct = (int) (stepCounter % octs);
        for (int i = 0; i < numPattern && numSounding < kMaxSounding; ++i)
        {
            const int nt = pattern[(size_t) i].note + 12 * oct;
            if (nt > 127) continue;
            o.addEvent (juce::MidiMessage::noteOn (1, nt, pattern[(size_t) i].vel), sample);
            sounding[(size_t) numSounding++] = nt;
        }
        displayNote = numSounding > 0 ? sounding[0] : -1;
    }
    else
    {
        int notes[kMaxHeld * 4 * 2]; float vels[kMaxHeld * 4 * 2];
        const int len = buildSequence (notes, vels);
        if (len == 0) return;
        int idx;
        if (mode == 4)   // Random, never the same note twice in a row
        {
            idx = rng.nextInt (len);
            if (len > 1 && idx == lastRandom) idx = (idx + 1 + rng.nextInt (len - 1)) % len;
            lastRandom = idx;
        }
        else
            idx = (int) (stepCounter % len);
        o.addEvent (juce::MidiMessage::noteOn (1, notes[idx], vels[idx]), sample);
        sounding[(size_t) numSounding++] = notes[idx];
        displayNote = notes[idx];
    }
    displayStep = (int) (stepCounter & 0x7fffffff);
    ++stepCounter;
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, const TransportInfo& transport) noexcept
{
    const bool on = pOn->load() > 0.5f;
    const bool latched = pLatch->load() > 0.5f;
    out.clear();

    if (! on)
    {
        if (wasOn)
        {
            // hand the keys still held back to the synth so nothing goes silent or hangs
            releaseSounding (out, 0);
            for (int i = 0; i < numPhysical; ++i)
                out.addEvent (juce::MidiMessage::noteOn (1, physical[(size_t) i].note, physical[(size_t) i].vel), 0);
            numPattern = 0; lastStep = LLONG_MIN;
            displayCount = 0;
        }
        wasOn = false;
        // keep tracking physical keys so switching on mid-chord works
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) noteDown (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff()) { noteUp (msg.getNoteNumber()); removeFromPattern (msg.getNoteNumber()); }
            out.addEvent (msg, m.samplePosition);
        }
        numPattern = 0;
        midi.swapWith (out);
        return;
    }

    if (! wasOn)
    {
        // switching on: silence keys the synth is holding directly, they now feed the pattern
        for (int i = 0; i < numPhysical; ++i) out.addEvent (juce::MidiMessage::noteOff (1, physical[(size_t) i].note), 0);
        numPattern = 0;
        for (int i = 0; i < numPhysical; ++i) pattern[(size_t) numPattern++] = physical[(size_t) i];
        lastStep = LLONG_MIN; stepCounter = 0; freePos = 0.0; offAtBeat = -1.0;
    }
    if (wasLatched && ! latched)
    {
        // latch released: keep only keys that are still down
        numPattern = 0;
        for (int i = 0; i < numPhysical; ++i) pattern[(size_t) numPattern++] = physical[(size_t) i];
    }
    wasOn = true; wasLatched = latched;

    const double bpm = juce::jlimit (20.0, 999.0, transport.bpm > 0.0 ? transport.bpm : 120.0);
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const bool hostSync = transport.isPlaying && transport.hasPpq;
    if (hostSync != wasHostSynced) { lastStep = LLONG_MIN; offAtBeat = -1.0; wasHostSynced = hostSync; }
    const double L = arpRateBeats (juce::roundToInt (pRate->load()));
    const double swingOff = (double) pSwing->load() * 0.5 * L;
    const double gate = (double) pGate->load();

    auto it = midi.begin();
    for (int s = 0; s < numSamples; ++s)
    {
        const double pos = hostSync ? transport.ppqAtBlockStart + s * beatsPerSample : freePos;
        const int countBefore = numPattern;

        // incoming events at this sample
        for (; it != midi.end() && (*it).samplePosition <= s; ++it)
        {
            const auto msg = (*it).getMessage();
            if (msg.isNoteOn())
            {
                if (latched && numPhysical == 0) numPattern = 0;   // new chord replaces the latched one
                noteDown (msg.getNoteNumber(), msg.getFloatVelocity());
            }
            else if (msg.isNoteOff()) noteUp (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { numPattern = numPhysical = 0; releaseSounding (out, s); out.addEvent (msg, s); }
            else out.addEvent (msg, (*it).samplePosition);
        }

        if (countBefore == 0 && numPattern > 0)
        {
            stepCounter = 0; lastRandom = -1;
            if (hostSync)
            {
                // start on the grid: immediately if we're right on a step, otherwise at the next one
                const long long k = (long long) std::floor (pos / L);
                const double into = pos - (double) k * L;
                lastStep = into < L * 0.125 ? k - 1 : k;
            }
            else { freePos = 0.0; lastStep = LLONG_MIN; }
        }

        const double p = hostSync ? pos : freePos;
        if (numSounding > 0 && offAtBeat >= 0.0 && p >= offAtBeat) { releaseSounding (out, s); offAtBeat = -1.0; }
        if (numPattern == 0 && numSounding > 0) { releaseSounding (out, s); offAtBeat = -1.0; }

        if (numPattern > 0)
        {
            const long long k = (long long) std::floor (p / L);
            const double start = (double) k * L + ((k & 1) ? swingOff : 0.0);
            if (k != lastStep && p >= start)
            {
                lastStep = k;
                fireStep (out, s);
                offAtBeat = gate >= 0.999 ? -1.0 : start + gate * L * ((k & 1) ? (1.0 - swingOff / L) : 1.0);
            }
        }
        if (! hostSync) freePos += beatsPerSample;
    }
    // events stamped past the block end (shouldn't happen, but never drop them)
    for (; it != midi.end(); ++it) out.addEvent ((*it).getMessage(), juce::jmin ((*it).samplePosition, numSamples - 1));

    displayCount = numPattern;
    midi.swapWith (out);
}

} // namespace divinity
