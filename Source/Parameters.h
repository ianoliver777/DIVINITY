#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
// DIVINITY parameter system.
//
// Every host-exposed parameter is declared here. IDs are permanent: never
// rename or reuse one once released (host automation + saved sessions depend
// on them). New parameters may be appended freely; the state loader fills
// any parameter missing from an older session with its default.
//==============================================================================
namespace divinity
{
    constexpr int kParamVersion = 1;   // juce::ParameterID version hint
    constexpr int kStateVersion = 1;   // bumped when the saved-state layout changes

    constexpr int kNumOscs     = 4;   // A, B, C (v0.1) + D (v0.7)
    constexpr int kNumEnvs     = 4;   // ENV 4 appended in v0.7
    constexpr int kNumLfos     = 4;
    constexpr int kNumMacros   = 8;   // TONE MOVEMENT TEXTURE SPACE (v0.1) + CHARACTER WIDTH PUNCH AIR (v0.7)
    constexpr int kNumSteps    = 16;
    constexpr int kNumModSlots = 8;
    constexpr int kMaxUnison   = 8;
    constexpr int kMaxVoices   = 32;

    //--------------------------------------------------------------------------
    enum class OscEngine { Wavetable = 0, Analog };
    enum class AnalogWave { Sine = 0, Triangle, Saw, Square, Pulse };
    enum class NoiseType { White = 0, Pink, Bright, Dark };
    enum class FilterType { Off = 0, Ladder24, LowPass12, HighPass12, BandPass, Notch, Peak, Comb,
                            LowPass24, HighPass24, CombNeg, Formant };   // appended in 0.7.0
    enum class FilterRouting { Serial = 0, Parallel };
    enum class LfoShape { Sine = 0, Square, Triangle, Random, SawUp, SawDown };   // saws appended in 0.7.0
    enum class StepMode { Modulate = 0, Trigger };
    enum class VoiceMode { Poly = 0, Legato, Mono };
    enum class EngineMode { Classic = 0, Divine2 };
    // 0.8.0
    enum class OscWarp { Off = 0, Sync, FM, AM, BendPlus, BendMinus, Pwm, PhaseDist, Mirror };
    enum class Quality { Eco = 0, Standard, HQ };

    enum class ModSource
    {
        None = 0,
        Lfo1, Lfo2, Lfo3, Lfo4,
        Env1, Env2, Env3,
        Velocity, ModWheel, Aftertouch, PitchBend, NoteNumber,
        Macro1, Macro2, Macro3, Macro4,
        SoulX, SoulY,                    // appended in 0.2.0 (choice indices only ever grow)
        Env4, Macro5, Macro6, Macro7, Macro8, Random, Step,   // appended in 0.7.0
        MpePressure, MpeSlide,                               // appended in 0.8.0
        NumSources
    };

    enum class ModDest
    {
        None = 0,
        OscAPitch, OscBPitch, OscCPitch,
        OscAPos, OscBPos, OscCPos,
        OscALevel, OscBLevel, OscCLevel,
        OscAFold, OscBFold, OscCFold,
        OscAPwm, OscBPwm, OscCPwm,
        OscADetune, OscBDetune, OscCDetune,
        SubLevel, NoiseLevel,
        Filter1Cutoff, Filter1Res, Filter1Drive,
        Filter2Cutoff, Filter2Res, Filter2Drive,
        Amp, Pan,
        Lfo1Rate, Lfo2Rate, Lfo3Rate, Lfo4Rate,
        SatDrive, ChorusDepth, DelayMix, ReverbMix,
        SoulX, SoulY, DivineAmount,      // appended in 0.2.0
        OscDPitch, OscDPos, OscDLevel, OscDFold, OscDPwm, OscDDetune,   // appended in 0.7.0
        SamplerLevel, SamplerPitch, SamplerStart,
        OscAWarp, OscBWarp, OscCWarp, OscDWarp,              // appended in 0.8.0
        NumDests
    };

    /** Per-oscillator matrix destination (kind: 0 pitch, 1 WT pos, 2 level, 3 fold, 4 PWM, 5 detune, 6 warp). */
    inline int oscModDest (int kind, int osc) noexcept
    {
        if (kind == 6) return (int) ModDest::OscAWarp + osc;
        if (osc < 3) return (int) ModDest::OscAPitch + kind * 3 + osc;
        return (int) ModDest::OscDPitch + kind;
    }

    juce::StringArray modSourceNames();
    juce::StringArray stepRateNames();
    double stepRateBeats (int index);
    juce::StringArray samplerSampleNames();   // factory samples + "User Sample"
    juce::StringArray modDestNames();
    juce::StringArray wavetableNames();
    /** Choices of the oscillator table parameter: the factory tables + "User Wavetable" (0.8.0). */
    juce::StringArray oscTableChoiceNames();
    juce::StringArray warpModeNames();
    juce::StringArray qualityNames();
    juce::StringArray filterTypeNames();
    juce::StringArray lfoDivisionNames();
    double lfoDivisionInBeats (int index);
    juce::StringArray delayDivisionNames();
    double delayDivisionInBeats (int index);
    juce::StringArray subModeNames();

