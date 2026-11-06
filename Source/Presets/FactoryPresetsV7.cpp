// DIVINITY 0.7.0 factory library: 777 presets in nine categories.
//
//   BASS 110 . 808 90 . SUB 50 . LEAD 100 . SYNTH 90 . PLUCK 80 . PAD 100 . PERCUSSION 77 . FX 80
//
// Every preset = a category archetype (the engine, envelopes, routing and core
// effect chain that define the family) x a variant number (seeded parameter
// spread, so no two are clones) x an effect flavour (Wide, Dirty, Space, Lo-Fi,
// Motion, Pumped, OTT, Shimmer, Tape ...). The flavour is a real, audible chain
// of the built-in effects and its words go into the tags + description.
// LFOs are only switched on in presets that route them somewhere.
#include "PresetManager.h"
#include "../DSP/FxCatalog.h"
#include "../DSP/Arpeggiator.h"
#include <map>
#include <set>

namespace divinity
{
namespace
{
    /** Collects parameter values (last write wins). */
    struct Patch
    {
        std::map<juce::String, juce::var> v;
        std::vector<juce::String> order;
        juce::Random* rng = nullptr;

        Patch& set (const juce::String& id, double x) { if (v.find (id) == v.end()) order.push_back (id); v[id] = x; return *this; }
        Patch& choice (const juce::String& id, const char* name) { if (v.find (id) == v.end()) order.push_back (id); v[id] = juce::String (name); return *this; }
        double get (const juce::String& id, double def) const { auto it = v.find (id); return (it != v.end() && ! it->second.isString()) ? (double) it->second : def; }
        juce::String getChoice (const juce::String& id) const { auto it = v.find (id); return (it != v.end() && it->second.isString()) ? it->second.toString() : juce::String(); }
        bool has (const juce::String& id) const { return v.find (id) != v.end(); }
        double j (double x, double amount) const { return x * (1.0 + amount * (rng->nextDouble() * 2.0 - 1.0)); }   // jitter

        // ---- sources ----
        Patch& wt (int o, const char* table, double pos, double level = 0.8)
        { return set (ids::osc (o, "on"), 1).choice (ids::osc (o, "engine"), "Wavetable").choice (ids::osc (o, "table"), table).set (ids::osc (o, "pos"), pos).set (ids::osc (o, "level"), level); }
        Patch& an (int o, const char* wave, double level = 0.8)
        { return set (ids::osc (o, "on"), 1).choice (ids::osc (o, "engine"), "Analog").choice (ids::osc (o, "wave"), wave).set (ids::osc (o, "level"), level); }
        Patch& uni (int o, int voices, double detune, double width, double blend = 1.0)
        { return set (ids::osc (o, "unison"), voices).set (ids::osc (o, "detune"), detune).set (ids::osc (o, "width"), width).set (ids::osc (o, "blend"), blend); }
        Patch& tune (int o, int oct, int semi = 0, double fine = 0.0) { return set (ids::osc (o, "oct"), oct).set (ids::osc (o, "semi"), semi).set (ids::osc (o, "fine"), fine); }
        Patch& tight (int o, double phase = 0.25) { return set (ids::osc (o, "randphase"), 0).set (ids::osc (o, "phase"), phase); }
        Patch& fold (int o, double f) { return set (ids::osc (o, "fold"), f); }
        Patch& pw (int o, double w) { return set (ids::osc (o, "pw"), w); }
        Patch& sub (const char* mode, double level) { return set (ids::subOn, 1).choice (ids::subMode, mode).set (ids::subLevel, level); }
        Patch& noise (const char* type, double level) { return set (ids::noiseOn, 1).choice (ids::noiseType, type).set (ids::noiseLevel, level); }
        Patch& smp (const char* name, double level, bool keytrack, int tuneSemis = 0, double start = 0.0)
        { return set (ids::smpOn, 1).choice (ids::smpSample, name).set (ids::smpLevel, level).set (ids::smpKeytrack, keytrack ? 1 : 0).set (ids::smpTune, tuneSemis).set (ids::smpStart, start); }
        Patch& smpLoop (double a, double b) { return set (ids::smpLoop, 1).set (ids::smpLoopStart, a).set (ids::smpLoopEnd, b); }

