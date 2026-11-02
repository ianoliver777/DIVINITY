#include "GuardianBrain.h"
#include "../PluginProcessor.h"

namespace divinity
{
namespace
{
    bool hasAny (const juce::String& q, std::initializer_list<const char*> words)
    {
        for (auto* w : words) if (q.contains (w)) return true;
        return false;
    }
    bool hasAnyOf (const juce::String& q, const std::vector<const char*>& words)
    {
        for (auto* w : words) if (q.contains (w)) return true;
        return false;
    }

    struct Knowledge { std::vector<const char*> keys; const char* answer; };

    const std::vector<Knowledge>& knowledge()
    {
        static const std::vector<Knowledge> k {
            { { "soul engine", "soul pad", "xy", "emotion" },
              "The SOUL ENGINE is the XY pad on the SOUL tab. Left is soft, right is aggressive, down is melancholy, up is euphoria. Each direction pushes ten targets (brightness, drive, attack, tail, width...) by amounts you can edit in Emotion Mapping. The centre leaves your patch untouched." },
            { { "divine", "intervention" },
              "Divine Intervention is one big transformation knob on the SOUL tab. At 0% nothing changes; at 100% the chosen mode takes over: Ascension rises into light, Collapse falls apart, Shatter splinters, Bloom opens slowly and Abyss sinks into darkness. Try automating it into a drop." },
            { { "macro", "tone", "movement", "texture", "space" },
              "The four macros always do what they say: TONE opens the filters, MOVEMENT boosts LFO motion and chorus, TEXTURE adds fold and saturation, SPACE adds reverb, delay and width. They're also sources in the mod matrix." },
            { { "mod matrix", "modulation", "matrix", "route", "routing" },
              "Open LAB (or the MATRIX tab on CREATE). Each row picks a source, a destination and an amount from -100% to +100%. Right-click an amount to type it or clear the row. Knobs that are being modulated show a bright outer ring." },
            { { "save", "preset", "favourite", "favorite" },
              "Save with the disk icon in the preset bar or the + next to the preset name. The heart marks favourites, and PLAY has a searchable browser with a Favourites filter. User presets live in Documents / TRIPLE SEVEN AUDIO / DIVINITY / Presets." },
            { { "undo", "redo", "mistake" },
              "Undo is Cmd/Ctrl+Z and redo is Cmd/Ctrl+Shift+Z, or use the arrows in the bottom bar. Everything I change for you is undoable too." },
            { { "wavetable", "position", "wt pos", "scan" },
              "A wavetable is a stack of single-cycle waves. POS scans through them, so modulating POS with an LFO or envelope makes the timbre evolve. Click the wave display to pick a different table." },
            { { "unison", "supersaw", "detune", "thick" },
              "Unison stacks up to 8 detuned copies of an oscillator. DETUNE sets how far apart they are and WIDTH spreads them in stereo. 5-7 voices with ~30-45% detune is classic supersaw territory." },
            { { "filter", "cutoff", "resonance", "ladder" },
              "MG Low 24 is a warm 4-pole ladder; Low/High/Band Pass, Notch and Peak are clean state-variable shapes and Comb rings at the cutoff pitch. Cutoff sets brightness, resonance adds a singing peak. Turn up Key Track in LAB so high notes stay bright." },
            { { "envelope", "adsr", "attack", "release", "pluck" },
              "ENV 1 shapes volume. ENV 2 and ENV 3 are free to route anywhere. For a pluck: ENV 2 with zero sustain and a short decay into Filter 1 Cutoff at about +50%. You can drag the points on any envelope display." },
            { { "lfo", "wobble", "sync" },
              "LFOs move things over time. Switch SYNC on to lock one to your project tempo; 1/8 into Filter 1 Cutoff is an instant wobble. SMOOTH rounds off sharp edges and DEPTH scales it everywhere it's routed." },
            { { "arp", "arpeggi" },
              "Hit ARP in the preset bar (it's there on every tab) and hold a chord. The full arpeggiator is on PLAY: Up, Down, Up/Down, Down/Up, Random, As Played and Chord modes, rate (T = triplet, D = dotted), octaves, gate, swing and LATCH to keep it going hands-free. It locks to your DAW's grid while the transport runs." },
            { { "mono", "poly", "voice mode", "monophonic" },
              "POLY / MONO / LEGATO sits in the preset bar on every tab. POLY plays chords. MONO plays one note at a time and restarts the envelopes on every key, great for punchy basses and eski leads. LEGATO also plays one note, but overlapping notes slide without restarting, perfect for 808 glides." },
            { { "hide", "collapse", "go away", "smaller window", "minimi" },
              "Hit HIDE ANGEL next to the tabs and I'll tuck myself away; the window gets narrower. SHOW ANGEL brings me back whenever you need me." },
            { { "wave music", "neogrime", "grime", "eski", "barnacle" },
              "DIVINITY ships with 110 wave presets (pads, supersaw leads, reese, 808s, vocal chops, plucks, bells, atmospheres, gated and pumping chords, arps) and 90 neogrime presets (eski square leads, subs, growls, ice bells, stabs, metal plucks, 8-bit leads, 808s, pads, arps, sirens). Pick WAVE or NEOGRIME in the category menu, or ask me for something like \"wave 808\" or \"grime bell\"." },
            { { "glide", "portamento", "legato", "slide" },
              "Set VOICES mode to LEGATO in LAB and raise GLIDE in the bottom bar: overlapping notes will slide instead of retriggering. 40-80 ms feels great on 808s and leads." },
            { { "cpu", "lag", "performance", "crackle" },
              "Unison is the biggest CPU cost: 3 oscillators x 8 unison x many voices adds up fast. Lower VOICES in the bottom bar, use fewer unison voices on layers you can't hear, or switch off unused oscillators." },
            { { "clip", "too loud", "distort", "limiter" },
              "The limiter on the EFFECTS tab keeps the output under its ceiling. If it's pumping, lower the master OUT or the oscillator levels a little." },
            { { "quiet", "can't hear", "no sound", "silent" },
              "No sound? Check an oscillator is switched on, its LEVEL is up, the filter cutoff isn't fully closed and the master OUT isn't at the bottom. MIDI panic (the stop icon) clears stuck notes." },
            { { "effect", "fx", "reverb", "delay", "chorus", "saturation" },
              "The EFFECTS tab holds the chain: saturation, chorus, tempo-synced ping-pong delay, reverb and a safety limiter. Each module has a power switch, and the SOUL ENGINE and macros can push its sends." },
            { { "lab" },
              "LAB is the deep end: the full mod matrix, every oscillator setting, both filters, all LFOs and global voice settings. Spectral, granular and the Mutation Lab are on the way." },
            { { "explain mode", "explain a feature", "what is this" },
              "Hit EXPLAIN A FEATURE, then click any control and I'll tell you what it does. You can also hover any knob for a tooltip, or right-click it for options." },
            { { "who are you", "your name", "guardian", "angel" },
              "I'm your Guardian Angel. I live in DIVINITY to help you find sounds, explain anything you point at and throw ideas at you when you're stuck. I work offline, right here inside the plugin." },
            { { "shortcut", "fine", "double", "right-click", "right click", "type a value" },
              "Shift-drag any knob for fine control, double-click to reset it and right-click to type an exact value. The computer keyboard plays notes when the keyboard on PLAY has focus." },
            { { "808", "bass", "sub" },
              "For heavy low end: a sine or the Deep Sub / 808 Boom presets, random phase OFF for consistent punch, a little saturation for audibility on small speakers, LEGATO with 40-60 ms glide for slides. Push SOUL toward AGGRESSIVE with the Bass Pressure mapping for grit." },
        };
        return k;
    }

