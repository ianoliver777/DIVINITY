#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthEngine.h"

namespace divinity
{
//==============================================================================
/**
    TRIGGER mode of the STEP sequencer: plays the held keys as a 16-step rhythm
    (per-step velocity, ratchets for trap rolls, gate length, pitch offset).
    Locked to the host grid while the transport plays; otherwise it free-runs
    at the host tempo from the first key. Real-time safe (fixed storage).
    MODULATE mode is evaluated inside the synth engine (STEP mod source).
*/
class StepSequencer
{
public:
    explicit StepSequencer (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate);
    void reset();

    /** True when TRIGGER mode is on (the processor then skips the arpeggiator). */
    bool isTriggering() const noexcept;

    /** Rewrites `midi` in place for this block. */
    void process (juce::MidiBuffer& midi, int numSamples, const TransportInfo& transport) noexcept;

    /** For the UI: the step that last fired (-1 = idle). */
    std::atomic<int> displayStep { -1 };

private:
    struct Held { int note; float vel; };
    struct Sounding { int note; double offBeat; };
    static constexpr int kMaxHeld = 16, kMaxSounding = 256;

    void allOff (juce::MidiBuffer& out, int sample);

    std::atomic<float> *pOn, *pMode, *pRate, *pLen, *pSwing;
    std::array<std::atomic<float>*, kNumSteps> pLvl {}, pRat {}, pGate {}, pPitch {};

    std::array<Held, kMaxHeld> held {};
    int numHeld = 0;
    std::array<Sounding, kMaxSounding> sounding {};
    int numSounding = 0;

    double sampleRate = 44100.0, freeBeats = 0.0, lastEventBeat = -1.0e9;
    bool wasActive = false, wasHostSynced = false;
    juce::MidiBuffer out;
};

} // namespace divinity
