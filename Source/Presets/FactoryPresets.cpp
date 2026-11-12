#include "PresetManager.h"
#include "../DSP/SoulEngine.h"
#include "../DSP/FxCatalog.h"

namespace divinity
{
void addLibraryV7 (std::vector<FactoryPreset>& list);   // FactoryPresetsV7.cpp

//==============================================================================
/** Finishing pass applied to every factory preset: the mod wheel and aftertouch
    always do something musical (velocity -> brightness is built into the engine). */
void applySoundEngine2Finish (FactoryPreset& p)
{
    const auto& c = p.category;
    auto is = [&c] (std::initializer_list<const char*> words) { for (auto* w : words) if (c.contains (w)) return true; return false; };

    const bool lowEnd = is ({ "PERCUSSION" });
    const bool bass   = is ({ "BASS", "REESE" });
    const bool pad    = is ({ "PAD" });

    // what's already routed, and which slots are free
    auto value = [&p] (const juce::String& id) -> juce::var
    {
        juce::var v;
        for (auto& [k, x] : p.values) if (k == id) v = x;   // last write wins (same as loading)
        return v;
    };
    bool wheelUsed = false, touchUsed = false;
    std::vector<int> free;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const auto src = value (ids::mod (s, "src"));
        if (src.isVoid() || src.toString() == "-") { free.push_back (s); continue; }
        wheelUsed |= src.toString() == "Mod Wheel";
        touchUsed |= src.toString() == "Aftertouch";
    }
    const auto ftype = value (ids::filt (0, "type"));
    const bool filterOn = ftype.isVoid() || ftype.toString() != "Off";   // default type is MG Low 24
    size_t next = 0;
    auto route = [&] (const char* src, const char* dst, double amt)
    {
        if (next >= free.size()) return;
        const int s = free[next++];
        p.choice (ids::mod (s, "src"), src).choice (ids::mod (s, "dst"), dst).set (ids::mod (s, "amt"), amt);
    };

    if (! wheelUsed && ! lowEnd)
    {
        if (filterOn) route ("Mod Wheel", "Filter 1 Cutoff", bass ? 0.3 : pad ? 0.22 : 0.18);
        else          route ("Mod Wheel", "Osc A Fold", 0.25);
    }
    if (! touchUsed && ! lowEnd)
    {
        if (filterOn) route ("Aftertouch", "Filter 1 Cutoff", 0.15);
        else          route ("Aftertouch", "Amp", 0.15);
    }
}

/** 1.1 "warm and thick" pass over the whole bank: analog drift, tape-style saturation, a warm EQ tilt
    (more low-mids, softer top) and a gentle chorus on melodic sounds. Anything a preset already sets
    is left alone; drums and FX keep their transients. */
static void applyWarmth (FactoryPreset& p)
{
    const auto& c = p.category;
    if (c == "PERCUSSION" || c == "FX") return;
    auto value = [&p] (const juce::String& id) -> juce::var
    {
        juce::var v;
        for (auto& [k, x] : p.values) if (k == id) v = x;
        return v;
    };
    auto num = [&] (const juce::String& id, double def) { auto v = value (id); return v.isVoid() || v.isString() ? def : (double) v; };
    const bool low = c == "BASS" || c == "REESE";
    p.set (ids::analog, juce::jmax (num (ids::analog, 0.0), 0.3));
    if (num (ids::satOn, 0) < 0.5) p.set (ids::satOn, 1).set (ids::satDrive, low ? 0.25 : 0.18).set (ids::satMix, 0.5);
    if (num (fxids::eqOn, 0) < 0.5)
        p.set (fxids::eqOn, 1).set (fxids::eqLowFreq, 140.0).set (fxids::eqLowGain, low ? 1.5 : 2.5)
         .set (fxids::eqMidFreq, 800.0).set (fxids::eqMidGain, 0.0).set (fxids::eqHighFreq, 7000.0).set (fxids::eqHighGain, -2.5);
    const bool melodic = c == "LEAD" || c == "SYNTH" || c == "PAD" || c == "TEXTURE" || c == "SEQUENCE";
    if (melodic && num (ids::choOn, 0) < 0.5) p.set (ids::choOn, 1).set (ids::choRate, 0.25).set (ids::choDepth, 0.3).set (ids::choMix, 0.18);
}

/** SOUL ENGINE character per category (emotion mapping + Divine Intervention mode), then the finishing pass. */
static void finishFactoryPreset (FactoryPreset& p)
{
    struct SoulStyle { const char* cat; int mapping; const char* divine; };
    static const SoulStyle styles[] = {
        { "REESE", 2, "Shatter" }, { "BASS", 2, "Collapse" }, { "LEAD", 0, "Ascension" }, { "SYNTH", 4, "Bloom" },
        { "PAD", 1, "Bloom" }, { "TEXTURE", 1, "Abyss" }, { "SEQUENCE", 0, "Bloom" }, { "PERCUSSION", 2, "Collapse" }, { "FX", 3, "Abyss" },
        { "WAVE", 1, "Bloom" }, { "TRAP", 2, "Collapse" }, { "TECHNO", 0, "Ascension" }, { "DUBSTEP", 2, "Shatter" },
        { "GARAGE", 4, "Bloom" }, { "DARK GARAGE", 1, "Abyss" }, { "DNB", 2, "Shatter" }, { "CINEMATIC", 3, "Abyss" } };
    for (auto& st : styles)
        if (p.category == st.cat)
        {
            const auto& m = soulMappingPresets()[(size_t) st.mapping];
            for (int t = 0; t < kNumSoulTargets; ++t)
                p.set (ids::soul (t, "x"), m.w[t][0]).set (ids::soul (t, "y"), m.w[t][1]);
            p.choice (ids::divineMode, st.divine);
        }
    applyWarmth (p);
    applySoundEngine2Finish (p);
}

std::vector<FactoryPreset> createFactoryPresets()
{
    std::vector<FactoryPreset> list;
    list.reserve (777);
    addLibraryV7 (list);
    for (auto& p : list) finishFactoryPreset (p);
    return list;
}

std::vector<FactoryPreset> createCandidatePresets (const juce::String& category, int first, int count,
                                                   std::vector<std::pair<int, int>>* heroSlots, int forcedSource, int forcedMotion)
{
    auto v = createCandidatePresetsV7 (category, first, count, heroSlots, forcedSource, forcedMotion);
    for (auto& p : v) finishFactoryPreset (p);
    return v;
}

} // namespace divinity