    const char* tabTips[][2] = {
        { "CREATE",  "This is the engine room: pick an oscillator on the left, shape it, then filter, envelope and LFO across the top. The strip below holds every envelope, LFO and the mod matrix." },
        { "EFFECTS", "Your effect chain lives here. Flip modules on with their power switches; the limiter at the end keeps everything safe." },
        { "PLAY",    "Browse and search presets, play the keyboard and ride the macros. Right-click a preset to favourite it." },
        { "SOUL",    "Drag the pad and feel the patch change mood. Then try Divine Intervention at about 40% in Bloom." },
        { "LAB",     "Deep controls: the full mod matrix, every oscillator setting, both filters and all four LFOs." },
    };
}

//==============================================================================
GuardianBrain::GuardianBrain (DivinityProcessor& p) : proc (p), rng (0x777) {}

float GuardianBrain::get (const juce::String& id) const
{
    auto* v = proc.apvts.getRawParameterValue (id);
    return v != nullptr ? v->load() : 0.0f;
}

void GuardianBrain::set (const juce::String& id, float real)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (real));
        p->endChangeGesture();
    }
}

void GuardianBrain::setChoice (const juce::String& id, const juce::String& name)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        const int idx = c->choices.indexOf (name);
        if (idx >= 0) set (id, (float) idx);
    }
}

void GuardianBrain::beginEdit (const juce::String& what)
{
    if (auto* um = proc.apvts.undoManager) um->beginNewTransaction ("Guardian Angel: " + what);
}

void GuardianBrain::clearModSlots()
{
    for (int i = 0; i < kNumModSlots; ++i)
    {
        set (ids::mod (i, "src"), 0); set (ids::mod (i, "dst"), 0); set (ids::mod (i, "amt"), 0);
    }
}

int GuardianBrain::firstFreeModSlot() const
{
    for (int i = 0; i < kNumModSlots; ++i)
        if (get (ids::mod (i, "src")) < 0.5f && get (ids::mod (i, "dst")) < 0.5f) return i;
    return -1;
}

