#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace divinity
{
//==============================================================================
/**
    Catalogue of rack effects. The order of this list is the module *index*
    (permanent); the user-facing chain order is stored separately as a list of
    keys, so modules can be reordered without touching parameter IDs.
*/
struct FxKnob   { juce::String id; const char* label; bool bipolar = false; };
struct FxDef
{
    const char* key;            // permanent: used in the saved chain order
    const char* name;
    const char* description;
    juce::String onID;
    std::vector<FxKnob> knobs;
    std::vector<juce::String> choices;       // choice parameter IDs (shown as dropdowns)
    std::vector<std::pair<juce::String, const char*>> toggles;   // bool parameter IDs + button label
};

const std::vector<FxDef>& fxCatalog();
int fxIndexForKey (const juce::String& key);
juce::StringArray defaultFxOrder();

/** Keeps known keys in their given order, drops unknown/duplicate ones and appends any missing modules. */
juce::StringArray sanitiseFxOrder (const juce::StringArray& keys);

void addFxRackParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
juce::String fxHelpText (const juce::String& paramID);

inline const juce::Identifier fxOrderProperty { "fxOrder" };

namespace fxids
{
    inline const juce::String distOn = "dist_on", distType = "dist_type", distDrive = "dist_drive", distTone = "dist_tone", distMix = "dist_mix";
    inline const juce::String crushOn = "crush_on", crushBits = "crush_bits", crushDown = "crush_down", crushMix = "crush_mix";
    inline const juce::String eqOn = "eq_on", eqLowFreq = "eq_low_freq", eqLowGain = "eq_low_gain", eqMidFreq = "eq_mid_freq",
                              eqMidGain = "eq_mid_gain", eqMidQ = "eq_mid_q", eqHighFreq = "eq_high_freq", eqHighGain = "eq_high_gain";
    inline const juce::String compOn = "comp_on", compThresh = "comp_thresh", compRatio = "comp_ratio", compAttack = "comp_attack",
                              compRelease = "comp_release", compMakeup = "comp_makeup", compMix = "comp_mix";
    inline const juce::String mbOn = "mb_on", mbDepth = "mb_depth", mbTime = "mb_time", mbOut = "mb_out";
    inline const juce::String mbLowX = "mb_low_x", mbHighX = "mb_high_x";
    inline juce::String mbBand (int b, const char* what) { static const char* n[3] = { "low", "mid", "high" }; return juce::String ("mb_") + n[b] + "_" + what; }
    inline const juce::String transOn = "trans_on", transAttack = "trans_attack", transSustain = "trans_sustain";
    inline const juce::String flangOn = "flanger_on", flangRate = "flanger_rate", flangDepth = "flanger_depth", flangFb = "flanger_feedback", flangMix = "flanger_mix";
    inline const juce::String phaseOn = "phaser_on", phaseRate = "phaser_rate", phaseDepth = "phaser_depth", phaseFb = "phaser_feedback",
                              phaseCentre = "phaser_centre", phaseMix = "phaser_mix";
    inline const juce::String tremOn = "trem_on", tremMode = "trem_mode", tremShape = "trem_shape", tremSync = "trem_sync",
                              tremRate = "trem_rate", tremDiv = "trem_div", tremDepth = "trem_depth";
    inline const juce::String ringOn = "ring_on", ringFreq = "ring_freq", ringMix = "ring_mix";
    inline const juce::String fshiftOn = "fshift_on", fshiftShift = "fshift_shift", fshiftMix = "fshift_mix";
    inline const juce::String stereoOn = "stereo_on", stereoWidth = "stereo_width", stereoPan = "stereo_pan",
                              stereoMonoBass = "stereo_monobass", stereoMonoFreq = "stereo_monofreq", stereoGain = "stereo_gain";
}

} // namespace divinity
