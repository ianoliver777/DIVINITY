#pragma once

#include "DSPCore.h"
#include "Wavetables.h"
#include "SoulEngine.h"
#include "Sampler.h"
#include "../Parameters.h"
#include <array>
#include <atomic>

namespace divinity
{
//==============================================================================
/** Plain-value snapshot of every engine parameter, refreshed once per control block. */
struct EngineSnapshot
{
    struct Osc   { bool on; int engine, table, wave, unison, oct, semi; float pos, fold, pw, detune, width, phase, fine, level; bool randPhase; float blend;
                   int warp, warpSrc; float warpAmt; };   // warp: 0.8.0
    struct Filt  { int type; float cutoff, res, drive, mix, keytrack; };
    struct Env   { float a, d, s, r, curve; };
    struct LfoP  { int shape, div; float rate, depth, phase, smooth; bool sync, on; };
    struct Step  { float lvl; };
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
    bool v2 = true;          // Divine 2.0 engine (false = Classic 1.0, bit-compatible with v0.5)
    float analog = 0.3f;     // organic drift amount (v2)

    // 0.7.0
    float velSens = 1.0f;
    float rndRate = 2.0f, rndSmooth = 0.5f; bool rndSync = false; int rndDiv = 5;
    bool stepOn = false; int stepMode = 0, stepRate = 3, stepLength = 16; float stepSwing = 0, stepSmooth = 0;
    std::array<float, kNumSteps> stepLvl {};
    bool smpOn = false; int smpSample = 0, smpTune = 0; float smpLevel = 0.75f, smpFine = 0, smpStart = 0, smpLoopStart = 0.1f, smpLoopEnd = 1.0f;
    bool smpLoop = false, smpReverse = false, smpKeytrack = true;
    // 0.8.0
    bool mpeOn = false; int mpeBend = 48;
    int quality = (int) Quality::Standard;
};

/** Holds the APVTS atomics and produces snapshots without locks or allocation. */
class EngineParamRefs
{
public:
    explicit EngineParamRefs (juce::AudioProcessorValueTreeState& apvts);
    void read (EngineSnapshot& s) const noexcept;

private:
    struct OscRefs { std::atomic<float>* p[20]; };
    std::array<OscRefs, kNumOscs> osc;
    std::atomic<float>* sub[3]; std::atomic<float>* noise[3];
    std::array<std::array<std::atomic<float>*, 6>, 2> filt;
    std::atomic<float>* routing;
    std::array<std::array<std::atomic<float>*, 5>, kNumEnvs> env;
    std::array<std::array<std::atomic<float>*, 8>, kNumLfos> lfo;
    std::array<std::array<std::atomic<float>*, 3>, kNumModSlots> mod;
    std::array<std::atomic<float>*, kNumMacros> macro;
    std::atomic<float>* global[5];
    std::atomic<float>* engineMode;
    std::atomic<float>* analog;
    std::atomic<float> *velSens, *rnd[4], *stepG[6], *smp[11];
    std::array<std::atomic<float>*, kNumSteps> stepLvl {};
    std::atomic<float> *mpeOn, *mpeBend, *quality;
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

    SynthEngine (juce::AudioProcessorValueTreeState& apvts, const WavetableBank& bank,
                 const FactorySamples& factorySamples, const UserSampleSlot& userSample,
                 const std::array<UserWavetableSlot, kNumOscs>& userTables);

    /** Eco quality (unison cap) is read from the parameter; HQ oversampling is done by the processor. */
    double getSampleRate() const noexcept { return sampleRate; }

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Renders (replaces) the stereo synth output for this block and consumes the MIDI. */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TransportInfo& transport);

    void allNotesOff (bool immediate);