bool GuardianBrain::hasRouting (const juce::String& src, const juce::String& dst) const
{
    const int s = modSourceNames().indexOf (src), d = modDestNames().indexOf (dst);
    for (int i = 0; i < kNumModSlots; ++i)
        if ((int) std::lround (get (ids::mod (i, "src"))) == s && (int) std::lround (get (ids::mod (i, "dst"))) == d) return true;
    return false;
}

void GuardianBrain::addRouting (const juce::String& src, const juce::String& dst, float amount)
{
    const int slot = firstFreeModSlot();
    if (slot < 0) return;
    setChoice (ids::mod (slot, "src"), src);
    setChoice (ids::mod (slot, "dst"), dst);
    set (ids::mod (slot, "amt"), amount);
}

//==============================================================================
GuardianBrain::Reply GuardianBrain::greet() const
{
    return { "Hey, I'm your Guardian Angel. Ask me anything about DIVINITY, describe a sound you want (\"dark evolving pad\", \"gritty 808\"), or tap a button below." };
}

juce::String GuardianBrain::tipForTab (const juce::String& tab) const
{
    for (auto& t : tabTips) if (tab == t[0]) return t[1];
    return {};
}

GuardianBrain::Reply GuardianBrain::explain (const juce::String& raw) const
{
    auto text = raw.upToFirstOccurrenceOf ("\n\n", false, false).trim();
    if (text.isEmpty())
        return { "That one's mostly decoration, but hover the controls around it: every knob and switch has a tooltip, and I can explain any of them." };
    return { text };
}

