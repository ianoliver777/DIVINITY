// Included inside FactoryPresetsV7.cpp's anonymous namespace (after Patch, applyFlavour, Pattern, Spec).
//
// DIVINITY 0.9.0 factory bank: 777 presets for wave music and Reese bass.
//
//   REESE 120 . BASS 80 . LEAD 90 . SYNTH 90 . PAD 95 . TEXTURE 70 . SEQUENCE 90 . PERCUSSION 62 . FX 80
//
// Every preset is built from ingredients: a SOURCE (what makes the sound), a MOVEMENT (what moves,
// how fast), a FILTER / TONE, a PLAY style, a SPACE (reverb + delay design; nothing in this bank is dry)
// and an optional CHARACTER chain. The NAME is assembled afterwards from the words of the ingredients
// that were actually used ("Breathing Hollow Reese" = a detuned-saw reese, through a negative comb, with
// a slow filter LFO). Words only exist where the sound backs them up, and the test suite measures the
// claims (beating for "Reese", movement for "Wobbling", tail for "Cathedral", rise for "Riser"...).
// Candidates are rendered offline and the most different ones are kept (FactoryPresetsV8Picks.inc).

struct Ing
{
    juce::String word, desc, tag;
    std::function<void (Patch&)> fn;
};
#define ING(word, desc, tag, ...) Ing { word, desc, tag, [] (Patch& p) { __VA_ARGS__; } }

bool isWT (const Patch& p, int o = 0) { return p.getChoice (ids::osc (o, "engine")) == "Wavetable"; }
bool filterOn (const Patch& p) { auto t = p.getChoice (ids::filt (0, "type")); return t.isNotEmpty() && t != "Off"; }
const char* posOrCutoff (const Patch& p) { return isWT (p) ? "Osc A WT Pos" : (filterOn (p) ? "Filter 1 Cutoff" : "Osc A Fold"); }
const char* cutoffOrPos (const Patch& p) { return filterOn (p) ? "Filter 1 Cutoff" : (isWT (p) ? "Osc A WT Pos" : "Osc A Fold"); }

struct CategoryRecipe
{
    juce::String category;
    int count = 0;
    std::vector<Ing> sources, motions, filters, styles, spaces;
    std::vector<Flavour> characters;              // optional extra chain (Clean = none)
    std::vector<const char*> templates;           // name layouts: S source, M movement, F filter, P space, Y style
    std::function<void (Patch&)> base, finish;
    /** Optional: rejects source / movement pairs that wouldn't do what the words say (a "Growling" pure sine). */
    std::function<bool (const juce::String& source, const juce::String& motion)> allowed;
    std::function<bool (const juce::String& source, const juce::String& filter)> allowedFilter;
    std::function<bool (const juce::String& motion, const juce::String& filter)> allowedMotionFilter;
    std::function<bool (const juce::String& source, const juce::String& space)> allowedSpace;
    bool lowEnd = false, superWet = false;
    juce::StringArray tags;
};

struct Choice { int src, mot, fil, sty, spa; Flavour ch; };

Choice drawChoice (const CategoryRecipe& R, int k)
{
    juce::Random cr (R.category.hashCode64() * 31 + (juce::int64) k * 7919 + 17);
    Choice c;
    c.src = cr.nextInt ((int) R.sources.size());
    c.mot = cr.nextInt ((int) R.motions.size());
    c.fil = cr.nextInt ((int) R.filters.size());
    // reject combinations that wouldn't do what their words say (a "Growling" pure sine, a filter LFO with no filter...)
    auto ok = [&R, &c]
    {
        const auto& S = R.sources[(size_t) c.src].word; const auto& M = R.motions[(size_t) c.mot].word; const auto& F = R.filters[(size_t) c.fil].word;
        return (! R.allowed || R.allowed (S, M)) && (! R.allowedFilter || R.allowedFilter (S, F)) && (! R.allowedMotionFilter || R.allowedMotionFilter (M, F));
    };
    for (int tries = 0; ! ok() && tries < 256; ++tries)
    {
        if (tries % 2 == 0) c.mot = cr.nextInt ((int) R.motions.size());
        else                c.fil = cr.nextInt ((int) R.filters.size());
    }
    c.sty = cr.nextInt ((int) R.styles.size());
    c.spa = cr.nextInt ((int) R.spaces.size());
    for (int tries = 0; R.allowedSpace && ! R.allowedSpace (R.sources[(size_t) c.src].word, R.spaces[(size_t) c.spa].word) && tries < 64; ++tries)
        c.spa = cr.nextInt ((int) R.spaces.size());
    c.ch = R.characters.empty() ? Clean : R.characters[(size_t) cr.nextInt ((int) R.characters.size())];
    return c;
}

Spec makeCandidate (const CategoryRecipe& R, int k)
{
    const auto c = drawChoice (R, k);
    juce::Random jitter (R.category.hashCode64() * 131 + (juce::int64) k * 104729 + 5);
    Spec s; s.category = R.category; s.name = R.category + " #" + juce::String (k);
    auto& p = s.patch; p.rng = &jitter;
    p.set (ids::osc (0, "on"), 0);   // the init patch has osc A on: only sources that use it switch it back on
    if (R.base) R.base (p);
    R.sources[(size_t) c.src].fn (p);
    R.filters[(size_t) c.fil].fn (p);
    R.styles[(size_t) c.sty].fn (p);
    R.motions[(size_t) c.mot].fn (p);
    if (R.finish) R.finish (p);
    if (c.ch != Clean) applyFlavour (p, c.ch, R.lowEnd);
    R.spaces[(size_t) c.spa].fn (p);   // space last: every preset ends wet, whatever the character chain did
    if (R.superWet)
    {   // synths, leads, pads, textures, sequences: never less than a big, long reverb
        p.set (ids::revOn, 1).set (ids::revMix, juce::jmax (0.38, p.get (ids::revMix, 0.0))).set (ids::revSize, juce::jmax (0.88, p.get (ids::revSize, 0.0)));
    }
    p.rng = nullptr;

    juce::StringArray tags;
    for (auto* i : { &R.sources[(size_t) c.src], &R.motions[(size_t) c.mot], &R.styles[(size_t) c.sty], &R.spaces[(size_t) c.spa] })
        if (i->tag.isNotEmpty() && ! tags.contains (i->tag)) tags.add (i->tag);
    if (c.ch != Clean) tags.add (flavourTag[(int) c.ch]);
    tags.addArray (R.tags);
    s.tags = tags.joinIntoString (", ");
    auto cap = [] (juce::String x) { return x.substring (0, 1).toUpperCase() + x.substring (1); };
    s.description = cap (R.sources[(size_t) c.src].desc) + ", " + R.filters[(size_t) c.fil].desc + ". "
                  + cap (R.motions[(size_t) c.mot].desc) + "; " + R.styles[(size_t) c.sty].desc + ". Space: "
                  + R.spaces[(size_t) c.spa].desc + (c.ch != Clean ? juce::String (". Character: ") + flavourPhrase[(int) c.ch] : juce::String()) + ".";
    return s;
}

/** Name from the ingredient words, first free layout wins. Returns empty if every layout is taken. */
juce::String nameFor (const CategoryRecipe& R, int k, const juce::StringArray& used)
{
    const auto c = drawChoice (R, k);
    const juce::String S = R.sources[(size_t) c.src].word, M = R.motions[(size_t) c.mot].word, F = R.filters[(size_t) c.fil].word,
                       P = R.spaces[(size_t) c.spa].word, Y = R.styles[(size_t) c.sty].word;
    for (auto* t : R.templates)
    {
        juce::StringArray parts;
        bool ok = true;
        for (auto* ch = t; *ch != 0; ++ch)
        {
            const juce::String* w = *ch == 'S' ? &S : *ch == 'M' ? &M : *ch == 'F' ? &F : *ch == 'P' ? &P : *ch == 'Y' ? &Y : nullptr;
            if (w == nullptr) continue;
            if (w->isEmpty()) { ok = false; break; }
            parts.add (*w);
        }
        if (! ok) continue;
        const auto n = parts.joinIntoString (" ");
        if (! used.contains (n)) return n;
    }
    return {};
}

