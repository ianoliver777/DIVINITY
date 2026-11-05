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

    //==========================================================================
    // BASS (110): reeses of every kind + wave-music basses
    //==========================================================================
    void buildBass (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Abyss", "Cold", "Toxic", "Velvet", "Black", "Molten", "Ghost", "Hollow", "Iron", "Neon", "Static",
                        "Midnight", "Venom", "Glacier", "Rust", "Phantom" },
                      { "Reese", "Growl", "Drift", "Sweep", "Pressure", "Snarl", "Current", "Tide", "Rumble", "Mangle", "Shift" },
                      { "Abyss Reese", "Wave Reese", "Neuro Reese", "Talking Reese", "Into The Void Bass", "Wobble Machine", "Concrete Growl", "Always Elsewhere Bass" } };
        const auto names = makeNames (pools, 110, used);
        static const char* desc[10] = {
            "Classic reese: two detuned saws through a driven ladder, breathing slowly.",
            "Wavetable reese whose harmonic stack drifts under a slow LFO.",
            "Neuro reese: FM growl folded through a comb for that hollow, talking edge.",
            "Talking reese: a formant filter swept by a synced LFO says 'yoi' on every bar.",
            "Wave bass: sine sub welded to a detuned saw reese, mono and heavy.",
            "Tempo-synced wobble on a resonant filter, built for drops.",
            "Square-wave bass with PWM motion: hollow, cold and stabby.",
            "Gliding wave bass: monophonic portamento between notes, vowel table moving with the wheel.",
            "FM bass: electric metallic punch with a decaying index.",
            "Plucked bass: filter envelope snap and a sine tail.",
        };
        const Flavour fl[] = { Dirty, Wide, OTT, Tape, Clean, Motion, Grit, Dream, Dirty, OTT, Space };
        for (int i = 0; i < 110; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "BASS";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 10, v = i / 10;
            const Flavour f = fl[(size_t) (v + a) % 11];
            p.mono (p.j (40.0, 0.5)).drift (0.15).voices (1).level (-7.0);
            switch (a)
            {
                case 0: p.an (0, "Saw", 0.8).uni (0, 2, p.j (0.18, 0.3), 0.4).an (1, "Saw", 0.7).tune (1, 0, 0, p.j (14, 0.3)).sub ("Sine -1", 0.55)
                         .filt (0, "MG Low 24", p.j (900, 0.35), 0.22, 0.35, 0.3).lfo (0, "Triangle", p.j (0.16, 0.4)).mod ("LFO 1", "Filter 1 Cutoff", 0.12).mod ("LFO 1", "Osc B Pitch", 0.004); break;
                case 1: p.wt (0, v % 2 ? "Reese Core" : "Saw Stack", p.j (0.4, 0.5), 0.85).uni (0, 3, p.j (0.22, 0.3), 0.5).sub ("Sine -1", 0.5)
                         .filt (0, "Low Pass 24", p.j (1300, 0.3), 0.18, 0.25, 0.25).lfo (0, "Sine", p.j (0.09, 0.4), 1.0, 0.5).mod ("LFO 1", "Osc A WT Pos", 0.3); break;
                case 2: p.wt (0, v % 2 ? "Neuro Growl" : "Growl FM", p.j (0.5, 0.4), 0.85).uni (0, 2, 0.15, 0.3).sub ("Sine -1", 0.5).fold (0, p.j (0.25, 0.4))
                         .filt (0, "Comb -", p.j (220, 0.3), 0.55, 0.0, 1.0, 0.5).filt (1, "Low Pass 12", 3200, 0.1).lfoSync (0, "Saw Down", "1/2").mod ("LFO 1", "Osc A WT Pos", 0.45); break;
                case 3: p.an (0, "Saw", 0.8).uni (0, 3, p.j (0.2, 0.3), 0.4).sub ("Sine -1", 0.55).filt (0, "Formant", p.j (700, 0.4), 0.55, 0.2)
                         .lfoSync (0, v % 2 ? "Triangle" : "Sine", v % 3 ? "1 bar" : "1/2", 1.0, 0.3).mod ("LFO 1", "Filter 1 Cutoff", 0.35); break;
                case 4: p.an (0, "Sine", 0.85).tight (0).an (1, "Saw", 0.45).uni (1, 4, p.j (0.25, 0.3), 0.5).tune (1, 1)
                         .filt (0, "Low Pass 24", p.j (1800, 0.3), 0.15, 0.2, 0.4).sub ("Sine -1", 0.35); break;
                case 5: p.wt (0, "Saw Stack", p.j (0.6, 0.3), 0.85).uni (0, 2, 0.12, 0.3).sub ("Sine -1", 0.5).filt (0, "MG Low 24", p.j (400, 0.3), 0.45, 0.4)
                         .lfoSync (0, "Sine", v % 3 == 0 ? "1/8" : v % 3 == 1 ? "1/8 T" : "1/4", 1.0, 0.1).mod ("LFO 1", "Filter 1 Cutoff", 0.5); break;
                case 6: p.an (0, "Pulse", 0.8).pw (0, 0.35).an (1, "Square", 0.5).tune (1, -1).sub ("Sine -1", 0.4).filt (0, "Low Pass 24", p.j (2200, 0.3), 0.2, 0.3)
                         .lfo (0, "Triangle", p.j (0.3, 0.4)).mod ("LFO 1", "Osc A PWM", 0.35); break;
                case 7: p.wt (0, "Vowel Sweep", p.j (0.3, 0.6), 0.85).uni (0, 2, 0.14, 0.4).sub ("Sine -1", 0.5).filt (0, "Low Pass 12", p.j (2500, 0.3), 0.2)
                         .mono (p.j (130, 0.3)).mod ("Mod Wheel", "Osc A WT Pos", 0.6); break;
                case 8: p.wt (0, v % 2 ? "Electric FM" : "Metal FM", p.j (0.55, 0.4), 0.85).sub ("Sine -1", 0.45).filt (0, "Low Pass 24", 5000, 0.1)
                         .env (1, 0.001, p.j (0.35, 0.3), 0.15, 0.2).mod ("ENV 2", "Osc A WT Pos", 0.5); break;
                default: p.an (0, "Saw", 0.8).uni (0, 2, 0.1, 0.3).sub ("Sine -1", 0.5).filt (0, "MG Low 24", p.j (300, 0.3), 0.35, 0.3, 0.3)
                         .env (0, 0.002, p.j (0.45, 0.3), 0.35, 0.15).env (1, 0.001, p.j (0.22, 0.3), 0.0, 0.1).mod ("ENV 2", "Filter 1 Cutoff", 0.55); break;
            }
            if (a != 9) p.env (0, 0.004, 0.5, 0.95, p.j (0.18, 0.3));
            p.stereo (1.1, true, 140.0).comp (-18, 3, 3).noSat();
            applyFlavour (p, f, true);
            finishSpec (s, desc[a], f, { a < 4 ? "Reese" : "Wave Bass", "Mono" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // 808 (90): trending distorted 808s + powerful clean low end
    //==========================================================================
    void build808 (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Viral", "Blown", "Gravel", "Lava", "Crushed", "Thunder", "Smoke", "Chrome", "Dirty", "Slime", "Tar", "Concrete", "Cursed", "Heavy", "Ghost" },
                      { "808", "Boom", "Slide", "Knock", "Rumble", "Quake", "Clipper", "Drop", "Pressure", "Gut Punch" },
                      { "Void 808", "Viral 808", "Glide 808", "Grime 808", "", "", "Knock 808", "", "Clipper 808" } };
        const auto names = makeNames (pools, 90, used);
        static const char* desc[9] = {
            "Long, clean 808: sine body, fast pitch drop, gentle saturation.",
            "Hard-clipped viral 808: drive into a clipper, OTT and a low-end lift. Loud and in your face.",
            "Tube 808 with glide: overlapping notes slide like a classic trap slide.",
            "808 Body wavetable folding into grit as you hit harder.",
            "Triangle-drive 808: square-ish growl over the sub.",
            "Detuned 808: a little unison beating for width in the harmonics, mono below.",
            "Knock 808: short, punchy, with a kick-thump click on the front.",
            "Lo-fi 808: crushed and downsampled for that dusty, sampled feel.",
            "Rectified 808: octave-up distortion that cuts through phone speakers.",
        };
        const Flavour fl[] = { Dirty, OTT, Clean, Grit, Tape, Dirty, OTT, LoFi, Grit, Clean };
        for (int i = 0; i < 90; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "808";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 9, v = i / 9;
            const Flavour f = fl[(size_t) (v + a) % 10];
            p.an (0, "Sine", 0.9).tight (0, 0.25).tune (0, -1).filt (0, "Off", 12000, 0.0).drift (0.05).voices (1)
             .env (0, 0.001, p.j (2.2, 0.3), 0.0, p.j (0.5, 0.3)).env (2, 0.001, p.j (0.07, 0.3), 0.0, 0.06, -0.5).mod ("ENV 3", "Osc A Pitch", p.j (0.45, 0.25))
             .mono (p.j (90, 0.4), v % 2 == 0).level (-8.0).noSat();
            switch (a)
            {
                case 0: p.sat (0.3); break;
                case 1: p.dist ("Hard Clip", p.j (0.65, 0.2), 0.85, 0.5).ott (0.5, 0.3, 0.0).eq (3.0, 180, 2.0, -2.0, 60.0); break;
                case 2: p.dist ("Tube", p.j (0.5, 0.25), 0.65, 0.45).mono (p.j (160, 0.3), true); break;
                case 3: p.wt (0, "808 Body", p.j (0.5, 0.4), 0.9).tune (0, -1).fold (0, p.j (0.2, 0.4)).mod ("Velocity", "Osc A Fold", 0.35).dist ("Soft Cubic", 0.4, 0.6); break;
                case 4: p.an (1, "Triangle", 0.45).tune (1, 0).tight (1).dist ("Tube", 0.55, 0.6, 0.4).sat (0.3); break;
                case 5: p.an (1, "Saw", 0.25).tune (1, 0).uni (1, 3, 0.12, 0.6).filt (1, "Low Pass 12", 1400, 0.1).dist ("Hard Clip", 0.45, 0.5).stereo (1.2, true, 180.0); break;
                case 6: p.env (0, 0.001, p.j (0.45, 0.3), 0.0, 0.12).smp ("Kick Thump", 0.55, false).trans (0.6, -0.2).dist ("Soft Cubic", 0.35, 0.5); break;
                case 7: p.crush (p.j (8, 0.2), p.j (6, 0.3), 0.55).sat (0.4).eq (2.0, 1000, 2.0, -6.0); break;
                default: p.dist ("Rectify", p.j (0.5, 0.25), 0.5, 0.65).dist ("Rectify", p.j (0.5, 0.25), 0.5, 0.65).sat (0.25); break;
            }
            p.comp (-16, 4, 4, 15, 160).stereo (1.0, true, 200.0).eq (p.get (fxids::eqLowGain, 2.5), p.get (fxids::eqMidFreq, 300), p.get (fxids::eqMidGain, -1.0), p.get (fxids::eqHighGain, 0.0), 55.0);
            applyFlavour (p, f, true);
            finishSpec (s, desc[a], f, { "808", "Trap" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // SUB (50): clean, powerful sub-bass
    //==========================================================================
    void buildSub (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Deep", "Pure", "Ocean", "Bedrock", "Core", "Silent", "Low", "Gravity", "Basement", "Earth" },
                      { "Sub", "Floor", "Weight", "Pulse", "Foundation", "Hum", "Swell" },
                      { "Pure Sub", "", "", "", "Deep Sub" } };
        const auto names = makeNames (pools, 50, used);
        static const char* desc[5] = {
            "Pure sine sub, mono, phase-locked for a consistent hit.",
            "Triangle sub: a touch of odd harmonics so it reads on small speakers.",
            "Sine sub with a whisper of 2nd harmonic from the 808 Body table.",
            "Warm saturated sub, glued and centred.",
            "Gliding sub: legato portamento for basslines that slide.",
        };
        for (int i = 0; i < 50; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "SUB";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 5, v = i / 5;
            p.tight (0, 0.25).tune (0, -1).filt (0, "Low Pass 24", p.j (900, 0.3), 0.05).drift (0.04).voices (1)
             .env (0, p.j (0.004, 0.5), 0.5, 1.0, p.j (0.15, 0.4)).mono (a == 4 ? p.j (120, 0.4) : 0.0, true).level (-7.0).noSat();
            switch (a)
            {
                case 0: p.an (0, "Sine", 0.9); break;
                case 1: p.an (0, "Triangle", 0.85); break;
                case 2: p.wt (0, "808 Body", p.j (0.25, 0.4), 0.9); break;
                case 3: p.an (0, "Sine", 0.9).sat (p.j (0.35, 0.3)); break;
                default: p.an (0, "Sine", 0.9); break;
            }
            const Flavour f = (v % 3 == 0) ? Clean : (v % 3 == 1 ? Tape : OTT);
            p.comp (-18, 3, 3, 20, 200).stereo (1.0, true, 300.0).eq (1.5, 250, -1.0, -3.0, 50.0);
            applyFlavour (p, f, true);
            finishSpec (s, desc[a], f, { "Sub", "Mono" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // LEAD (100)
    //==========================================================================
    void buildLead (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Seraph", "Halo", "Neon", "Crystal", "Solar", "Astral", "Tear", "Silver", "Falling", "Lucid", "Holy", "Electric", "Nova", "Prism" },
                      { "Lead", "Saw", "Cry", "Song", "Scream", "Wire", "Line", "Hymn", "Ray", "Signal" },
                      { "Into The Void", "Banshee", "Angel Chop", "Always Elsewhere", "Eskimo Ice", "", "8-Bit Ghost" } };
        const auto names = makeNames (pools, 100, used);
        static const char* desc[10] = {
            "Seven-voice supersaw lead for drops and toplines.",
            "Folded screech lead: aggressive and bright, cuts through anything.",
            "Vocal-chop style formant lead; the mod wheel moves the vowel.",
            "Hard-sync lead: the sync ratio sweeps on every note.",
            "PWM square lead, eski-cold and hollow.",
            "FM bell lead: glassy attack, round body.",
            "8-bit chip lead: crushed square with a fast vibrato.",
            "Soft sine lead with vibrato on the wheel: nostalgic and emotional.",
            "Glide lead: monophonic portamento between notes.",
            "Choir lead: sampled ahh layered with a detuned saw.",
        };
        const Flavour fl[] = { Space, Wide, Dirty, Shimmer, Dream, Motion, Tape, OTT, Space, LoFi, Wide, Grit };
        for (int i = 0; i < 100; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "LEAD";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 10, v = i / 10;
            const Flavour f = fl[(size_t) (v * 3 + a) % 12];
            p.env (0, 0.004, 0.4, 0.85, p.j (0.32, 0.3)).voices (8).level (-8.0).drift (0.3).noSat();
            switch (a)
            {
                case 0: p.an (0, "Saw", 0.75).uni (0, 7, p.j (0.42, 0.2), 0.85, 0.85).wt (1, "Saw Stack", 0.5, 0.35).uni (1, 3, 0.3, 0.8).tune (1, 1)
                         .filt (0, "MG Low 24", p.j (6500, 0.25), 0.18, 0.1, 0.3).env (1, 0.002, 0.5, 0.3, 0.4).mod ("ENV 2", "Filter 1 Cutoff", 0.25); break;
                case 1: p.wt (0, "Sine Fold", p.j (0.6, 0.3), 0.8).uni (0, 3, 0.18, 0.7).fold (0, p.j (0.35, 0.3)).filt (0, "Low Pass 12", 7000, 0.2)
                         .dist ("Hard Clip", 0.4, 0.4); break;
                case 2: p.wt (0, v % 2 ? "Vowel Sweep" : "Formant Vox", p.j (0.4, 0.4), 0.85).uni (0, 3, 0.2, 0.7).filt (0, "Low Pass 12", p.j (5500, 0.25), 0.15)
                         .mod ("Mod Wheel", "Osc A WT Pos", 0.5).mono (50.0); break;
                case 3: p.wt (0, v % 2 ? "Sync Saw" : "Laser Sync", 0.2, 0.8).uni (0, 3, 0.2, 0.6).filt (0, "Low Pass 12", 8000, 0.1)
                         .env (1, 0.001, p.j (0.35, 0.3), 0.2, 0.3).mod ("ENV 2", "Osc A WT Pos", 0.6); break;
                case 4: p.an (0, "Pulse", 0.8).pw (0, 0.3).an (1, "Square", 0.4).tune (1, 1).filt (0, "Low Pass 24", 4500, 0.15)
                         .lfo (0, "Triangle", p.j (0.4, 0.3)).mod ("LFO 1", "Osc A PWM", 0.35).delay ("1/8 D", 0.35, 0.18); break;
                case 5: p.wt (0, v % 2 ? "Bell FM" : "Glass FM", p.j (0.35, 0.4), 0.8).filt (0, "Low Pass 12", 9000, 0.1).env (0, 0.002, 0.8, 0.5, 0.6)
                         .env (1, 0.001, 0.6, 0.1, 0.4).mod ("ENV 2", "Osc A WT Pos", 0.4); break;
                case 6: p.an (0, "Square", 0.8).crush (6, 6, 0.6).filt (0, "Off", 12000, 0.0).lfo (0, "Sine", 6.5).mod ("LFO 1", "Osc A Pitch", 0.004).mono (30.0); break;
                case 7: p.an (0, "Sine", 0.85).an (1, "Triangle", 0.35).tune (1, 1).filt (0, "Low Pass 12", 6000, 0.1).lfo (0, "Sine", 5.2, 0.0)
                         .mod ("Mod Wheel", "Osc A Pitch", 0.004).mod ("LFO 1", "Osc A Pitch", 0.003).set (ids::lfo (0, "depth"), 0.6); break;
                case 8: p.an (0, "Saw", 0.8).uni (0, 3, 0.18, 0.5).an (1, "Square", 0.4).tune (1, -1).filt (0, "MG Low 24", p.j (4000, 0.3), 0.25, 0.2)
                         .mono (p.j (110, 0.4)); break;
                default: p.smp ("Choir Ahh", 0.7, true).smpLoop (0.2, 0.9).an (0, "Saw", 0.4).uni (0, 5, 0.3, 0.8).filt (0, "Low Pass 12", 5000, 0.1)
                          .env (0, 0.03, 0.5, 0.85, 0.6); break;
            }
            p.reverb (p.j (0.75, 0.15), 0.4, 0.22, 30.0).eq (-2.0, 2500, 1.5, 2.0, 150.0).comp (-20, 3, 3);
            applyFlavour (p, f, false);
            finishSpec (s, desc[a], f, { "Lead" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // SYNTH (90): chords, stabs, keys, organs, arps, gated / pumping synths
    //==========================================================================
    void buildSynth (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Strobe", "Pulse", "Velvet", "Ivory", "Midnight", "Chrome", "Retro", "Gold", "Static", "Paper", "Dusk", "Arcade" },
                      { "Stab", "Keys", "Chords", "Organ", "Synth", "Arp", "Brass", "Strings", "Machine", "Run" },
                      { "Cold Stab", "Gospel Organ", "Soft Keys", "Analog Brass", "", "Strobe Heaven", "Pulse Of Heaven", "Falling Stars" } };
        const auto names = makeNames (pools, 90, used);
        static const char* desc[9] = {
            "Supersaw chord stab: short, wide and loud.",
            "Drawbar organ that opens up as you play.",
            "E-piano tine: bell attack, warm body, tremolo on the wheel.",
            "Brass swell: the table brightens as each chord blooms.",
            "Poly synth: two detuned saws, filter envelope, classic and warm.",
            "Gated chords: a hard sixteenth-note trance gate.",
            "Pumping chords: wide saws ducking on every quarter note.",
            "Hold a chord: a synced arpeggio plays it for you.",
            "String ensemble: slow, lush, cinematic.",
        };
        const Flavour fl[] = { Wide, Space, Tape, Dream, Motion, OTT, Clean, Shimmer, LoFi, Pumped };
        for (int i = 0; i < 90; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "SYNTH";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 9, v = i / 9;
            Flavour f = fl[(size_t) (v + 2 * a) % 10];
            if ((a == 5 || a == 6) && f == Pumped) f = Wide;
            p.voices (12).level (-8.0).drift (0.25).noSat();
            switch (a)
            {
                case 0: p.an (0, "Saw", 0.75).uni (0, 7, 0.4, 0.9).an (1, "Saw", 0.5).uni (1, 5, 0.3, 0.9).tune (1, 1).filt (0, "MG Low 24", p.j (5000, 0.3), 0.2)
                         .env (0, 0.002, p.j (0.35, 0.3), 0.0, 0.25).env (1, 0.001, 0.3, 0.0, 0.2).mod ("ENV 2", "Filter 1 Cutoff", 0.3); break;
                case 1: p.wt (0, v % 3 == 0 ? "Organ Jazz" : v % 3 == 1 ? "Organ Gospel" : "Church Reed", p.j (0.3, 0.6), 0.8).filt (0, "Low Pass 12", 9000, 0.1)
                         .env (0, 0.005, 0.2, 0.9, 0.15).mod ("Velocity", "Osc A WT Pos", 0.4).set (fxids::tremOn, 0); break;
                case 2: p.wt (0, "E-Piano Tine", p.j (0.2, 0.6), 0.8).filt (0, "Low Pass 12", 7000, 0.1).env (0, 0.002, 1.6, 0.25, 0.5)
                         .mod ("Velocity", "Osc A WT Pos", -0.3).trem ("Sine", "1/8", 0.0).mod ("Mod Wheel", "Amp", -0.3); break;
                case 3: p.wt (0, "Brass Swell", 0.1, 0.8).uni (0, 3, 0.15, 0.6).filt (0, "Low Pass 12", 6000, 0.1).env (0, 0.06, 0.6, 0.8, 0.4)
                         .env (1, 0.25, 0.6, 0.6, 0.4).mod ("ENV 2", "Osc A WT Pos", 0.7); break;
                case 4: p.an (0, "Saw", 0.7).uni (0, 2, 0.12, 0.6).an (1, "Saw", 0.6).tune (1, 0, 0, 9).filt (0, "MG Low 24", p.j (1500, 0.3), 0.3, 0.2, 0.3)
                         .env (0, 0.005, 0.8, 0.6, 0.4).env (1, 0.002, p.j (0.5, 0.3), 0.2, 0.3).mod ("ENV 2", "Filter 1 Cutoff", 0.4); break;
                case 5: p.an (0, "Saw", 0.7).uni (0, 7, 0.35, 0.9).filt (0, "Low Pass 12", 6000, 0.15).env (0, 0.005, 0.5, 0.9, 0.3).trem ("Square", "1/16", 0.95); break;
                case 6: p.an (0, "Saw", 0.7).uni (0, 7, 0.38, 1.0).an (1, "Square", 0.35).tune (1, -1).filt (0, "MG Low 24", 7000, 0.15).env (0, 0.01, 0.5, 0.9, 0.3)
                         .trem ("Sine", "1/4", 0.7); break;
                case 7: p.wt (0, v % 2 ? "Crystal" : "Glass FM", p.j (0.3, 0.5), 0.8).filt (0, "Low Pass 12", 7000, 0.15).env (0, 0.002, 0.3, 0.2, 0.25)
                         .arp (v % 3 == 0 ? "Up" : v % 3 == 1 ? "Up/Down" : "Random", "1/16", 2 + v % 2, 0.5, v % 4 == 0 ? 0.2 : 0.0).delay ("1/8 D", 0.4, 0.25); break;
                default: p.wt (0, "String Ensemble", p.j (0.5, 0.4), 0.8).uni (0, 5, 0.25, 0.9).filt (0, "Low Pass 12", 5000, 0.1).env (0, p.j (0.35, 0.3), 1.0, 0.9, 1.2); break;
            }
            p.reverb (p.j (0.7, 0.15), 0.4, 0.22, 25.0).eq (-2.0, 3000, 1.0, 1.5, 150.0).comp (-20, 3, 3).stereo (1.3);
            applyFlavour (p, f, false);
            finishSpec (s, desc[a], f, { a == 7 ? "Arp" : "Synth" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // PLUCK (80)
    //==========================================================================
    void buildPluck (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Crystal", "Glass", "Frost", "Rain", "Dew", "Star", "Ice", "Moon", "Silk", "Pearl", "Snow", "Spark" },
                      { "Pluck", "Drop", "Harp", "Chime", "Bell", "Tine", "Box", "Mallet" },
                      { "Glass Pluck", "Crystal Rain", "", "", "", "Halo Bells", "", "Clang Pluck", "", "", "", "", "", "Ice Bell" } };
        const auto names = makeNames (pools, 80, used);
        static const char* desc[8] = {
            "Glassy wavetable pluck with a fast filter snap.",
            "Plucked string: a physical pluck sample with a filtered tail.",
            "Mallet: woody attack, round decay.",
            "Kalimba: tuned tines, warm and intimate.",
            "Music box: tiny, nostalgic, high and fragile.",
            "Bell pluck: FM bell partials ringing out.",
            "Wave pluck: detuned saw blip into dotted echoes.",
            "Metal pluck: a pulse through a key-tracked comb, clangy and tuned.",
        };
        const Flavour fl[] = { Space, Dream, Wide, Tape, Shimmer, LoFi, Clean, Motion, OTT, Space };
        for (int i = 0; i < 80; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "PLUCK";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 8, v = i / 8;
            const Flavour f = fl[(size_t) (v + a) % 10];
            p.voices (12).level (-8.0).drift (0.2).noSat().env (0, 0.001, p.j (0.45, 0.3), 0.0, p.j (0.4, 0.3));
            switch (a)
            {
                case 0: p.wt (0, "Glass Halo", p.j (0.4, 0.5), 0.8).uni (0, 2, 0.12, 0.6).filt (0, "Low Pass 24", p.j (1200, 0.3), 0.2, 0.0, 0.4)
                         .env (1, 0.001, p.j (0.18, 0.3), 0.0, 0.15).mod ("ENV 2", "Filter 1 Cutoff", 0.55); break;
                case 1: p.smp ("Pluck Body", 0.85, true).filt (0, "Low Pass 12", p.j (5000, 0.3), 0.1, 0.0, 0.5).an (0, "Triangle", 0.25).env (0, 0.001, 1.6, 0.0, 0.5); break;
                case 2: p.wt (0, "Mallet", p.j (0.3, 0.6), 0.85).filt (0, "Low Pass 12", 6000, 0.1).env (0, 0.001, 0.6, 0.0, 0.3); break;
                case 3: p.wt (0, "Kalimba", p.j (0.3, 0.6), 0.85).filt (0, "Low Pass 12", 5000, 0.1).env (0, 0.001, 0.9, 0.0, 0.5); break;
                case 4: p.wt (0, "Music Box", p.j (0.3, 0.6), 0.8).tune (0, 1).filt (0, "Low Pass 12", 9000, 0.1).env (0, 0.001, 1.2, 0.0, 0.7); break;
                case 5: p.wt (0, v % 2 ? "Bell FM" : "Glass FM", p.j (0.5, 0.4), 0.8).filt (0, "Low Pass 12", 9000, 0.1).env (0, 0.001, 1.4, 0.0, 0.9)
                         .env (1, 0.001, 0.5, 0.0, 0.4).mod ("ENV 2", "Osc A WT Pos", 0.4); break;
                case 6: p.an (0, "Saw", 0.75).uni (0, 5, 0.3, 0.8).filt (0, "MG Low 24", p.j (800, 0.3), 0.3, 0.1, 0.4)
                         .env (1, 0.001, p.j (0.15, 0.3), 0.0, 0.1).mod ("ENV 2", "Filter 1 Cutoff", 0.6).delay ("1/8 D", 0.45, 0.28); break;
                default: p.an (0, "Pulse", 0.75).pw (0, 0.2).filt (0, "Comb +", 261.6, 0.6, 0.0, 1.0, 0.6).filt (1, "Low Pass 12", 6000, 0.1)
                          .env (0, 0.001, 0.35, 0.0, 0.3); break;
            }
            p.reverb (p.j (0.65, 0.2), 0.45, 0.24, 22.0).eq (-3.0, 2500, 1.0, 2.0, 180.0).stereo (1.3);
            applyFlavour (p, f, false);
            finishSpec (s, desc[a], f, { "Pluck" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // PAD (100)
    //==========================================================================
    void buildPad (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Ether", "Celestial", "Seraph", "Winter", "Northern", "Fading", "Endless", "Sacred", "Void", "Heaven", "Lunar", "Distant", "Velvet", "Last" },
                      { "Pad", "Bloom", "Choir", "Haze", "Light", "Dream", "Veil", "Horizon", "Glow", "Memory" },
                      { "Seraph Bloom", "Angel Choir", "Grey Skies", "Celestial Pad", "", "", "", "Dark Drone", "Ambient Wash", "Northern Lights" } };
        const auto names = makeNames (pools, 100, used);
        static const char* desc[10] = {
            "Slow, emotional wavetable pad whose frames drift under a slow LFO.",
            "Choir pad: sampled ahh voices and a formant table.",
            "Dark pad: hollow organ and low-passed saws.",
            "Shimmer pad: glass partials and an octave-up halo.",
            "Evolving pad: random-LFO motion through the chaos tables.",
            "String pad: wide ensemble, slow attack.",
            "Nostalgic tape pad: warm, wobbly and a little worn.",
            "Ambient drone: sustained, gliding, endless.",
            "Air pad: breathy noise layer over soft sines.",
            "Supersaw pad: big detuned chords for wave music drops.",
        };
        const Flavour fl[] = { Shimmer, Dream, Space, Wide, Motion, Tape, LoFi, Space, Shimmer, Dream, Wide, Clean };
        for (int i = 0; i < 100; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "PAD";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 10, v = i / 10;
            const Flavour f = fl[(size_t) (v * 5 + a) % 12];
            p.voices (16).level (-9.0).drift (0.42).noSat().env (0, p.j (0.9, 0.4), 1.5, 0.85, p.j (2.6, 0.3));
            switch (a)
            {
                case 0: p.wt (0, v % 2 ? "Formant Vox" : "Harmonic Sweep", p.j (0.35, 0.5), 0.85).uni (0, 5, 0.35, 0.85).wt (1, "Glass Halo", 0.2, 0.4).uni (1, 3, 0.25, 0.9).tune (1, 1)
                         .filt (0, "Low Pass 12", p.j (4500, 0.3), 0.15).lfo (0, "Triangle", p.j (0.07, 0.4), 1.0, 0.6).mod ("LFO 1", "Osc A WT Pos", 0.3); break;
                case 1: p.smp ("Choir Ahh", 0.8, true).smpLoop (0.2, 0.92).wt (0, "Choir Formant", p.j (0.4, 0.5), 0.5).uni (0, 5, 0.3, 0.9)
                         .filt (0, "Low Pass 12", 5000, 0.1); break;
                case 2: p.wt (0, "Pipe Organ", p.j (0.5, 0.4), 0.7).an (1, "Saw", 0.45).uni (1, 5, 0.3, 0.9).filt (0, "Low Pass 24", p.j (1100, 0.3), 0.2, 0.1); break;
                case 3: p.wt (0, "Crystal", p.j (0.5, 0.4), 0.7).uni (0, 3, 0.2, 0.9).wt (1, "Seventh Heaven", 0.6, 0.45).tune (1, 1).filt (0, "Low Pass 12", 9000, 0.1); break;
                case 4: p.wt (0, v % 4 == 0 ? "Chaos I" : v % 4 == 1 ? "Chaos II" : v % 4 == 2 ? "Chaos III" : "Chaos IV", 0.5, 0.75).uni (0, 4, 0.3, 0.9)
                         .filt (0, "Low Pass 12", 4000, 0.15).lfo (1, "Random", p.j (0.15, 0.4), 1.0, 1.0).mod ("LFO 2", "Osc A WT Pos", 0.4).mod ("LFO 2", "Filter 1 Cutoff", 0.08); break;
                case 5: p.wt (0, "String Ensemble", p.j (0.5, 0.4), 0.8).uni (0, 7, 0.35, 1.0).filt (0, "Low Pass 12", 5500, 0.1); break;
                case 6: p.an (0, "Saw", 0.6).uni (0, 3, 0.2, 0.7).an (1, "Triangle", 0.5).tune (1, -1).filt (0, "MG Low 24", 2500, 0.15).crush (10, 3, 0.25)
                         .lfo (0, "Sine", 0.35).mod ("LFO 1", "Osc A Pitch", 0.002); break;
                case 7: p.wt (0, "Throat Singer", p.j (0.4, 0.5), 0.7).tune (0, -1).wt (1, "Dark To Bright", p.j (0.3, 0.5), 0.5).filt (0, "Low Pass 24", 2200, 0.25)
                         .env (0, 2.5, 2.0, 1.0, 5.0).lfo (0, "Sine", 0.05, 1.0, 0.5).mod ("LFO 1", "Osc B WT Pos", 0.4); break;
                case 8: p.an (0, "Sine", 0.6).an (1, "Sine", 0.4).tune (1, 1, 7).noise ("Bright", 0.06).smp ("Air Breath", 0.3, false).smpLoop (0.1, 0.95).filt (0, "Low Pass 12", 7000, 0.1); break;
                default: p.an (0, "Saw", 0.7).uni (0, 8, 0.45, 1.0, 0.8).an (1, "Saw", 0.4).uni (1, 6, 0.35, 1.0).tune (1, 1).filt (0, "MG Low 24", 6000, 0.15); break;
            }
            p.reverb (p.j (0.85, 0.1), 0.4, 0.38, 32.0).eq (-3.0, 400, -1.5, 1.5, 200.0).stereo (1.4);
            applyFlavour (p, f, false);
            finishSpec (s, desc[a], f, { "Pad" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // PERCUSSION (77): tempo-synced hat + perc loops (hold one key)
    //==========================================================================
    void buildPerc (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Midnight", "Cold", "Glass", "Trap", "Drill", "Wave", "Ghost", "Rusty", "Silver", "Smoke", "Ice", "Neon" },
                      { "Hats", "Rolls", "Shakers", "Ticks", "Perc", "Rims", "Bounce", "Groove" },
                      { "Trap Hats", "Metal Hats", "Loose Shaker", "Noise Hats", "Rim Groove", "Clap Rolls", "Wave Perc" } };
        const auto names = makeNames (pools, 77, used);
        static const char* desc[7] = {
            "Hold any key: a synced closed-hat loop.",
            "Hold any key: metallic 808-style hats from synthesized oscillators.",
            "Hold any key: shaker loop with a loose, human swing.",
            "Hold any key: synthesized noise hats, filtered and crisp.",
            "Hold a key around C4: pitched rim / tick perc loop (the key transposes it).",
            "Hold any key: claps and snaps with trap rolls.",
            "Hold a key around C4: wave-style perc in a big space.",
        };
        static const char* hatSamples[] = { "Hat Closed", "Hat Metal", "Hat Closed" };
        const Flavour fl[] = { Clean, Space, LoFi, Grit, Wide, Tape, Dream, OTT };
        for (int i = 0; i < 77; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "PERCUSSION";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 7, v = i / 7;
            const Flavour f = fl[(size_t) (v + a) % 8];
            const auto& hp = hatPatterns[(size_t) (i % 11)];
            const auto& pp = percPatterns[(size_t) (i % 6)];
            p.set (ids::osc (0, "on"), 0).filt (0, "Off", 12000, 0.0).voices (16).level (-9.0).drift (0.0).noSat()
             .env (0, 0.001, 0.3, 1.0, 0.04).set (ids::velSens, 0.9);
            juce::String patternName;
            switch (a)
            {
                case 0: p.smp (hatSamples[v % 3], 0.85, false, v % 4 - 1).filt (0, "High Pass 12", p.j (5000, 0.3), 0.1)
                         .steps (hp.lvl, hp.rat, hp.gate, {}, hp.rate, (int) strlen (hp.lvl), hp.swing, true); patternName = hp.name; break;
                case 1: p.wt (0, "Metal FM", p.j (0.7, 0.2), 0.7).tune (0, 2).filt (0, "High Pass 24", p.j (6500, 0.2), 0.2).filt (1, "Band Pass", 9000, 0.3)
                         .env (0, 0.001, p.j (0.045, 0.3), 0.0, 0.04).ring (p.j (3400, 0.3), 0.3)
                         .steps (hp.lvl, hp.rat, hp.gate, {}, hp.rate, (int) strlen (hp.lvl), hp.swing, true); patternName = hp.name; break;
                case 2: p.smp ("Shaker", 0.85, false).filt (0, "High Pass 12", 4000, 0.1)
                         .steps ("7595759575957595", "1", "s", {}, v % 2 ? "1/16 T" : "1/16", 16, 0.3 + 0.05 * (v % 3), true); patternName = "loose shaker"; break;
                case 3: p.noise ("White", 0.8).filt (0, "High Pass 24", p.j (7500, 0.25), 0.25).filt (1, "Peak", 11000, 0.4)
                         .env (0, 0.001, p.j (0.04, 0.3), 0.0, 0.03)
                         .steps (hp.lvl, hp.rat, hp.gate, {}, hp.rate, (int) strlen (hp.lvl), hp.swing, true); patternName = hp.name; break;
                case 4: p.smp (v % 2 ? "Rim" : "Perc Tick", 0.85, true).filt (0, "Low Pass 12", 9000, 0.1)
                         .steps (pp.lvl, pp.rat, pp.gate, std::initializer_list<int> {}, pp.rate, (int) strlen (pp.lvl), pp.swing, true); patternName = pp.name;
                        for (int st = 0; st < kNumSteps; ++st) p.set (ids::step (st, "pitch"), pp.pitch[(size_t) st % pp.pitch.size()]);
                        break;
                case 5: p.smp (v % 2 ? "Snap" : "Clap", 0.85, false).filt (0, "High Pass 12", 600, 0.1)
                         .steps ("0000900000009000", "1111111111111131", "m", {}, "1/16", 16, 0.0, true).reverb (0.5, 0.5, 0.2, 10.0); patternName = "backbeat with a roll"; break;
                default: p.smp (v % 3 == 0 ? "Glass Hit" : v % 3 == 1 ? "Metal Hit" : "Rim", 0.75, true, -12).filt (0, "Low Pass 12", 7000, 0.1)
                          .env (0, 0.001, 0.25, 0.0, 0.2)
                          .steps (pp.lvl, pp.rat, pp.gate, {}, pp.rate, (int) strlen (pp.lvl), pp.swing, true).reverb (0.9, 0.4, 0.42, 40.0).delay ("1/8 D", 0.4, 0.22);
                         for (int st = 0; st < kNumSteps; ++st) p.set (ids::step (st, "pitch"), pp.pitch[(size_t) st % pp.pitch.size()]);
                         patternName = pp.name; break;
            }
            p.eq (-6.0, 3000, 1.5, 2.5, 250.0).comp (-18, 3, 3, 2, 60).trans (0.35, -0.2).stereo (1.2);
            applyFlavour (p, f, false);
            finishSpec (s, juce::String (desc[a]) + " Pattern: " + patternName + ", locked to your project tempo.", f, { "Loop", "Synced" });
            out.push_back (std::move (s));
        }
    }

    //==========================================================================
    // FX (80): risers, downlifters, impacts, sweeps, noise patterns, swells, sirens, zaps, atmospheres, glitches
    //==========================================================================
    void buildFx (std::vector<Spec>& out, juce::StringArray& used)
    {
        Pools pools { { "Heaven", "Void", "Tension", "Rapture", "Gravity", "Static", "Storm", "Signal", "Eclipse", "Comet", "Pressure", "Ascension" },
                      { "Riser", "Sweep", "Impact", "Downer", "Wash", "Siren", "Zap", "Swell", "Glitch", "Transition" },
                      { "Noise Riser", "Downlifter", "Cinematic Impact", "Transition Sweep", "Noise Pattern", "Reverse Swell", "Pirate Siren", "Laser Zap", "Rain Atmos", "Glitch Machine" } };
        const auto names = makeNames (pools, 80, used);
        static const char* desc[10] = {
            "White-noise riser: hold for four bars, the filter and pitch climb the whole way.",
            "Downlifter: pitch and filter fall away for transitions.",
            "Impact: boom, crack and a long dark tail.",
            "Transition sweep: band-passed noise swept by a synced LFO.",
            "White-noise pattern: synced gated noise bursts for builds.",
            "Reverse swell into the drop.",
            "Siren: a classic up-down pitch sweep.",
            "Laser zap: a fast pitch dive, great as ear candy.",
            "Atmosphere: rain, wind and vinyl dust, looped.",
            "Glitch: stuttered, crushed, tempo-synced chops.",
        };
        const Flavour fl[] = { Space, Shimmer, Dirty, Wide, LoFi, Motion, Dream, Grit, Space, OTT };
        static const char* atmos[] = { "Rain", "Wind", "Vinyl Crackle", "Tape Hiss" };
        for (int i = 0; i < 80; ++i)
        {
            juce::Random r ((juce::int64) names[i].hashCode64()); Spec s; s.name = names[i]; s.category = "FX";
            auto& p = s.patch; p.rng = &r;
            const int a = i % 10, v = i / 10;
            const Flavour f = fl[(size_t) (v + a) % 10];
            p.voices (8).level (-9.0).drift (0.35).noSat();
            switch (a)
            {
                case 0: p.set (ids::osc (0, "on"), 0).noise ("White", 0.7).an (1, "Saw", 0.3).uni (1, 5, 0.5, 1.0).filt (0, "Band Pass", 300, 0.45)
                         .env (0, 0.6, 1.0, 1.0, 1.5).env (1, p.j (6.0, 0.3), 0.1, 1.0, 1.5).mod ("ENV 2", "Filter 1 Cutoff", 0.75).mod ("ENV 2", "Osc B Pitch", 1.0)
                         .delay ("1/8", 0.5, 0.3).reverb (0.9, 0.3, 0.45, 20.0); break;
                case 1: p.set (ids::osc (0, "on"), 0).noise ("Pink", 0.7).an (1, "Saw", 0.35).uni (1, 5, 0.4, 1.0).filt (0, "Low Pass 12", 9000, 0.3)
                         .env (0, 0.01, 3.5, 0.0, 1.0).env (1, 0.001, p.j (3.0, 0.3), 0.0, 1.0).mod ("ENV 2", "Filter 1 Cutoff", 0.6).mod ("ENV 2", "Osc B Pitch", 1.0)
                         .set (ids::filt (0, "cutoff"), 300).reverb (0.85, 0.4, 0.4, 10.0); break;
                case 2: p.set (ids::osc (0, "on"), 0).smp ("Impact Boom", 0.9, false).sub ("Sine -2", 0.4).filt (0, "Off", 12000, 0.0).env (0, 0.001, 3.5, 0.0, 2.5)
                         .reverb (0.95, 0.5, 0.35, 0.0).dist ("Tube", 0.35, 0.4).ott (0.4, 0.5, 0.0); break;
                case 3: p.set (ids::osc (0, "on"), 0).noise ("White", 0.75).filt (0, "Band Pass", 1500, 0.55).env (0, 0.2, 1.0, 1.0, 1.0)
                         .lfoSync (0, v % 2 ? "Saw Up" : "Triangle", v % 3 == 0 ? "1 bar" : "2 bars").mod ("LFO 1", "Filter 1 Cutoff", 0.5).reverb (0.8, 0.3, 0.35); break;
                case 4: p.set (ids::osc (0, "on"), 0).noise ("White", 0.8).filt (0, "High Pass 12", p.j (1200, 0.4), 0.2).env (0, 0.001, 0.12, 0.0, 0.08)
                         .steps ("9070907790709999", "1111111211111234", "smsmsmsmsmsmssss", {}, "1/16", 16, 0.0, true).delay ("1/8", 0.3, 0.2); break;
                case 5: p.set (ids::osc (0, "on"), 0).smp ("Reverse Swell", 0.85, true).filt (0, "Low Pass 12", 12000, 0.1).env (0, 0.001, 3.0, 1.0, 0.3)
                         .reverb (0.8, 0.4, 0.3, 10.0); break;
                case 6: p.an (0, "Saw", 0.6).uni (0, 3, 0.2, 0.7).an (1, "Square", 0.4).filt (0, "Low Pass 12", 5000, 0.2).env (0, 0.05, 1.0, 1.0, 0.8)
                         .lfo (0, v % 2 ? "Triangle" : "Sine", p.j (0.5, 0.4)).mod ("LFO 1", "Osc A Pitch", 0.3).mod ("LFO 1", "Osc B Pitch", 0.3).delay ("1/4", 0.5, 0.3); break;
                case 7: p.an (0, "Square", 0.7).tune (0, 2).filt (0, "Low Pass 12", 9000, 0.3).env (0, 0.001, 0.4, 0.0, 0.2)
                         .env (2, 0.001, p.j (0.25, 0.4), 0.0, 0.1).mod ("ENV 3", "Osc A Pitch", 1.0).delay ("1/16", 0.55, 0.3); break;
                case 8: p.set (ids::osc (0, "on"), 0).smp (atmos[v % 4], 0.85, false).smpLoop (0.05, 0.98).filt (0, "Low Pass 12", 8000, 0.1)
                         .env (0, 1.0, 1.0, 1.0, 2.5).reverb (0.8, 0.5, 0.3, 30.0); break;
                default: p.wt (0, v % 2 ? "Digital Grit" : "8-Bit Crush", p.j (0.6, 0.3), 0.75).filt (0, "Low Pass 12", 7000, 0.2).env (0, 0.001, 0.15, 0.4, 0.05)
                          .steps ("9090999390909933", "1121112411211143", "s", { 0, 0, 12, 0, 0, -12, 0, 7 }, "1/16", 16, 0.0, true).crush (7, 5, 0.5); break;
            }
            p.eq (-4.0, 2500, 0.0, 2.0, 150.0).stereo (1.4);
            applyFlavour (p, f, false);
            finishSpec (s, desc[a], f, { "FX", a == 4 || a == 9 ? "Synced" : "Transition" });
            out.push_back (std::move (s));
        }
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
    buildBass (specs, used);
    build808 (specs, used);
    buildSub (specs, used);
    buildLead (specs, used);
    buildSynth (specs, used);
    buildPluck (specs, used);
    buildPad (specs, used);
    buildPerc (specs, used);
    buildFx (specs, used);

    for (auto& s : specs)
    {
        auto& p = s.patch;
        // LFO power follows usage: an LFO is on only if a route actually uses it
        for (int l = 0; l < kNumLfos; ++l)
        {
            bool usedLfo = false;
            for (int m = 0; m < kNumModSlots; ++m) usedLfo |= p.getChoice (ids::mod (m, "src")) == "LFO " + juce::String (l + 1);
            p.set (ids::lfo (l, "on"), usedLfo ? 1 : 0);
        }
        p.set (ids::master, juce::jlimit (-60.0, 6.0, p.get (ids::master, -8.0) + levelTrim (s.name)));

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
        list.push_back (std::move (fp));
    }
}

} // namespace divinity