//==============================================================================
GuardianBrain::Reply GuardianBrain::randomize()
{
    beginEdit ("randomize patch");
    auto r = [this] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto pick = [this] (std::initializer_list<const char*> l) { return juce::String (*(l.begin() + rng.nextInt ((int) l.size()))); };
    const auto tables = wavetableNames();

    // start from a clean, safe base: everything back to default except master level and voice count
    for (auto* prm : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != ids::master && rp->getParameterID() != ids::voices)
            {
                rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture();
            }

    juce::StringArray parts;
    for (int o = 0; o < kNumOscs; ++o)
    {
        const bool on = o == 0 || rng.nextFloat() < 0.55f;
        set (ids::osc (o, "on"), on ? 1.0f : 0.0f);
        if (! on) continue;
        const bool wt = rng.nextFloat() < 0.6f;
        setChoice (ids::osc (o, "engine"), wt ? "Wavetable" : "Analog");
        if (wt)
        {
            const auto t = tables[rng.nextInt (tables.size())];
            setChoice (ids::osc (o, "table"), t);
            set (ids::osc (o, "pos"), r (0.0f, 0.8f));
            parts.add ("'" + t + "' wavetable");
        }
        else
        {
            const auto w = pick ({ "Saw", "Saw", "Square", "Pulse", "Triangle", "Sine" });
            setChoice (ids::osc (o, "wave"), w);
            set (ids::osc (o, "pw"), r (0.2f, 0.8f));
            parts.add ("analog " + w.toLowerCase());
        }
        const int uni[] = { 1, 1, 2, 3, 5, 7 };
        set (ids::osc (o, "unison"), (float) uni[rng.nextInt (6)]);
        set (ids::osc (o, "detune"), r (0.1f, 0.45f));
        set (ids::osc (o, "width"), r (0.4f, 1.0f));
        set (ids::osc (o, "fold"), rng.nextFloat() < 0.25f ? r (0.05f, 0.35f) : 0.0f);
        if (o > 0)
        {
            const int octs[] = { -1, 0, 0, 1 };
            const int semis[] = { 0, 0, 0, 7, -5 };
            set (ids::osc (o, "oct"), (float) octs[rng.nextInt (4)]);
            set (ids::osc (o, "semi"), (float) semis[rng.nextInt (5)]);
            set (ids::osc (o, "fine"), r (-8.0f, 8.0f));
        }
        set (ids::osc (o, "level"), o == 0 ? r (0.7f, 0.9f) : r (0.3f, 0.6f));
    }
    if (rng.nextFloat() < 0.35f) { set (ids::subOn, 1); set (ids::subLevel, r (0.3f, 0.55f)); parts.add ("sub"); }
    if (rng.nextFloat() < 0.25f) { set (ids::noiseOn, 1); set (ids::noiseLevel, r (0.04f, 0.18f)); setChoice (ids::noiseType, pick ({ "White", "Pink", "Dark", "Bright" })); }

    const auto ftype = pick ({ "MG Low 24", "MG Low 24", "Low Pass 12", "Band Pass", "Low Pass 12", "Comb" });
    setChoice (ids::filt (0, "type"), ftype);
    set (ids::filt (0, "cutoff"), 300.0f * std::pow (30.0f, rng.nextFloat()));
    set (ids::filt (0, "res"), r (0.0f, ftype == "Comb" ? 0.5f : 0.55f));
    set (ids::filt (0, "drive"), r (0.0f, 0.35f));
    if (ftype == "Comb") { set (ids::filt (0, "keytrack"), 1.0f); set (ids::filt (0, "cutoff"), 261.6f); set (ids::filt (0, "mix"), 0.5f); }

    juce::String shape;
    switch (rng.nextInt (3))
    {
        case 0:  shape = "pluck"; set (ids::env (0, "attack"), 0.001f); set (ids::env (0, "decay"), r (0.2f, 0.8f)); set (ids::env (0, "sustain"), r (0.0f, 0.25f)); set (ids::env (0, "release"), r (0.15f, 0.6f)); break;
        case 1:  shape = "pad";   set (ids::env (0, "attack"), r (0.3f, 1.8f)); set (ids::env (0, "decay"), 1.0f); set (ids::env (0, "sustain"), r (0.7f, 1.0f)); set (ids::env (0, "release"), r (1.0f, 4.0f)); break;
        default: shape = "held";  set (ids::env (0, "attack"), r (0.002f, 0.02f)); set (ids::env (0, "decay"), 0.4f); set (ids::env (0, "sustain"), r (0.7f, 0.95f)); set (ids::env (0, "release"), r (0.2f, 0.5f)); break;
    }
    set (ids::env (1, "attack"), r (0.001f, 0.2f)); set (ids::env (1, "decay"), r (0.1f, 1.2f));
    set (ids::env (1, "sustain"), r (0.0f, 0.4f));  set (ids::env (1, "release"), r (0.2f, 1.0f));

    setChoice (ids::lfo (0, "shape"), pick ({ "Sine", "Triangle", "Random", "Sine" }));
    if (rng.nextFloat() < 0.4f) { set (ids::lfo (0, "sync"), 1); setChoice (ids::lfo (0, "div"), pick ({ "1/8", "1/4", "1 bar", "2 bars", "1/8 D" })); }
    else set (ids::lfo (0, "rate"), r (0.08f, 4.0f));
    set (ids::lfo (0, "smooth"), r (0.0f, 0.6f));
    set (ids::lfo (1, "rate"), r (0.05f, 0.6f));

    clearModSlots();
    if (std::lround (get (ids::osc (0, "engine"))) == 0) addRouting ("LFO 1", "Osc A WT Pos", r (0.15f, 0.4f));
    if (shape == "pluck" || rng.nextFloat() < 0.5f) addRouting ("ENV 2", "Filter 1 Cutoff", r (0.25f, 0.6f));
    addRouting ("Velocity", "Filter 1 Cutoff", r (0.1f, 0.3f));
    if (rng.nextFloat() < 0.5f) addRouting ("LFO 2", "Filter 1 Cutoff", r (0.04f, 0.15f));
    addRouting ("Mod Wheel", "Osc A Fold", r (0.3f, 0.6f));

    set (ids::satOn, rng.nextFloat() < 0.6f ? 1.0f : 0.0f); set (ids::satDrive, r (0.05f, 0.4f));
    if (rng.nextFloat() < 0.35f) { set (ids::choOn, 1); set (ids::choMix, r (0.2f, 0.45f)); }
    if (rng.nextFloat() < 0.45f) { set (ids::dlyOn, 1); set (ids::dlyMix, r (0.1f, 0.3f)); setChoice (ids::dlyDiv, pick ({ "1/8 D", "1/4", "1/8", "1/4 D" })); }
    if (rng.nextFloat() < 0.65f) { set (ids::revOn, 1); set (ids::revMix, r (0.15f, 0.45f)); set (ids::revSize, r (0.5f, 0.95f)); }
    set (ids::limOn, 1);

    Reply rep;
    rep.text = "Fresh patch: " + parts.joinIntoString (" + ") + " through " + ftype + ", with a " + shape
             + " envelope and LFO motion. Mod wheel adds grit. Don't love it? Cmd/Ctrl+Z, or roll again.";
    rep.actionLabel = "ROLL AGAIN";
    rep.action = [this] { randomize(); };
    return rep;
}

