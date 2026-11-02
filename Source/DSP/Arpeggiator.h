#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <climits>
#include "SynthEngine.h"

namespace divinity
{
namespace arpids
{
    inline const juce::String on = "arp_on", mode = "arp_mode", rate = "arp_rate", octaves = "arp_octaves",
                              gate = "arp_gate", swing = "arp_swing", latch = "arp_latch";
}

const juce::StringArray& arpModeNames();   // Up, Down, Up/Down, Down/Up, Random, As Played, Chord
const juce::StringArray& arpRateNames();   // 1/1 ... 1/32, triplets, dotted
double arpRateBeats (int rateIndex);         // length of one step in quarter-note beats

void addArpParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
juce::String arpHelpText (const juce::String& id);

//==============================================================================
/**
    MIDI arpeggiator that sits in front of the synth engine. It consumes incoming
    note-ons/offs and replaces them with a stepped pattern. Follows the host's
    beat grid while the transport plays, otherwise free-runs at the host (or
    120) BPM starting on the first held key. Real-time safe: fixed-size storage,
    no allocation in process().
*/
class Arpeggiator
{
public:
    explicit Arpeggiator (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate);
    void reset();
    /** Rewrites `midi` in place for this block. */
    void process (juce::MidiBuffer& midi, int numSamples, const TransportInfo& transport) noexcept;

    /** For the UI: the note currently sounding (-1 when silent) and how many keys feed the pattern. */
    std::atomic<int> displayNote { -1 }, displayCount { 0 }, displayStep { 0 };

private:
    struct Held { int note; float vel; };
    static constexpr int kMaxHeld = 32, kMaxSounding = 128;

    void noteDown (int note, float vel);
    void noteUp (int note);
    void removeFromPattern (int note);
    void fireStep (juce::MidiBuffer& out, int sample);
    void releaseSounding (juce::MidiBuffer& out, int sample);
    int  buildSequence (int* outNotes, float* outVels) const;   // returns length for non-chord modes

    std::atomic<float> *pOn, *pMode, *pRate, *pOct, *pGate, *pSwing, *pLatch;

    std::array<Held, kMaxHeld> physical {}, pattern {};
    int numPhysical = 0, numPattern = 0;
    std::array<int, kMaxSounding> sounding {};
    int numSounding = 0;

    double sampleRate = 44100.0;
    double freePos = 0.0;        // beats, free-running clock
    double offAtBeat = -1.0;     // when the sounding step should end (absolute beat on the active clock)
    long long lastStep = LLONG_MIN;
    long long stepCounter = 0;
    int lastRandom = -1;
    bool wasOn = false, wasLatched = false, wasHostSynced = false;
    juce::Random rng { 777 };
    juce::MidiBuffer out;
};

} // namespace divinity
