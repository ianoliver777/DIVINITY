#include "StepSequencer.h"

namespace divinity
{
StepSequencer::StepSequencer (juce::AudioProcessorValueTreeState& s)
{
    pOn = s.getRawParameterValue (ids::stepOn);
    pMode = s.getRawParameterValue (ids::stepMode);
    pRate = s.getRawParameterValue (ids::stepRate);
    pLen = s.getRawParameterValue (ids::stepLength);
    pSwing = s.getRawParameterValue (ids::stepSwing);
    for (int i = 0; i < kNumSteps; ++i)
    {
        pLvl[(size_t) i] = s.getRawParameterValue (ids::step (i, "lvl"));
        pRat[(size_t) i] = s.getRawParameterValue (ids::step (i, "rat"));
        pGate[(size_t) i] = s.getRawParameterValue (ids::step (i, "len"));
        pPitch[(size_t) i] = s.getRawParameterValue (ids::step (i, "pitch"));
    }
    out.ensureSize (8192);
}

void StepSequencer::prepare (double sr) { sampleRate = sr; reset(); }

void StepSequencer::reset()
{
    numHeld = numSounding = 0;
    freeBeats = 0.0; lastEventBeat = -1.0e9;
    wasActive = false; wasHostSynced = false;
    displayStep = -1;
}

bool StepSequencer::isTriggering() const noexcept
{
    return pOn->load() > 0.5f && (int) std::lround (pMode->load()) == (int) StepMode::Trigger;
}

void StepSequencer::allOff (juce::MidiBuffer& o, int sample)
{
    for (int i = 0; i < numSounding; ++i)
        o.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) i].note), sample);
    numSounding = 0;
}

void StepSequencer::process (juce::MidiBuffer& midi, int numSamples, const TransportInfo& tr) noexcept
{
    const bool active = isTriggering();
    if (! active)
    {
        if (wasActive)
        {
            // hand control back cleanly: stop our notes, forget held keys (the arp / synth takes over)
            out.clear();
            allOff (out, 0);
            for (const auto m : midi) out.addEvent (m.getMessage(), m.samplePosition);
            midi.swapWith (out);
            numHeld = 0;
        }
        wasActive = false;
        displayStep = -1;
        return;
    }
    wasActive = true;

    const double bpm = juce::jmax (20.0, tr.bpm);
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const bool hostSynced = tr.isPlaying && tr.hasPpq;
    if (hostSynced != wasHostSynced) { lastEventBeat = -1.0e9; }
    wasHostSynced = hostSynced;

    const double stepBeats = stepRateBeats ((int) std::lround (pRate->load()));
    const int len = juce::jlimit (1, kNumSteps, (int) std::lround (pLen->load()));
    const double swing = juce::jlimit (0.0f, 0.75f, pSwing->load()) * 0.66;
    auto stepStart = [stepBeats, swing] (long long k) { return (double) k * stepBeats + ((k & 1) ? swing * stepBeats : 0.0); };

    out.clear();
    double beat = hostSynced ? tr.ppqAtBlockStart : freeBeats;
    if (hostSynced && beat < lastEventBeat - 1.0e-6) lastEventBeat = -1.0e9;   // host looped / jumped back
    int cursor = 0;

    // emits everything due in [cursor, end) samples
    auto runTo = [&] (int end)
    {
        if (end <= cursor) return;
        const double b0 = beat, b1 = beat + (end - cursor) * beatsPerSample;
        auto toSample = [&] (double b) { return juce::jlimit (cursor, end - 1, cursor + (int) std::floor ((b - b0) / beatsPerSample)); };

        // note-offs first (so a retrigger of the same note in this block is clean)
        for (int i = 0; i < numSounding;)
        {
            if (sounding[(size_t) i].offBeat < b1)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) i].note), toSample (sounding[(size_t) i].offBeat));
                sounding[(size_t) i] = sounding[(size_t) --numSounding];
            }
            else ++i;
        }

        if (numHeld > 0)
        {
            const long long kFirst = (long long) std::floor (b0 / stepBeats) - 1, kLast = (long long) std::floor (b1 / stepBeats) + 1;
            for (long long k = kFirst; k <= kLast; ++k)
            {
                const int si = (int) (((k % len) + len) % len);
                const double st = stepStart (k), dur = stepStart (k + 1) - st;
                const float lvl = pLvl[(size_t) si]->load();
                if (lvl < 0.01f) continue;
                const int rat = juce::jlimit (1, 4, (int) std::lround (pRat[(size_t) si]->load()));
                const double sub = dur / rat;
                const double gate = juce::jlimit (0.05, 1.0, (double) pGate[(size_t) si]->load()) * sub;
                const int pitch = (int) std::lround (pPitch[(size_t) si]->load());
                for (int r = 0; r < rat; ++r)
                {
                    const double t = st + r * sub;
                    if (t < b0 || t >= b1 || t <= lastEventBeat + 1.0e-9) continue;
                    lastEventBeat = t;
                    const int pos = toSample (t);
                    displayStep = si;
                    for (int h = 0; h < numHeld; ++h)
                    {
                        const int note = juce::jlimit (0, 127, held[(size_t) h].note + pitch);
                        for (int i = 0; i < numSounding;)   // retrigger: end the previous copy first
                            if (sounding[(size_t) i].note == note)
                            {
                                out.addEvent (juce::MidiMessage::noteOff (1, note), pos);
                                sounding[(size_t) i] = sounding[(size_t) --numSounding];
                            }
                            else ++i;
                        // ratchet repeats fade a little, like a rolled hat
                        const float vel = juce::jlimit (0.05f, 1.0f, held[(size_t) h].vel * lvl * (1.0f - 0.12f * (float) r));
                        out.addEvent (juce::MidiMessage::noteOn (1, note, vel), pos);
                        if (numSounding < kMaxSounding) sounding[(size_t) numSounding++] = { note, t + gate };
                    }
                }
            }
        }
        beat = b1;
        cursor = end;
    };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, numSamples, meta.samplePosition);
        runTo (pos);
        if (m.isNoteOn())
        {
            if (numHeld == 0 && ! hostSynced)
            {
                // free-running: the pattern starts on the first key
                beat = 0.0; lastEventBeat = -1.0e9;
            }
            bool found = false;
            for (int h = 0; h < numHeld; ++h) if (held[(size_t) h].note == m.getNoteNumber()) { held[(size_t) h].vel = m.getFloatVelocity(); found = true; }
            if (! found && numHeld < kMaxHeld) held[(size_t) numHeld++] = { m.getNoteNumber(), m.getFloatVelocity() };
        }
        else if (m.isNoteOff())
        {
            for (int h = 0; h < numHeld; ++h)
                if (held[(size_t) h].note == m.getNoteNumber()) { held[(size_t) h] = held[(size_t) --numHeld]; break; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            numHeld = 0;
            allOff (out, pos);
            out.addEvent (m, pos);
        }
        else out.addEvent (m, pos);
    }
    runTo (numSamples);
    if (! hostSynced) freeBeats = beat;
    if (numHeld == 0 && numSounding == 0) displayStep = -1;
    midi.swapWith (out);
}

} // namespace divinity
