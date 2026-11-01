#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace divinity
{
//==============================================================================
/** SOUL ENGINE targets. Order is permanent (parameter IDs derive from `key`). */
struct SoulTarget { const char* key; const char* name; const char* help; };

constexpr int kNumSoulTargets = 10;
const std::array<SoulTarget, kNumSoulTargets>& soulTargets();

/** Mapping presets: per target {aggressive-pole weight, euphoria-pole weight}. */
struct SoulMappingPreset { const char* name; const char* description; float w[kNumSoulTargets][2]; };
const std::vector<SoulMappingPreset>& soulMappingPresets();

juce::StringArray divineModeNames();
juce::String divineModeDescription (int mode);

namespace ids
{
    inline const juce::String soulX = "soul_x", soulY = "soul_y", soulIntensity = "soul_intensity",
                              soulCurve = "soul_curve", soulSmooth = "soul_smooth";
    inline juce::String soul (int target, const char* field) { return juce::String ("soul_") + soulTargets()[(size_t) target].key + "_" + field; }
    inline const juce::String divineAmount = "divine_amount", divineMode = "divine_mode";
}

void addSoulParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

//==============================================================================
/** Offsets produced by SOUL ENGINE + Divine Intervention, applied on top of the patch. */
struct PerformanceMods
{
    float brightOct = 0;     // filter cutoff, octaves
    float drive = 0;         // + saturation / filter drive
    float attackLog2 = 0;    // amp attack multiplier = 2^x
    float releaseLog2 = 0;   // amp release multiplier = 2^x
    float detune = 0;        // + unison detune
    float width = 0;         // + unison width
    float reverb = 0;        // + reverb mix
    float reverbSize = 0;    // + reverb size
    float delay = 0;         // + delay mix
    float driftCents = 0;    // slow random per-voice pitch wander
    float motion = 0;        // LFO depth multiplier = 1 + motion
    float fold = 0;          // + oscillator fold (harmonics)
};

/**
    Computes PerformanceMods once per control block on the audio thread.
    Reads parameter atomics only; no locks or allocation.
*/
class SoulEngine
{
public:
    explicit SoulEngine (juce::AudioProcessorValueTreeState& apvts);
    void reset();

    /** xMod / yMod: mod-matrix offsets in bipolar units; divMod: offset to the Divine amount. */
    PerformanceMods compute (double seconds, float xMod, float yMod, float divMod) noexcept;
    float getEffectiveDivine() const noexcept { return effDiv.load(); }

    float getEffectiveX() const noexcept { return effX.load(); }   // -1..1, after mod + smoothing
    float getEffectiveY() const noexcept { return effY.load(); }

private:
    std::atomic<float> *x, *y, *intensity, *curve, *smooth, *divAmount, *divMode;
    std::array<std::atomic<float>*, kNumSoulTargets> on {}, wx {}, wy {};
    float sx = 0, sy = 0, sDiv = 0;
    bool primed = false;
    std::atomic<float> effX { 0 }, effY { 0 }, effDiv { 0 };
};

} // namespace divinity
