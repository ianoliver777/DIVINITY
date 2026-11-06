#include "PresetManager.h"
#include "../DSP/SoulEngine.h"

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

    const bool lowEnd = is ({ "SUB", "808", "PERCUSSION" });
    const bool bass   = is ({ "BASS" });
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

/** SOUL ENGINE character per category (emotion mapping + Divine Intervention mode), then the finishing pass. */
static void finishFactoryPreset (FactoryPreset& p)
{
    struct SoulStyle { const char* cat; int mapping; const char* divine; };
    static const SoulStyle styles[] = {
        { "BASS", 2, "Shatter" }, { "808", 2, "Collapse" }, { "SUB", 2, "Collapse" }, { "LEAD", 0, "Ascension" },
        { "SYNTH", 4, "Bloom" }, { "PLUCK", 0, "Bloom" }, { "PAD", 1, "Bloom" }, { "PERCUSSION", 2, "Collapse" }, { "FX", 3, "Abyss" } };
    for (auto& st : styles)
        if (p.category == st.cat)
        {
            const auto& m = soulMappingPresets()[(size_t) st.mapping];
            for (int t = 0; t < kNumSoulTargets; ++t)
                p.set (ids::soul (t, "x"), m.w[t][0]).set (ids::soul (t, "y"), m.w[t][1]);
            p.choice (ids::divineMode, st.divine);
        }
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