    /** True for destinations that live after the voice mix (global FX). */
    bool isGlobalDest (ModDest d);

    //--------------------------------------------------------------------------
    namespace ids
    {
        inline juce::String osc (int i, const char* name)   { return juce::String ("osc") + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + "_" + name; }
        inline juce::String env (int i, const char* name)   { return "env" + juce::String (i + 1) + "_" + name; }
        inline juce::String lfo (int i, const char* name)   { return "lfo" + juce::String (i + 1) + "_" + name; }
        inline juce::String filt (int i, const char* name)  { return "filter" + juce::String (i + 1) + "_" + name; }
        inline juce::String mod (int i, const char* name)   { return "mod" + juce::String (i + 1) + "_" + name; }
        inline juce::String macro (int i)                   { return "macro" + juce::String (i + 1); }
        inline juce::String step (int i, const char* name)  { return "step" + juce::String (i + 1) + "_" + name; }

        inline const juce::String subOn       = "sub_on";
        inline const juce::String subMode     = "sub_mode";
        inline const juce::String subLevel    = "sub_level";
        inline const juce::String noiseOn     = "noise_on";
        inline const juce::String noiseType   = "noise_type";
        inline const juce::String noiseLevel  = "noise_level";
        inline const juce::String filterRouting = "filter_routing";

        inline const juce::String satOn = "sat_on", satDrive = "sat_drive", satMix = "sat_mix";
        inline const juce::String choOn = "chorus_on", choRate = "chorus_rate", choDepth = "chorus_depth", choMix = "chorus_mix";
        inline const juce::String dlyOn = "delay_on", dlyDiv = "delay_div", dlyFeedback = "delay_feedback",
                                  dlyMix = "delay_mix", dlyPingPong = "delay_pingpong", dlyTone = "delay_tone";
        inline const juce::String revOn = "reverb_on", revSize = "reverb_size", revDamp = "reverb_damp",
                                  revWidth = "reverb_width", revMix = "reverb_mix";
        inline const juce::String limOn = "limiter_on", limCeiling = "limiter_ceiling";

        inline const juce::String voices    = "voices";
        inline const juce::String bendRange = "bend_range";
        inline const juce::String glide     = "glide";
        inline const juce::String voiceMode = "voice_mode";
        inline const juce::String masterTune = "master_tune";
        inline const juce::String master    = "master";
        // appended in 0.6.0 (Sound Engine 2.0)
        inline const juce::String engineMode = "engine_mode";    // 0 = Classic (v1 sound, old sessions), 1 = Divine 2.0
        inline const juce::String analog     = "analog";         // organic drift: unison / pitch / cutoff wander
        inline const juce::String revPredelay = "reverb_predelay";
        // appended in 0.7.0
        inline const juce::String velSens   = "vel_sens";
        inline const juce::String masterPan = "master_pan";
        inline const juce::String rndRate = "random_rate", rndSmooth = "random_smooth", rndSync = "random_sync", rndDiv = "random_div";
        inline const juce::String stepOn = "step_on", stepMode = "step_mode", stepRate = "step_rate", stepLength = "step_length",
                                  stepSwing = "step_swing", stepSmooth = "step_smooth";
        inline const juce::String smpOn = "smp_on", smpSample = "smp_sample", smpLevel = "smp_level", smpTune = "smp_tune",
                                  smpFine = "smp_fine", smpStart = "smp_start", smpLoop = "smp_loop", smpLoopStart = "smp_loop_start",
                                  smpLoopEnd = "smp_loop_end", smpReverse = "smp_reverse", smpKeytrack = "smp_keytrack";
        // appended in 0.8.0 (per osc: ids::osc (i, "warp"), "warp_amt", "warp_src")
        inline const juce::String mpeOn = "mpe_on", mpeBend = "mpe_bend";
        inline const juce::String quality = "quality", qualityRender = "quality_render";
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Studio settings rather than sound (quality, MPE): presets neither store nor reset them. */
    constexpr int kLfoPoints = 16;   // drawable LFO (shape "Custom")
namespace ids { inline juce::String lfoPoint (int l, int k) { return "lfo" + juce::String (l + 1) + "_pt" + juce::String (k + 1); } }

inline bool isGlobalSetting (const juce::String& id)
    {
        return id == ids::quality || id == ids::qualityRender || id == ids::mpeOn || id == ids::mpeBend;
    }

    /** Sessions / presets saved before 0.7 have no LFO power switches; their LFOs were always running,
        so switch them on to keep the sound. `tag`/`idAttr` describe the XML layout of the parameter list. */
    void applyPre07Defaults (const juce::XmlElement& params, const char* tag, const char* idAttr,
                             juce::AudioProcessorValueTreeState& apvts);

    /** Plain-English help text used for tooltips and the help system. */
    juce::String helpTextFor (const juce::String& paramID);
}