//==============================================================================
GuardianBrain::Reply GuardianBrain::createSound (const juce::String& raw)
{
    const auto q = raw.toLowerCase();
    struct Base { std::vector<const char*> words; const char* preset; const char* noun; };
    static const std::vector<Base> bases {
        // wave + neogrime library first ("a+b" = both words must appear)
        { { "grime+808", "eski+808" }, "Grime 808", "neogrime 808" },
        { { "grime+sub", "eski+sub" }, "Pure Sub", "grime sub" },
        { { "grime+bass", "eski+bass" }, "Concrete Growl", "grime bass" },
        { { "grime+bell", "eski+bell", "ice bell" }, "Ice Bell", "ice bell" },
        { { "grime+arp", "eski+arp" }, "Tick Tock", "grime arp" },
        { { "grime+pad", "grime+atmos" }, "Grey Skies", "neogrime pad" },
        { { "grime+pluck", "metal+pluck" }, "Clang Pluck", "metal pluck" },
        { { "stab" }, "Cold Stab", "cold stab" },
        { { "8-bit", "8 bit", "chiptune", "chip" }, "8-Bit Ghost", "8-bit lead" },
        { { "siren" }, "Pirate Siren", "siren" },
        { { "eski", "grime", "square lead" }, "Eskimo Ice", "eski lead" },
        { { "wave+808" }, "Void 808", "wave 808" },
        { { "wave+reese", "wave+bass" }, "Wave Reese", "wave reese" },
        { { "wave+pluck" }, "Crystal Rain", "wave pluck" },
        { { "wave+bell" }, "Mourning Bell", "wave bell" },
        { { "wave+arp" }, "Falling Stars", "wave arp" },
        { { "wave+vox", "wave+vocal", "wave+choir", "vocal chop", "vox chop" }, "Angel Chop", "vocal chop lead" },
        { { "wave+lead", "wave+supersaw" }, "Into The Void", "wave lead" },
        { { "wave+pad", "ethereal" }, "Seraph Bloom", "wave pad" },
        { { "wave+atmos", "wave+ambient" }, "Northern Lights", "wave atmosphere" },
        { { "gated", "trance gate" }, "Strobe Heaven", "gated pad" },
        { { "pump", "sidechain" }, "Pulse Of Heaven", "pumping chords" },
        { { "screech", "scream" }, "Banshee", "screech lead" },
        { { "wave music", "wavey" }, "Seraph Bloom", "wave pad" },
        { { "808" }, "808 Boom", "808" },
        { { "kick", "drum" }, "Punch Kick", "kick" },
        { { "wobble", "dubstep", "riddim" }, "Wobble Machine", "wobble bass" },
        { { "reese", "dnb", "neuro" }, "Reese Bass", "reese bass" },
        { { "sub", "deep bass" }, "Deep Sub", "sub bass" },
        { { "bass" }, "Reese Bass", "bass" },
        { { "choir", "vocal", "voice", "vox", "angel" }, "Vox Choir", "vocal texture" },
        { { "drone", "dark ambient" }, "Dark Drone", "drone" },
        { { "ambient", "atmosphere", "wash", "texture" }, "Ambient Wash", "atmosphere" },
        { { "pad" }, "Celestial Pad", "pad" },
        { { "hyper", "hyperpop" }, "Hyper Lead", "hyper lead" },
        { { "brass" }, "Analog Brass", "brass lead" },
        { { "lead", "supersaw", "trance" }, "Supersaw Lead", "lead" },
        { { "pluck" }, "Glass Pluck", "pluck" },
        { { "bell", "chime" }, "Halo Bells", "bell" },
        { { "keys", "piano", "organ", "rhodes" }, "Soft Keys", "keys" },
        { { "arp", "sequence" }, "Arp Machine", "arp" },
        { { "riser", "sweep", "build" }, "Noise Riser", "riser" },
        { { "impact", "hit", "cinematic", "boom" }, "Cinematic Impact", "impact" },
        { { "metal", "comb", "experimental", "weird", "glitch" }, "Grit Comb", "experimental sound" },
    };

    const Base* base = nullptr;
    for (auto& b : bases)
    {
        for (auto* w : b.words)
        {
            bool all = true;
            for (auto& part : juce::StringArray::fromTokens (w, "+", "")) all &= q.contains (part);
            if (all) { base = &b; break; }
        }
        if (base != nullptr) break;
    }
    if (base == nullptr)
        return { "Tell me what kind of sound: pad, bass, 808, lead, pluck, keys, bell, choir, drone, riser, impact, arp... plus a vibe like dark, bright, aggressive, dreamy or wide. Example: \"dark aggressive bass\"." };

    beginEdit ("create " + juce::String (base->noun));
    const auto& all = proc.presets.getPresets();
    for (int i = 0; i < all.size(); ++i)
        if (all[i].isFactory && all[i].name == base->preset) { proc.presets.loadPreset (i); break; }

    juce::StringArray did;
    auto soulTo = [this] (float x, float y) { set (ids::soulX, x); set (ids::soulY, y); };
    float sx = 0.5f, sy = 0.5f;

    if (hasAny (q, { "dark", "deep", "evil", "moody" }))      { sy -= 0.3f; set (ids::filt (0, "cutoff"), get (ids::filt (0, "cutoff")) * 0.55f); did.add ("darker"); }
    if (hasAny (q, { "bright", "shiny", "crisp", "airy" }))   { set (ids::macro (0), 0.4f); did.add ("brighter"); }
    if (hasAny (q, { "aggressive", "hard", "angry", "heavy", "gritty", "dirty", "distorted", "nasty" }))
                                                              { sx += 0.35f; set (ids::macro (2), 0.45f); did.add ("aggressive"); }
    if (hasAny (q, { "soft", "gentle", "calm", "smooth", "warm" })) { sx -= 0.3f; did.add ("softer"); }
    if (hasAny (q, { "sad", "melanchol", "emotional", "nostalg", "lonely" })) { sy -= 0.35f; did.add ("melancholy"); }
    if (hasAny (q, { "happy", "euphor", "uplift", "festival", "joy" }))    { sy += 0.35f; did.add ("euphoric"); }
    if (hasAny (q, { "wide", "huge", "big", "massive", "epic" }))           { set (ids::macro (3), 0.45f); set (ids::osc (0, "unison"), juce::jmax (get (ids::osc (0, "unison")), 5.0f)); did.add ("wider"); }
    if (hasAny (q, { "space", "reverb", "spacey", "cosmic", "distant" }))   { set (ids::macro (3), 0.7f); did.add ("spacious"); }
    if (hasAny (q, { "evolv", "moving", "motion", "alive", "animated" }))   { set (ids::macro (1), 0.5f); if (! hasRouting ("LFO 2", "Osc A WT Pos")) { set (ids::lfo (1, "rate"), 0.12f); addRouting ("LFO 2", "Osc A WT Pos", 0.35f); } did.add ("evolving"); }
    if (hasAny (q, { "dream", "ethereal", "heaven", "celestial", "angelic", "divine" }))
                                                              { setChoice (ids::divineMode, "Ascension"); set (ids::divineAmount, 0.3f); did.add ("heavenly"); }
    if (hasAny (q, { "short", "tight", "staccato", "snappy" })) { set (ids::env (0, "release"), 0.12f); set (ids::env (0, "decay"), juce::jmin (get (ids::env (0, "decay")), 0.35f)); did.add ("tighter"); }
    if (hasAny (q, { "long", "endless", "sustained" }))       { set (ids::env (0, "release"), juce::jmax (get (ids::env (0, "release")), 3.0f)); did.add ("longer"); }
    if (hasAny (q, { "vintage", "analog", "retro", "tape", "warm" })) { set (ids::soul (7, "y"), -0.7f); sy -= 0.1f; did.add ("vintage drift"); }
    if (hasAny (q, { "wobbl" }) && ! hasRouting ("LFO 1", "Filter 1 Cutoff"))
    {
        set (ids::lfo (0, "sync"), 1); setChoice (ids::lfo (0, "div"), "1/8"); addRouting ("LFO 1", "Filter 1 Cutoff", 0.5f); did.add ("wobble");
    }
    if (hasAny (q, { "glide", "slide" })) { setChoice (ids::voiceMode, "Legato"); set (ids::glide, 70.0f); did.add ("gliding"); }

    soulTo (juce::jlimit (0.0f, 1.0f, sx), juce::jlimit (0.0f, 1.0f, sy));

    Reply rep;
    rep.text = "Here's a " + (did.isEmpty() ? juce::String() : did.joinIntoString (", ") + " ") + base->noun
             + ", built from \"" + base->preset + "\". Play it, then drag the SOUL pad or ride the macros to make it yours. Save it with the + button if it's a keeper.";
    rep.actionLabel = "MAKE IT WILDER";
    rep.action = [this] { set (ids::macro (2), juce::jmin (1.0f, get (ids::macro (2)) + 0.3f)); set (ids::soulX, juce::jmin (1.0f, get (ids::soulX) + 0.25f)); };
    rep.secondaryLabel = "MAKE IT CALMER";
    rep.secondary = [this] { set (ids::macro (2), juce::jmax (0.0f, get (ids::macro (2)) - 0.3f)); set (ids::soulX, juce::jmax (0.0f, get (ids::soulX) - 0.25f)); };
    return rep;
}