        // ---- filters / envelopes / modulation ----
        Patch& filt (int f, const char* type, double cutoff, double res, double drive = 0.0, double keytrack = 0.0, double mix = 1.0)
        { return choice (ids::filt (f, "type"), type).set (ids::filt (f, "cutoff"), cutoff).set (ids::filt (f, "res"), res).set (ids::filt (f, "drive"), drive).set (ids::filt (f, "keytrack"), keytrack).set (ids::filt (f, "mix"), mix); }
        Patch& parallel() { return choice (ids::filterRouting, "Parallel"); }
        Patch& env (int e, double a, double d, double s, double r, double curve = 0.0)
        { return set (ids::env (e, "attack"), a).set (ids::env (e, "decay"), d).set (ids::env (e, "sustain"), s).set (ids::env (e, "release"), r).set (ids::env (e, "curve"), curve); }
        Patch& lfo (int l, const char* shape, double rate, double depth = 1.0, double smooth = 0.0)
        { return set (ids::lfo (l, "on"), 1).choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "rate"), rate).set (ids::lfo (l, "depth"), depth).set (ids::lfo (l, "smooth"), smooth).set (ids::lfo (l, "sync"), 0); }
        Patch& lfoSync (int l, const char* shape, const char* div, double depth = 1.0, double smooth = 0.0)
        { return set (ids::lfo (l, "on"), 1).choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "sync"), 1).choice (ids::lfo (l, "div"), div).set (ids::lfo (l, "depth"), depth).set (ids::lfo (l, "smooth"), smooth); }
        Patch& mod (const char* src, const char* dst, double amt)
        {
            for (int s = 0; s < kNumModSlots; ++s)
                if (! has (ids::mod (s, "src"))) return choice (ids::mod (s, "src"), src).choice (ids::mod (s, "dst"), dst).set (ids::mod (s, "amt"), amt);
            return *this;
        }
        Patch& mono (double glideMs, bool legato = true) { return choice (ids::voiceMode, legato ? "Legato" : "Mono").set (ids::glide, glideMs); }
        Patch& voices (int n) { return set (ids::voices, n); }
        Patch& macros (double a, double b, double c, double d) { return set (ids::macro (0), a).set (ids::macro (1), b).set (ids::macro (2), c).set (ids::macro (3), d); }
        Patch& level (double dB) { return set (ids::master, dB); }
        Patch& drift (double a) { return set (ids::analog, a); }

        // ---- effects ----
        Patch& sat (double drive, double mix = 1.0) { return set (ids::satOn, 1).set (ids::satDrive, drive).set (ids::satMix, mix); }
        Patch& noSat() { return set (ids::satOn, 0); }
        Patch& chorus (double rate, double depth, double mix) { return set (ids::choOn, 1).set (ids::choRate, rate).set (ids::choDepth, depth).set (ids::choMix, mix); }
        Patch& delay (const char* div, double fb, double mix, bool ping = true, double tone = 0.55)
        { return set (ids::dlyOn, 1).choice (ids::dlyDiv, div).set (ids::dlyFeedback, fb).set (ids::dlyMix, mix).set (ids::dlyPingPong, ping ? 1 : 0).set (ids::dlyTone, tone); }
        Patch& reverb (double size, double damp, double mix, double predelay = 20.0, double width = 1.0)
        { return set (ids::revOn, 1).set (ids::revSize, size).set (ids::revDamp, damp).set (ids::revMix, mix).set (ids::revPredelay, predelay).set (ids::revWidth, width); }
        Patch& dist (const char* type, double drive, double mix, double tone = 0.6)
        { return set (fxids::distOn, 1).choice (fxids::distType, type).set (fxids::distDrive, drive).set (fxids::distMix, mix).set (fxids::distTone, tone); }
        Patch& crush (double bits, double down, double mix) { return set (fxids::crushOn, 1).set (fxids::crushBits, bits).set (fxids::crushDown, down).set (fxids::crushMix, mix); }
        Patch& eq (double lowGain, double midFreq, double midGain, double highGain, double lowFreq = 120.0, double highFreq = 6000.0)
        { return set (fxids::eqOn, 1).set (fxids::eqLowGain, lowGain).set (fxids::eqMidFreq, midFreq).set (fxids::eqMidGain, midGain).set (fxids::eqHighGain, highGain).set (fxids::eqLowFreq, lowFreq).set (fxids::eqHighFreq, highFreq); }
        Patch& comp (double thresh, double ratio, double makeup, double attack = 8.0, double release = 120.0, double mix = 1.0)
        { return set (fxids::compOn, 1).set (fxids::compThresh, thresh).set (fxids::compRatio, ratio).set (fxids::compMakeup, makeup).set (fxids::compAttack, attack).set (fxids::compRelease, release).set (fxids::compMix, mix); }
        Patch& ott (double depth, double time = 0.4, double out = 0.0) { return set (fxids::mbOn, 1).set (fxids::mbDepth, depth).set (fxids::mbTime, time).set (fxids::mbOut, out); }
        Patch& trans (double attack, double sustain) { return set (fxids::transOn, 1).set (fxids::transAttack, attack).set (fxids::transSustain, sustain); }
        Patch& flanger (double rate, double depth, double fb, double mix) { return set (fxids::flangOn, 1).set (fxids::flangRate, rate).set (fxids::flangDepth, depth).set (fxids::flangFb, fb).set (fxids::flangMix, mix); }
        Patch& phaser (double rate, double depth, double centre, double fb, double mix)
        { return set (fxids::phaseOn, 1).set (fxids::phaseRate, rate).set (fxids::phaseDepth, depth).set (fxids::phaseCentre, centre).set (fxids::phaseFb, fb).set (fxids::phaseMix, mix); }
        Patch& trem (const char* shape, const char* div, double depth, bool autopan = false)
        { return set (fxids::tremOn, 1).choice (fxids::tremMode, autopan ? "Autopan" : "Tremolo").choice (fxids::tremShape, shape).set (fxids::tremSync, 1).choice (fxids::tremDiv, div).set (fxids::tremDepth, depth); }
        Patch& ring (double freq, double mix) { return set (fxids::ringOn, 1).set (fxids::ringFreq, freq).set (fxids::ringMix, mix); }
        Patch& fshift (double shift, double mix) { return set (fxids::fshiftOn, 1).set (fxids::fshiftShift, shift).set (fxids::fshiftMix, mix); }
        Patch& stereo (double width, bool monoBass = true, double monoFreq = 120.0)
        { return set (fxids::stereoOn, 1).set (fxids::stereoWidth, width).set (fxids::stereoMonoBass, monoBass ? 1 : 0).set (fxids::stereoMonoFreq, monoFreq); }
        Patch& arp (const char* modeName, const char* rate, int octaves, double gate, double swing = 0.0)
        { return set (arpids::on, 1).choice (arpids::mode, modeName).choice (arpids::rate, rate).set (arpids::octaves, octaves).set (arpids::gate, gate).set (arpids::swing, swing); }

        // ---- step sequencer ----
        /** lvl: '0'-'9' (0 = rest), rat: '1'-'4', gate: 's' 0.12 'm' 0.4 'l' 0.95, pitch: semitones per step (or empty). */
        Patch& steps (const char* lvl, const char* rat, const char* gate, std::initializer_list<int> pitch, const char* rate, int length, double swing, bool trigger)
        {
            set (ids::stepOn, 1).choice (ids::stepMode, trigger ? "Trigger" : "Modulate").choice (ids::stepRate, rate).set (ids::stepLength, length).set (ids::stepSwing, swing);
            std::vector<int> pv (pitch);
            for (int s = 0; s < kNumSteps; ++s)
            {
                const char L = lvl[s % (int) strlen (lvl)], R = rat[s % (int) strlen (rat)], G = gate[s % (int) strlen (gate)];
                set (ids::step (s, "lvl"), (L - '0') / 9.0);
                set (ids::step (s, "rat"), juce::jlimit (1, 4, R - '0'));
                set (ids::step (s, "len"), G == 's' ? 0.12 : G == 'm' ? 0.4 : G == 'l' ? 0.95 : 0.25);
                set (ids::step (s, "pitch"), pv.empty() ? 0 : pv[(size_t) s % pv.size()]);
            }
            return *this;
        }
    };

    //==========================================================================
    // Effect flavours: each is a real chain on top of the archetype's own effects.
    //==========================================================================
    enum Flavour { Clean, Wide, Dirty, Space, LoFi, Motion, Pumped, OTT, Shimmer, Tape, Grit, Dream, NumFlavours };
    const char* flavourTag[] = { "Clean", "Wide", "Dirty", "Spacious", "Lo-Fi", "Moving", "Pumping", "OTT", "Shimmer", "Tape", "Gritty", "Dreamy" };
    const char* flavourPhrase[] = {
        "kept clean and upfront: EQ and glue compression only",
        "dimension chorus and a wide stereo stage, low end kept mono",
        "driven through distortion and saturation",
        "long ping-pong delay into a big hall",
        "bitcrushed, downsampled and rolled off like a worn sampler",
        "a slow phaser and flanger sweeping through it",
        "quarter-note volume pump, like heavy sidechain",
        "OTT-style multiband squash for density and detail",
        "frequency-shifted halo feeding a huge reverb",
        "tape saturation, slow chorus wobble and a darker top",
        "rectified grit and a transient snap",
        "soft chorus, airy top and a dreamy predelayed room"
    };

    void applyFlavour (Patch& p, Flavour f, bool lowEnd)
    {
        switch (f)
        {
            case Clean:   p.eq (lowEnd ? 2.0 : 0.0, 800, -1.5, 1.5).comp (-20, 3, 3, 10, 120); break;
            case Wide:    p.chorus (p.j (0.45, 0.3), 0.5, lowEnd ? 0.25 : 0.38).stereo (lowEnd ? 1.15 : 1.5, true, lowEnd ? 160.0 : 120.0).eq (0, 1500, -1, 2.0); break;
            case Dirty:   p.dist (lowEnd ? "Tube" : "Hard Clip", p.j (0.45, 0.3), lowEnd ? 0.55 : 0.4, 0.55).sat (0.35).eq (lowEnd ? 2.0 : 0.0, 2500, 2.0, -1.5); break;
            case Space:   p.delay ("1/8 D", 0.42, lowEnd ? 0.12 : 0.26).reverb (0.9, 0.35, lowEnd ? 0.15 : 0.38, 40.0); break;
            case LoFi:    p.crush (p.j (9.0, 0.15), p.j (5.0, 0.3), 0.45).eq (0, 1200, 2.0, -6.0).sat (0.3); break;
            case Motion:  p.phaser (0.12, 0.75, 900, 0.5, 0.45).flanger (0.08, 0.5, 0.35, 0.25); break;
            case Pumped:  p.trem ("Sine", "1/4", 0.6).comp (-22, 4, 4); break;
            case OTT:     p.ott (p.j (0.55, 0.25), 0.35, 1.0).eq (lowEnd ? 1.5 : 0.0, 2000, 0, 2.0); break;
            case Shimmer: p.fshift (0.16, 0.18).reverb (0.97, 0.25, lowEnd ? 0.2 : 0.45, 60.0).chorus (0.3, 0.4, 0.25); break;
            case Tape:    p.sat (0.45).chorus (0.18, 0.35, 0.3).eq (1.5, 900, 1.0, -4.0); break;
            case Grit:    p.dist ("Rectify", 0.35, 0.3, 0.5).trans (0.45, -0.1).eq (0, 3000, 2.5, 0); break;
            case Dream:   p.chorus (0.3, 0.45, 0.35).reverb (0.8, 0.5, lowEnd ? 0.12 : 0.32, 70.0).eq (0, 700, -2.0, 3.0); break;
            default: break;
        }
    }

    //==========================================================================
    // Names: two curated word pools per category; combinations walk the grid so
    // neighbouring presets don't share a word.
    //==========================================================================
    struct Pools { std::vector<const char*> a, b; std::vector<const char*> heroes; };

    /** heroes[i] (when non-empty) names slot i, so hand-picked names land on the archetype they describe. */
    juce::StringArray makeNames (const Pools& pools, int count, juce::StringArray& used)
    {
        juce::StringArray pooled;
        const int A = (int) pools.a.size(), B = (int) pools.b.size();
        // walk every (a, b) pair once, diagonally, so neighbours never share a word
        for (int k = 0; k < A * B; ++k)
        {
            const int i = k % A, jj = (k / A + i) % B;
            const juce::String n = juce::String (pools.a[(size_t) i]) + " " + pools.b[(size_t) jj];
            if (! used.contains (n) && ! pooled.contains (n)) pooled.add (n);
        }
        for (auto* h : pools.heroes) pooled.removeString (h);
        juce::StringArray out;
        int next = 0;
        for (int i = 0; i < count; ++i)
        {
            juce::String n = i < (int) pools.heroes.size() ? juce::String (pools.heroes[(size_t) i]) : juce::String();
            if (n.isEmpty() || used.contains (n))
            {
                while (next < pooled.size() && used.contains (pooled[next])) ++next;
                n = next < pooled.size() ? pooled[next++] : juce::String (pools.a[(size_t) (i % A)]) + " " + pools.b[(size_t) (i % B)] + " " + juce::String (i);
            }
            out.add (n); used.add (n);
        }
        return out;
    }

    //==========================================================================
    // Percussion patterns (16 steps)
    //==========================================================================
    struct Pattern { const char* name; const char* lvl; const char* rat; const char* gate; std::vector<int> pitch; const char* rate; double swing; };
    const Pattern hatPatterns[] = {
        { "straight eighths",      "9050905090509050", "1",                "s",                {},                 "1/16",   0.0 },
        { "trap sixteenths",       "9575957595759575", "1111111111111121", "s",                {},                 "1/16",   0.0 },
        { "rolled trap hats",      "9575957595759575", "1111211111113141", "s",                {},                 "1/16",   0.0 },
        { "triplet rolls",         "957957957957",     "111121111131",     "s",                {},                 "1/16 T", 0.0 },
        { "half-time bounce",      "9006900690069060", "1111111111111131", "sssssssssssssmss", {},                 "1/16",   0.18 },
        { "open-hat lift",         "9575957595759575", "1",                "sssssssssssssssl", {},                 "1/16",   0.0 },
        { "drill skitter",         "9070607090706070", "1112111211131114", "s",                {},                 "1/16",   0.0 },
        { "swung sixteenths",      "9686968696869686", "1",                "s",                {},                 "1/16",   0.35 },
        { "thirty-second burst",   "9554955495549554", "1",                "s",                {},                 "1/32",   0.0 },
        { "offbeat open hats",     "0090009000900090", "1",                "l",                {},                 "1/16",   0.0 },
        { "stutter gaps",          "9900990099009990", "1111111111111112", "s",                {},                 "1/16",   0.0 },
    };
    const Pattern percPatterns[] = {
        { "rim groove",            "9007009070090700", "1",                "s",                { 0, 0, 0, 5, 0, 0, 7, 0, 0, 0, 0, 3, 0, 0, 5, 0 }, "1/16", 0.12 },
        { "tom walk",              "9008006009007000", "1",                "m",                { 0, 0, 0, -3, 0, 0, -5, 0, 0, 0, 2, 0, 0, 0, -7, 0 }, "1/16", 0.0 },
        { "click and snap",        "9070907090709575", "1111111111111121", "s",                { 12, 0, 0, 0, 7, 0, 0, 0, 12, 0, 0, 0, 7, 0, 0, 12 }, "1/16", 0.0 },
        { "wave perc shuffle",     "9506950695069506", "1",                "s",                { 0, 0, 5, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 7, 0 }, "1/16", 0.25 },
        { "triplet perc",          "960960960965",     "1111111111121",    "s",                { 0, 3, 7, 0, 3, 7, 0, 5, 8, 0, 5, 12 }, "1/16 T", 0.0 },
        { "afro knock",            "9009009009009090", "1",                "m",                { 0, 0, 0, 7, 0, 0, 5, 0, 0, 3, 0, 0, 0, 7, 0, 5 }, "1/16", 0.08 },
    };

    //==========================================================================
    struct Spec { juce::String name, category, tags, description; Patch patch; };

    void finishSpec (Spec& s, const juce::String& base, Flavour f, const juce::StringArray& extraTags)
    {
        juce::StringArray tags { flavourTag[(int) f] };
        tags.addArray (extraTags);
        s.tags = tags.joinIntoString (", ");
        s.description = base + " This one: " + flavourPhrase[(int) f] + ".";
    }

    #include "FactoryPresetsV7Recipes.inc"

    //==========================================================================
    FactoryPreset toFactoryPreset (Spec& s, double trim)
    {
        auto& p = s.patch;
        // LFO power follows usage: an LFO is on only if a route actually uses it
        for (int l = 0; l < kNumLfos; ++l)
        {
            bool usedLfo = false;
            for (int m = 0; m < kNumModSlots; ++m) usedLfo |= p.getChoice (ids::mod (m, "src")) == "LFO " + juce::String (l + 1);
            p.set (ids::lfo (l, "on"), usedLfo ? 1 : 0);
        }
        p.set (ids::master, juce::jlimit (-60.0, 6.0, p.get (ids::master, -8.0) + trim));
        FactoryPreset fp;
        fp.name = s.name;
        fp.category = s.category;
        fp.tags = s.tags;
        fp.description = s.description;
        for (auto& id : p.order)
        {
            const auto& val = p.v[id];
            if (val.isString()) fp.choice (id, val.toString().toRawUTF8());
            else fp.set (id, (double) val);
        }
        return fp;
    }

    //==========================================================================
    double levelTrim (const juce::String& name)
    {
        static const std::map<juce::String, double> trims {
           #include "FactoryPresetsV7Levels.inc"
        };
        auto it = trims.find (name);
        return it != trims.end() ? it->second : 0.0;
    }
}

