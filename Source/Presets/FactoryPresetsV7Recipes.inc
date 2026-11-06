// Included inside FactoryPresetsV7.cpp's anonymous namespace.
//
// Combinatorial preset design (0.7.3). The 0.7.0 bank cycled through ~10 fixed archetypes
// per category, so every 10th preset was the same core sound with a different effect
// flavour. Now each preset draws its SOURCE (oscillators / tables / layering), its
// MOVEMENT (what is modulated, by what, how fast), its FILTER, its ENVELOPE / PLAY style
// and its EFFECTS independently, each from a shuffled deck (every option is used before
// any repeats, decks have different lengths, so no fixed period) and no two presets in a
// category share the same source + movement + filter combination.
// Descriptions are assembled from the chosen ingredients, so they describe the actual patch.

struct Ing
{
    juce::String desc, tag;
    std::function<void (Patch&)> fn;
};
#define ING(desc, tag, ...) Ing { desc, tag, [] (Patch& p) { __VA_ARGS__; } }

/** Draw-without-replacement deck with reshuffle. */
struct Deck
{
    Deck (int size, juce::int64 seed) : n (size), rng (seed) { refill(); }
    int draw() { if (pos >= (int) order.size()) refill(); return order[(size_t) pos++]; }
    void refill()
    {
        order.resize ((size_t) n);
        for (int i = 0; i < n; ++i) order[(size_t) i] = i;
        for (int i = n - 1; i > 0; --i) std::swap (order[(size_t) i], order[(size_t) rng.nextInt (i + 1)]);
        pos = 0;
    }
    int n, pos = 0;
    juce::Random rng;
    std::vector<int> order;
};

bool isWT (const Patch& p, int o = 0) { return p.getChoice (ids::osc (o, "engine")) == "Wavetable"; }
bool filterOn (const Patch& p) { auto t = p.getChoice (ids::filt (0, "type")); return t.isNotEmpty() && t != "Off"; }
/** A modulation target that exists on this patch: WT position on wavetable sources, else cutoff. */
const char* posOrCutoff (const Patch& p) { return isWT (p) ? "Osc A WT Pos" : (filterOn (p) ? "Filter 1 Cutoff" : "Osc A Fold"); }
const char* cutoffOrPos (const Patch& p) { return filterOn (p) ? "Filter 1 Cutoff" : (isWT (p) ? "Osc A WT Pos" : "Osc A Fold"); }

struct CategoryRecipe
{
    juce::String category;
    int count;
    Pools pools;
    std::vector<Ing> sources, motions, filters, styles;
    std::vector<Flavour> flavours;
    std::map<juce::String, int> heroSource;     // hero name -> forced source index
    std::map<juce::String, int> heroMotion;     // hero name -> forced movement index (optional)
    std::function<void (Patch&)> base;            // applied first
    std::function<void (Patch&)> finish;          // applied last (before effects)
    bool lowEnd = false;
    juce::StringArray tags;
};

/** One candidate design. Everything is drawn from a seed (not from the preset's name), so an offline
    tool can render hundreds of candidates, measure how they actually sound, and keep the set that is
    furthest apart (FactoryPresetsV7Picks.inc). */
Spec makeCandidate (const CategoryRecipe& R, int k, int forcedSource = -1, int forcedMotion = -1, int* sourceOut = nullptr)
{
    const juce::int64 seed = R.category.hashCode64();
    juce::Random cr (seed * 31 + (juce::int64) k * 7919 + 17);
    juce::Random jitter (seed * 131 + (juce::int64) k * 104729 + 5);
    Spec s; s.category = R.category; s.name = R.category + " #" + juce::String (k);
    auto& p = s.patch; p.rng = &jitter;

    const int src = forcedSource >= 0 ? forcedSource : cr.nextInt ((int) R.sources.size());
    const int drawnMot = cr.nextInt ((int) R.motions.size());
    const int mot = forcedMotion >= 0 ? forcedMotion : drawnMot, fil = cr.nextInt ((int) R.filters.size()), sty = cr.nextInt ((int) R.styles.size());
    const Flavour f1 = R.flavours[(size_t) cr.nextInt ((int) R.flavours.size())];
    const Flavour f2 = R.flavours[(size_t) cr.nextInt ((int) R.flavours.size())];
    const bool layered = cr.nextFloat() < 0.55f && f2 != f1;
    const int low = cr.nextInt (6);
    if (sourceOut != nullptr) *sourceOut = src;

    if (R.base) R.base (p);
    R.sources[(size_t) src].fn (p);
    R.filters[(size_t) fil].fn (p);
    R.styles[(size_t) sty].fn (p);
    R.motions[(size_t) mot].fn (p);
    // BASS: the sub layer is its own choice (it used to be the same sine under everything)
    juce::String lowDesc;
    if (R.category == "BASS" && ! p.has (ids::subOn))
    {
        switch (low)
        {
            case 0: lowDesc = "no sub layer: the source carries the low end"; break;
            case 1: p.sub ("Sine -1", p.j (0.5, 0.3)); lowDesc = "a sine sub an octave down"; break;
            case 2: p.sub ("Triangle -1", p.j (0.45, 0.3)); lowDesc = "a triangle sub for audible weight"; break;
            case 3: p.sub ("Square -1", p.j (0.3, 0.3)); lowDesc = "a square sub growling underneath"; break;
            case 4: p.sub ("Sine -2", p.j (0.4, 0.3)); lowDesc = "a deep sine two octaves down"; break;
            default: lowDesc = "no sub: mid-range bass that cuts"; p.tune (0, 1); break;
        }
    }
    if (R.finish) R.finish (p);
    applyFlavour (p, f1, R.lowEnd);
    if (layered) applyFlavour (p, f2, R.lowEnd);
    p.rng = nullptr;

    juce::StringArray tags { flavourTag[(int) f1] };
    if (layered) tags.add (flavourTag[(int) f2]);
    for (auto* t : { &R.sources[(size_t) src].tag, &R.motions[(size_t) mot].tag, &R.styles[(size_t) sty].tag })
        if (t->isNotEmpty() && ! tags.contains (*t)) tags.add (*t);
    tags.addArray (R.tags);
    s.tags = tags.joinIntoString (", ");
    auto cap = [] (juce::String x) { return x.substring (0, 1).toUpperCase() + x.substring (1); };
    s.description = cap (R.sources[(size_t) src].desc) + ", " + R.filters[(size_t) fil].desc + (lowDesc.isNotEmpty() ? ", " + lowDesc : juce::String()) + ". "
                  + cap (R.motions[(size_t) mot].desc) + "; " + R.styles[(size_t) sty].desc + ". FX: "
                  + flavourPhrase[(int) f1] + (layered ? juce::String (", plus ") + flavourPhrase[(int) f2] : juce::String()) + ".";
    return s;
}

/** Hero slots: slot index -> (forced source, forced motion), -1 = free. */
std::vector<std::pair<int, int>> heroSlots (const CategoryRecipe& R)
{
    std::vector<std::pair<int, int>> v ((size_t) R.count, { -1, -1 });
    for (size_t i = 0; i < R.pools.heroes.size() && i < v.size(); ++i)
    {
        const juce::String h (R.pools.heroes[i]);
        if (auto it = R.heroSource.find (h); it != R.heroSource.end()) v[i].first = it->second;
        if (auto it = R.heroMotion.find (h); it != R.heroMotion.end()) v[i].second = it->second;
    }
    return v;
}

const std::map<juce::String, std::vector<int>>& variedPicks()
{
    static const std::map<juce::String, std::vector<int>> m {
       #include "FactoryPresetsV7Picks.inc"
    };
    return m;
}

void buildFromRecipe (const CategoryRecipe& R, std::vector<Spec>& out, juce::StringArray& used)
{
    const auto names = makeNames (R.pools, R.count, used);
    const auto heroes = heroSlots (R);
    auto it = variedPicks().find (R.category);
    const bool havePicks = it != variedPicks().end() && (int) it->second.size() == R.count;
    for (int i = 0; i < R.count; ++i)
    {
        const int k = havePicks ? it->second[(size_t) i] : i;
        // a picked candidate for a hero slot was drawn with the hero's source forced
        auto s = makeCandidate (R, k, heroes[(size_t) i].first, heroes[(size_t) i].second);
        s.name = names[i];
        out.push_back (std::move (s));
    }
}