//==============================================================================
GuardianBrain::Reply GuardianBrain::suggestIdea()
{
    struct Idea { std::function<bool()> applies; juce::String text; std::function<void()> apply; };
    std::vector<Idea> ideas {
        { [this] { return std::lround (get (ids::osc (0, "engine"))) == 0 && ! hasRouting ("LFO 1", "Osc A WT Pos") && firstFreeModSlot() >= 0; },
          "Make Osc A breathe: route LFO 1 to its wavetable position at +30%. The timbre will slowly morph through the table.",
          [this] { addRouting ("LFO 1", "Osc A WT Pos", 0.3f); } },
        { [this] { return ! hasRouting ("ENV 2", "Filter 1 Cutoff") && firstFreeModSlot() >= 0; },
          "Give it a pluck: ENV 2 into Filter 1 Cutoff at +45% with a short decay. Every note gets a bright snap.",
          [this] { set (ids::env (1, "attack"), 0.001f); set (ids::env (1, "decay"), 0.25f); set (ids::env (1, "sustain"), 0.0f); addRouting ("ENV 2", "Filter 1 Cutoff", 0.45f); } },
        { [this] { return get (ids::osc (0, "unison")) < 3.0f; },
          "Thicken Osc A with 5 unison voices at 30% detune. Instant width without touching the low end.",
          [this] { set (ids::osc (0, "unison"), 5); set (ids::osc (0, "detune"), 0.3f); } },
        { [this] { return get (ids::revOn) < 0.5f; },
          "Put it in a room: reverb on at 25%. Even a little space makes a dry patch feel finished.",
          [this] { set (ids::revOn, 1); set (ids::revMix, 0.25f); } },
        { [this] { return std::abs (get (ids::soulY) - 0.5f) < 0.05f; },
          "Lift the mood: nudge the SOUL pad toward EUPHORIA. Brighter harmonics, wider image, more motion.",
          [this] { set (ids::soulY, 0.78f); } },
        { [this] { return get (ids::divineAmount) < 0.01f; },
          "Automate Divine Intervention from 0 to 60% in Bloom over the bar before your drop. Huge tension release.",
          [this] { setChoice (ids::divineMode, "Bloom"); set (ids::divineAmount, 0.35f); } },
        { [this] { return ! hasRouting ("Velocity", "Filter 1 Cutoff") && firstFreeModSlot() >= 0; },
          "Make it react to your playing: Velocity into Filter 1 Cutoff at +20%. Hard hits get brighter.",
          [this] { addRouting ("Velocity", "Filter 1 Cutoff", 0.2f); } },
        { [this] { return ! hasRouting ("Mod Wheel", "Osc A Fold") && firstFreeModSlot() >= 0; },
          "Map the mod wheel to Osc A fold at +50% and ride it live for grit on demand.",
          [this] { addRouting ("Mod Wheel", "Osc A Fold", 0.5f); } },
        { [this] { return get (ids::lfo (0, "sync")) < 0.5f; },
          "Lock LFO 1 to your tempo at 1/8. Whatever it's modulating turns rhythmic.",
          [this] { set (ids::lfo (0, "sync"), 1); setChoice (ids::lfo (0, "div"), "1/8"); } },
        { [this] { return get (ids::subOn) < 0.5f; },
          "Add weight: switch on the Sub (sine, one octave down) at 40%. Great under basses and keys.",
          [this] { set (ids::subOn, 1); setChoice (ids::subMode, "Sine -1"); set (ids::subLevel, 0.4f); } },
        { [this] { return get (ids::osc (1, "on")) < 0.5f; },
          "Layer Osc B an octave up as a quiet glassy wavetable. Air on top without muddying the body.",
          [this] { set (ids::osc (1, "on"), 1); setChoice (ids::osc (1, "engine"), "Wavetable"); setChoice (ids::osc (1, "table"), "Glass Halo"); set (ids::osc (1, "oct"), 1); set (ids::osc (1, "level"), 0.3f); } },
        { [this] { return get (ids::glide) < 1.0f; },
          "Try LEGATO with 60 ms glide. Overlap notes and they'll slide. Perfect for leads and 808s.",
          [this] { setChoice (ids::voiceMode, "Legato"); set (ids::glide, 60.0f); } },
    };

    for (size_t n = 0; n < ideas.size(); ++n)
    {
        auto& idea = ideas[(size_t) ((ideaCursor + (int) n) % (int) ideas.size())];
        if (idea.applies())
        {
            ideaCursor = (ideaCursor + (int) n + 1) % (int) ideas.size();
            Reply rep;
            rep.text = idea.text;
            rep.actionLabel = "DO IT";
            auto apply = idea.apply;
            rep.action = [this, apply] { beginEdit ("idea"); apply(); };
            rep.secondaryLabel = "ANOTHER IDEA";
            rep.secondary = [] {};   // the panel asks for the next idea
            return rep;
        }
    }
    return { "This patch already has everything I'd suggest. Try RANDOMIZE for a fresh direction, or push Divine Intervention to 100% just to hear what happens." };
}