//==============================================================================
void addLibraryV7 (std::vector<FactoryPreset>& list)
{
    std::vector<Spec> specs;
    juce::StringArray used;
    buildFromRecipe (bassRecipe(), specs, used);
    buildFromRecipe (recipe808(), specs, used);
    buildFromRecipe (recipeSub(), specs, used);
    buildFromRecipe (recipeLead(), specs, used);
    buildFromRecipe (recipeSynth(), specs, used);
    buildFromRecipe (recipePluck(), specs, used);
    buildFromRecipe (recipePad(), specs, used);
    buildFromRecipe (recipePerc(), specs, used);
    buildFromRecipe (recipeFx(), specs, used);

    for (auto& s : specs) list.push_back (toFactoryPreset (s, levelTrim (s.name)));

    // 0.8.1: display names (celestial / nostalgic set, every word used once). Applied after the bank is
    // built and level-matched, so the sounds are exactly the 0.7.3 ones; the internal design names above
    // stay as keys for the recipes, picks and level table. Manifest: docs/PRESET_NAMES.csv.
    static const char* const kNames[] = {
       #include "FactoryPresetsV7Names.inc"
    };
    constexpr int kNumNames = (int) (sizeof (kNames) / sizeof (kNames[0]));
    const int first = (int) list.size() - (int) specs.size();
    jassert ((int) specs.size() == kNumNames);
    for (int i = 0; i < juce::jmin ((int) specs.size(), kNumNames); ++i)
        list[(size_t) (first + i)].name = kNames[i];
}