    int  getActiveVoiceCount() const noexcept { return activeVoiceCount.load(); }
    float getLfoValue (int i) const noexcept  { return lfoDisplay[(size_t) i].load(); }
    float getRandomValue() const noexcept     { return rndDisplay.load(); }
    float getStepValue() const noexcept       { return stepDisplay.load(); }
    int   getStepIndex() const noexcept       { return stepIndexDisplay.load(); }

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
        int note = 60, channel = 1;
        float mpeBend = 0.0f, mpePressure = 0.0f, mpeSlide = 0.0f;   // 0.8.0 per-note expression (semitones / 0..1)
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

        // ---- Divine 2.0 ----
        std::array<std::array<float, kMaxUnison>, kNumOscs> foldPrev {};          // ADAA fold memory
        std::array<std::array<dsp::DriftGen, kMaxUnison>, kNumOscs> uniDrift;    // per unison voice wander
        dsp::DriftGen pitchDrift, cutoffDrift;                                    // whole-voice wander
        dsp::Rng driftRng;                                                        // own generator: drift never depends on other random state
        float noteCents = 0.0f, noteCutOct = 0.0f;                                // per-note static variation

        // ---- 0.7.0 ----
        std::array<std::array<dsp::SvfState, 2>, 2> svf2;                        // second stage of the 24 dB SVFs
        std::array<std::array<std::array<dsp::SvfState, 3>, 2>, 2> formant;      // 3 vowel band-passes per filter/channel
        std::array<float, kNumOscs> lastWidth {};
        double smpPos = 0.0;                                                      // sampler read position (source samples)
        bool smpDone = false;
        float lastSmp = 0.0f;
        int ageSamples = 0;                                                       // for the PUNCH transient

        // ---- 0.8.0 warp ----
        std::array<double, kNumOscs> modPhase {};                                // modulator (sync master / FM / AM source) phases
        std::array<std::array<float, kMaxUnison>, kNumOscs> syncBlep {};         // pending sync-reset correction for the next sample
        std::array<float, kNumOscs> lastWarp {};
        std::array<std::array<float, kMaxUnison>, kNumOscs> warpDc {};           // running DC of a warped oscillator
        std::array<std::array<bool, kMaxUnison>, kNumOscs> warpDcValid {};
    };

    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity, int channel = 1);
    void noteOff (int note, int channel = 1);
    /** Renders the modulator signal of oscillator `o` (centre voice, pre-level) for SYNC / FM / AM targets. */
    void renderModulator (Voice& v, int o, double basePitch, const float* dest, int n) noexcept;
    void renderWarpedUnison (Voice& v, int o, int u, const EngineSnapshot::Osc& op, double& ph, double inc, int n, float invN,
                             float gL, float gR, float level0, float levelT, float pos0, float posT, float fold0, float foldT,
                             float warp0, float warpT, const UserWavetable* ut, float* mixL, float* mixR) noexcept;
    int warpSourceOf (int o) const noexcept;
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
    const FactorySamples& factorySamples;
    const UserSampleSlot& userSample;
    const std::array<UserWavetableSlot, kNumOscs>& userTables;
    EngineSnapshot snap;
    // modulator scratch (n <= kControlBlock): signal + master wrap position (-1 = none in this sample)
    std::array<std::array<float, kControlBlock>, kNumOscs> modBuf {}, wrapBuf {};
    std::array<double, kNumOscs> modInc {};
    int currentChannel = 1;
    std::array<float, 17> mpeChBend {}, mpeChPressure {}, mpeChSlide {};   // per member channel (MPE)

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
    std::atomic<float> rndDisplay { 0.0f }, stepDisplay { 0.0f };
    std::atomic<int> stepIndexDisplay { -1 };
    // RANDOM + STEP (modulate) sources
    dsp::Rng rndRng;
    double rndPhase = 0.0, stepFreeBeats = 0.0;
    float rndTarget = 0.0f, rndValue = 0.0f, rndPrevTarget = 0.0f, stepValue = 0.0f;
    std::atomic<int> activeVoiceCount { 0 };
    GlobalMods globalMods;
    double samplesElapsed = 0.0;

    std::vector<float> scratchL, scratchR;
    float darkNoiseCoeff = 0.05f;
};

} // namespace divinity