//==============================================================================
bool GuardianBrain::tryTweak (const juce::String& q, Reply& out)
{
    struct Tweak { std::vector<const char*> words; const char* reply; std::function<void()> apply; };
    std::vector<Tweak> tweaks {
        { { "brighter", "more bright", "more highs", "more air" }, "Opened it up: TONE macro +25%.", [this] { set (ids::macro (0), juce::jmin (1.0f, get (ids::macro (0)) + 0.25f)); } },
        { { "darker", "less bright", "duller", "muffled" }, "Darkened it: Filter 1 cutoff down about an octave.", [this] { set (ids::filt (0, "cutoff"), juce::jmax (40.0f, get (ids::filt (0, "cutoff")) * 0.5f)); } },
        { { "wider", "more stereo", "bigger" }, "Wider now: SPACE macro +25% and more unison spread.", [this] { set (ids::macro (3), juce::jmin (1.0f, get (ids::macro (3)) + 0.25f)); set (ids::osc (0, "width"), 1.0f); } },
        { { "drier", "less reverb", "less space", "closer" }, "Pulled it closer: less reverb and SPACE.", [this] { set (ids::revMix, get (ids::revMix) * 0.5f); set (ids::macro (3), 0.0f); } },
        { { "more space", "more reverb", "spacier" }, "Added space: reverb on and SPACE macro up.", [this] { set (ids::revOn, 1); set (ids::macro (3), juce::jmin (1.0f, get (ids::macro (3)) + 0.3f)); } },
        { { "grittier", "dirtier", "more grit", "more distortion", "harder", "more aggressive" }, "More grit: TEXTURE macro +30% and SOUL toward AGGRESSIVE.", [this] { set (ids::macro (2), juce::jmin (1.0f, get (ids::macro (2)) + 0.3f)); set (ids::soulX, juce::jmin (1.0f, get (ids::soulX) + 0.2f)); } },
        { { "softer", "smoother", "less harsh", "gentler" }, "Softened: SOUL toward SOFT and TEXTURE down.", [this] { set (ids::soulX, juce::jmax (0.0f, get (ids::soulX) - 0.25f)); set (ids::macro (2), juce::jmax (0.0f, get (ids::macro (2)) - 0.3f)); } },
        { { "more movement", "evolve", "more motion", "less static" }, "Brought it to life: MOVEMENT macro +30%.", [this] { set (ids::macro (1), juce::jmin (1.0f, get (ids::macro (1)) + 0.3f)); } },
        { { "punchier", "more punch", "snappier" }, "Punchier: faster attack and a touch more drive.", [this] { set (ids::env (0, "attack"), 0.001f); set (ids::satOn, 1); set (ids::satDrive, juce::jmin (1.0f, get (ids::satDrive) + 0.15f)); } },
        { { "fatter", "thicker", "heavier", "more weight" }, "Thickened: sub on and more unison.", [this] { set (ids::subOn, 1); set (ids::subLevel, juce::jmax (get (ids::subLevel), 0.45f)); set (ids::osc (0, "unison"), juce::jmax (get (ids::osc (0, "unison")), 3.0f)); } },
        { { "louder", "turn it up" }, "Master up 3 dB (the limiter keeps it safe).", [this] { set (ids::master, juce::jmin (6.0f, get (ids::master) + 3.0f)); } },
        { { "quieter", "turn it down" }, "Master down 3 dB.", [this] { set (ids::master, juce::jmax (-60.0f, get (ids::master) - 3.0f)); } },
        { { "sadder", "more emotional", "more melancholy" }, "Moved the SOUL pad toward MELANCHOLY.", [this] { set (ids::soulY, juce::jmax (0.0f, get (ids::soulY) - 0.25f)); } },
        { { "happier", "more euphoric", "uplifting" }, "Moved the SOUL pad toward EUPHORIA.", [this] { set (ids::soulY, juce::jmin (1.0f, get (ids::soulY) + 0.25f)); } },
    };
    for (auto& t : tweaks)
        if (hasAnyOf (q, t.words))
        {
            beginEdit ("tweak");
            t.apply();
            out = { juce::String (t.reply) + " Cmd/Ctrl+Z undoes it." };
            return true;
        }
    return false;
}