namespace
{
    CategoryRecipe recipeFor (const juce::String& cat)
    {
        if (cat == "BASS") return bassRecipe();
        if (cat == "808") return recipe808();
        if (cat == "SUB") return recipeSub();
        if (cat == "LEAD") return recipeLead();
        if (cat == "SYNTH") return recipeSynth();
        if (cat == "PLUCK") return recipePluck();
        if (cat == "PAD") return recipePad();
        if (cat == "PERCUSSION") return recipePerc();
        return recipeFx();
    }
}

juce::StringArray factoryCategoriesV7() { return { "BASS", "808", "SUB", "LEAD", "SYNTH", "PLUCK", "PAD", "PERCUSSION", "FX" }; }

std::vector<FactoryPreset> createCandidatePresetsV7 (const juce::String& category, int first, int count, std::vector<std::pair<int, int>>* heroesOut, int forcedSource, int forcedMotion)
{
    const auto R = recipeFor (category);
    if (heroesOut != nullptr) *heroesOut = heroSlots (R);
    std::vector<FactoryPreset> v;
    for (int k = first; k < first + count; ++k)
    {
        auto s = makeCandidate (R, k, forcedSource, forcedMotion);
        v.push_back (toFactoryPreset (s, 0.0));
    }
    return v;
}

int factoryCountV7 (const juce::String& category) { return recipeFor (category).count; }

} // namespace divinity