//==============================================================================
// Shared spaces. Synth-family presets are drenched; low-end and drums keep their core dry-ish but
// still sit in a room, with the reverb tail high-damped and the lows kept mono.
//==============================================================================
std::vector<Ing> wetSpaces()
{
    return {
        ING ("Cathedral", "a huge cathedral reverb with a slow quarter-note echo", "Cathedral",
             p.reverb (0.97, 0.3, p.j (0.5, 0.1), 60.0).delay ("1/4", 0.35, 0.22, true, 0.5)),
        ING ("Echo", "ping-pong dotted-eighth echoes into a hall", "Echo",
             p.delay ("1/8 D", 0.55, 0.4, true, 0.55).reverb (0.8, 0.4, 0.35, 30.0)),
        ING ("Abyss", "a bottomless dark reverb and long dark echoes", "Abyss",
             p.reverb (0.98, 0.75, p.j (0.55, 0.1), 90.0).delay ("1/4 D", 0.5, 0.25, true, 0.3)),
        ING ("Halo", "a frequency-shifted halo feeding a bright hall", "Shimmer",
             p.fshift (0.16, 0.16).reverb (0.95, 0.2, 0.5, 50.0).chorus (0.3, 0.4, 0.25).delay ("1/8", 0.3, 0.18, true, 0.7)),
        ING ("Mist", "soft chorus into a misty hall and quiet eighth echoes", "Misty",
             p.chorus (0.25, 0.5, 0.35).reverb (0.88, 0.5, 0.45, 40.0).delay ("1/8", 0.35, 0.2, true, 0.45)),
        ING ("Tunnel", "dark triplet echoes down a tunnel", "Echo",
             p.delay ("1/4 T", 0.6, 0.38, false, 0.3).reverb (0.7, 0.6, 0.32, 20.0)),
        ING ("Distant", "far away: long predelay, mostly reverb, top rolled off", "Distant",
             p.reverb (0.9, 0.6, 0.6, 120.0).delay ("1/2 D", 0.4, 0.28, true, 0.35).eq (0, 900, -1.0, -4.0)),
        ING ("Haze", "tape warmth, slow chorus and a hazy room with eighth echoes", "Hazy",
             p.sat (0.3).chorus (0.18, 0.4, 0.3).reverb (0.85, 0.55, 0.42, 70.0).delay ("1/8", 0.35, 0.22, true, 0.4)),
    };
}

std::vector<Ing> lowSpaces()
{
    return {
        ING ("", "a short damped room and a faint eighth-note slap", "", p.reverb (0.45, 0.7, 0.12, 10.0).delay ("1/8", 0.2, 0.08, true, 0.35)),
        ING ("", "a dark plate tail on the upper harmonics, low end kept mono", "", p.reverb (0.65, 0.75, 0.15, 25.0).delay ("1/4", 0.25, 0.08, true, 0.3)),
        ING ("", "a wide dark hall kept low in the mix", "Wide", p.reverb (0.8, 0.8, 0.14, 40.0).chorus (0.2, 0.3, 0.15)),
        ING ("Cavern", "a cavernous dark reverb, low end mono", "Cavern", p.reverb (0.9, 0.8, 0.2, 60.0).delay ("1/4 D", 0.3, 0.1, true, 0.25)),
        ING ("Echo", "dark dotted echoes on the top end", "Echo", p.delay ("1/8 D", 0.4, 0.16, true, 0.3).reverb (0.6, 0.7, 0.1, 20.0)),
    };
}

std::vector<Ing> drumSpaces()
{
    return {
        ING ("", "a tight room", "", p.reverb (0.4, 0.6, 0.14, 5.0)),
        ING ("", "a dark plate with a quiet sixteenth slap", "", p.reverb (0.6, 0.7, 0.16, 10.0).delay ("1/16", 0.25, 0.1, true, 0.4)),
        ING ("Echo", "dotted echoes trailing each hit", "Echo", p.delay ("1/8 D", 0.45, 0.2, true, 0.45).reverb (0.55, 0.6, 0.12, 8.0)),
        ING ("Hall", "a big hall behind the hits (transients stay dry and upfront)", "Hall", p.reverb (0.85, 0.5, 0.2, 30.0)),
    };
}

//==============================================================================
// REESE (120): every source is two or more detuned saw-family voices, so every preset beats.
//==============================================================================
std::vector<Ing> reeseSources()
{
    return {
        ING ("Twin Saw Reese", "two analog saws detuned against each other: the classic reese", "Reese",
             p.an (0, "Saw", 0.8).uni (0, 2, p.j (0.16, 0.3), 0.4).an (1, "Saw", 0.7).tune (1, 0, 0, p.j (14, 0.3))),
        ING ("Supersaw Reese", "a five-voice detuned saw stack", "Reese", p.an (0, "Saw", 0.75).uni (0, 5, p.j (0.28, 0.2), 0.7)),
        ING ("Core Reese", "the Reese Core table, three unison voices beating", "Reese",
             p.wt (0, "Reese Core", p.j (0.45, 0.4), 0.85).uni (0, 3, p.j (0.2, 0.3), 0.5)),
        ING ("Stacked Reese", "the Saw Stack table doubled with a detuned saw", "Reese",
             p.wt (0, "Saw Stack", p.j (0.55, 0.3), 0.8).uni (0, 2, p.j (0.14, 0.3), 0.4).an (1, "Saw", 0.45).tune (1, 0, 0, -11)),
        ING ("Square Reese", "detuned square stack: hollow, cold beating", "Reese",
             p.wt (0, "Square Stack", p.j (0.5, 0.3), 0.8).uni (0, 3, p.j (0.18, 0.3), 0.5)),
        ING ("Tube Reese", "a warm Tube Saw pair, one voice an octave down", "Reese",
             p.wt (0, "Tube Saw", p.j (0.5, 0.4), 0.85).uni (0, 2, p.j (0.15, 0.3), 0.4).an (1, "Saw", 0.45).tune (1, -1, 0, 9)),
        ING ("Sync Reese", "hard-synced detuned saws that tear as they move", "Reese",
             p.wt (0, "Sync Saw", p.j (0.3, 0.5), 0.8).uni (0, 2, 0.15, 0.4).an (1, "Saw", 0.5).tune (1, 0, 0, 12)),
        ING ("Folded Reese", "wavefolded detuned saws", "Reese",
             p.wt (0, "Saw Fold", p.j (0.35, 0.4), 0.8).fold (0, p.j (0.25, 0.4)).uni (0, 2, 0.16, 0.4).an (1, "Saw", 0.4).tune (1, 0, 0, -13)),
        ING ("PWM Reese", "a detuned pulse-width stack", "Reese", p.wt (0, "PWM Stack", p.j (0.5, 0.3), 0.8).uni (0, 3, p.j (0.18, 0.3), 0.5)),
        ING ("Octave Reese", "two detuned saw pairs an octave apart", "Reese",
             p.an (0, "Saw", 0.75).uni (0, 2, 0.14, 0.4).an (1, "Saw", 0.5).uni (1, 2, 0.18, 0.6).tune (1, 1, 0, 7)),
        ING ("Fifth Reese", "detuned saws with a detuned fifth on top", "Reese",
             p.an (0, "Saw", 0.75).uni (0, 2, 0.16, 0.4).an (1, "Saw", 0.32).uni (1, 2, 0.2, 0.6).tune (1, 0, 7, 6)),
        ING ("Neuro Reese", "the Neuro Growl table over a detuned saw pair", "Reese",
             p.wt (0, "Neuro Growl", p.j (0.5, 0.4), 0.7).uni (0, 2, 0.12, 0.3).an (1, "Saw", 0.55).uni (1, 2, 0.16, 0.4)),
        ING ("Vowel Reese", "detuned saws under a vowel-sweep table", "Reese",
             p.an (0, "Saw", 0.7).uni (0, 2, 0.15, 0.4).wt (1, "Vowel Sweep", p.j (0.4, 0.5), 0.55).uni (1, 2, 0.12, 0.4)),
        ING ("FM Reese", "detuned saws with a touch of FM from a sine: grit inside the beating", "Reese",
             p.an (0, "Saw", 0.8).uni (0, 3, 0.16, 0.5).choice (ids::osc (0, "warp"), "FM").set (ids::osc (0, "warp_amt"), p.j (0.22, 0.3))
              .choice (ids::osc (0, "warp_src"), "Osc B").set (ids::osc (1, "on"), 0).choice (ids::osc (1, "engine"), "Analog").choice (ids::osc (1, "wave"), "Sine")),
        ING ("Bent Reese", "detuned saws with their phase bent forward", "Reese",
             p.an (0, "Saw", 0.8).uni (0, 3, 0.17, 0.5).choice (ids::osc (0, "warp"), "Bend +").set (ids::osc (0, "warp_amt"), p.j (0.45, 0.3))),
        ING ("Triple Reese", "three saws at 0, +9 and -11 cents", "Reese",
             p.an (0, "Saw", 0.6).an (1, "Saw", 0.55).tune (1, 0, 0, 9).an (2, "Saw", 0.55).tune (2, 0, 0, -11)),
        ING ("Morphing Reese", "a saw-to-square table, detuned, sliding between shapes", "Reese",
             p.wt (0, "Saw To Square", p.j (0.4, 0.5), 0.85).uni (0, 3, p.j (0.2, 0.3), 0.5)),
        ING ("Weighted Reese", "detuned saws over a sine sub an octave down", "Reese",
             p.an (0, "Saw", 0.7).uni (0, 2, 0.16, 0.4).an (1, "Saw", 0.55).tune (1, 0, 0, -12).sub ("Sine -1", 0.55)),
    };
}