bool GuardianBrain::searchKnowledge (const juce::String& q, Reply& out) const
{
    int best = 0; const Knowledge* hit = nullptr;
    for (auto& k : knowledge())
    {
        int score = 0;
        for (auto* key : k.keys) if (q.contains (key)) score += juce::String (key).length();
        if (score > best) { best = score; hit = &k; }
    }
    if (hit == nullptr) return false;
    out = { hit->answer };
    return true;
}

bool GuardianBrain::searchParameters (const juce::String& q, Reply& out) const
{
    int best = 0; juce::String bestId, bestName;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto name = rp->getName (64).toLowerCase();
            int score = 0;
            for (auto& word : juce::StringArray::fromTokens (name, " -|", ""))
                if (word.length() > 2 && q.contains (word)) score += word.length();
            if (score > best) { best = score; bestId = rp->getParameterID(); bestName = rp->getName (64); }
        }
    if (best < 4) return false;
    const auto help = helpTextFor (bestId);
    if (help.isEmpty()) return false;
    out = { bestName + ": " + help };
    return true;
}

GuardianBrain::Reply GuardianBrain::ask (const juce::String& raw)
{
    const auto q = " " + raw.toLowerCase().trim() + " ";
    if (q.trim().isEmpty()) return greet();

    if (hasAny (q, { "random", "surprise me", "roll" })) return randomize();
    if (hasAny (q, { "idea", "inspire", "suggest", "stuck", "what should i" })) return suggestIdea();

    const bool wantsSound = hasAny (q, { "make", "create", "design", "build", "give me", "i want", "i need", "patch for", "sound like", "preset for" });
    if (wantsSound)
    {
        auto rep = createSound (raw);
        if (rep.actionLabel.isNotEmpty()) return rep;   // a recipe matched
    }

    Reply rep;
    if (tryTweak (q, rep)) return rep;
    if (searchKnowledge (q, rep)) return rep;
    if (searchParameters (q, rep)) return rep;
    if (wantsSound) return createSound (raw);

    return { "I'm not sure about that one yet. Ask me about a control (\"what does fold do?\"), a feature (SOUL ENGINE, mod matrix, presets), ask for a sound (\"make a dreamy pad\") or say \"make it brighter\"." };
}

} // namespace divinity
