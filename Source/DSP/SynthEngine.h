#pragma once

#include "DSPCore.h"
#include "Wavetables.h"
#include "SoulEngine.h"
#include "../Parameters.h"
#include <array>
#include <atomic>

namespace divinity
{
//==============================================================================
/** Plain-value snapshot of every engine parameter, refreshed once per control block. */
struct EngineSnapshot
{
    struct Osc   { bool on; int engine, table, wave, unison, oct, semi; float pos, fold, pw, detune, width, phase, fine, level; bool randPhase; };
    struct Filt  { int type; float cutoff, res, drive, mix, keytrack; };
    struct Env   { float a, d, s, r, curve; };
    struct LfoP  { int shape, div; float rate, depth, phase, smooth; bool sync; };
    struct Slot  { int src, dst; float amt; };

    std::array<Osc, kNumOscs> osc {};
    bool subOn = false; int subMode = 0; float subLevel = 0;
    bool noiseOn = false; int noiseType = 0; float noiseLevel = 0;
    std::array<Filt, 2> filt {};
    int routing = 0;
    std::array<Env, kNumEnvs> env {};
    std::array<LfoP, kNumLfos> lfo {};
    std::array<Slot, kNumModSlots> mod {};
    std::array<float, kNumMacros> macro {};
    int maxVoices = 8, bendRange = 2, voiceMode = 0;
    float glideMs = 0, masterTune = 0;
};

/** Holds the APVTS atomics and produces snapshots without locks or allocation. */
class EngineParamRefs
{
public:
    explicit EngineParamRefs (juce::AudioProcessorValueTreeState& apvts);
    void read (EngineSnapshot& s) const noexcept;

private:
    struct OscRefs { std::atomic<float>* p[16]; };
    std::array<OscRefs, kNumOscs> osc;
    std::atomic<float>* sub[3]; std::atomic<float>* noise[3];
    std::array<std::array<std::atomic<float>*, 6>, 2> filt;
    std::atomic<float>* routing;
    std::array<std::array<std::atomic<float>*, 5>, kNumEnvs> env;
    std::array<std::array<std::atomic<float>*, 7>, kNumLfos> lfo;
    std::array<std::array<std::atomic<float>*, 3>, kNumModSlots> mod;
    std::array<std::atomic<float>*, kNumMacros> macro;
    std::atomic<float>* global[5];
};

/** Host transport info needed by the engine. */
struct TransportInfo
{
    double bpm = 120.0;
    double ppqAtBlockStart = 0.0;
    bool   isPlaying = false;
    bool   hasPpq = false;
};

//==============================================================================
class SynthEngine
{
public:
    static constexpr int kControlBlock = 32;

    SynthEngine (juce::AudioProcessorValueTreeState& apvts, const WavetableBank& bank);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Renders (replaces) the stereo synth output for this block and consumes the MIDI. */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TransportInfo& transport);

    void allNotesOff (bool immediate);

    int  getActiveVoiceCount() const noexcept { return activeVoiceCount.load(); }
    float getLfoValue (int i) const noexcept  { return lfoDisplay[(size_t) i].load(); }

    /** Global values of the post-voice (FX) destinations computed for the last block. */
    struct GlobalMods { float satDrive = 0, chorusDepth = 0, delayMix = 0, reverbMix = 0, reverbSize = 0, width = 0; };
    GlobalMods getGlobalMods() const noexcept { return globalMods; }
    const EngineSnapshot& getSnapshot() const noexcept { return snap; }
    const SoulEngine& getSoul() const noexcept { return soul; }

private:
    //--------------------------------------------------------------------------
    struct Voice
    {
        bool active = false, keyDown = false, sustained = false;
        int note = 60;
        float velocity = 1.0f, aftertouch = 0.0f;
        uint64_t order = 0;
        double pitch = 60.0, targetPitch = 60.0;

        std::array<dsp::Envelope, kNumEnvs> env;
        std::array<std::array<double, kMaxUnison>, kNumOscs> phase {};
        double subPhase = 0.0;
        dsp::NoiseGen noiseL, noiseR;
        dsp::Rng rng;

        std::array<std::array<dsp::SvfState, 2>, 2> svf;
        std::array<std::array<dsp::LadderState, 2>, 2> ladder;
        std::array<std::array<dsp::CombState, 2>, 2> comb;

        // ramp memory (previous control-block values) for zipper-free modulation
        std::array<float, kNumOscs> lastLevel {}, lastPos {}, lastFold {};
        std::array<dsp::FilterCoeffs, 2> lastCoeffs {};
        float lastPanL = 1, lastPanR = 1, lastSub = 0, lastNoise = 0;
        float drift = 0, driftTarget = 0, driftTimer = 0;   // SOUL instability state
        bool  rampsValid = false;
    };

    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void startVoice (Voice& v, int note, float velocity, bool retriggerFromScratch);
    void releaseVoice (Voice& v);
    Voice* findVoiceToSteal();

    void renderChunk (float* outL, float* outR, int numSamples, const TransportInfo& transport, int sampleOffset);
    void renderVoice (Voice& v, float* outL, float* outR, int n, const float* globalSrc);
    void computeVoiceMods (const Voice& v, const float* globalSrc, float* dest) const noexcept;

    //--------------------------------------------------------------------------
    EngineParamRefs refs;
    SoulEngine soul;
    PerformanceMods perf;
    const WavetableBank& bank;
    EngineSnapshot snap;

    double sampleRate = 44100.0;
    std::array<Voice, kMaxVoices> voices;
    Voice silentVoice;   // stands in for global mods when no voice sounds
    uint64_t noteCounter = 0;
    double lastNotePitch = -1.0;

    std::vector<int> heldNotes;     // legato note stack (pre-allocated)
    bool sustainPedal = false;
    float modWheel = 0, channelPressure = 0, pitchBend = 0;   // pitchBend -1..1

    std::array<dsp::Lfo, kNumLfos> lfos;
    std::array<float, kNumLfos> lfoValues {};
    std::array<float, (size_t) ModDest::NumDests> lastGlobalDest {};
    std::array<std::atomic<float>, kNumLfos> lfoDisplay {};
    std::atomic<int> activeVoiceCount { 0 };
    GlobalMods globalMods;
    double samplesElapsed = 0.0;

    std::vector<float> scratchL, scratchR;
    float darkNoiseCoeff = 0.05f;
};

} // namespace divinity