std::vector<Ing> lowMotions()
{
    return {
        ING ("", "no modulation: the detuning does the moving", "", p.set (ids::analog, 0.12)),
        ING ("Breathing", "a slow free LFO breathes the filter open and shut", "Moving",
             p.lfo (0, "Sine", p.j (0.14, 0.3), 1.0, 0.4).mod ("LFO 1", cutoffOrPos (p), p.j (0.4, 0.2))),
        ING ("Drifting", "a slow random wander on the detune and tone", "Moving",
             p.lfo (1, "Random", p.j (1.0, 0.2), 1.0, 0.7).mod ("LFO 2", "Osc A Detune", 0.4).mod ("LFO 2", cutoffOrPos (p), 0.8)),
        ING ("Wobbling", "a quarter-note wobble on the filter", "Wobble",
             p.lfoSync (0, "Sine", "1/4", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.55)),
        ING ("Rolling", "an eighth-note roll on the filter", "Wobble",
             p.lfoSync (0, "Sine", "1/8", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.8)),
        ING ("Triplet", "a triplet-eighth wobble", "Wobble",
             p.lfoSync (0, "Triangle", "1/8 T", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.5)),
        ING ("Sweeping", "a one-bar sweep that opens and closes", "Moving",
             p.lfoSync (0, "Triangle", "1 bar", 1.0, 0.3).mod ("LFO 1", cutoffOrPos (p), 0.75)),
        ING ("Growling", "an envelope growls through the tone on every note", "Growl",
             p.env (1, 0.005, p.j (1.2, 0.2), 0.15, 0.3).mod ("ENV 2", cutoffOrPos (p), 0.7).mod ("ENV 2", "Osc A Fold", 0.35)),
        ING ("Stepping", "a sixteenth-step pattern gating the filter", "Rhythmic",
             p.steps ("9373937393739373", "1", "m", {}, "1/16", 16, 0.0, false).mod ("Step", cutoffOrPos (p), 0.6)),
        ING ("Twisting", "a half-note LFO twisting the fold and detune", "Moving",
             p.lfoSync (0, "Triangle", "1/2", 1.0, 0.3).mod ("LFO 1", "Osc A Fold", 0.45).mod ("LFO 1", "Osc A Detune", 0.35).mod ("LFO 1", cutoffOrPos (p), 0.55)),
    };
}