//==============================================================================
// BASS: reeses of every kind + wave-music bass
//==============================================================================
CategoryRecipe bassRecipe()
{
    CategoryRecipe R;
    R.category = "BASS"; R.count = 110; R.lowEnd = true; R.tags = { "Mono" };
    R.pools = { { "Abyss", "Cold", "Toxic", "Velvet", "Black", "Molten", "Ghost", "Hollow", "Iron", "Neon", "Static",
                  "Midnight", "Venom", "Glacier", "Rust", "Phantom", "Obsidian", "Cobalt" },
                { "Reese", "Growl", "Drift", "Sweep", "Pressure", "Snarl", "Current", "Tide", "Rumble", "Mangle", "Shift", "Engine" },
                { "Abyss Reese", "Wave Reese", "Neuro Reese", "Talking Reese", "Into The Void Bass", "Wobble Machine", "Concrete Growl", "Always Elsewhere Bass" } };
    R.base = [] (Patch& p) { p.mono (p.j (35.0, 0.6)).drift (0.15).voices (1).level (-7.0).env (0, 0.004, 0.5, 0.95, p.j (0.18, 0.3)).noSat(); };
    R.finish = [] (Patch& p) { p.stereo (1.1, true, 140.0).comp (-18, 3, 3); };
    R.sources = {
        ING ("two detuned analog saws: the classic reese", "Reese", p.an (0, "Saw", 0.8).uni (0, 2, p.j (0.16, 0.3), 0.4).an (1, "Saw", 0.7).tune (1, 0, 0, p.j (14, 0.4))),
        ING ("a five-voice supersaw reese over a sine sub", "Reese", p.an (0, "Saw", 0.75).uni (0, 5, p.j (0.28, 0.25), 0.7)),
        ING ("the Reese Core table, wide and beating", "Reese", p.wt (0, "Reese Core", p.j (0.45, 0.5), 0.85).uni (0, 3, p.j (0.2, 0.3), 0.5)),
        ING ("a Saw Stack wavetable reese", "Reese", p.wt (0, "Saw Stack", p.j (0.55, 0.4), 0.85).uni (0, 2, p.j (0.14, 0.3), 0.4)),
        ING ("a square-stack reese: hollow and cold", "Reese", p.wt (0, "Square Stack", p.j (0.5, 0.4), 0.8).uni (0, 3, p.j (0.18, 0.3), 0.5)),
        ING ("a warm Tube Saw reese", "Reese", p.wt (0, "Tube Saw", p.j (0.5, 0.5), 0.85).uni (0, 2, p.j (0.15, 0.3), 0.4).an (1, "Saw", 0.4).tune (1, -1, 0, 9)),
        ING ("a hard-sync saw reese that tears as it moves", "Reese", p.wt (0, "Sync Saw", p.j (0.3, 0.6), 0.8).uni (0, 2, 0.14, 0.4)),
        ING ("a wavefolded saw reese", "Reese", p.wt (0, "Saw Fold", p.j (0.35, 0.5), 0.8).fold (0, p.j (0.25, 0.5)).uni (0, 2, 0.15, 0.4)),
        ING ("a pulse-width reese (PWM Stack)", "Reese", p.wt (0, "PWM Stack", p.j (0.5, 0.4), 0.8).uni (0, 3, p.j (0.18, 0.3), 0.5)),
        ING ("a saw-to-square morphing reese", "Reese", p.wt (0, "Saw To Square", p.j (0.4, 0.6), 0.85).uni (0, 3, p.j (0.2, 0.3), 0.5)),
        ING ("a Neuro Growl table with a hollow comb edge", "Neuro", p.wt (0, "Neuro Growl", p.j (0.5, 0.5), 0.85).uni (0, 2, 0.12, 0.3).fold (0, p.j (0.2, 0.5))),
        ING ("FM growl (Growl FM table)", "Neuro", p.wt (0, "Growl FM", p.j (0.5, 0.5), 0.85)),
        ING ("a talking Wobble Talk table", "Talking", p.wt (0, "Wobble Talk", p.j (0.4, 0.6), 0.85).uni (0, 2, 0.12, 0.3)),
        ING ("the Yoi formant table: it says 'yoi'", "Talking", p.wt (0, "Yoi", p.j (0.4, 0.6), 0.85).uni (0, 2, 0.1, 0.3)),
        ING ("a Talk Box table, vocal and nasal", "Talking", p.wt (0, "Talk Box", p.j (0.4, 0.6), 0.85)),
        ING ("metallic FM grind (Metal FM)", "Neuro", p.wt (0, "Metal FM", p.j (0.45, 0.5), 0.8)),
        ING ("feedback-FM snarl", "Neuro", p.wt (0, "Feedback FM", p.j (0.5, 0.5), 0.8).uni (0, 2, 0.1, 0.3)),
        ING ("bit-crushed Digital Grit", "Grit", p.wt (0, "Digital Grit", p.j (0.5, 0.5), 0.8)),
        ING ("a robot-voice table", "Talking", p.wt (0, "Robot Voice", p.j (0.5, 0.5), 0.8).uni (0, 2, 0.1, 0.3)),
        ING ("a wave bass: pure sine welded to a detuned saw", "Wave Bass", p.an (0, "Sine", 0.85).tight (0).an (1, "Saw", 0.42).uni (1, 4, p.j (0.25, 0.3), 0.5).tune (1, 1)),
        ING ("a dark-to-bright sweep table", "Wave Bass", p.wt (0, "Dark To Bright", p.j (0.3, 0.6), 0.85).uni (0, 2, 0.12, 0.4)),
        ING ("odd harmonics only: a hollow, clarinet-ish bass", "Wave Bass", p.wt (0, "Odd Harmonics", p.j (0.5, 0.5), 0.85)),
        ING ("phase-distortion resonance (Reso PD), CZ-style", "Wave Bass", p.wt (0, "Reso PD", p.j (0.4, 0.6), 0.8)),
        ING ("a round Sub FM tone with harmonics on top", "Wave Bass", p.wt (0, "Sub FM", p.j (0.45, 0.5), 0.85).uni (0, 2, 0.08, 0.3)),
        ING ("an Electric FM bass", "Wave Bass", p.wt (0, "Electric FM", p.j (0.45, 0.5), 0.8)),
        ING ("diode-clipped wave: fuzzy and loud", "Grit", p.wt (0, "Diode Clip", p.j (0.5, 0.5), 0.8)),
        ING ("a fuzz-triangle bass", "Grit", p.wt (0, "Fuzz Tri", p.j (0.5, 0.5), 0.85).uni (0, 2, 0.1, 0.3)),
        ING ("a comb-swept table: phasey and metallic", "Neuro", p.wt (0, "Comb Sweep", p.j (0.4, 0.6), 0.8)),
        ING ("twin resonant peaks (Twin Peaks)", "Wave Bass", p.wt (0, "Twin Peaks", p.j (0.4, 0.6), 0.8).uni (0, 2, 0.1, 0.3)),
        ING ("an ooh-to-ahh vowel bass", "Talking", p.wt (0, "Ooh To Ahh", p.j (0.4, 0.6), 0.85)),
        ING ("analog square + pulse, an octave apart", "Wave Bass", p.an (0, "Pulse", 0.8).pw (0, p.j (0.32, 0.3)).an (1, "Square", 0.5).tune (1, -1)),
        ING ("a Hollow Pulse table under a quiet fifth", "Wave Bass", p.wt (0, "Hollow Pulse", p.j (0.5, 0.5), 0.85).an (1, "Saw", 0.25).tune (1, 0, 7)),
    };
    R.motions = {
        ING ("static, no movement: just weight", "", p.set (ids::analog, 0.12)),
        ING ("a slow free LFO drifts the timbre", "Moving", p.lfo (0, "Triangle", p.j (0.12, 0.5), 1.0, 0.5).mod ("LFO 1", posOrCutoff (p), p.j (0.3, 0.3))),
        ING ("a 1/4 synced wobble on the filter", "Wobble", p.lfoSync (0, "Sine", "1/4", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.5)),
        ING ("a 1/8 synced wobble", "Wobble", p.lfoSync (0, "Sine", "1/8", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.45)),
        ING ("a triplet (1/8 T) wobble", "Wobble", p.lfoSync (0, "Triangle", "1/8 T", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.45)),
        ING ("a half-note wobble that opens and closes", "Wobble", p.lfoSync (0, "Saw Down", "1/2", 1.0, 0.3).mod ("LFO 1", cutoffOrPos (p), 0.5)),
        ING ("a 1-bar synced sweep through the table", "Moving", p.lfoSync (0, "Triangle", "1 bar", 1.0, 0.3).mod ("LFO 1", posOrCutoff (p), 0.5)),
        ING ("a filter envelope snap on every note", "Pluck", p.env (1, 0.001, p.j (0.22, 0.4), 0.15, 0.15).mod ("ENV 2", cutoffOrPos (p), 0.55)),
        ING ("an envelope that growls through the table on each note", "Growl", p.env (1, 0.005, p.j (0.5, 0.4), 0.3, 0.3).mod ("ENV 2", posOrCutoff (p), 0.6)),
        ING ("a 16th-step rhythm gating the filter (STEP)", "Rhythmic",
             p.steps ("9373937393739373", "1", "m", {}, "1/16", 16, 0.0, false).mod ("Step", cutoffOrPos (p), 0.55)),
        ING ("sample-and-hold random steps on the filter", "Glitchy", p.lfoSync (1, "Random", "1/16", 1.0, 0.2).mod ("LFO 2", cutoffOrPos (p), 0.35)),
        ING ("the mod wheel folds and growls it", "Expressive", p.mod ("Mod Wheel", "Osc A Fold", 0.6).mod ("Mod Wheel", cutoffOrPos (p), 0.3)),
        ING ("velocity drives the filter: hit harder for more bite", "Expressive", p.mod ("Velocity", cutoffOrPos (p), 0.45)),
        ING ("a slow random wander, never quite the same twice", "Moving", p.lfo (1, "Random", p.j (0.5, 0.4), 1.0, 1.0).mod ("LFO 2", posOrCutoff (p), 0.3)),
        ING ("a dotted-quarter synced pump on the cutoff", "Wobble", p.lfoSync (0, "Saw Down", "1/4 D", 1.0, 0.2).mod ("LFO 1", cutoffOrPos (p), 0.5)),
    };
    R.filters = {
        ING ("through a driven ladder (MG 24)", "", p.filt (0, "MG Low 24", p.j (900, 0.4), p.j (0.25, 0.4), p.j (0.35, 0.4), 0.3)),
        ING ("through a dark 24 dB low-pass", "", p.filt (0, "Low Pass 24", p.j (1200, 0.4), 0.15, 0.2, 0.3)),
        ING ("through a resonant 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (1800, 0.4), p.j (0.45, 0.3), 0.15, 0.3)),
        ING ("through a key-tracked comb (+)", "", p.filt (0, "Comb +", p.j (300, 0.3), 0.5, 0.0, 1.0, 0.45).filt (1, "Low Pass 12", 3500, 0.1)),
        ING ("through a negative comb: hollow and phasey", "", p.filt (0, "Comb -", p.j (260, 0.3), 0.55, 0.0, 1.0, 0.5).filt (1, "Low Pass 12", 3200, 0.1)),
        ING ("through a formant filter", "", p.filt (0, "Formant", p.j (750, 0.4), 0.5, 0.2)),
        ING ("through a band-pass with the sub carrying the low end", "", p.filt (0, "Band Pass", p.j (700, 0.4), 0.35, 0.2, 0.3).sub ("Sine -1", 0.65)),
        ING ("through a resonant peak", "", p.filt (0, "Peak", p.j (900, 0.4), 0.5, 0.1).filt (1, "Low Pass 24", 4000, 0.1)),
        ING ("through parallel ladder + band-pass", "", p.filt (0, "MG Low 24", p.j (700, 0.4), 0.2, 0.3, 0.3).filt (1, "Band Pass", p.j (1500, 0.4), 0.4).parallel()),
        ING ("wide open, no filter: all the harmonics", "", p.filt (0, "Off", 12000, 0.0)),
    };
    R.styles = {
        ING ("tight legato glide", "", p.mono (p.j (40, 0.4), true)),
        ING ("long portamento slides", "Glide", p.mono (p.j (140, 0.3), true)),
        ING ("plucky: decays to a lower sustain", "", p.env (0, 0.002, p.j (0.4, 0.3), 0.55, 0.15)),
        ING ("slow swell in", "", p.env (0, p.j (0.06, 0.4), 0.5, 1.0, 0.3)),
        ING ("stabby retrigger, no glide", "", p.mono (0.0, false).env (0, 0.001, 0.25, 0.7, 0.08)),
        ING ("octave-up layer for presence", "", p.an (2, "Saw", 0.22).tune (2, 1).uni (2, 3, 0.25, 0.9)),
        ING ("a quiet noise layer for air", "", p.noise ("Bright", 0.06)),
        ING ("detuned osc D a fifth up", "", p.an (3, "Square", 0.18).tune (3, 0, 7, -6)),
    };
    R.flavours = { Clean, Wide, Dirty, Space, LoFi, Motion, OTT, Tape, Grit, Dream, Pumped, Shimmer };
    R.heroMotion = { { "Wobble Machine", 2 } };
    R.heroSource = { { "Abyss Reese", 0 }, { "Wave Reese", 3 }, { "Neuro Reese", 10 }, { "Talking Reese", 13 },
                     { "Into The Void Bass", 19 }, { "Wobble Machine", 2 }, { "Concrete Growl", 11 }, { "Always Elsewhere Bass", 20 } };
    return R;
}

//==============================================================================
// 808: trending distorted 808s + powerful low end
//==============================================================================
CategoryRecipe recipe808()
{
    CategoryRecipe R;
    R.category = "808"; R.count = 90; R.lowEnd = true; R.tags = { "808", "Trap" };
    R.pools = { { "Viral", "Blown", "Gravel", "Lava", "Crushed", "Thunder", "Smoke", "Chrome", "Dirty", "Slime", "Tar", "Concrete", "Cursed", "Heavy", "Ghost", "Ruby" },
                { "808", "Boom", "Slide", "Knock", "Rumble", "Quake", "Clipper", "Drop", "Pressure", "Gut Punch", "Stomp" },
                { "Void 808", "Viral 808", "Glide 808", "Grime 808", "", "", "Knock 808", "", "Clipper 808" } };
    R.base = [] (Patch& p)
    {
        p.tight (0, 0.25).tune (0, -1).filt (0, "Off", 12000, 0.0).drift (0.05).voices (1).level (-8.0).noSat()
         .env (0, 0.001, p.j (2.2, 0.4), 0.0, p.j (0.5, 0.3)).env (2, 0.001, p.j (0.07, 0.4), 0.0, 0.06, -0.5)
         .mod ("ENV 3", "Osc A Pitch", p.j (0.42, 0.3)).mono (p.j (90, 0.4), true);
    };
    R.finish = [] (Patch& p) { p.comp (-16, 4, 4, 15, 160).stereo (1.0, true, 200.0); };
    R.sources = {
        ING ("a pure sine 808", "Clean", p.an (0, "Sine", 0.9)),
        ING ("sine body with a triangle layer for bite", "", p.an (0, "Sine", 0.9).an (1, "Triangle", 0.4).tune (1, 0).tight (1)),
        ING ("the 808 Body table, folding as you dig in", "", p.wt (0, "808 Body", p.j (0.45, 0.5), 0.9).tune (0, -1).fold (0, p.j (0.2, 0.5)).mod ("Velocity", "Osc A Fold", 0.35)),
        ING ("a Sub FM 808 with a woody top", "", p.wt (0, "Sub FM", p.j (0.35, 0.5), 0.9).tune (0, -1)),
        ING ("an Overdrive Sine 808", "Dirty", p.wt (0, "Overdrive Sine", p.j (0.5, 0.5), 0.9).tune (0, -1)),
        ING ("a crushed-sine 808", "Lo-Fi", p.wt (0, "Crushed Sine", p.j (0.4, 0.5), 0.9).tune (0, -1)),
        ING ("sine + a detuned saw for growl in the mids", "", p.an (0, "Sine", 0.9).an (1, "Saw", 0.28).tune (1, 0).uni (1, 3, 0.12, 0.6).filt (1, "Low Pass 12", p.j (1400, 0.3), 0.1)),
        ING ("sine + square harmonics", "", p.an (0, "Sine", 0.9).an (1, "Square", 0.25).tune (1, 0).filt (1, "Low Pass 12", p.j (1800, 0.3), 0.1)),
        ING ("a Tube Saw 808 under a sine", "Dirty", p.an (0, "Sine", 0.85).wt (1, "Tube Saw", 0.3, 0.3).tune (1, -1).filt (1, "Low Pass 24", p.j (900, 0.3), 0.15)),
        ING ("an Electric FM 808 with a metallic edge", "", p.wt (0, "Electric FM", p.j (0.3, 0.5), 0.85).tune (0, -1)),
    };
    R.motions = {
        ING ("short, steep pitch dive", "", p.set (ids::env (2, "decay"), p.j (0.04, 0.3)).set (ids::mod (0, "amt"), 0.55)),
        ING ("long pitch drop, the classic boom", "", p.set (ids::env (2, "decay"), p.j (0.16, 0.3)).set (ids::mod (0, "amt"), 0.5)),
        ING ("barely any pitch drop: round and smooth", "", p.set (ids::mod (0, "amt"), 0.2)),
        ING ("a kick-thump transient on top", "Knock", p.smp ("Kick Thump", 0.5, false).trans (0.55, -0.2)),
        ING ("a noise click at the front", "Knock", p.noise ("Bright", 0.2).env (3, 0.001, 0.02, 0.0, 0.02).mod ("ENV 4", "Noise Level", 1.0).set (ids::noiseLevel, 0.0)),
        ING ("a short knock decay", "Knock", p.env (0, 0.001, p.j (0.45, 0.3), 0.0, 0.12)),
        ING ("a long tail that rings out", "", p.env (0, 0.001, p.j (3.5, 0.2), 0.0, 0.8)),
        ING ("velocity controls the drive", "Expressive", p.mod ("Velocity", "Saturation", 0.4)),
        ING ("the mod wheel bends it down an octave", "Expressive", p.mod ("Mod Wheel", "Osc A Pitch", -0.5)),
    };
    R.filters = {
        ING ("hard-clipped and loud", "Clipper", p.dist ("Hard Clip", p.j (0.65, 0.2), 0.85, 0.5).eq (3.0, 180, 2.0, -2.0, 60.0)),
        ING ("through warm tube distortion", "", p.dist ("Tube", p.j (0.5, 0.3), 0.65, 0.45)),
        ING ("soft-clipped with a cubic curve", "", p.dist ("Soft Cubic", p.j (0.5, 0.3), 0.6)),
        ING ("rectified: octave-up grit that cuts through phones", "", p.dist ("Rectify", p.j (0.45, 0.3), 0.5, 0.65)),
        ING ("sine-folded into a buzzy growl", "", p.dist ("Sine Fold", p.j (0.35, 0.3), 0.45)),
        ING ("clean, just saturation", "Clean", p.sat (p.j (0.3, 0.3))),
        ING ("double distortion: tube into clipper", "Clipper", p.dist ("Tube", 0.45, 0.6).sat (0.5)),
        ING ("crushed and downsampled", "Lo-Fi", p.crush (p.j (8, 0.2), p.j (6, 0.3), 0.55).sat (0.35)),
    };
    R.styles = {
        ING ("legato slides between overlapping notes", "Glide", p.mono (p.j (140, 0.3), true)),
        ING ("retriggers on every note", "", p.mono (0.0, false)),
        ING ("fast glide", "Glide", p.mono (p.j (60, 0.3), true)),
        ING ("a little unison for width in the harmonics, mono below", "", p.uni (0, 2, 0.06, 0.4).stereo (1.2, true, 180.0)),
        ING ("a sub-octave sine underneath", "", p.sub ("Sine -1", 0.35)),
    };
    R.flavours = { Clean, Dirty, OTT, Grit, Tape, LoFi, Wide, Pumped };
    R.heroSource = { { "Void 808", 0 }, { "Viral 808", 4 }, { "Glide 808", 1 }, { "Grime 808", 7 }, { "Knock 808", 2 }, { "Clipper 808", 8 } };
    return R;
}

//==============================================================================
// SUB
//==============================================================================
CategoryRecipe recipeSub()
{
    CategoryRecipe R;
    R.category = "SUB"; R.count = 50; R.lowEnd = true; R.tags = { "Sub", "Mono" };
    R.pools = { { "Deep", "Pure", "Ocean", "Bedrock", "Core", "Silent", "Low", "Gravity", "Basement", "Earth", "Tectonic" },
                { "Sub", "Floor", "Weight", "Pulse", "Foundation", "Hum", "Swell", "Trench" },
                { "Pure Sub", "", "", "", "Deep Sub" } };
    R.base = [] (Patch& p)
    {
        p.tight (0, 0.25).tune (0, -1).filt (0, "Low Pass 24", p.j (700, 0.3), 0.05).drift (0.04).voices (1)
         .env (0, p.j (0.004, 0.5), 0.5, 1.0, p.j (0.15, 0.4)).mono (0.0, true).level (-7.0).noSat();
    };
    R.finish = [] (Patch& p) { p.comp (-18, 3, 3, 20, 200).stereo (1.0, true, 300.0).eq (1.5, 250, -1.0, -3.0, 50.0); };
    R.sources = {
        ING ("a pure sine sub", "Clean", p.an (0, "Sine", 0.9)),
        ING ("a triangle sub (odd harmonics so it reads on phones)", "", p.an (0, "Triangle", 0.85)),
        ING ("the 808 Body table held as a sustained sub", "", p.wt (0, "808 Body", p.j (0.25, 0.5), 0.9)),
        ING ("a Sub FM tone", "", p.wt (0, "Sub FM", p.j (0.25, 0.5), 0.9)),
        ING ("an Overdrive Sine sub", "Dirty", p.wt (0, "Overdrive Sine", p.j (0.35, 0.5), 0.9)),
        ING ("a low-passed square sub", "", p.an (0, "Square", 0.8).filt (0, "Low Pass 24", p.j (350, 0.3), 0.1)),
        ING ("sine + a whisper of octave-up sine", "", p.an (0, "Sine", 0.9).an (1, "Sine", 0.15).tune (1, 0)),
        ING ("a crushed-sine sub", "Lo-Fi", p.wt (0, "Crushed Sine", p.j (0.25, 0.4), 0.9)),
        ING ("an Even Harmonics sub: warm and tube-like", "", p.wt (0, "Even Harmonics", p.j (0.2, 0.5), 0.9)),
    };
    R.motions = {
        ING ("steady", "", p.set (ids::analog, 0.04)),
        ING ("a quarter-note volume wobble", "Wobble", p.lfoSync (0, "Sine", "1/4", 1.0, 0.2).mod ("LFO 1", "Amp", -0.5)),
        ING ("an eighth-note sub wobble", "Wobble", p.lfoSync (0, "Sine", "1/8", 1.0, 0.2).mod ("LFO 1", "Amp", -0.45)),
        ING ("a tiny pitch dip on each note", "", p.env (2, 0.001, 0.05, 0.0, 0.05).mod ("ENV 3", "Osc A Pitch", 0.2)),
        ING ("velocity sets the drive", "Expressive", p.mod ("Velocity", "Saturation", 0.4).sat (0.2)),
        ING ("the mod wheel adds harmonics", "Expressive", p.mod ("Mod Wheel", "Osc A Fold", 0.5)),
    };
    R.filters = {
        ING ("rolled off at 24 dB", "", p.set (ids::filt (0, "cutoff"), p.j (700, 0.3))),
        ING ("open, unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
        ING ("warmed with saturation", "", p.sat (p.j (0.35, 0.3))),
        ING ("through soft tube drive", "", p.dist ("Tube", p.j (0.3, 0.3), 0.4, 0.4)),
    };
    R.styles = {
        ING ("no glide", "", p.mono (0.0, false)),
        ING ("legato glide", "Glide", p.mono (p.j (110, 0.4), true)),
        ING ("plucky: a decaying sub", "", p.env (0, 0.002, p.j (0.6, 0.3), 0.0, 0.2)),
        ING ("long and sustained", "", p.env (0, 0.005, 0.5, 1.0, p.j (0.4, 0.3))),
        ING ("soft attack swell", "", p.env (0, p.j (0.05, 0.4), 0.5, 1.0, 0.25)),
    };
    R.flavours = { Clean, Tape, OTT, Clean, Pumped };
    R.heroSource = { { "Pure Sub", 0 }, { "Deep Sub", 6 } };
    return R;
}

//==============================================================================
// LEAD
//==============================================================================
CategoryRecipe recipeLead()
{
    CategoryRecipe R;
    R.category = "LEAD"; R.count = 100; R.tags = { "Lead" };
    R.pools = { { "Seraph", "Halo", "Neon", "Crystal", "Solar", "Astral", "Tear", "Silver", "Falling", "Lucid", "Holy", "Electric", "Nova", "Prism", "Velvet" },
                { "Lead", "Saw", "Cry", "Song", "Scream", "Wire", "Line", "Hymn", "Ray", "Signal", "Siren" },
                { "Into The Void", "Banshee", "Angel Chop", "Always Elsewhere", "Eskimo Ice", "", "8-Bit Ghost" } };
    R.base = [] (Patch& p) { p.env (0, 0.004, 0.4, 0.85, p.j (0.32, 0.3)).voices (8).level (-8.0).drift (0.3).noSat(); };
    R.finish = [] (Patch& p) { p.reverb (p.j (0.75, 0.15), 0.4, 0.22, 30.0).eq (-2.0, 2500, 1.5, 2.0, 150.0).comp (-20, 3, 3); };
    R.sources = {
        ING ("a seven-voice supersaw", "Supersaw", p.an (0, "Saw", 0.75).uni (0, 7, p.j (0.42, 0.2), 0.85, 0.85).wt (1, "Saw Stack", 0.5, 0.3).uni (1, 3, 0.3, 0.8).tune (1, 1)),
        ING ("a folded sine screech", "Screech", p.wt (0, "Sine Fold", p.j (0.6, 0.3), 0.8).uni (0, 3, 0.18, 0.7).fold (0, p.j (0.35, 0.3))),
        ING ("a vocal formant table (vocal-chop style)", "Vocal", p.wt (0, "Formant Vox", p.j (0.4, 0.5), 0.85).uni (0, 3, 0.2, 0.7)),
        ING ("a hard-sync lead", "Sync", p.wt (0, "Laser Sync", 0.2, 0.8).uni (0, 3, 0.2, 0.6)),
        ING ("a PWM square lead, eski-cold", "Square", p.an (0, "Pulse", 0.8).pw (0, 0.3).an (1, "Square", 0.4).tune (1, 1)),
        ING ("glassy FM bells", "Bell", p.wt (0, "Glass FM", p.j (0.35, 0.5), 0.8)),
        ING ("an 8-bit square", "8-Bit", p.an (0, "Square", 0.8).crush (6, 6, 0.6)),
        ING ("a soft sine + triangle lead", "Soft", p.an (0, "Sine", 0.85).an (1, "Triangle", 0.35).tune (1, 1)),
        ING ("a sampled choir 'ahh' with saws under it", "Choir", p.smp ("Choir Ahh", 0.7, true).smpLoop (0.2, 0.9).an (0, "Saw", 0.4).uni (0, 5, 0.3, 0.8)),
        ING ("a breathy flute table", "Flute", p.wt (0, "Breathy Flute", p.j (0.4, 0.5), 0.8).noise ("Pink", 0.04)),
        ING ("a nasal reed lead", "Reed", p.wt (0, "Nasal Reed", p.j (0.4, 0.5), 0.8).uni (0, 2, 0.1, 0.4)),
        ING ("a Cheby Shine table: bright and singing", "", p.wt (0, "Cheby Shine", p.j (0.5, 0.4), 0.8).uni (0, 3, 0.2, 0.7)),
        ING ("a talk-box lead", "Vocal", p.wt (0, "Talk Box", p.j (0.4, 0.5), 0.8).uni (0, 2, 0.15, 0.5)),
        ING ("a phase-warped CZ lead", "", p.wt (0, "Phase Warp", p.j (0.4, 0.5), 0.8).uni (0, 3, 0.2, 0.6)),
        ING ("ring-modulated bells", "Bell", p.wt (0, "Ring Bells", p.j (0.4, 0.5), 0.8)),
        ING ("a clarinet table, woody and hollow", "", p.wt (0, "Clarinet", p.j (0.4, 0.5), 0.8)),
        ING ("a Tube Saw lead with a sub-octave", "", p.wt (0, "Tube Saw", p.j (0.5, 0.4), 0.8).uni (0, 3, 0.2, 0.6).sub ("Triangle -1", 0.25)),
        ING ("a vowel-sweep lead", "Vocal", p.wt (0, "Vowel Sweep", p.j (0.3, 0.6), 0.85).uni (0, 3, 0.2, 0.7)),
        ING ("a Crystal table, sparkling", "", p.wt (0, "Crystal", p.j (0.5, 0.4), 0.8).uni (0, 3, 0.2, 0.8)),
        ING ("an arcade-steps chip lead", "8-Bit", p.wt (0, "Arcade Steps", p.j (0.5, 0.4), 0.8)),
    };
    R.motions = {
        ING ("vibrato on the mod wheel", "Expressive", p.lfo (0, "Sine", 5.4, 1.0).mod ("LFO 1", "Osc A Pitch", 0.0).mod ("Mod Wheel", "Osc A Pitch", 0.004)),
        ING ("constant gentle vibrato", "", p.lfo (0, "Sine", p.j (5.2, 0.1), 0.6).mod ("LFO 1", "Osc A Pitch", 0.003)),
        ING ("a filter envelope on each note", "", p.env (1, 0.002, p.j (0.5, 0.3), 0.3, 0.4).mod ("ENV 2", cutoffOrPos (p), 0.3)),
        ING ("the table sweeps on each note", "Moving", p.env (1, 0.001, p.j (0.4, 0.4), 0.2, 0.3).mod ("ENV 2", posOrCutoff (p), 0.5)),
        ING ("slow LFO drift through the timbre", "Moving", p.lfo (1, "Triangle", p.j (0.2, 0.4), 1.0, 0.5).mod ("LFO 2", posOrCutoff (p), 0.3)),
        ING ("mono with portamento", "Glide", p.mono (p.j (90, 0.4))),
        ING ("a synced 1/16 trance gate", "Gated", p.trem ("Square", "1/16", 0.85)),
        ING ("velocity brightens it", "Expressive", p.mod ("Velocity", cutoffOrPos (p), 0.4)),
        ING ("aftertouch adds fold", "Expressive", p.mod ("Aftertouch", "Osc A Fold", 0.5)),
        ING ("dotted-eighth echoes", "", p.delay ("1/8 D", 0.4, 0.22)),
    };
    R.filters = {
        ING ("through a ladder", "", p.filt (0, "MG Low 24", p.j (6000, 0.3), 0.18, 0.1, 0.3)),
        ING ("through a bright 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (7000, 0.3), 0.12)),
        ING ("through a resonant 24 dB low-pass", "", p.filt (0, "Low Pass 24", p.j (4000, 0.3), 0.4, 0.15)),
        ING ("through a band-pass for a thin, radio feel", "", p.filt (0, "Band Pass", p.j (1800, 0.3), 0.3)),
        ING ("unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
        ING ("through a formant filter", "", p.filt (0, "Formant", p.j (1200, 0.3), 0.5)),
        ING ("high-passed so it floats over the mix", "", p.filt (0, "High Pass 12", p.j (300, 0.3), 0.1)),
    };
    R.styles = {
        ING ("poly", "", p.voices (8)),
        ING ("mono legato", "Mono", p.mono (p.j (50, 0.4))),
        ING ("an octave-up layer", "", p.an (2, "Saw", 0.2).tune (2, 1).uni (2, 3, 0.25, 0.9)),
        ING ("a soft attack", "", p.env (0, p.j (0.04, 0.4), 0.4, 0.85, 0.4)),
        ING ("a long release tail", "", p.env (0, 0.004, 0.4, 0.85, p.j (0.9, 0.3))),
    };
    R.flavours = { Space, Wide, Dirty, Shimmer, Dream, Motion, Tape, OTT, LoFi, Grit, Clean };
    R.heroSource = { { "Into The Void", 0 }, { "Banshee", 1 }, { "Angel Chop", 2 }, { "Always Elsewhere", 3 }, { "Eskimo Ice", 4 }, { "8-Bit Ghost", 6 } };
    return R;
}

//==============================================================================
// SYNTH
//==============================================================================
CategoryRecipe recipeSynth()
{
    CategoryRecipe R;
    R.category = "SYNTH"; R.count = 90; R.tags = { "Synth" };
    R.pools = { { "Strobe", "Pulse", "Velvet", "Ivory", "Midnight", "Chrome", "Retro", "Gold", "Static", "Paper", "Dusk", "Arcade", "Satin" },
                { "Stab", "Keys", "Chords", "Organ", "Synth", "Arp", "Brass", "Strings", "Machine", "Run", "Bloom" },
                { "Cold Stab", "Gospel Organ", "Soft Keys", "Analog Brass", "", "Strobe Heaven", "Pulse Of Heaven", "Falling Stars" } };
    R.base = [] (Patch& p) { p.voices (12).level (-8.0).drift (0.25).noSat().env (0, 0.005, 0.6, 0.8, 0.35); };
    R.finish = [] (Patch& p) { p.reverb (p.j (0.7, 0.15), 0.4, 0.22, 25.0).eq (-2.0, 3000, 1.0, 1.5, 150.0).comp (-20, 3, 3).stereo (1.3); };
    R.sources = {
        ING ("big supersaw chords", "Supersaw", p.an (0, "Saw", 0.75).uni (0, 7, 0.4, 0.9).an (1, "Saw", 0.5).uni (1, 5, 0.3, 0.9).tune (1, 1)),
        ING ("a jazz drawbar organ", "Organ", p.wt (0, "Organ Jazz", p.j (0.3, 0.6), 0.8)),
        ING ("a gospel organ", "Organ", p.wt (0, "Organ Gospel", p.j (0.3, 0.6), 0.8)),
        ING ("an e-piano tine", "Keys", p.wt (0, "E-Piano Tine", p.j (0.2, 0.6), 0.8).env (0, 0.002, 1.6, 0.25, 0.5)),
        ING ("a clavi", "Keys", p.wt (0, "Clavi", p.j (0.3, 0.5), 0.8).env (0, 0.002, 0.8, 0.2, 0.3)),
        ING ("brass swell", "Brass", p.wt (0, "Brass Swell", 0.1, 0.8).uni (0, 3, 0.15, 0.6).env (1, 0.25, 0.6, 0.6, 0.4).mod ("ENV 2", "Osc A WT Pos", 0.7)),
        ING ("a classic two-saw poly", "Analog", p.an (0, "Saw", 0.7).uni (0, 2, 0.12, 0.6).an (1, "Saw", 0.6).tune (1, 0, 0, 9)),
        ING ("a string ensemble", "Strings", p.wt (0, "String Ensemble", p.j (0.5, 0.4), 0.8).uni (0, 5, 0.25, 0.9)),
        ING ("square-wave chords, an octave apart", "", p.an (0, "Square", 0.6).an (1, "Square", 0.4).tune (1, 1).pw (0, 0.4)),
        ING ("a Seventh Heaven table: already a chord inside", "", p.wt (0, "Seventh Heaven", p.j (0.5, 0.4), 0.75).uni (0, 3, 0.2, 0.8)),
        ING ("a pipe organ", "Organ", p.wt (0, "Pipe Organ", p.j (0.4, 0.5), 0.75)),
        ING ("a hollow-organ table with a sub-octave", "Organ", p.wt (0, "Hollow Organ", p.j (0.4, 0.5), 0.8).sub ("Sine -1", 0.25)),
        ING ("Saw To Square morph chords", "", p.wt (0, "Saw To Square", p.j (0.4, 0.5), 0.8).uni (0, 4, 0.25, 0.8)),
        ING ("a Church Reed", "Organ", p.wt (0, "Church Reed", p.j (0.4, 0.5), 0.75)),
    };
    R.motions = {
        ING ("short stabs", "Stab", p.env (0, 0.002, p.j (0.35, 0.3), 0.0, 0.25).env (1, 0.001, 0.3, 0.0, 0.2).mod ("ENV 2", cutoffOrPos (p), 0.3)),
        ING ("a 16th trance gate", "Gated", p.trem ("Square", "1/16", 0.95)),
        ING ("a quarter-note pump, like heavy sidechain", "Pumping", p.trem ("Sine", "1/4", 0.7)),
        ING ("an up/down arp plays held chords", "Arp", p.arp ("Up/Down", "1/16", 2, 0.5).delay ("1/8 D", 0.35, 0.2)),
        ING ("a random arp at 1/8 T", "Arp", p.arp ("Random", "1/8T", 2, 0.45)),
        ING ("a slow filter sweep (1 bar)", "Moving", p.lfoSync (0, "Triangle", "1 bar", 1.0, 0.3).mod ("LFO 1", cutoffOrPos (p), 0.35)),
        ING ("velocity opens the filter", "Expressive", p.mod ("Velocity", cutoffOrPos (p), 0.4)),
        ING ("tremolo on the mod wheel", "Expressive", p.trem ("Sine", "1/8", 0.0).mod ("Mod Wheel", "Amp", -0.3)),
        ING ("held and sustained", "", p.env (0, p.j (0.02, 0.5), 1.0, 0.85, 0.6)),
        ING ("an 8th-note step rhythm on the filter", "Rhythmic", p.steps ("9595959595959595", "1", "m", {}, "1/8", 16, 0.1, false).mod ("Step", cutoffOrPos (p), 0.4)),
    };
    R.filters = {
        ING ("through a ladder with a filter envelope", "", p.filt (0, "MG Low 24", p.j (2500, 0.4), 0.25, 0.1, 0.3)),
        ING ("through a bright 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (7000, 0.3), 0.12)),
        ING ("unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
        ING ("through a resonant 24 dB low-pass", "", p.filt (0, "Low Pass 24", p.j (3000, 0.3), 0.35)),
        ING ("through a notch for a phasey hole", "", p.filt (0, "Notch", p.j (1200, 0.4), 0.3)),
        ING ("through a peak boost", "", p.filt (0, "Peak", p.j (1600, 0.3), 0.4)),
    };
    R.styles = {
        ING ("wide stereo", "", p.stereo (1.6)),
        ING ("an octave-down sub layer", "", p.sub ("Sine -1", 0.25)),
        ING ("an air layer", "", p.smp ("Air Breath", 0.12, false).smpLoop (0.1, 0.95)),
        ING ("tight and centred", "", p.stereo (1.0)),
        ING ("a long release", "", p.set (ids::env (0, "release"), p.j (1.0, 0.3))),
    };
    R.flavours = { Wide, Space, Tape, Dream, Motion, OTT, Clean, Shimmer, LoFi };
    R.heroSource = { { "Cold Stab", 0 }, { "Gospel Organ", 2 }, { "Soft Keys", 3 }, { "Analog Brass", 5 }, { "Strobe Heaven", 0 }, { "Pulse Of Heaven", 0 }, { "Falling Stars", 9 } };
    return R;
}

//==============================================================================
// PLUCK
//==============================================================================
CategoryRecipe recipePluck()
{
    CategoryRecipe R;
    R.category = "PLUCK"; R.count = 80; R.tags = { "Pluck" };
    R.pools = { { "Crystal", "Glass", "Frost", "Rain", "Dew", "Star", "Ice", "Moon", "Silk", "Pearl", "Snow", "Spark", "Ember" },
                { "Pluck", "Drop", "Harp", "Chime", "Bell", "Tine", "Box", "Mallet", "Drip" },
                { "Glass Pluck", "Crystal Rain", "", "", "", "Halo Bells", "", "Clang Pluck", "", "", "", "", "", "Ice Bell" } };
    R.base = [] (Patch& p) { p.voices (12).level (-8.0).drift (0.2).noSat().env (0, 0.001, p.j (0.5, 0.3), 0.0, p.j (0.4, 0.3)); };
    R.finish = [] (Patch& p) { p.reverb (p.j (0.65, 0.2), 0.45, 0.24, 22.0).eq (-3.0, 2500, 1.0, 2.0, 180.0).stereo (1.3); };
    R.sources = {
        ING ("a Glass Halo table", "Glass", p.wt (0, "Glass Halo", p.j (0.4, 0.5), 0.8).uni (0, 2, 0.12, 0.6)),
        ING ("a physical plucked-string sample", "String", p.smp ("Pluck Body", 0.85, true).an (0, "Triangle", 0.25)),
        ING ("a mallet", "Mallet", p.wt (0, "Mallet", p.j (0.3, 0.6), 0.85)),
        ING ("a kalimba", "Kalimba", p.wt (0, "Kalimba", p.j (0.3, 0.6), 0.85)),
        ING ("a music box", "Music Box", p.wt (0, "Music Box", p.j (0.3, 0.6), 0.8).tune (0, 1)),
        ING ("FM bells", "Bell", p.wt (0, "Bell FM", p.j (0.5, 0.4), 0.8)),
        ING ("detuned saws (wave pluck)", "Wave", p.an (0, "Saw", 0.75).uni (0, 5, 0.3, 0.8)),
        ING ("a pulse through a tuned comb: clangy", "Metal", p.an (0, "Pulse", 0.75).pw (0, 0.2).filt (1, "Comb +", 261.6, 0.6, 0.0, 1.0, 0.6)),
        ING ("a Wood FM table", "Wood", p.wt (0, "Wood FM", p.j (0.4, 0.5), 0.8)),
        ING ("a sampled bell strike", "Bell", p.smp ("Bell Strike", 0.8, true).an (0, "Sine", 0.2)),
        ING ("a glass hit sample", "Glass", p.smp ("Glass Hit", 0.8, true)),
        ING ("a Cheby Even table", "", p.wt (0, "Cheby Even", p.j (0.4, 0.5), 0.8)),
        ING ("ring-hollow bells", "Bell", p.wt (0, "Ring Hollow", p.j (0.4, 0.5), 0.8)),
        ING ("a square pluck (chip)", "8-Bit", p.an (0, "Square", 0.75).crush (10, 3, 0.3)),
    };
    R.motions = {
        ING ("a fast filter snap", "", p.env (1, 0.001, p.j (0.16, 0.3), 0.0, 0.1).mod ("ENV 2", cutoffOrPos (p), 0.6)),
        ING ("a slower filter decay", "", p.env (1, 0.001, p.j (0.5, 0.3), 0.0, 0.3).mod ("ENV 2", cutoffOrPos (p), 0.5)),
        ING ("the table drops on each hit", "Moving", p.env (1, 0.001, 0.4, 0.0, 0.3).mod ("ENV 2", posOrCutoff (p), -0.5)),
        ING ("dotted-eighth ping-pong echoes", "Echo", p.delay ("1/8 D", 0.45, 0.28)),
        ING ("quarter-note triplet echoes", "Echo", p.delay ("1/4 T", 0.4, 0.25)),
        ING ("a pitch blip on the attack", "", p.env (2, 0.001, 0.03, 0.0, 0.02).mod ("ENV 3", "Osc A Pitch", 0.15)),
        ING ("velocity controls brightness", "Expressive", p.mod ("Velocity", cutoffOrPos (p), 0.5)),
        ING ("an arp at 1/16", "Arp", p.arp ("Up", "1/16", 2, 0.4)),
    };
    R.filters = {
        ING ("through a 24 dB low-pass", "", p.filt (0, "Low Pass 24", p.j (1300, 0.4), 0.2, 0.0, 0.4)),
        ING ("through a 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (3500, 0.4), 0.15, 0.0, 0.5)),
        ING ("through a ladder", "", p.filt (0, "MG Low 24", p.j (900, 0.4), 0.3, 0.1, 0.4)),
        ING ("unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
        ING ("through a band-pass", "", p.filt (0, "Band Pass", p.j (1600, 0.4), 0.3)),
    };
    R.styles = {
        ING ("short", "", p.env (0, 0.001, p.j (0.3, 0.3), 0.0, 0.25)),
        ING ("long ringing decay", "", p.env (0, 0.001, p.j (1.5, 0.3), 0.0, 0.8)),
        ING ("an octave-up sparkle layer", "", p.an (2, "Sine", 0.2).tune (2, 1)),
        ING ("a soft sub-octave", "", p.sub ("Sine -1", 0.2)),
    };
    R.flavours = { Space, Dream, Wide, Tape, Shimmer, LoFi, Clean, Motion, OTT };
    R.heroSource = { { "Glass Pluck", 0 }, { "Crystal Rain", 6 }, { "Halo Bells", 5 }, { "Clang Pluck", 7 }, { "Ice Bell", 9 } };
    return R;
}

//==============================================================================
// PAD
//==============================================================================
CategoryRecipe recipePad()
{
    CategoryRecipe R;
    R.category = "PAD"; R.count = 100; R.tags = { "Pad" };
    R.pools = { { "Ether", "Celestial", "Seraph", "Winter", "Northern", "Fading", "Endless", "Sacred", "Void", "Heaven", "Lunar", "Distant", "Velvet", "Last", "Silent" },
                { "Pad", "Bloom", "Choir", "Haze", "Light", "Dream", "Veil", "Horizon", "Glow", "Memory", "Drift" },
                { "Seraph Bloom", "Angel Choir", "Grey Skies", "Celestial Pad", "", "", "", "Dark Drone", "Ambient Wash", "Northern Lights" } };
    R.base = [] (Patch& p) { p.voices (16).level (-9.0).drift (0.42).noSat().env (0, p.j (0.9, 0.5), 1.5, 0.85, p.j (2.6, 0.3)); };
    R.finish = [] (Patch& p) { p.reverb (p.j (0.85, 0.1), 0.4, 0.38, 32.0).eq (-3.0, 400, -1.5, 1.5, 200.0).stereo (1.4); };
    R.sources = {
        ING ("a formant-vox table with a glass layer", "Vocal", p.wt (0, "Formant Vox", p.j (0.35, 0.5), 0.85).uni (0, 5, 0.35, 0.85).wt (1, "Glass Halo", 0.2, 0.4).tune (1, 1)),
        ING ("sampled choir voices", "Choir", p.smp ("Choir Ahh", 0.8, true).smpLoop (0.2, 0.92).wt (0, "Choir Formant", p.j (0.4, 0.5), 0.5).uni (0, 5, 0.3, 0.9)),
        ING ("a dark pipe organ under low saws", "Dark", p.wt (0, "Pipe Organ", p.j (0.5, 0.4), 0.7).an (1, "Saw", 0.45).uni (1, 5, 0.3, 0.9)),
        ING ("crystal partials with a Seventh Heaven layer", "Shimmer", p.wt (0, "Crystal", p.j (0.5, 0.4), 0.7).uni (0, 3, 0.2, 0.9).wt (1, "Seventh Heaven", 0.6, 0.45).tune (1, 1)),
        ING ("a Chaos table", "Evolving", p.wt (0, "Chaos II", 0.5, 0.75).uni (0, 4, 0.3, 0.9)),
        ING ("a wide string ensemble", "Strings", p.wt (0, "String Ensemble", p.j (0.5, 0.4), 0.8).uni (0, 7, 0.35, 1.0)),
        ING ("warm analog saw + triangle", "Analog", p.an (0, "Saw", 0.6).uni (0, 3, 0.2, 0.7).an (1, "Triangle", 0.5).tune (1, -1)),
        ING ("a throat-singer drone with a dark-to-bright layer", "Drone", p.wt (0, "Throat Singer", p.j (0.4, 0.5), 0.7).tune (0, -1).wt (1, "Dark To Bright", p.j (0.3, 0.5), 0.5)),
        ING ("soft sines with breath", "Air", p.an (0, "Sine", 0.6).an (1, "Sine", 0.4).tune (1, 1, 7).noise ("Bright", 0.06).smp ("Air Breath", 0.3, false).smpLoop (0.1, 0.95)),
        ING ("a supersaw pad", "Supersaw", p.an (0, "Saw", 0.7).uni (0, 8, 0.45, 1.0, 0.8).an (1, "Saw", 0.4).uni (1, 6, 0.35, 1.0).tune (1, 1)),
        ING ("a Harmonic Sweep table", "", p.wt (0, "Harmonic Sweep", p.j (0.4, 0.5), 0.8).uni (0, 5, 0.3, 0.9)),
        ING ("Ooh-to-Ahh voices", "Vocal", p.wt (0, "Ooh To Ahh", p.j (0.4, 0.5), 0.8).uni (0, 5, 0.3, 0.9)),
        ING ("a Comb Shimmer table", "Shimmer", p.wt (0, "Comb Shimmer", p.j (0.4, 0.5), 0.75).uni (0, 4, 0.25, 0.9)),
        ING ("an Air Tilt table", "Air", p.wt (0, "Air Tilt", p.j (0.4, 0.5), 0.75).uni (0, 5, 0.3, 0.9)),
        ING ("breathy flutes in unison", "", p.wt (0, "Breathy Flute", p.j (0.4, 0.5), 0.75).uni (0, 5, 0.3, 0.9)),
        ING ("a Chaos IV table under a sub", "Dark", p.wt (0, "Chaos IV", 0.5, 0.75).uni (0, 3, 0.25, 0.8).sub ("Sine -1", 0.25)),
    };
    R.motions = {
        ING ("frames drift under a slow LFO", "Evolving", p.lfo (0, "Triangle", p.j (0.07, 0.4), 1.0, 0.6).mod ("LFO 1", posOrCutoff (p), 0.3)),
        ING ("random-LFO motion through the timbre", "Evolving", p.lfo (1, "Random", p.j (0.15, 0.4), 1.0, 1.0).mod ("LFO 2", posOrCutoff (p), 0.4)),
        ING ("a slow 4-bar filter breath", "Moving", p.lfoSync (0, "Sine", "4 bars", 1.0, 0.5).mod ("LFO 1", cutoffOrPos (p), 0.3)),
        ING ("still and calm", "", p.set (ids::analog, 0.35)),
        ING ("a gentle tape wobble in pitch", "Nostalgic", p.lfo (0, "Sine", p.j (0.35, 0.3)).mod ("LFO 1", "Osc A Pitch", 0.002)),
        ING ("a synced 1/8 gate", "Gated", p.trem ("Square", "1/8", 0.8)),
        ING ("a quarter-note pump", "Pumping", p.trem ("Sine", "1/4", 0.65)),
        ING ("the mod wheel opens it up", "Expressive", p.mod ("Mod Wheel", cutoffOrPos (p), 0.5)),
        ING ("autopan drifting left to right", "Moving", p.trem ("Sine", "2 bars", 0.6, true)),
    };
    R.filters = {
        ING ("through a soft 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (4500, 0.3), 0.15)),
        ING ("through a dark 24 dB low-pass", "", p.filt (0, "Low Pass 24", p.j (1400, 0.3), 0.2, 0.1)),
        ING ("through a ladder", "", p.filt (0, "MG Low 24", p.j (3000, 0.3), 0.2, 0.1)),
        ING ("unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
        ING ("through a formant filter", "", p.filt (0, "Formant", p.j (900, 0.3), 0.4)),
        ING ("high-passed and airy", "", p.filt (0, "High Pass 12", p.j (250, 0.3), 0.1)),
    };
    R.styles = {
        ING ("slow attack", "", p.env (0, p.j (1.2, 0.3), 1.5, 0.85, 2.6)),
        ING ("medium attack, long tail", "", p.env (0, p.j (0.4, 0.3), 1.5, 0.9, p.j (3.5, 0.2))),
        ING ("an ambient drone that never ends", "Drone", p.env (0, 2.5, 2.0, 1.0, 5.0)),
        ING ("an octave-up air layer", "", p.an (2, "Sine", 0.15).tune (2, 1)),
        ING ("lo-fi crush", "Lo-Fi", p.crush (10, 3, 0.25)),
    };
    R.flavours = { Shimmer, Dream, Space, Wide, Motion, Tape, LoFi, Clean };
    R.heroSource = { { "Seraph Bloom", 0 }, { "Angel Choir", 1 }, { "Grey Skies", 2 }, { "Celestial Pad", 3 }, { "Dark Drone", 7 }, { "Ambient Wash", 8 }, { "Northern Lights", 12 } };
    return R;
}

//==============================================================================
// PERCUSSION: tempo-synced loops (hold one key). Sound x pattern x filter x decay/tune x FX.
//==============================================================================
bool isBackbeatSource (const Patch& p)
{
    const auto smp = p.getChoice (ids::smpSample);
    return smp == "Clap" || smp == "Snap" || smp == "Kick Thump";
}

Ing patternIng (const Pattern& pt, bool pitched)
{
    const Pattern* ptr = &pt;
    return Ing { juce::String (pitched ? "a pitched " : "a ") + pt.name + " pattern", "",
                 [ptr, pitched] (Patch& p)
                 {
                     static const Pattern backbeats[] = {
                         { "backbeat",            "0000900000009000", "1",                "m", {}, "1/16",   0.0 },
                         { "backbeat with a roll", "0000900000009000", "1111111111111131", "m", {}, "1/16",   0.0 },
                         { "half-time clap",      "0000000090000000", "1",                "m", {}, "1/16",   0.0 },
                         { "late snap",           "0000900000009009", "1",                "m", {}, "1/16",   0.0 },
                         { "triplet stutter",     "000900000900",     "111111111141",     "m", {}, "1/16 T", 0.0 },
                         { "four on the floor",   "9000900090009000", "1",                "m", {}, "1/16",   0.0 },
                     };
                     const Pattern& use = isBackbeatSource (p) ? backbeats[(size_t) (juce::String (ptr->name).hashCode() & 0x7fffffff) % 6] : *ptr;
                     p.steps (use.lvl, use.rat, use.gate, {}, use.rate, (int) strlen (use.lvl), use.swing, true);
                     if (pitched && ! isBackbeatSource (p))
                         for (int st = 0; st < kNumSteps; ++st) p.set (ids::step (st, "pitch"), use.pitch.empty() ? 0 : use.pitch[(size_t) st % use.pitch.size()]);
                 } };
}

CategoryRecipe recipePerc()
{
    CategoryRecipe R;
    R.category = "PERCUSSION"; R.count = 77; R.tags = { "Loop", "Synced" };
    R.pools = { { "Midnight", "Cold", "Glass", "Trap", "Drill", "Wave", "Ghost", "Rusty", "Silver", "Smoke", "Ice", "Neon", "Paper", "Chrome", "Static", "Velvet" },
                { "Hats", "Rolls", "Shakers", "Ticks", "Perc", "Rims", "Bounce", "Groove", "Skips", "Clicks" },
                { "Trap Hats", "Metal Hats", "Loose Shaker", "Noise Hats", "Rim Groove", "Clap Rolls", "Wave Perc" } };
    R.base = [] (Patch& p) { p.set (ids::osc (0, "on"), 0).filt (0, "Off", 12000, 0.0).voices (16).level (-9.0).drift (0.0).noSat().env (0, 0.001, 0.3, 1.0, 0.04).set (ids::velSens, 0.9); };
    R.finish = [] (Patch& p) { p.eq (-6.0, 3000, 1.5, 2.5, 250.0).comp (-18, 3, 3, 2, 60).trans (0.35, -0.2).stereo (1.2); };
    R.sources = {
        ING ("a closed-hat loop (hold any key)", "Hats", p.smp ("Hat Closed", 0.85, false)),
        ING ("a bright, pitched-up closed hat (hold any key)", "Hats", p.smp ("Hat Closed", 0.85, false, 4)),
        ING ("a metallic hat sample (hold any key)", "Hats", p.smp ("Hat Metal", 0.85, false)),
        ING ("a choked open hat (hold any key)", "Hats", p.smp ("Hat Open", 0.8, false).env (0, 0.001, 0.09, 0.0, 0.05)),
        ING ("synthesized 808-style metal hats from FM (hold any key)", "Hats", p.wt (0, "Metal FM", p.j (0.7, 0.2), 0.7).tune (0, 2).filt (1, "High Pass 24", p.j (6500, 0.2), 0.2).ring (p.j (3400, 0.3), 0.3).env (0, 0.001, p.j (0.045, 0.3), 0.0, 0.04)),
        ING ("synthesized white-noise hats (hold any key)", "Hats", p.noise ("White", 0.8).filt (1, "High Pass 24", p.j (7500, 0.2), 0.25).env (0, 0.001, p.j (0.04, 0.3), 0.0, 0.03)),
        ING ("sizzling bright-noise hats (hold any key)", "Hats", p.noise ("Bright", 0.85).filt (1, "Peak", 11000, 0.45).env (0, 0.001, p.j (0.06, 0.3), 0.0, 0.04)),
        ING ("a shaker loop (hold any key)", "Shaker", p.smp ("Shaker", 0.85, false)),
        ING ("pitched rims (hold a key around C4; the key transposes it)", "Perc", p.smp ("Rim", 0.85, true)),
        ING ("tick percussion (hold a key around C4)", "Perc", p.smp ("Perc Tick", 0.85, true)),
        ING ("claps (hold any key)", "Clap", p.smp ("Clap", 0.85, false)),
        ING ("finger snaps (hold any key)", "Clap", p.smp ("Snap", 0.85, false)),
        ING ("wave-style glass hits, an octave down (hold a key around C4)", "Perc", p.smp ("Glass Hit", 0.75, true, -12).env (0, 0.001, 0.25, 0.0, 0.2)),
        ING ("metal hits (hold a key around C4)", "Perc", p.smp ("Metal Hit", 0.75, true, -12).env (0, 0.001, 0.2, 0.0, 0.15)),
        ING ("bell strikes (hold a key around C4)", "Perc", p.smp ("Bell Strike", 0.7, true, -12).env (0, 0.001, 0.3, 0.0, 0.25)),
        ING ("synth mallet perc (hold a key around C4)", "Perc", p.set (ids::osc (0, "on"), 1).wt (0, "Mallet", p.j (0.3, 0.5), 0.8).env (0, 0.001, 0.12, 0.0, 0.08)),
        ING ("a kick-thump groove (hold any key)", "Kick", p.smp ("Kick Thump", 0.85, false)),
    };
    for (auto& pt : hatPatterns) R.motions.push_back (patternIng (pt, false));
    for (auto& pt : percPatterns) R.motions.push_back (patternIng (pt, true));
    R.filters = {
        ING ("high-passed", "", p.filt (0, "High Pass 12", p.j (3500, 0.4), 0.1)),
        ING ("crisp 24 dB high-pass", "", p.filt (0, "High Pass 24", p.j (6000, 0.3), 0.2)),
        ING ("band-passed and boxy", "", p.filt (0, "Band Pass", p.j (2500, 0.4), 0.35)),
        ING ("with a resonant peak ringing", "", p.filt (0, "Peak", p.j (5000, 0.4), 0.6)),
        ING ("low-passed and dusty", "", p.filt (0, "Low Pass 12", p.j (4500, 0.3), 0.15)),
        ING ("through a tuned comb: metallic", "", p.filt (0, "Comb +", p.j (800, 0.4), 0.55, 0.0, 0.0, 0.5)),
        ING ("unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
    };
    R.styles = {
        ING ("tight, clipped decays", "", p.env (0, 0.001, 0.03, 0.0, 0.02)),
        ING ("short decays", "", p.env (0, 0.001, 0.08, 0.0, 0.05)),
        ING ("medium decays", "", p.env (0, 0.001, 0.18, 0.0, 0.12)),
        ING ("ringing, open decays", "", p.env (0, 0.001, 0.4, 0.1, 0.3)),
        ING ("tuned down 3 semitones", "", p.set (ids::smpTune, -3).tune (0, 0, -3)),
        ING ("tuned up 5 semitones", "", p.set (ids::smpTune, 5).tune (0, 0, 5)),
        ING ("velocity accents exaggerated", "", p.set (ids::velSens, 1.0)),
    };
    R.flavours = { Clean, Space, LoFi, Grit, Wide, Tape, Dream, OTT, Motion, Dirty };
    R.heroMotion = { { "Trap Hats", 1 }, { "Metal Hats", 2 }, { "Loose Shaker", 7 }, { "Noise Hats", 6 }, { "Rim Groove", 11 }, { "Wave Perc", 14 } };
    R.heroSource = { { "Trap Hats", 0 }, { "Metal Hats", 4 }, { "Loose Shaker", 7 }, { "Noise Hats", 5 }, { "Rim Groove", 8 }, { "Clap Rolls", 10 }, { "Wave Perc", 12 } };
    return R;
}

//==============================================================================
// FX: risers, downlifters, impacts, sweeps, noise patterns, swells, sirens, zaps, atmospheres, glitches
//==============================================================================
CategoryRecipe recipeFx()
{
    CategoryRecipe R;
    R.category = "FX"; R.count = 80; R.tags = { "FX" };
    R.pools = { { "Heaven", "Void", "Tension", "Rapture", "Gravity", "Static", "Storm", "Signal", "Eclipse", "Comet", "Pressure", "Ascension", "Nova", "Abyss" },
                { "Riser", "Sweep", "Impact", "Downer", "Wash", "Siren", "Zap", "Swell", "Glitch", "Transition", "Pulse" },
                { "Noise Riser", "Downlifter", "Cinematic Impact", "Transition Sweep", "Noise Pattern", "Reverse Swell", "Pirate Siren", "Laser Zap", "Rain Atmos", "Glitch Machine" } };
    R.base = [] (Patch& p) { p.set (ids::osc (0, "on"), 0).voices (8).level (-9.0).drift (0.35).noSat().env (0, 0.05, 1.0, 1.0, 1.0); };
    R.finish = [] (Patch& p) { p.eq (-4.0, 2500, 0.0, 2.0, 150.0).stereo (1.4); };
    R.sources = {
        ING ("white noise", "Noise", p.noise ("White", 0.75)),
        ING ("pink noise", "Noise", p.noise ("Pink", 0.75)),
        ING ("bright noise over a detuned saw stack", "Noise", p.noise ("Bright", 0.5).an (1, "Saw", 0.35).uni (1, 5, 0.45, 1.0)),
        ING ("a seven-voice supersaw", "Synth", p.set (ids::osc (0, "on"), 1).an (0, "Saw", 0.6).uni (0, 7, 0.4, 1.0)),
        ING ("square and pulse synths", "Synth", p.set (ids::osc (0, "on"), 1).an (0, "Square", 0.6).an (1, "Pulse", 0.4).pw (1, 0.25)),
        ING ("a cinematic boom sample with a deep sub", "Impact", p.smp ("Impact Boom", 0.9, false).sub ("Sine -2", 0.4)),
        ING ("a reversed swell sample", "Swell", p.smp ("Reverse Swell", 0.85, true)),
        ING ("a noise-riser sample", "Riser", p.smp ("Noise Riser", 0.85, false)),
        ING ("a sub-drop sample", "Impact", p.smp ("Sub Drop", 0.9, false)),
        ING ("looped rain", "Atmos", p.smp ("Rain", 0.85, false).smpLoop (0.05, 0.98)),
        ING ("looped wind", "Atmos", p.smp ("Wind", 0.85, false).smpLoop (0.05, 0.98)),
        ING ("vinyl crackle", "Atmos", p.smp ("Vinyl Crackle", 0.85, false).smpLoop (0.05, 0.98)),
        ING ("tape hiss", "Atmos", p.smp ("Tape Hiss", 0.85, false).smpLoop (0.05, 0.98)),
        ING ("a Digital Grit wavetable", "Glitch", p.set (ids::osc (0, "on"), 1).wt (0, "Digital Grit", p.j (0.6, 0.3), 0.75)),
        ING ("a Laser Sync wavetable", "Synth", p.set (ids::osc (0, "on"), 1).wt (0, "Laser Sync", p.j (0.5, 0.4), 0.75)),
        ING ("a Chaos wavetable", "Synth", p.set (ids::osc (0, "on"), 1).wt (0, "Chaos III", p.j (0.5, 0.4), 0.7).uni (0, 3, 0.3, 0.9)),
        ING ("a sampled choir", "Swell", p.smp ("Choir Ahh", 0.8, true).smpLoop (0.2, 0.92)),
        ING ("metal hits", "Impact", p.smp ("Metal Hit", 0.8, true)),
    };
    R.motions = {
        ING ("rises for four bars as you hold it (filter + pitch climb)", "Riser", p.env (1, p.j (6.5, 0.2), 0.1, 1.0, 1.5).mod ("ENV 2", "Filter 1 Cutoff", 0.75).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Osc B Pitch", 1.0).env (0, 0.6, 1.0, 1.0, 1.5)),
        ING ("a fast two-bar riser", "Riser", p.env (1, p.j (3.2, 0.2), 0.1, 1.0, 1.0).mod ("ENV 2", "Filter 1 Cutoff", 0.7).mod ("ENV 2", "Osc A Pitch", 0.7).env (0, 0.3, 1.0, 1.0, 1.0)),
        ING ("falls away: a downlifter", "Downlifter", p.env (1, 0.001, p.j (3.0, 0.3), 0.0, 1.0).mod ("ENV 2", "Filter 1 Cutoff", 0.6).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Osc B Pitch", 1.0).env (0, 0.01, 3.5, 0.0, 1.0)),
        ING ("hits once and rings out: an impact", "Impact", p.env (0, 0.001, p.j (3.0, 0.3), 0.0, 2.5)),
        ING ("a one-bar synced sweep", "Sweep", p.lfoSync (0, "Triangle", "1 bar", 1.0, 0.3).mod ("LFO 1", "Filter 1 Cutoff", 0.55)),
        ING ("a two-bar rising saw sweep", "Sweep", p.lfoSync (0, "Saw Up", "2 bars", 1.0, 0.2).mod ("LFO 1", "Filter 1 Cutoff", 0.6)),
        ING ("chopped into synced 16th bursts (STEP)", "Synced", p.env (0, 0.001, 0.12, 0.0, 0.08).steps ("9070907790709999", "1111111211111234", "smsmsmsmsmsmssss", {}, "1/16", 16, 0.0, true)),
        ING ("a siren: the pitch swoops up and down", "Siren", p.lfo (0, "Triangle", p.j (0.5, 0.4)).mod ("LFO 1", "Osc A Pitch", 0.3).mod ("LFO 1", "Osc B Pitch", 0.3).mod ("LFO 1", "Filter 1 Cutoff", 0.2)),
        ING ("a laser zap: fast pitch dive", "Zap", p.env (2, 0.001, p.j (0.25, 0.4), 0.0, 0.1).mod ("ENV 3", "Osc A Pitch", 1.0).env (0, 0.001, 0.4, 0.0, 0.2)),
        ING ("stuttered, pitched glitch chops (STEP)", "Glitch", p.env (0, 0.001, 0.15, 0.4, 0.05).steps ("9090999390909933", "1121112411211143", "s", { 0, 0, 12, 0, 0, -12, 0, 7 }, "1/16", 16, 0.0, true)),
        ING ("a tremolo that speeds the build", "Build", p.trem ("Square", "1/16", 0.85).env (0, 1.5, 1.0, 1.0, 1.0)),
        ING ("a slow swell that just hangs there", "Swell", p.env (0, p.j (1.8, 0.3), 1.0, 1.0, 3.0)),
    };
    R.filters = {
        ING ("band-passed", "", p.filt (0, "Band Pass", p.j (700, 0.5), 0.45)),
        ING ("low-passed", "", p.filt (0, "Low Pass 12", p.j (3000, 0.5), 0.3)),
        ING ("high-passed", "", p.filt (0, "High Pass 12", p.j (900, 0.5), 0.25)),
        ING ("through a resonant comb", "", p.filt (0, "Comb +", p.j (400, 0.5), 0.6, 0.0, 0.0, 0.5)),
        ING ("through a formant filter", "", p.filt (0, "Formant", p.j (800, 0.4), 0.5)),
        ING ("unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
    };
    R.styles = {
        ING ("short and dry", "", p.reverb (0.4, 0.5, 0.12, 0.0)),
        ING ("a long washed-out tail", "", p.reverb (0.97, 0.3, 0.45, 30.0)),
        ING ("pitched an octave up", "", p.tune (0, 1).tune (1, 1).set (ids::smpTune, 12)),
        ING ("pitched an octave down", "", p.tune (0, -1).tune (1, -1).set (ids::smpTune, -12)),
        ING ("thrown into a dotted delay", "", p.delay ("1/8 D", 0.55, 0.32)),
        ING ("crushed to bits", "", p.crush (7, 5, 0.45)),
    };
    R.flavours = { Space, Shimmer, Dirty, Wide, LoFi, Motion, Dream, Grit, OTT, Tape };
    R.heroMotion = { { "Noise Riser", 0 }, { "Downlifter", 2 }, { "Cinematic Impact", 3 }, { "Transition Sweep", 4 }, { "Noise Pattern", 6 },
                     { "Reverse Swell", 11 }, { "Pirate Siren", 7 }, { "Laser Zap", 8 }, { "Rain Atmos", 11 }, { "Glitch Machine", 9 } };
    R.heroSource = { { "Noise Riser", 0 }, { "Downlifter", 1 }, { "Cinematic Impact", 5 }, { "Transition Sweep", 0 }, { "Noise Pattern", 0 },
                     { "Reverse Swell", 6 }, { "Pirate Siren", 3 }, { "Laser Zap", 4 }, { "Rain Atmos", 9 }, { "Glitch Machine", 13 } };
    return R;
}
