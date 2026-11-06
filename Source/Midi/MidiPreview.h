#pragma once

#include "MidiGenerator.h"
#include "../DSP/Sampler.h"
#include <array>
#include <memory>

namespace divinity
{
//==============================================================================
/**
    Plays a generated MIDI clip in a loop so it can be auditioned inside DIVINITY.
    Melodic parts are injected as MIDI into the synth; DRUMS trigger the built-in
    drum one-shots (kick, snare, clap, hats, rim, perc) mixed into the output.

    Clips are handed over from the message thread under a SpinLock that the audio
    thread only ever try-locks; the old clip is freed on the message thread.
*/
class MidiPreview
{
public:
    /** Message thread. */
    void play (const midigen::Clip& clip)
    {
        auto c = std::make_unique<midigen::Clip> (clip);
        const juce::SpinLock::ScopedLockType sl (lock);
        pending = std::move (c);   // frees any clip the audio thread already swapped out
        hasPending = true;
        playing.store (true);
    }
    void stop() { playing.store (false); }
    bool isPlaying() const noexcept { return playing.load(); }
    /** Position inside the loop (beats), for the UI playhead. */
    double getPosition() const noexcept { return uiPos.load(); }

    /** Audio thread, before the synth renders: adds note events for melodic clips to `midi`
        and schedules drum hits (rendered later by renderDrums). */
    void process (juce::MidiBuffer& midi, int n, double sampleRate, double bpm, const FactorySamples& samples)
    {
        {
            const juce::SpinLock::ScopedTryLockType tl (lock);
            if (tl.isLocked() && hasPending)
            {
                std::swap (current, pending);
                hasPending = false;
                releaseNotes (midi);
                pos = 0.0;
            }
        }
        const bool want = playing.load() && current != nullptr;
        if (! want)
        {
            if (wasPlaying) { releaseNotes (midi); wasPlaying = false; }
            return;
        }
        wasPlaying = true;
        const auto& clip = *current;
        const double beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
        const double blockBeats = n * beatsPerSample;
        const bool drumsPart = clip.part == midigen::Part::Drums;

        double a = pos, b = pos + blockBeats;
        auto emitRange = [&] (double from, double to, double beatOffset)
        {
            for (auto& note : clip.notes)
            {
                const double on = note.start, off = note.start + note.length;
                if (on >= from && on < to)
                {
                    const int s = juce::jlimit (0, n - 1, (int) ((on - from + beatOffset) / beatsPerSample));
                    if (drumsPart) triggerDrum (note.pitch, note.velocity, s, samples, sampleRate);
                    else { midi.addEvent (juce::MidiMessage::noteOn (1, note.pitch, note.velocity), s); held[(size_t) note.pitch] = true; }
                }
                if (! drumsPart && off >= from && off < to)
                {
                    const int s = juce::jlimit (0, n - 1, (int) ((off - from + beatOffset) / beatsPerSample));
                    midi.addEvent (juce::MidiMessage::noteOff (1, note.pitch), s);
                    held[(size_t) note.pitch] = false;
                }
            }
        };
        if (b <= clip.lengthBeats) emitRange (a, b, 0.0);
        else
        {
            emitRange (a, clip.lengthBeats + 1.0e-9, 0.0);
            const double wrapAt = clip.lengthBeats - a;
            // loop: release anything still sounding, start again from the top
            const int s = juce::jlimit (0, n - 1, (int) (wrapAt / beatsPerSample));
            for (int k = 0; k < 128; ++k) if (held[(size_t) k]) { midi.addEvent (juce::MidiMessage::noteOff (1, k), s); held[(size_t) k] = false; }
            b -= clip.lengthBeats;
            emitRange (0.0, b, wrapAt);
        }
        pos = b;
        uiPos.store (pos);
    }

    /** Audio thread, after the effects: mixes the drum one-shots into the output. */
    void renderDrums (juce::AudioBuffer<float>& out) { renderDrums (out, 0, out.getNumSamples()); }

private:
    void releaseNotes (juce::MidiBuffer& midi)
    {
        for (int k = 0; k < 128; ++k) if (held[(size_t) k]) { midi.addEvent (juce::MidiMessage::noteOff (1, k), 0); held[(size_t) k] = false; }
    }

    struct DrumVoice { const SampleData* s = nullptr; double pos = 0.0, step = 1.0; float gain = 0.0f; int delay = 0; };

    static int sampleForDrum (int note)
    {
        // indices into samplerSampleNames()
        switch (note)
        {
            case midigen::drums::kick:      return 8;   // Kick Thump
            case midigen::drums::snare:     return 4;   // Snap
            case midigen::drums::clap:      return 3;   // Clap
            case midigen::drums::rim:       return 5;   // Rim
            case midigen::drums::hatOpen:   return 1;   // Hat Open
            case midigen::drums::hatPedal:  return 2;   // Hat Metal
            case midigen::drums::perc:      return 7;   // Perc Tick
            default:                        return 0;   // Hat Closed
        }
    }

    void triggerDrum (int note, float vel, int offset, const FactorySamples& samples, double sr)
    {
        auto* s = samples.get (sampleForDrum (note));
        if (s == nullptr || s->length() == 0) return;
        if (note == midigen::drums::hatClosed || note == midigen::drums::hatPedal)   // a new closed hat chokes the open one
            for (auto& v : voices) if (v.s == samples.get (1)) v.gain *= 0.2f;
        auto& v = voices[(size_t) (nextVoice++ % voices.size())];
        v.s = s; v.pos = 0.0; v.step = s->sampleRate / sr; v.gain = vel * vel * (note == midigen::drums::kick ? 0.9f : 0.6f); v.delay = offset;
    }

    void renderDrums (juce::AudioBuffer<float>& out, int start, int n)
    {
        const int chans = out.getNumChannels();
        for (auto& v : voices)
        {
            if (v.s == nullptr) continue;
            const int len = v.s->length();
            for (int i = start + v.delay; i < start + n; ++i)
            {
                const int i0 = (int) v.pos;
                if (i0 + 1 >= len) { v.s = nullptr; break; }
                const float fr = (float) (v.pos - i0);
                const float l = v.s->left[(size_t) i0] + fr * (v.s->left[(size_t) i0 + 1] - v.s->left[(size_t) i0]);
                const auto& rr = v.s->right.empty() ? v.s->left : v.s->right;
                const float r = rr[(size_t) i0] + fr * (rr[(size_t) i0 + 1] - rr[(size_t) i0]);
                out.addSample (0, i, l * v.gain);
                if (chans > 1) out.addSample (1, i, r * v.gain);
                v.pos += v.step;
            }
            v.delay = 0;
        }
    }

    juce::SpinLock lock;
    std::unique_ptr<midigen::Clip> pending, current;
    bool hasPending = false, wasPlaying = false;
    std::atomic<bool> playing { false };
    std::atomic<double> uiPos { 0.0 };
    double pos = 0.0;
    std::array<bool, 128> held {};
    std::array<DrumVoice, 16> voices {};
    unsigned nextVoice = 0;
};

} // namespace divinity