CategoryRecipe reeseRecipe()
{
    CategoryRecipe R;
    R.category = "REESE"; R.count = 120; R.lowEnd = true; R.tags = { "Reese", "Mono", "Wave" };
    R.sources = reeseSources();
    R.motions = lowMotions();
    R.filters = {
        ING ("Dark", "through a dark 24 dB low-pass", "", p.filt (0, "Low Pass 24", p.j (650, 0.3), 0.15, 0.2, 0.3)),
        ING ("Warm", "through a driven ladder", "", p.filt (0, "MG Low 24", p.j (1300, 0.3), p.j (0.25, 0.3), p.j (0.4, 0.3), 0.3)),
        ING ("Hollow", "through a negative comb: hollow and phasey", "", p.filt (0, "Comb -", p.j (260, 0.3), 0.55, 0.0, 1.0, 0.5).filt (1, "Low Pass 12", 3000, 0.1)),
        ING ("Vocal", "through a formant filter", "", p.filt (0, "Formant", p.j (750, 0.3), 0.5, 0.2)),
        ING ("Open", "wide open (a gentle 12 dB low-pass at 9 kHz): every harmonic of the beating", "", p.filt (0, "Low Pass 12", 9000, 0.0, 0.0, 0.0)),
        ING ("Nasal", "through a band-pass, the sub carrying the weight", "", p.filt (0, "Band Pass", p.j (800, 0.3), 0.35, 0.2, 0.3).sub ("Sine -1", 0.6)),
        ING ("Resonant", "through a resonant 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (1500, 0.3), p.j (0.5, 0.2), 0.15, 0.3)),
        ING ("Gritty", "through a driven ladder into tube distortion", "", p.filt (0, "MG Low 24", p.j (1700, 0.3), 0.2, 0.6, 0.3).dist ("Tube", 0.4, 0.45, 0.5)),
    };
    R.styles = {
        ING ("Gliding", "legato with a slow glide", "Glide", p.mono (p.j (120, 0.3), true)),
        ING ("", "tight legato", "", p.mono (p.j (35, 0.3), true)),
        ING ("", "plucky: drops to a lower sustain", "", p.mono (20, true).env (0, 0.002, p.j (0.45, 0.3), 0.55, 0.15)),
        ING ("Swelling", "slow swell in", "", p.mono (40, true).env (0, p.j (0.12, 0.3), 0.5, 1.0, 0.3)),
        ING ("", "stabby retrigger, no glide", "", p.mono (0.0, false).env (0, 0.001, 0.25, 0.7, 0.08)),
    };
    R.spaces = lowSpaces();
    R.allowedMotionFilter = [] (const juce::String& m, const juce::String& f) { return ! ((f == "Open" || f == "Hollow") && m.isNotEmpty()); };
    R.allowed = [] (const juce::String& s, const juce::String& m) { return ! (s == "Sync Reese" && m == "Drifting"); };
    R.characters = { Clean, Clean, Wide, Dirty, Tape, OTT, Motion, Grit };
    R.templates = { "M S", "F S", "M F S", "P S", "Y S", "P M S", "Y F S", "M P S", "F P S" };
    R.base = [] (Patch& p) { p.voices (1).drift (0.15).level (-7.0).env (0, 0.004, 0.5, 0.95, p.j (0.18, 0.3)).noSat(); };
    R.finish = [] (Patch& p) { p.stereo (1.2, true, 200.0).comp (-18, 3, 3); };
    return R;
}

//==============================================================================
// BASS (80): wave basses that aren't reeses: 808s, subs, growls, FM and vocal basses.
//==============================================================================
CategoryRecipe bassRecipe()
{
    CategoryRecipe R;
    R.category = "BASS"; R.count = 80; R.lowEnd = true; R.tags = { "Mono", "Wave" };
    R.sources = {
        ING ("808", "a sine 808: pitch drops from an octave up, long boom", "808",
             p.an (0, "Sine", 0.9).tight (0, 0.25).env (1, 0.0, p.j (0.07, 0.2), 0.0, 0.05).mod ("ENV 2", "Osc A Pitch", 1.0)
              .env (0, 0.001, 1.6, 0.0, 0.6).dist ("Soft Cubic", 0.35, 0.5, 0.5)),
        ING ("Distorted 808", "a sine 808 through hard clipping: the pitch drop and a buzzing tail", "808",
             p.an (0, "Sine", 0.9).tight (0, 0.25).env (1, 0.0, p.j (0.06, 0.2), 0.0, 0.05).mod ("ENV 2", "Osc A Pitch", 1.0)
              .env (0, 0.001, 1.4, 0.0, 0.5).dist ("Hard Clip", p.j (0.6, 0.2), 0.75, 0.55)),
        ING ("Sub", "a pure sine sub, slightly rounded by the ladder", "Sub", p.an (0, "Sine", 0.95).tight (0, 0.25).sub ("Sine -1", 0.2)),
        ING ("Triangle Sub", "a triangle sub: audible on small speakers", "Sub", p.an (0, "Triangle", 0.9).tight (0, 0.25)),
        ING ("Growl Bass", "FM growl (Growl FM table)", "Growl", p.wt (0, "Growl FM", p.j (0.5, 0.4), 0.85).sub ("Sine -1", 0.4)),
        ING ("FM Bass", "an Electric FM bass", "FM", p.wt (0, "Electric FM", p.j (0.45, 0.4), 0.8).sub ("Sine -1", 0.45)),
        ING ("Talk Bass", "a Talk Box table: vocal and nasal", "Talking", p.wt (0, "Talk Box", p.j (0.4, 0.5), 0.85).sub ("Sine -1", 0.45)),
        ING ("Vowel Bass", "an ooh-to-ahh vowel bass", "Talking", p.wt (0, "Ooh To Ahh", p.j (0.4, 0.5), 0.85).sub ("Sine -1", 0.4)),
        ING ("Hollow Bass", "odd harmonics only: hollow, clarinet-like", "Wave Bass", p.wt (0, "Odd Harmonics", p.j (0.5, 0.4), 0.85).sub ("Sine -1", 0.35)),
        ING ("Phase Bass", "phase-distortion resonance, CZ-style", "Wave Bass", p.wt (0, "Reso PD", p.j (0.4, 0.5), 0.8).sub ("Sine -1", 0.4)),
        ING ("Fuzz Bass", "a fuzz-triangle bass", "Grit", p.wt (0, "Fuzz Tri", p.j (0.5, 0.4), 0.85)),
        ING ("Pulse Bass", "analog pulse and square an octave apart", "Wave Bass", p.an (0, "Pulse", 0.8).pw (0, p.j (0.32, 0.3)).an (1, "Square", 0.45).tune (1, -1)),
        ING ("Organ Bass", "a hollow organ bass", "Wave Bass", p.wt (0, "Hollow Organ", p.j (0.4, 0.4), 0.85).sub ("Sine -1", 0.4)),
        ING ("Sync Bass", "a hard-sync bass: sine sub under a synced saw", "Wave Bass",
             p.an (0, "Saw", 0.7).choice (ids::osc (0, "warp"), "Sync").set (ids::osc (0, "warp_amt"), p.j (0.35, 0.3)).choice (ids::osc (0, "warp_src"), "Osc B")
              .set (ids::osc (1, "on"), 0).choice (ids::osc (1, "engine"), "Analog").choice (ids::osc (1, "wave"), "Sine").sub ("Sine -1", 0.5)),
    };
    R.motions = {
        ING ("", "steady: weight first", "", p.set (ids::analog, 0.1)),
        ING ("Breathing", "a slow LFO breathes the tone", "Moving", p.lfo (0, "Sine", p.j (0.18, 0.3), 1.0, 0.4).mod ("LFO 1", cutoffOrPos (p), 0.6)),
        ING ("Wobbling", "a quarter-note wobble", "Wobble", p.lfoSync (0, "Sine", "1/4", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.5)),
        ING ("Growling", "an envelope growls through the tone on each note", "Growl", p.env (1, 0.005, p.j (1.2, 0.2), 0.15, 0.3).mod ("ENV 2", cutoffOrPos (p), 0.85).mod ("ENV 2", "Osc A Fold", 0.45)),
        ING ("Snapping", "a filter envelope snaps open on each note", "Pluck", p.env (2, 0.001, p.j (0.2, 0.3), 0.1, 0.15).mod ("ENV 3", cutoffOrPos (p), 0.55)),
        ING ("Stepping", "a sixteenth-step pattern on the tone", "Rhythmic", p.steps ("9373937393739373", "1", "m", {}, "1/16", 16, 0.0, false).mod ("Step", cutoffOrPos (p), 0.65)),
    };
    R.filters = {
        ING ("Dark", "through a dark low-pass", "", p.filt (0, "Low Pass 24", p.j (320, 0.2), 0.1, 0.2, 0.3)),
        ING ("Warm", "through a driven ladder", "", p.filt (0, "MG Low 24", p.j (1100, 0.3), 0.25, 0.35, 0.3)),
        ING ("Round", "through a gentle 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (1600, 0.3), 0.15, 0.1, 0.3)),
        ING ("Open", "unfiltered", "", p.filt (0, "Off", 12000, 0.0)),
    };
    R.styles = {
        ING ("Gliding", "legato with long portamento", "Glide", p.mono (p.j (140, 0.3), true)),
        ING ("", "tight legato", "", p.mono (p.j (30, 0.3), true)),
        ING ("", "retriggered on every note", "", p.mono (0.0, false)),
    };
    R.spaces = lowSpaces();
    R.characters = { Clean, Clean, Dirty, Tape, OTT, Grit, Wide };
    R.templates = { "M S", "F S", "Y S", "P S", "M F S", "Y F S", "P M S", "F P S" };
    R.allowed = [] (const juce::String& s, const juce::String& m)
    {   // pure subs and 808s don't have harmonics to growl / step / breathe through: they stay steady
        if (s == "Sub" || s == "Triangle Sub" || s.contains ("808")) return m.isEmpty();
        if ((s == "Vowel Bass" || s == "Talk Bass") && m == "Growling") return false;
        return true;
    };
    R.allowedSpace = [] (const juce::String& s, const juce::String& sp) { return ! (s.contains ("808") && sp.isNotEmpty()); };   // 808s decay: no "Cavern" claims
    R.allowedFilter = [] (const juce::String& s, const juce::String& f)
    {   // nothing to darken on a sine: subs and 808s only get the gentle ladder / round low-pass
        if (s == "Sub" || s == "Triangle Sub" || s.contains ("808")) return f == "Warm" || f == "Round";
        return true;
    };
    R.base = [] (Patch& p) { p.voices (1).drift (0.1).level (-7.0).env (0, 0.002, 0.6, 0.9, p.j (0.2, 0.3)).noSat(); };
    R.finish = [] (Patch& p) { p.stereo (1.1, true, 160.0).comp (-18, 3, 3); };
    return R;
}

//==============================================================================
// Synth-family sources (lead / synth / pad / sequence) share some honest building blocks.
//==============================================================================
CategoryRecipe leadRecipe()
{
    CategoryRecipe R;
    R.category = "LEAD"; R.superWet = true; R.count = 90; R.tags = { "Wave", "Wet" };
    R.sources = {
        ING ("Reese Lead", "two detuned saw pairs: a reese, an octave up as a lead", "Reese",
             p.an (0, "Saw", 0.7).uni (0, 3, p.j (0.18, 0.3), 0.6).an (1, "Saw", 0.5).tune (1, 0, 0, 12).uni (1, 2, 0.15, 0.6)),
        ING ("Supersaw Lead", "a seven-voice supersaw", "Supersaw", p.an (0, "Saw", 0.75).uni (0, 7, p.j (0.3, 0.2), 0.9)),
        ING ("Sync Lead", "a sweeping hard-sync table", "Sync", p.wt (0, "Laser Sync", p.j (0.35, 0.4), 0.8).uni (0, 2, 0.08, 0.5)),
        ING ("Glass Lead", "the Glass FM table, bell-clear", "Glass", p.wt (0, "Glass FM", p.j (0.4, 0.4), 0.8).uni (0, 2, 0.06, 0.6)),
        ING ("Bell Lead", "ring-modulated bells", "Bell", p.wt (0, "Ring Bells", p.j (0.4, 0.4), 0.75)),
        ING ("Choir Lead", "a choir-formant table", "Vocal", p.wt (0, "Choir Formant", p.j (0.4, 0.4), 0.8).uni (0, 3, 0.1, 0.7)),
        ING ("Flute Lead", "a breathy flute table with air", "Flute", p.wt (0, "Breathy Flute", p.j (0.4, 0.4), 0.85).noise ("Pink", 0.04)),
        ING ("Square Lead", "a hollow square, slightly detuned", "Square", p.an (0, "Square", 0.75).uni (0, 2, 0.08, 0.5)),
        ING ("Chip Lead", "8-bit arcade steps", "8-Bit", p.wt (0, "Arcade Steps", p.j (0.5, 0.4), 0.75)),
        ING ("PWM Lead", "a moving pulse-width table", "PWM", p.wt (0, "PWM Sweep", p.j (0.4, 0.4), 0.8).uni (0, 2, 0.08, 0.5)),
        ING ("Brass Lead", "a brass-swell table", "Brass", p.wt (0, "Brass Swell", p.j (0.5, 0.3), 0.8).uni (0, 2, 0.08, 0.5)),
        ING ("FM Lead", "an Electric FM lead", "FM", p.wt (0, "Electric FM", p.j (0.5, 0.3), 0.8)),
        ING ("Mirror Lead", "a saw mirrored into itself (mirror warp)", "Warp",
             p.an (0, "Saw", 0.8).uni (0, 2, 0.1, 0.5).choice (ids::osc (0, "warp"), "Mirror").set (ids::osc (0, "warp_amt"), p.j (0.45, 0.3))),
        ING ("Organ Lead", "a gospel organ table", "Organ", p.wt (0, "Organ Gospel", p.j (0.4, 0.4), 0.8)),
        ING ("Saw Lead", "a single bright saw with a sub octave", "Saw", p.an (0, "Saw", 0.85).an (1, "Saw", 0.35).tune (1, -1, 0, 5)),
    };
    R.motions = {
        ING ("", "steady, with a touch of delayed vibrato on the mod wheel", "", p.lfo (1, "Sine", 5.0, 1.0, 0.2).mod ("LFO 2", "Osc A Pitch", 0.004)),
        ING ("Breathing", "a slow filter breath", "Moving", p.lfo (0, "Sine", p.j (0.18, 0.3), 1.0, 0.4).mod ("LFO 1", cutoffOrPos (p), 0.35)),
        ING ("Pulsing", "an eighth-note tremolo pulse", "Pulsing", p.trem ("Sine", "1/8", 0.9)),
        ING ("Sweeping", "a two-bar sweep through the tone", "Moving", p.lfoSync (0, "Triangle", "2 bars", 1.0, 0.3).mod ("LFO 1", posOrCutoff (p), 0.5)),
        ING ("Swirling", "a slow phaser and flanger swirl", "Moving", p.phaser (0.15, 0.8, 900, 0.5, 0.5).flanger (0.08, 0.5, 0.35, 0.25)),
        ING ("Stepping", "a sixteenth-step pattern on the tone", "Rhythmic", p.steps ("9575937595739373", "1", "m", {}, "1/16", 16, 0.0, false).mod ("Step", cutoffOrPos (p), 0.45)),
    };
    R.filters = {
        ING ("Dark", "through a dark low-pass", "", p.filt (0, "Low Pass 24", p.j (1500, 0.3), 0.15, 0.1, 0.5)),
        ING ("Warm", "through a soft ladder", "", p.filt (0, "MG Low 24", p.j (3000, 0.3), 0.2, 0.2, 0.5)),
        ING ("Open", "unfiltered and bright", "", p.filt (0, "Off", 15000, 0.0)),
        ING ("Hollow", "through a notch: hollow middle", "", p.filt (0, "Notch", p.j (1200, 0.3), 0.3, 0.0, 0.5)),
    };
    R.styles = {
        ING ("Gliding", "mono legato with glide", "Glide", p.mono (p.j (90, 0.3), true)),
        ING ("", "poly, soft attack", "", p.voices (8).env (0, 0.015, 0.4, 0.85, 0.5)),
        ING ("", "mono, punchy", "", p.mono (15, true).env (0, 0.003, 0.3, 0.8, 0.35)),
    };
    R.spaces = wetSpaces();
    R.characters = { Clean, Clean, Wide, Tape, Dream, OTT, Shimmer };
    R.templates = { "P S", "M S", "F S", "P M S", "Y S", "M F S", "P F S", "Y P S" };
    R.base = [] (Patch& p) { p.level (-9.0).drift (0.2).env (0, 0.01, 0.4, 0.85, 0.45); };
    R.finish = [] (Patch& p) { p.stereo (1.35, true, 120.0); };
    return R;
}

CategoryRecipe synthRecipe()
{
    CategoryRecipe R;
    R.category = "SYNTH"; R.superWet = true; R.count = 90; R.tags = { "Wave", "Wet", "Poly" };
    R.sources = {
        ING ("Reese Stab", "a stab of detuned saw pairs (a reese, played as chords)", "Reese",
             p.an (0, "Saw", 0.7).uni (0, 2, 0.16, 0.7).an (1, "Saw", 0.6).uni (1, 2, 0.2, 0.7).tune (1, 0, 0, 10)),
        ING ("Keys", "an electric-piano tine", "Keys", p.wt (0, "E-Piano Tine", p.j (0.4, 0.4), 0.85)),
        ING ("Organ", "a jazz organ table", "Organ", p.wt (0, "Organ Jazz", p.j (0.4, 0.4), 0.8)),
        ING ("Strings", "a string-ensemble table, wide", "Strings", p.wt (0, "String Ensemble", p.j (0.5, 0.3), 0.8).uni (0, 4, 0.15, 0.9)),
        ING ("Brass", "a brass-swell table", "Brass", p.wt (0, "Brass Swell", p.j (0.5, 0.3), 0.8).uni (0, 3, 0.1, 0.7)),
        ING ("Mallet", "a mallet table", "Mallet", p.wt (0, "Mallet", p.j (0.4, 0.4), 0.85)),
        ING ("Kalimba", "a kalimba table", "Kalimba", p.wt (0, "Kalimba", p.j (0.4, 0.4), 0.85)),
        ING ("Music Box", "a music-box table", "Music Box", p.wt (0, "Music Box", p.j (0.4, 0.4), 0.8)),
        ING ("Bell", "Bell FM", "Bell", p.wt (0, "Bell FM", p.j (0.4, 0.4), 0.8)),
        ING ("Clav", "a clavinet table", "Clav", p.wt (0, "Clavi", p.j (0.4, 0.4), 0.8)),
        ING ("Glass Keys", "the Glass Halo table", "Glass", p.wt (0, "Glass Halo", p.j (0.4, 0.4), 0.8).uni (0, 2, 0.07, 0.6)),
        ING ("Saw Stab", "a detuned supersaw stab", "Supersaw", p.an (0, "Saw", 0.75).uni (0, 5, 0.25, 0.9)),
        ING ("PWM Keys", "a wide PWM table", "PWM", p.wt (0, "PWM Wide", p.j (0.4, 0.4), 0.8).uni (0, 2, 0.1, 0.7)),
        ING ("Choir Keys", "a choir sample under a formant table", "Vocal", p.wt (0, "Choir Formant", 0.4, 0.6).smp ("Choir Ahh", 0.4, true)),
        ING ("Pluck", "a plucked-body sample over a soft saw", "Pluck", p.smp ("Pluck Body", 0.6, true).an (0, "Saw", 0.35).uni (0, 2, 0.1, 0.6)),
    };
    R.motions = {
        ING ("", "steady", "", p.set (ids::analog, 0.2)),
        ING ("Breathing", "a slow filter breath", "Moving", p.lfo (0, "Sine", p.j (0.16, 0.3), 1.0, 0.4).mod ("LFO 1", cutoffOrPos (p), 0.35)),
        ING ("Pulsing", "a sixteenth-note gate", "Pulsing", p.trem ("Square", "1/16", 0.75)),
        ING ("Evolving", "a slow drift through the table", "Moving", p.lfo (0, "Triangle", p.j (0.08, 0.3), 1.0, 0.5).mod ("LFO 1", posOrCutoff (p), 0.6)),
        ING ("Swirling", "a slow phaser swirl", "Moving", p.phaser (0.12, 0.8, 900, 0.5, 0.5)),
        ING ("Wobbling", "a quarter-note wobble", "Wobble", p.lfoSync (0, "Sine", "1/4", 1.0, 0.1).mod ("LFO 1", cutoffOrPos (p), 0.45)),
    };
    R.filters = {
        ING ("Dark", "through a dark low-pass", "", p.filt (0, "Low Pass 24", p.j (1300, 0.3), 0.15, 0.1, 0.5)),
        ING ("Soft", "through a soft ladder", "", p.filt (0, "MG Low 24", p.j (2600, 0.3), 0.2, 0.1, 0.5)),
        ING ("Open", "unfiltered", "", p.filt (0, "Off", 15000, 0.0)),
        ING ("Vocal", "through a formant filter", "", p.filt (0, "Formant", p.j (900, 0.3), 0.45, 0.1)),
    };
    R.styles = {
        ING ("Plucked", "plucked: fast decay to silence", "Pluck", p.env (0, 0.001, p.j (0.5, 0.3), 0.0, 0.5)),
        ING ("", "keys: decays to a soft sustain", "", p.env (0, 0.002, 0.8, 0.45, 0.6)),
        ING ("", "held chords", "", p.env (0, 0.01, 0.5, 0.85, 0.7)),
        ING ("Swelling", "swells in slowly", "", p.env (0, p.j (0.45, 0.3), 0.5, 1.0, 0.9)),
    };
    R.spaces = wetSpaces();
    R.characters = { Clean, Clean, Wide, Tape, Dream, LoFi, Motion };
    R.templates = { "P S", "M S", "F S", "Y S", "P M S", "Y P S", "M F S", "P F S" };
    // the choir sample is louder than its formant table, so a table drift barely shows: no "Evolving" choir keys
    R.allowed = [] (const juce::String& s, const juce::String& m) { return ! (s == "Choir Keys" && m == "Evolving"); };
    R.base = [] (Patch& p) { p.voices (10).level (-10.0).drift (0.25); };
    R.finish = [] (Patch& p) { p.stereo (1.4, true, 120.0); };
    return R;
}

CategoryRecipe padRecipe()
{
    CategoryRecipe R;
    R.category = "PAD"; R.superWet = true; R.count = 95; R.tags = { "Wave", "Wet", "Pad" };
    R.sources = {
        ING ("Reese Pad", "two wide detuned saw pairs held as a pad: a slow reese", "Reese",
             p.an (0, "Saw", 0.6).uni (0, 4, p.j (0.22, 0.3), 0.9).an (1, "Saw", 0.5).uni (1, 3, 0.2, 0.9).tune (1, 0, 0, 9)),
        ING ("Choir", "a choir sample over a formant table", "Vocal", p.smp ("Choir Ahh", 0.55, true).wt (0, "Choir Formant", p.j (0.4, 0.4), 0.5).uni (0, 3, 0.1, 0.9)),
        ING ("Strings", "a wide string ensemble", "Strings", p.wt (0, "String Ensemble", p.j (0.5, 0.3), 0.75).uni (0, 5, 0.16, 1.0)),
        ING ("Glass Pad", "the Glass Halo table", "Glass", p.wt (0, "Glass Halo", p.j (0.4, 0.4), 0.75).uni (0, 4, 0.1, 1.0)),
        ING ("Crystal Pad", "the Crystal table", "Crystal", p.wt (0, "Crystal", p.j (0.4, 0.4), 0.75).uni (0, 3, 0.1, 1.0)),
        ING ("Heaven Pad", "the Seventh Heaven chord table", "Heaven", p.wt (0, "Seventh Heaven", p.j (0.4, 0.4), 0.75).uni (0, 3, 0.1, 1.0)),
        ING ("Air Pad", "filtered breath and a soft sine bed", "Air", p.smp ("Air Breath", 0.5, false).smpLoop (0.1, 0.9).an (0, "Sine", 0.4).uni (0, 3, 0.1, 1.0)),
        ING ("Organ Pad", "a pipe organ table", "Organ", p.wt (0, "Pipe Organ", p.j (0.4, 0.4), 0.7).uni (0, 3, 0.1, 1.0)),
        ING ("PWM Pad", "a stacked PWM table", "PWM", p.wt (0, "PWM Stack", p.j (0.5, 0.3), 0.7).uni (0, 4, 0.14, 1.0)),
        ING ("Vowel Pad", "a vowel-sweep table", "Vocal", p.wt (0, "Vowel Sweep", p.j (0.4, 0.4), 0.75).uni (0, 3, 0.1, 1.0)),
        ING ("Supersaw Pad", "a seven-voice supersaw held soft", "Supersaw", p.an (0, "Saw", 0.6).uni (0, 7, 0.28, 1.0)),
        ING ("Bell Pad", "bells over a held sine bed", "Bell", p.wt (0, "Bell FM", p.j (0.4, 0.4), 0.55).an (1, "Sine", 0.4).uni (1, 3, 0.1, 1.0)),
        ING ("Formant Pad", "a talking formant table", "Vocal", p.wt (0, "Formant Vox", p.j (0.5, 0.3), 0.7).uni (0, 3, 0.1, 1.0)),
        ING ("Flute Pad", "a breathy flute ensemble", "Flute", p.wt (0, "Breathy Flute", p.j (0.4, 0.4), 0.75).uni (0, 3, 0.1, 1.0)),
    };
    R.motions = {
        ING ("", "steady, with a slow analog drift", "", p.set (ids::analog, 0.35)),
        ING ("Breathing", "a slow filter breath", "Moving", p.lfo (0, "Sine", p.j (0.12, 0.3), 1.0, 0.4).mod ("LFO 1", cutoffOrPos (p), 0.4)),
        ING ("Evolving", "a slow drift through the table", "Moving", p.lfo (0, "Triangle", p.j (0.06, 0.3), 1.0, 0.5).mod ("LFO 1", posOrCutoff (p), 0.5)),
        ING ("Pulsing", "a quarter-note pulse, like sidechain", "Pulsing", p.trem ("Sine", "1/4", 0.9)),
        ING ("Gated", "a sixteenth-note trance gate", "Gated", p.trem ("Square", "1/16", 0.85)),
        ING ("Drifting", "slow random drift of pitch and tone", "Moving", p.lfo (1, "Random", 0.3, 1.0, 1.0).mod ("LFO 2", posOrCutoff (p), 0.35).mod ("LFO 2", "Osc A Pitch", 0.004)),
        ING ("Swirling", "a slow phaser and flanger swirl", "Moving", p.phaser (0.1, 0.8, 800, 0.5, 0.5).flanger (0.06, 0.5, 0.3, 0.25)),
    };
    R.filters = {
        ING ("Dark", "through a dark low-pass", "", p.filt (0, "Low Pass 24", p.j (1100, 0.3), 0.1, 0.0, 0.5)),
        ING ("Soft", "through a soft ladder", "", p.filt (0, "MG Low 24", p.j (2400, 0.3), 0.15, 0.0, 0.5)),
        ING ("Open", "unfiltered", "", p.filt (0, "Off", 15000, 0.0)),
        ING ("Hollow", "through a comb: hollow and phasey", "", p.filt (0, "Comb -", p.j (400, 0.3), 0.45, 0.0, 1.0, 0.45).filt (1, "Low Pass 12", 5000, 0.1)),
    };
    R.styles = {
        ING ("", "slow attack, long release", "", p.env (0, p.j (0.7, 0.3), 1.0, 0.9, 2.0)),
        ING ("Swelling", "a very slow swell", "", p.env (0, p.j (1.8, 0.3), 1.0, 1.0, 2.5)),
        ING ("", "soft attack, held", "", p.env (0, 0.15, 1.0, 0.9, 1.6)),
    };
    R.spaces = wetSpaces();
    R.characters = { Clean, Clean, Wide, Tape, Dream, Shimmer, LoFi };
    R.templates = { "P S", "M S", "F S", "P M S", "Y S", "M F S", "P F S", "Y P S" };
    R.base = [] (Patch& p) { p.voices (10).level (-11.0); };
    R.finish = [] (Patch& p) { p.stereo (1.5, true, 120.0); };
    return R;
}

CategoryRecipe textureRecipe()
{
    CategoryRecipe R;
    R.category = "TEXTURE"; R.superWet = true; R.count = 70; R.tags = { "Wave", "Atmosphere", "Wet" };
    R.sources = {
        ING ("Rain", "a rain loop", "Rain", p.smp ("Rain", 0.7, false).smpLoop (0.05, 0.95)),
        ING ("Vinyl", "vinyl crackle", "Vinyl", p.smp ("Vinyl Crackle", 0.6, false).smpLoop (0.05, 0.95)),
        ING ("Tape Hiss", "tape hiss", "Tape", p.smp ("Tape Hiss", 0.6, false).smpLoop (0.05, 0.95)),
        ING ("Wind", "a wind loop", "Wind", p.smp ("Wind", 0.7, false).smpLoop (0.05, 0.95)),
        ING ("Breath", "a breath loop", "Air", p.smp ("Air Breath", 0.7, false).smpLoop (0.1, 0.9)),
        ING ("Reese Drone", "a low held reese drone: detuned saws, beating slowly", "Reese",
             p.an (0, "Saw", 0.6).uni (0, 3, 0.14, 0.9).an (1, "Saw", 0.5).tune (1, -1, 0, 7).uni (1, 2, 0.16, 0.9)),
        ING ("Noise Wash", "filtered pink noise washing in", "Noise", p.noise ("Pink", 0.6)),
        ING ("Choir Haze", "a choir sample smeared out", "Vocal", p.smp ("Choir Ahh", 0.6, true).wt (0, "Choir Formant", 0.5, 0.3)),
        ING ("Static", "bright static", "Noise", p.noise ("Bright", 0.45).crush (8, 4, 0.4)),
        ING ("Drone", "a held drone of a fifth on hollow organ tables", "Drone", p.wt (0, "Hollow Organ", 0.4, 0.6).wt (1, "Pipe Organ", 0.3, 0.45).tune (1, 0, 7)),
        ING ("Chimes", "a glass-hit loop of chimes", "Glass", p.smp ("Glass Hit", 0.6, true).steps ("9050703060509070", "1", "s", { 0, 7, 12, 5, 0, 10, 7, 3 }, "1/8", 16, 0.0, true)),
        ING ("Swell", "a reversed swell", "Swell", p.smp ("Reverse Swell", 0.7, true)),
    };
    R.motions = {
        ING ("", "steady", "", p.set (ids::analog, 0.3)),
        ING ("Breathing", "a slow filter breath", "Moving", p.lfo (0, "Sine", p.j (0.1, 0.3), 1.0, 0.4).mod ("LFO 1", "Filter 1 Cutoff", 0.45)),
        ING ("Drifting", "slow random drift", "Moving", p.lfo (1, "Random", 0.25, 1.0, 1.0).mod ("LFO 2", "Filter 1 Cutoff", 0.4)),
        ING ("Swirling", "a slow phaser swirl", "Moving", p.phaser (0.08, 0.9, 700, 0.6, 0.6)),
        ING ("Tidal", "a slow tide: the filter rolls in and out every couple of seconds", "Moving",
             p.lfo (0, "Sine", p.j (0.45, 0.2), 1.0, 0.3).mod ("LFO 1", "Filter 1 Cutoff", 0.7)),
    };
    R.filters = {
        ING ("Dark", "through a dark low-pass", "", p.filt (0, "Low Pass 24", p.j (550, 0.2), 0.1, 0.0, 0.0)),
        ING ("Distant", "through a band-pass, like it's next door", "", p.filt (0, "Band Pass", p.j (900, 0.3), 0.3, 0.0, 0.0)),
        ING ("Soft", "through a soft 12 dB low-pass", "", p.filt (0, "Low Pass 12", p.j (3500, 0.3), 0.1, 0.0, 0.0)),
        ING ("Open", "unfiltered", "", p.filt (0, "Off", 15000, 0.0)),
    };
    R.styles = {
        ING ("", "fades in and lingers", "", p.env (0, p.j (0.8, 0.3), 1.0, 1.0, 2.5)),
        ING ("", "starts at once, long release", "", p.env (0, 0.05, 1.0, 1.0, 2.0)),
    };
    R.spaces = wetSpaces();
    R.characters = { Clean, Clean, LoFi, Tape, Shimmer, Wide };
    R.templates = { "M S", "F S", "P S", "M F S", "P M S", "F P S" };
    R.allowedMotionFilter = [] (const juce::String& m, const juce::String& f) { return f != "Open" || m.isEmpty() || m == "Swirling"; };
    R.allowed = [] (const juce::String& s, const juce::String& m)
    {   // noise beds don't swirl audibly through a phaser
        if (m == "Swirling") return s == "Reese Drone" || s == "Choir Haze" || s == "Drone" || s == "Chimes" || s == "Swell";
        return true;
    };
    R.base = [] (Patch& p) { p.voices (6).level (-12.0); };
    R.finish = [] (Patch& p) { p.stereo (1.5, true, 120.0); };
    return R;
}

//==============================================================================
// SEQUENCE (90): arps and step sequences. Hold a chord / one key.
//==============================================================================
CategoryRecipe sequenceRecipe()
{
    CategoryRecipe R;
    R.category = "SEQUENCE"; R.superWet = true; R.count = 90; R.tags = { "Wave", "Wet", "Sequence" };
    R.sources = {
        ING ("Reese Arp", "detuned saw pairs (a reese) arpeggiated", "Reese",
             p.an (0, "Saw", 0.7).uni (0, 2, 0.16, 0.6).an (1, "Saw", 0.55).uni (1, 2, 0.2, 0.6).tune (1, 0, 0, 10)),
        ING ("Pluck Arp", "a plucked body over a soft saw", "Pluck", p.smp ("Pluck Body", 0.6, true).an (0, "Saw", 0.3).uni (0, 2, 0.1, 0.6)),
        ING ("Bell Arp", "Bell FM", "Bell", p.wt (0, "Bell FM", p.j (0.4, 0.4), 0.8)),
        ING ("Glass Arp", "the Glass FM table", "Glass", p.wt (0, "Glass FM", p.j (0.4, 0.4), 0.8)),
        ING ("Kalimba Arp", "a kalimba table", "Kalimba", p.wt (0, "Kalimba", p.j (0.4, 0.4), 0.85)),
        ING ("Chip Arp", "8-bit arcade steps", "8-Bit", p.wt (0, "Arcade Steps", p.j (0.5, 0.4), 0.75)),
        ING ("Keys Arp", "an electric-piano tine", "Keys", p.wt (0, "E-Piano Tine", p.j (0.4, 0.4), 0.85)),
        ING ("PWM Arp", "a moving pulse width", "PWM", p.wt (0, "PWM Sweep", p.j (0.4, 0.4), 0.8)),
        ING ("Supersaw Arp", "a five-voice supersaw", "Supersaw", p.an (0, "Saw", 0.7).uni (0, 5, 0.25, 0.9)),
        ING ("Music Box Arp", "a music-box table", "Music Box", p.wt (0, "Music Box", p.j (0.4, 0.4), 0.8)),
        ING ("Mallet Arp", "a mallet table", "Mallet", p.wt (0, "Mallet", p.j (0.4, 0.4), 0.85)),
        ING ("Vocal Arp", "a choir-formant table", "Vocal", p.wt (0, "Choir Formant", p.j (0.4, 0.4), 0.8)),
    };
    // the movement here is the pattern itself
    R.motions = {
        ING ("Rising", "arpeggiated upwards in sixteenths over two octaves", "Arp", p.arp ("Up", "1/16", 2, 0.5)),
        ING ("Falling", "arpeggiated downwards in sixteenths", "Arp", p.arp ("Down", "1/16", 2, 0.5)),
        ING ("Bouncing", "up and down in eighths, three octaves", "Arp", p.arp ("Up/Down", "1/8", 3, 0.55)),
        ING ("Scattered", "random order, sixteenths", "Arp", p.arp ("Random", "1/16", 2, 0.4)),
        ING ("Triplet", "triplet-eighth arpeggio", "Arp", p.arp ("Up", "1/8T", 2, 0.5)),
        ING ("Dotted", "dotted-eighth arpeggio", "Arp", p.arp ("Up/Down", "1/8D", 2, 0.6)),
        ING ("Swung", "swung sixteenths", "Arp", p.arp ("Up", "1/16", 1, 0.45, 0.35)),
        ING ("Stepping", "a sixteenth step sequence with its own melody (hold one key)", "Sequence",
             p.steps ("9575957595759575", "1", "m", { 0, 12, 7, 0, 3, 12, 7, 10, 0, 12, 7, 0, 5, 12, 3, 7 }, "1/16", 16, 0.0, true)),
        ING ("Pulsing", "a pulsing eighth step sequence (hold one key)", "Sequence",
             p.steps ("9070907090709070", "1", "m", { 0, 0, 7, 0, 12, 0, 10, 0 }, "1/8", 16, 0.0, true)),
    };
    R.filters = {
        ING ("Dark", "through a dark low-pass", "", p.filt (0, "Low Pass 24", p.j (1400, 0.3), 0.15, 0.1, 0.5)),
        ING ("Open", "unfiltered", "", p.filt (0, "Off", 15000, 0.0)),
        ING ("Snapping", "with a filter envelope snapping each note", "",
             p.filt (0, "MG Low 24", p.j (900, 0.3), 0.3, 0.2, 0.5).env (1, 0.001, 0.18, 0.0, 0.15).mod ("ENV 2", "Filter 1 Cutoff", 0.55)),
    };
    R.styles = {
        ING ("", "short notes", "", p.env (0, 0.001, 0.25, 0.2, 0.25)),
        ING ("", "longer notes that ring into each other", "", p.env (0, 0.002, 0.5, 0.5, 0.6)),
    };
    R.spaces = wetSpaces();
    R.characters = { Clean, Clean, Wide, Tape, Dream, LoFi };
    R.templates = { "M S", "P S", "P M S", "F S", "M F S", "P F S", "F M S" };
    R.base = [] (Patch& p) { p.voices (8).level (-10.0).drift (0.2); };
    R.finish = [] (Patch& p) { p.stereo (1.4, true, 120.0); };
    return R;
}

//==============================================================================
// PERCUSSION (62): loops on the step sequencer (hold one key) + one-shots.
//==============================================================================
CategoryRecipe percRecipe()
{
    CategoryRecipe R;
    R.category = "PERCUSSION"; R.count = 62; R.lowEnd = true; R.tags = { "Drums", "Wave" };
    R.sources = {
        ING ("Hats", "closed hats", "Hats", p.smp ("Hat Closed", 0.8, false)),
        ING ("Metal Hats", "metallic hats", "Hats", p.smp ("Hat Metal", 0.75, false)),
        ING ("Shaker", "a shaker", "Shaker", p.smp ("Shaker", 0.8, false)),
        ING ("Rims", "rimshots", "Rims", p.smp ("Rim", 0.8, false)),
        ING ("Claps", "claps", "Claps", p.smp ("Clap", 0.8, false)),
        ING ("Snaps", "finger snaps", "Snaps", p.smp ("Snap", 0.8, false)),
        ING ("Ticks", "perc ticks", "Ticks", p.smp ("Perc Tick", 0.8, false)),
        ING ("Metal Hits", "metal hits", "Metal", p.smp ("Metal Hit", 0.7, false)),
        ING ("Open Hats", "open hats", "Hats", p.smp ("Hat Open", 0.7, false)),
    };
    R.motions = {
        ING ("Trap", "trap sixteenths with rolls", "Loop", p.steps ("9575957595759575", "1111211111113141", "s", {}, "1/16", 16, 0.0, true)),
        ING ("Triplet", "triplet rolls", "Loop", p.steps ("957957957957", "111121111131", "s", {}, "1/16 T", 12, 0.0, true)),
        ING ("Half-Time", "a half-time bounce", "Loop", p.steps ("9006900690069060", "1111111111111131", "s", {}, "1/16", 16, 0.18, true)),
        ING ("Drill", "a drill skitter", "Loop", p.steps ("9070607090706070", "1112111211131114", "s", {}, "1/16", 16, 0.0, true)),
        ING ("Swung", "swung sixteenths", "Loop", p.steps ("9686968696869686", "1", "s", {}, "1/16", 16, 0.35, true)),
        ING ("Offbeat", "offbeats only", "Loop", p.steps ("0090009000900090", "1", "s", {}, "1/16", 16, 0.0, true)),
        ING ("Stutter", "stuttering gaps", "Loop", p.steps ("9900990099009990", "1111111111111112", "s", {}, "1/16", 16, 0.0, true)),
        ING ("Shuffled", "a shuffle with pitched accents", "Loop", p.steps ("9506950695069506", "1", "s", { 0, 0, 5, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 7, 0 }, "1/16", 16, 0.25, true)),
    };
    R.filters = {
        ING ("", "full range", "", p.filt (0, "Off", 15000, 0.0)),
        ING ("Dusty", "rolled off and bit-crushed", "", p.filt (0, "Low Pass 12", 6000, 0.1, 0.0, 0.0).crush (10, 3, 0.35)),
        ING ("Crisp", "high-passed and bright", "", p.filt (0, "High Pass 12", 600, 0.1, 0.0, 0.0)),
    };
    R.styles = {
        ING ("", "tight", "", p.trans (0.35, -0.2)),
        ING ("", "natural decay", "", p.set (ids::analog, 0.0)),
    };
    R.spaces = drumSpaces();
    R.characters = { Clean, Clean, Grit, LoFi, Tape };
    R.templates = { "M S", "F M S", "P M S", "M F S", "F S", "P S" };
    R.base = [] (Patch& p) { p.voices (8).level (-8.0).env (0, 0.0, 0.5, 1.0, 0.3).filt (0, "Off", 15000, 0.0); };
    R.finish = [] (Patch& p) { p.stereo (1.2, true, 150.0); };
    return R;
}

//==============================================================================
// FX (80): risers, downlifters, impacts, sweeps, transitions, atmospheres.
//==============================================================================
CategoryRecipe fxRecipe()
{
    CategoryRecipe R;
    R.category = "FX"; R.count = 80; R.tags = { "FX", "Wave", "Transition" };
    R.sources = {
        ING ("Riser", "white noise rising through a band-pass over three seconds", "Riser",
             p.noise ("White", 0.7).filt (0, "Band Pass", 300, 0.4, 0.0, 0.0).env (1, 3.0, 0.1, 1.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.85)),
        ING ("Reese Riser", "a detuned saw reese pitching up an octave as it rises", "Riser",
             p.an (0, "Saw", 0.6).uni (0, 3, 0.2, 0.8).env (1, 3.0, 0.1, 1.0, 0.5).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Filter 1 Cutoff", 0.5)
              .filt (0, "Low Pass 24", 500, 0.2, 0.0, 0.0)),
        ING ("Noise Riser", "the noise-riser sample", "Riser", p.smp ("Noise Riser", 0.8, false)),
        ING ("Downlifter", "noise falling through a closing filter", "Downlifter",
             p.noise ("White", 0.7).filt (0, "Low Pass 24", 150, 0.3, 0.0, 0.0).env (1, 0.0, 2.8, 0.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 1.0)),
        ING ("Reese Downlifter", "a detuned saw reese dropping two octaves", "Downlifter",
             p.an (0, "Saw", 0.6).uni (0, 3, 0.2, 0.8).env (1, 0.0, 2.5, 0.0, 0.5).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Filter 1 Cutoff", 0.7)
              .tune (0, 1).filt (0, "Low Pass 24", 400, 0.2, 0.0, 0.0)),
        ING ("Sub Drop", "the sub-drop sample", "Drop", p.smp ("Sub Drop", 0.85, false)),
        ING ("Impact", "the impact-boom sample", "Impact", p.smp ("Impact Boom", 0.85, false)),
        ING ("Reverse Swell", "a reversed swell into the downbeat", "Swell", p.smp ("Reverse Swell", 0.8, false)),
        ING ("Sweep", "pink noise swept by a slow LFO", "Sweep", p.noise ("Pink", 0.6).filt (0, "Band Pass", 1200, 0.5, 0.0, 0.0).lfo (0, "Triangle", 0.25, 1.0, 0.3).mod ("LFO 1", "Filter 1 Cutoff", 0.7)),
        ING ("Siren", "a pitch-swinging siren", "Siren", p.an (0, "Square", 0.5).uni (0, 2, 0.1, 0.6).lfo (0, "Triangle", 0.5, 1.0, 0.0).mod ("LFO 1", "Osc A Pitch", 0.4)),
        ING ("Glitch", "sample-and-hold pitch and filter chaos", "Glitch", p.wt (0, "Digital Grit", 0.5, 0.7).lfoSync (1, "Random", "1/16", 1.0, 0.0)
              .mod ("LFO 2", "Osc A Pitch", 0.5).mod ("LFO 2", "Filter 1 Cutoff", 0.4).crush (7, 6, 0.5)),
        ING ("Whoosh", "a noise whoosh rising and falling", "Whoosh", p.noise ("Pink", 0.7).filt (0, "Band Pass", 400, 0.4, 0.0, 0.0)
              .env (1, 1.2, 1.5, 0.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.75)),
        ING ("Rain Atmosphere", "a rain atmosphere with a slow choir behind", "Atmosphere", p.smp ("Rain", 0.6, false).smpLoop (0.05, 0.95).noise ("Dark", 0.2)),
        ING ("Wind Atmosphere", "a wind atmosphere", "Atmosphere", p.smp ("Wind", 0.7, false).smpLoop (0.05, 0.95)),
        ING ("Short Riser", "a quick noise riser: a high-pass sweeping up over about a second", "Riser",
             p.noise ("White", 0.7).filt (0, "High Pass 12", 200, 0.3, 0.0, 0.0).env (1, 1.2, 0.1, 1.0, 0.3).mod ("ENV 2", "Filter 1 Cutoff", 0.8)),
        ING ("Pitch Riser", "a supersaw climbing two octaves", "Riser",
             p.an (0, "Saw", 0.55).uni (0, 5, 0.25, 0.9).env (1, 3.0, 0.1, 1.0, 0.5).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Osc A Pitch", 1.0)),
        ING ("Choir Riser", "a choir that rises in pitch and opens up", "Riser",
             p.smp ("Choir Ahh", 0.7, true).filt (0, "Low Pass 24", 400, 0.2, 0.0, 0.0).env (1, 3.0, 0.1, 1.0, 0.5)
              .mod ("ENV 2", "Sampler Pitch", 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.8)),
        ING ("Wind Riser", "wind gusting up through an opening filter", "Riser",
             p.smp ("Wind", 0.8, false).smpLoop (0.05, 0.95).filt (0, "Band Pass", 250, 0.4, 0.0, 0.0).env (1, 3.0, 0.1, 1.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.85)),
        ING ("Tape Stop", "a held saw chord slowing to a halt like a tape stop", "Drop",
             p.an (0, "Saw", 0.6).uni (0, 3, 0.15, 0.7).env (1, 0.9, 0.1, 1.0, 0.3).mod ("ENV 2", "Osc A Pitch", -1.0).mod ("ENV 2", "Osc A Pitch", -1.0).filt (0, "Low Pass 24", 3000, 0.1, 0.0, 0.0)),
        ING ("Laser Zap", "a square zap diving three octaves in a blink", "Zap",
             p.an (0, "Square", 0.6).env (1, 0.0, 0.18, 0.0, 0.1).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Osc A Pitch", 1.0).mod ("ENV 2", "Osc A Pitch", 1.0).env (0, 0.001, 0.4, 0.0, 0.2)),
        ING ("Noise Burst", "a short burst of white noise", "Burst", p.noise ("White", 0.8).env (0, 0.001, 0.35, 0.0, 0.3).filt (0, "Low Pass 12", 9000, 0.1, 0.0, 0.0)),
        ING ("Noise Pattern", "white noise chopped into a sixteenth pattern (hold one key)", "Pattern",
             p.noise ("White", 0.7).steps ("9070907790709999", "1111111211111234", "s", {}, "1/16", 16, 0.0, true).env (0, 0.001, 0.12, 0.0, 0.08)),
        ING ("Up Sweep", "noise swept upward through a resonant band-pass", "Sweep",
             p.noise ("Pink", 0.7).filt (0, "Band Pass", 200, 0.6, 0.0, 0.0).env (1, 2.8, 0.1, 1.0, 0.4).mod ("ENV 2", "Filter 1 Cutoff", 0.9)),
        ING ("Down Sweep", "noise swept downward through a resonant band-pass", "Sweep",
             p.noise ("Pink", 0.7).filt (0, "Band Pass", 200, 0.6, 0.0, 0.0).env (1, 0.0, 2.8, 0.0, 0.4).mod ("ENV 2", "Filter 1 Cutoff", 0.9)),
        ING ("Reese Swell", "a reese that swells in from nothing, filter opening (reverse-style)", "Swell",
             p.an (0, "Saw", 0.6).uni (0, 2, 0.16, 0.5).an (1, "Saw", 0.5).tune (1, 0, 0, 12).env (0, 2.5, 0.1, 1.0, 0.3)
              .filt (0, "Low Pass 24", 300, 0.2, 0.0, 0.0).env (1, 2.5, 0.1, 1.0, 0.3).mod ("ENV 2", "Filter 1 Cutoff", 0.7)),
        ING ("Glass Impact", "a glass hit over a sine boom that drops an octave", "Impact",
             p.smp ("Glass Hit", 0.6, true).an (0, "Sine", 0.8).tune (0, -2).env (1, 0.0, 0.12, 0.0, 0.1).mod ("ENV 2", "Osc A Pitch", 1.0).env (0, 0.001, 1.2, 0.0, 0.8)),
        ING ("Metal Impact", "a metal hit", "Impact", p.smp ("Metal Hit", 0.8, false)),
        ING ("Breath Whoosh", "a breath loop swept up and back down", "Whoosh",
             p.smp ("Air Breath", 0.8, false).smpLoop (0.1, 0.9).filt (0, "Band Pass", 300, 0.4, 0.0, 0.0).env (1, 1.0, 1.5, 0.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.8)),
        ING ("Vinyl Atmosphere", "vinyl crackle and a low dark hum", "Atmosphere", p.smp ("Vinyl Crackle", 0.7, false).smpLoop (0.05, 0.95).an (0, "Sine", 0.15).tune (0, -2)),
    };
    R.motions = {
        ING ("", "as designed", "", p.set (ids::analog, 0.2)),
        ING ("Stuttering", "gated in sixteenths", "Gated", p.trem ("Square", "1/16", 0.85)),
        ING ("Swirling", "a phaser swirl", "Moving", p.phaser (0.2, 0.9, 900, 0.6, 0.6)),
        ING ("Crushed", "bit-crushed", "Lo-Fi", p.crush (6, 8, 0.6)),
    };
    R.filters = { ING ("", "", "", p.set (ids::analog, 0.2)) };
    R.styles = { ING ("", "plays while the key is held", "", p.env (0, 0.005, 1.0, 1.0, 1.5)) };
    R.spaces = wetSpaces();
    R.characters = { Clean, Clean, Wide, Tape, Dirty };
    R.templates = { "M S", "P S", "P M S", "M P S" };
    R.allowed = [] (const juce::String& s, const juce::String& m)
    {
        if (s.contains ("Impact") || s == "Sub Drop" || s == "Reverse Swell" || s == "Laser Zap" || s == "Noise Burst" || s == "Tape Stop" || s == "Noise Pattern")
            return m.isEmpty() || m == "Crushed";
        if (s.contains ("Downlifter") || s.contains ("Riser") || s.contains ("Sweep") || s.contains ("Swell")) return m != "Stuttering";
        if (s.contains ("Atmosphere")) return m != "Swirling";
        return true;
    };
    R.base = [] (Patch& p) { p.voices (4).level (-10.0); };
    R.finish = [] (Patch& p) { p.stereo (1.5, true, 120.0); };
    return R;
}

//==============================================================================
std::vector<CategoryRecipe> allRecipes()
{
    return { reeseRecipe(), bassRecipe(), leadRecipe(), synthRecipe(), padRecipe(), textureRecipe(), sequenceRecipe(), percRecipe(), fxRecipe() };
}

const std::map<juce::String, std::vector<int>>& variedPicks()
{
    static const std::map<juce::String, std::vector<int>> m {
       #include "FactoryPresetsV8Picks.inc"
    };
    return m;
}

/** The picked candidates, in bank order, with names. Names come from the ingredients; a candidate whose
    every name layout is already taken is skipped in favour of the next unused candidate. */
void buildFromRecipe (const CategoryRecipe& R, std::vector<Spec>& out, juce::StringArray& used)
{
    auto it = variedPicks().find (R.category);
    std::vector<int> order;
    if (it != variedPicks().end()) order = it->second;
    std::set<int> seen (order.begin(), order.end());
    for (int k = 0; (int) order.size() < R.count * 6; ++k) if (! seen.count (k)) order.push_back (k);   // fallbacks
    int made = 0;
    for (int k : order)
    {
        if (made >= R.count) break;
        const auto n = nameFor (R, k, used);
        if (n.isEmpty()) continue;
        auto s = makeCandidate (R, k);
        s.name = n;
        used.add (n);
        out.push_back (std::move (s));
        ++made;
    }
}
