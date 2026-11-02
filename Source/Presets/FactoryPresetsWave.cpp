// 200 factory presets in the barnacle boi lane: wave music + neogrime.
//
// Each sound starts from a hand-built archetype (the engine, envelopes, routing
// and effect chain that define the family) and is then shaped by a short list
// of named traits ("dark", "gritty", "glide"...). Every trait is a concrete,
// audible parameter change, and the preset's description and tags are written
// from the same traits, so what the browser says is what you hear.
#include "PresetManager.h"
#include "../DSP/FxCatalog.h"
#include "../DSP/Arpeggiator.h"
#include <map>

namespace divinity
{
namespace
{
    /** Collects parameter values (last write wins) so traits can scale what the archetype set. */
    struct Patch
    {
        std::map<juce::String, juce::var> v;
        std::vector<juce::String> order;

        Patch& set (const juce::String& id, double x)
        {
            if (v.find (id) == v.end()) order.push_back (id);
            v[id] = x; return *this;
        }
        Patch& choice (const juce::String& id, const char* name)
        {
            if (v.find (id) == v.end()) order.push_back (id);
            v[id] = juce::String (name); return *this;
        }
        double get (const juce::String& id, double def) const
        {
            auto it = v.find (id);
            return (it != v.end() && ! it->second.isString()) ? (double) it->second : def;
        }
        bool has (const juce::String& id) const { return v.find (id) != v.end(); }

        // ---- building blocks -------------------------------------------------
        Patch& wt (int o, const char* table, double pos, double level = 0.8)
        {
            return set (ids::osc (o, "on"), 1).choice (ids::osc (o, "engine"), "Wavetable").choice (ids::osc (o, "table"), table)
                     .set (ids::osc (o, "pos"), pos).set (ids::osc (o, "level"), level);
        }
        Patch& an (int o, const char* wave, double level = 0.8)
        {
            return set (ids::osc (o, "on"), 1).choice (ids::osc (o, "engine"), "Analog").choice (ids::osc (o, "wave"), wave)
                     .set (ids::osc (o, "level"), level);
        }
        Patch& uni (int o, int voices, double detune, double width)
        {
            return set (ids::osc (o, "unison"), voices).set (ids::osc (o, "detune"), detune).set (ids::osc (o, "width"), width);
        }
        Patch& tune (int o, int oct, int semi = 0, double fine = 0.0)
        {
            return set (ids::osc (o, "oct"), oct).set (ids::osc (o, "semi"), semi).set (ids::osc (o, "fine"), fine);
        }
        Patch& tight (int o) { return set (ids::osc (o, "randphase"), 0).set (ids::osc (o, "phase"), 0.25); }
        Patch& filt (int f, const char* type, double cutoff, double res, double drive = 0.0, double keytrack = 0.0)
        {
            return choice (ids::filt (f, "type"), type).set (ids::filt (f, "cutoff"), cutoff).set (ids::filt (f, "res"), res)
                     .set (ids::filt (f, "drive"), drive).set (ids::filt (f, "keytrack"), keytrack);
        }
        Patch& env (int e, double a, double d, double s, double r, double curve = 0.0)
        {
            return set (ids::env (e, "attack"), a).set (ids::env (e, "decay"), d).set (ids::env (e, "sustain"), s)
                     .set (ids::env (e, "release"), r).set (ids::env (e, "curve"), curve);
        }
        Patch& lfo (int l, const char* shape, double rate, double smooth = 0.0)
        {
            return choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "rate"), rate).set (ids::lfo (l, "smooth"), smooth);
        }
        Patch& lfoSync (int l, const char* shape, const char* div)
        {
            return choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "sync"), 1).choice (ids::lfo (l, "div"), div);
        }
        /** Adds a matrix route in the next free slot. */
        Patch& mod (const char* src, const char* dst, double amt)
        {
            for (int s = 0; s < kNumModSlots; ++s)
                if (! has (ids::mod (s, "src")))
                    return choice (ids::mod (s, "src"), src).choice (ids::mod (s, "dst"), dst).set (ids::mod (s, "amt"), amt);
            return *this;   // matrix full: skip quietly (never happens with these recipes)
        }
        Patch& sub (const char* mode, double level) { return set (ids::subOn, 1).choice (ids::subMode, mode).set (ids::subLevel, level); }
        Patch& noise (const char* type, double level) { return set (ids::noiseOn, 1).choice (ids::noiseType, type).set (ids::noiseLevel, level); }
        Patch& sat (double drive, double mix = 1.0) { return set (ids::satOn, 1).set (ids::satDrive, drive).set (ids::satMix, mix); }
        Patch& chorus (double rate, double depth, double mix)
        {
            return set (ids::choOn, 1).set (ids::choRate, rate).set (ids::choDepth, depth).set (ids::choMix, mix);
        }
        Patch& delay (const char* div, double fb, double mix, bool ping = true, double tone = 0.55)
        {
            return set (ids::dlyOn, 1).choice (ids::dlyDiv, div).set (ids::dlyFeedback, fb).set (ids::dlyMix, mix)
                     .set (ids::dlyPingPong, ping ? 1 : 0).set (ids::dlyTone, tone);
        }
        Patch& reverb (double size, double damp, double mix)
        {
            return set (ids::revOn, 1).set (ids::revSize, size).set (ids::revDamp, damp).set (ids::revMix, mix);
        }
        Patch& dist (const char* type, double drive, double mix, double tone = 0.6)
        {
            return set (fxids::distOn, 1).choice (fxids::distType, type).set (fxids::distDrive, drive).set (fxids::distMix, mix).set (fxids::distTone, tone);
        }
        Patch& crush (double bits, double down, double mix)
        {
            return set (fxids::crushOn, 1).set (fxids::crushBits, bits).set (fxids::crushDown, down).set (fxids::crushMix, mix);
        }
        Patch& eq (double lowGain, double midFreq, double midGain, double highGain)
        {
            return set (fxids::eqOn, 1).set (fxids::eqLowGain, lowGain).set (fxids::eqMidFreq, midFreq)
                     .set (fxids::eqMidGain, midGain).set (fxids::eqHighGain, highGain);
        }
        Patch& comp (double thresh, double ratio, double makeup)
        {
            return set (fxids::compOn, 1).set (fxids::compThresh, thresh).set (fxids::compRatio, ratio).set (fxids::compMakeup, makeup);
        }
        Patch& trem (const char* shape, const char* div, double depth)
        {
            return set (fxids::tremOn, 1).choice (fxids::tremMode, "Tremolo").choice (fxids::tremShape, shape)
                     .set (fxids::tremSync, 1).choice (fxids::tremDiv, div).set (fxids::tremDepth, depth);
        }
        Patch& stereo (double width, bool monoBass = true)
        {
            return set (fxids::stereoOn, 1).set (fxids::stereoWidth, width).set (fxids::stereoMonoBass, monoBass ? 1 : 0);
        }
        Patch& mono (double glideMs, bool legato = true)
        {
            return choice (ids::voiceMode, legato ? "Legato" : "Mono").set (ids::glide, glideMs);
        }
        Patch& arp (const char* mode, const char* rate, int octaves, double gate, double swing = 0.0)
        {
            return set (arpids::on, 1).choice (arpids::mode, mode).choice (arpids::rate, rate)
                     .set (arpids::octaves, octaves).set (arpids::gate, gate).set (arpids::swing, swing);
        }
        Patch& macros (double a, double b, double c, double d)
        {
            return set (ids::macro (0), a).set (ids::macro (1), b).set (ids::macro (2), c).set (ids::macro (3), d);
        }
        Patch& level (double dB) { return set (ids::master, dB); }
    };

    //==========================================================================
    // Traits: each one is a real parameter change plus the words that describe it.
    struct TraitInfo { const char* tag; const char* phrase; };

    const std::map<juce::String, TraitInfo>& traitTable()
    {
        static const std::map<juce::String, TraitInfo> t {
            { "dark",     { "Dark",      "filter pulled down and the top end shelved off" } },
            { "bright",   { "Bright",    "opened filter and a lifted top end" } },
            { "wide",     { "Wide",      "unison spread to the edges and widened in the stereo stage" } },
            { "gritty",   { "Gritty",    "tube distortion blended underneath" } },
            { "crushed",  { "Lo-Fi",     "bitcrushed and downsampled" } },
            { "airy",     { "Airy",      "breath noise and an air shelf on top" } },
            { "lush",     { "Lush",      "chorus and a deeper reverb" } },
            { "hollow",   { "Hollow",    "a slow phaser carving out the middle" } },
            { "metallic", { "Metallic",  "ring modulation for cold, inharmonic edges" } },
            { "shimmer",  { "Shimmer",   "a frequency-shifted halo in the reverb" } },
            { "glide",    { "Glide",     "monophonic with legato glide" } },
            { "slow",     { "Slow",      "a slower swell in and a longer tail" } },
            { "tight",    { "Tight",     "short, punchy envelope" } },
            { "pump",     { "Pumping",   "quarter-note volume pump" } },
            { "gated",    { "Gated",     "hard sixteenth-note trance gate" } },
            { "detuned",  { "Detuned",   "extra unison voices, detuned wider" } },
            { "warm",     { "Warm",      "saturated and a touch more low end" } },
            { "moving",   { "Evolving",  "random LFO drifting the wavetable and filter" } },
            { "sub",      { "Sub",       "a sine sub an octave down" } },
            { "octave",   { "Octave",    "an extra layer an octave up" } },
            { "vox",      { "Vocal",     "a formant vowel layer" } },
            { "glass",    { "Glassy",    "glass partials layered on top" } },
            { "sweep",    { "Sweep",     "a filter envelope sweeping each note" } },
            { "squash",   { "Squashed",  "compressed hard for density" } },
            { "drunk",    { "Wonky",     "slow pitch wobble like a worn tape" } },
            { "swing",    { "Swung",     "swung arp steps" } },
        };
        return t;
    }

    void applyTrait (Patch& p, const juce::String& t)
    {
        const double cut = p.get (ids::filt (0, "cutoff"), 12000.0);
        if (t == "dark")          { p.set (ids::filt (0, "cutoff"), juce::jmax (180.0, cut * 0.5)); p.eq (p.get (fxids::eqLowGain, 0), 1200, p.get (fxids::eqMidGain, 0), -5.0); }
        else if (t == "bright")   { p.set (ids::filt (0, "cutoff"), juce::jmin (17000.0, cut * 1.9)); p.eq (p.get (fxids::eqLowGain, 0), 2500, p.get (fxids::eqMidGain, 0), 3.5); }
        else if (t == "wide")     { for (int o = 0; o < 3; ++o) if (p.has (ids::osc (o, "unison"))) p.set (ids::osc (o, "width"), 1.0); p.stereo (1.45); }
        else if (t == "gritty")   p.dist ("Tube", 0.45, 0.4, 0.55);
        else if (t == "crushed")  p.crush (9, 5, 0.5).eq (p.get (fxids::eqLowGain, 0), 1500, 1.5, -4.0);
        else if (t == "airy")     p.noise ("Bright", 0.05).eq (p.get (fxids::eqLowGain, 0), 2000, p.get (fxids::eqMidGain, 0), 4.0);
        else if (t == "lush")     p.chorus (0.35, 0.45, 0.4).set (ids::revOn, 1).set (ids::revMix, juce::jmin (0.75, p.get (ids::revMix, 0.2) + 0.15)).set (ids::revSize, juce::jmax (0.8, p.get (ids::revSize, 0.6)));
        else if (t == "hollow")   p.set (fxids::phaseOn, 1).set (fxids::phaseRate, 0.12).set (fxids::phaseDepth, 0.8).set (fxids::phaseFb, 0.55).set (fxids::phaseMix, 0.55);
        else if (t == "metallic") p.set (fxids::ringOn, 1).set (fxids::ringFreq, 740).set (fxids::ringMix, 0.22);
        else if (t == "shimmer")  p.set (fxids::fshiftOn, 1).set (fxids::fshiftShift, 0.18).set (fxids::fshiftMix, 0.22).reverb (0.95, 0.3, juce::jmax (0.45, p.get (ids::revMix, 0.3)));
        else if (t == "glide")    p.mono (85.0);
        else if (t == "slow")     p.set (ids::env (0, "attack"), juce::jmax (0.6, p.get (ids::env (0, "attack"), 0.01) * 3.0)).set (ids::env (0, "release"), p.get (ids::env (0, "release"), 0.5) * 1.6);
        else if (t == "tight")    p.set (ids::env (0, "decay"), p.get (ids::env (0, "decay"), 0.3) * 0.6).set (ids::env (0, "release"), juce::jmax (0.05, p.get (ids::env (0, "release"), 0.3) * 0.4)).set (fxids::transOn, 1).set (fxids::transAttack, 0.5);
        else if (t == "pump")     p.trem ("Sine", "1/4", 0.65);
        else if (t == "gated")    p.trem ("Square", "1/16", 0.95);
        else if (t == "detuned")  { for (int o = 0; o < 3; ++o) if (p.has (ids::osc (o, "unison"))) p.set (ids::osc (o, "unison"), juce::jmin (16.0, p.get (ids::osc (o, "unison"), 1) + 3)).set (ids::osc (o, "detune"), juce::jmin (0.9, p.get (ids::osc (o, "detune"), 0.25) + 0.2)); }
        else if (t == "warm")     p.sat (0.35).eq (2.5, p.get (fxids::eqMidFreq, 1000), p.get (fxids::eqMidGain, 0), p.get (fxids::eqHighGain, 0) - 1.0);
        else if (t == "moving")   p.lfo (3, "Random", 0.35, 0.85).mod ("LFO 4", "Osc A WT Pos", 0.22).mod ("LFO 4", "Filter 1 Cutoff", 0.1);
        else if (t == "sub")      p.sub ("Sine -1", 0.45);
        else if (t == "octave")   { if (! p.has (ids::osc (2, "on"))) p.an (2, "Saw", 0.3).tune (2, 1).uni (2, 3, 0.3, 0.8); }
        else if (t == "vox")      { if (! p.has (ids::osc (1, "on"))) p.wt (1, "Formant Vox", 0.45, 0.45).uni (1, 3, 0.25, 0.8).mod ("LFO 2", "Osc B WT Pos", 0.3).lfo (1, "Sine", 0.18); }
        else if (t == "glass")    { if (! p.has (ids::osc (2, "on"))) p.wt (2, "Glass Halo", 0.3, 0.35).tune (2, 1); }
        else if (t == "sweep")    p.env (1, 0.005, 0.35, 0.15, 0.4).mod ("ENV 2", "Filter 1 Cutoff", 0.45);
        else if (t == "squash")   p.comp (-26, 6, 7);
        else if (t == "drunk")    p.lfo (2, "Sine", 0.6, 0.4).mod ("LFO 3", "Osc A Pitch", 0.006);
        else if (t == "swing")    p.set (arpids::swing, 0.45);
    }

    //==========================================================================
    // Archetypes: the musical core of each family.
    enum class Arch
    {
        // wave
        SuperLead, EtherPad, Reese, Wave808, VoxLead, GlassPluck, DarkBell, Atmos, GatePad, WaveArp, Screech, PumpChord,
        // neogrime
        EskiLead, GrimeSub, GrowlBass, IceBell, ColdStab, MetalPluck, CrushLead, Grime808, VoidPad, GrimeArp, Siren
    };

    struct Family { Arch arch; const char* category; const char* style; const char* base; };

    const Family families[] = {
        { Arch::SuperLead,  "Wave Lead",   "Wave",     "Detuned supersaw lead for drops and toplines, with ping-pong echoes into a long hall." },
        { Arch::EtherPad,   "Wave Pad",    "Wave",     "Slow, emotional wavetable pad that blooms into a huge reverb." },
        { Arch::Reese,      "Wave Bass",   "Wave",     "Reese bass: two detuned saws through a breathing ladder filter, mono in the low end." },
        { Arch::Wave808,    "Wave 808",    "Wave",     "Long 808 with a pitch drop, tube grit and glide between notes." },
        { Arch::VoxLead,    "Wave Vox",    "Wave",     "Chopped-vocal style formant lead. Mod wheel moves the vowel." },
        { Arch::GlassPluck, "Wave Pluck",  "Wave",     "Glassy pluck with a quick filter snap, dotted echoes and a soft room." },
        { Arch::DarkBell,   "Wave Bell",   "Wave",     "Dark, ringing bell: hollow organ and glass partials with a long release." },
        { Arch::Atmos,      "Wave Atmos",  "Wave",     "Drifting atmosphere for intros and breakdowns: noise, slow motion and endless space." },
        { Arch::GatePad,    "Wave Pad",    "Wave",     "Supersaw chord pad chopped by a tempo-synced gate." },
        { Arch::WaveArp,    "Wave Arp",    "Wave",     "Hold a chord: the arpeggiator plays it as a shimmering sixteenth-note line." },
        { Arch::Screech,    "Wave Lead",   "Wave",     "Folded, distorted screech lead that cuts through a dense drop." },
        { Arch::PumpChord,  "Wave Pad",    "Wave",     "Wide saw chords pumping on the quarter note, like heavy sidechain." },

        { Arch::EskiLead,   "Grime Lead",  "Neogrime", "Eski-style square lead: hollow, cold and stabby, with dotted echoes." },
        { Arch::GrimeSub,   "Grime Sub",   "Neogrime", "Clean sine sub that slides between notes. Pure weight, no mud." },
        { Arch::GrowlBass,  "Grime Bass",  "Neogrime", "Mid-range square growl over a sine sub, distorted and compressed." },
        { Arch::IceBell,    "Grime Bell",  "Neogrime", "Icy, metallic bell for sparse melodies." },
        { Arch::ColdStab,   "Grime Stab",  "Neogrime", "Short, cold chord stab with a dark room behind it." },
        { Arch::MetalPluck, "Grime Pluck", "Neogrime", "Pulse pluck through a comb filter: tuned, clangy and percussive." },
        { Arch::CrushLead,  "Grime Lead",  "Neogrime", "8-bit square lead, crushed and downsampled." },
        { Arch::Grime808,   "Grime 808",   "Neogrime", "Short, punchy 808 with a fast pitch drop and hard distortion." },
        { Arch::VoidPad,    "Grime Pad",   "Neogrime", "Bleak, hollow pad: organ drones and dark noise in a cold space." },
        { Arch::GrimeArp,   "Grime Arp",   "Neogrime", "Hold a chord: a square-wave arp ticks it out in tight steps." },
        { Arch::Siren,      "Grime FX",    "Neogrime", "Siren / riser FX: pitch sweeps, shifted noise and long echoes." },
    };

    void buildArchetype (Patch& p, Arch a, juce::Random& r)
    {
        auto jit = [&r] (double v, double amount) { return v * (1.0 + amount * (r.nextDouble() * 2.0 - 1.0)); };

        switch (a)
        {
            case Arch::SuperLead:
                p.an (0, "Saw", 0.75).uni (0, 7, jit (0.42, 0.2), 0.85);
                p.wt (1, "Harmonic Sweep", jit (0.55, 0.3), 0.4).uni (1, 3, 0.3, 0.8).tune (1, 1);
                p.filt (0, "MG Low 24", jit (6500, 0.25), 0.18, 0.1, 0.3);
                p.env (0, 0.004, 0.4, 0.85, 0.35).env (1, 0.002, 0.5, 0.3, 0.4).mod ("ENV 2", "Filter 1 Cutoff", 0.25);
                p.mod ("Mod Wheel", "Osc A Detune", 0.3);
                p.mono (35.0).delay ("1/8 D", 0.38, 0.22).reverb (0.85, 0.45, 0.3).sat (0.2);
                p.macros (0.15, 0.0, 0.1, 0.25).level (-9.0);
                break;
            case Arch::EtherPad:
                p.wt (0, r.nextBool() ? "Formant Vox" : "Glass Halo", jit (0.35, 0.4), 0.8).uni (0, 6, jit (0.35, 0.2), 0.9);
                p.wt (1, "Basic Shapes", jit (0.15, 0.5), 0.45).uni (1, 4, 0.4, 0.95).tune (1, -1);
                p.filt (0, "MG Low 24", jit (2600, 0.3), 0.15);
                p.env (0, jit (1.2, 0.3), 1.8, 0.85, jit (3.2, 0.2), 0.2).env (1, 2.0, 3.0, 0.4, 3.0);
                p.lfo (0, "Sine", jit (0.12, 0.4)).mod ("ENV 2", "Osc A WT Pos", 0.35).mod ("LFO 1", "Filter 1 Cutoff", 0.12);
                p.chorus (0.3, 0.4, 0.35).delay ("1/4 D", 0.45, 0.18, true, 0.4).reverb (0.95, 0.45, 0.48);
                p.macros (0.0, 0.2, 0.0, 0.35).level (-8.0);
                break;
            case Arch::Reese:
                p.an (0, "Saw", 0.75).uni (0, 2, jit (0.22, 0.3), 0.3).tight (0).tune (0, -1);
                p.an (1, "Saw", 0.6).uni (1, 2, jit (0.3, 0.3), 0.4).tune (1, -1, 0, 9).tight (1);
                p.sub ("Sine -1", 0.55);
                p.filt (0, "MG Low 24", jit (900, 0.3), 0.3, 0.35);
                p.env (0, 0.003, 0.3, 1.0, 0.18);
                p.lfoSync (0, "Sine", "1/2").set (ids::lfo (0, "depth"), 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.28);
                p.mono (40.0).dist ("Soft Cubic", 0.35, 0.45).stereo (1.1, true).set (fxids::stereoMonoFreq, 160);
                p.macros (0.0, 0.25, 0.15, 0.0).level (-9.0);
                break;
            case Arch::Wave808:
                p.an (0, "Sine", 0.9).tight (0).tune (0, -1);
                p.filt (0, "Off", 12000, 0.0);
                p.env (0, 0.001, jit (1.6, 0.25), 0.0, 0.5).env (2, 0.001, 0.07, 0.0, 0.06, -0.5).mod ("ENV 3", "Osc A Pitch", 0.55);
                p.mono (jit (110, 0.3), true).dist ("Tube", jit (0.45, 0.3), 0.6, 0.45).stereo (0.0, true);
                p.macros (0.0, 0.0, 0.2, 0.0).level (-8.0);
                break;
            case Arch::VoxLead:
                p.wt (0, "Formant Vox", jit (0.4, 0.4), 0.85).uni (0, 3, 0.2, 0.7);
                p.filt (0, "Low Pass 12", jit (5500, 0.25), 0.15);
                p.env (0, 0.01, 0.6, 0.7, 0.45);
                p.lfo (0, "Sine", 4.6, 0.3).mod ("LFO 1", "Osc A Pitch", 0.003).mod ("Mod Wheel", "Osc A WT Pos", 0.5).mod ("Velocity", "Osc A WT Pos", 0.2);
                p.mono (50.0).delay ("1/8", 0.35, 0.2).reverb (0.8, 0.4, 0.32);
                p.macros (0.1, 0.0, 0.0, 0.2).level (-8.0);
                break;
            case Arch::GlassPluck:
                p.wt (0, "Glass Halo", jit (0.25, 0.5), 0.85).uni (0, 3, 0.18, 0.75);
                p.an (1, "Triangle", 0.35).tune (1, 1);
                p.filt (0, "MG Low 24", jit (900, 0.3), 0.25, 0.0, 0.5);
                p.env (0, 0.001, jit (0.45, 0.3), 0.0, 0.4).env (1, 0.001, jit (0.22, 0.3), 0.0, 0.2).mod ("ENV 2", "Filter 1 Cutoff", 0.62);
                p.delay ("1/8 D", 0.42, 0.24).reverb (0.75, 0.4, 0.3);
                p.macros (0.0, 0.0, 0.0, 0.2).level (-7.0);
                break;
            case Arch::DarkBell:
                p.wt (0, "Hollow Organ", jit (0.6, 0.3), 0.7).tune (0, 0);
                p.wt (1, "Glass Halo", jit (0.7, 0.3), 0.45).tune (1, 1, 7);
                p.filt (0, "Low Pass 12", jit (3500, 0.3), 0.1);
                p.env (0, 0.001, jit (2.2, 0.3), 0.0, jit (2.5, 0.3), -0.2).env (1, 0.001, 0.6, 0.0, 0.5).mod ("ENV 2", "Osc B WT Pos", -0.35);
                p.delay ("1/4", 0.35, 0.16, true, 0.35).reverb (0.9, 0.55, 0.4);
                p.macros (0.0, 0.0, 0.0, 0.3).level (-8.0);
                break;
            case Arch::Atmos:
                p.wt (0, r.nextBool() ? "Harmonic Sweep" : "Formant Vox", jit (0.5, 0.4), 0.6).uni (0, 8, 0.5, 1.0);
                p.noise (r.nextBool() ? "Pink" : "Dark", jit (0.14, 0.3));
                p.filt (0, "Band Pass", jit (1400, 0.4), 0.3).filt (1, "Low Pass 12", 5000, 0.1);
                p.env (0, jit (2.5, 0.3), 2.0, 1.0, jit (5.0, 0.2));
                p.lfo (0, "Random", 0.2, 0.9).lfo (1, "Sine", 0.05).mod ("LFO 1", "Osc A WT Pos", 0.35).mod ("LFO 2", "Filter 1 Cutoff", 0.35);
                p.delay ("1/2 D", 0.6, 0.28, true, 0.35).reverb (1.0, 0.5, 0.65);
                p.macros (0.0, 0.4, 0.2, 0.5).level (-7.0);
                break;
            case Arch::GatePad:
                p.an (0, "Saw", 0.7).uni (0, 8, jit (0.4, 0.2), 0.95);
                p.wt (1, "Basic Shapes", 0.66, 0.35).uni (1, 3, 0.3, 0.8).tune (1, 1);
                p.filt (0, "MG Low 24", jit (4200, 0.3), 0.15);
                p.env (0, 0.02, 0.8, 0.9, 0.6);
                p.trem ("Square", r.nextBool() ? "1/16" : "1/8", 0.95).reverb (0.85, 0.4, 0.35).delay ("1/8 D", 0.3, 0.12);
                p.macros (0.1, 0.0, 0.0, 0.2).level (-9.0);
                break;
            case Arch::WaveArp:
                p.wt (0, r.nextBool() ? "Glass Halo" : "Basic Shapes", jit (0.3, 0.5), 0.8).uni (0, 3, 0.2, 0.8);
                p.filt (0, "MG Low 24", jit (1800, 0.3), 0.25, 0.0, 0.4);
                p.env (0, 0.001, 0.35, 0.2, 0.3).env (1, 0.001, 0.18, 0.0, 0.2).mod ("ENV 2", "Filter 1 Cutoff", 0.5);
                p.arp (r.nextBool() ? "Up/Down" : "Up", "1/16", 2, jit (0.55, 0.2));
                p.delay ("1/8 D", 0.45, 0.25).reverb (0.85, 0.4, 0.35);
                p.macros (0.0, 0.0, 0.0, 0.25).level (-8.0);
                break;
            case Arch::Screech:
                p.wt (0, "Pulse Grit", jit (0.6, 0.3), 0.75).uni (0, 4, 0.25, 0.7).set (ids::osc (0, "fold"), jit (0.35, 0.3));
                p.an (1, "Square", 0.4).tune (1, 1);
                p.filt (0, "Band Pass", jit (2200, 0.3), 0.45, 0.3).set (ids::filt (0, "mix"), 0.7);
                p.env (0, 0.003, 0.4, 0.8, 0.3);
                p.lfoSync (0, "Triangle", "1/8").mod ("LFO 1", "Osc A Fold", 0.25).mod ("Mod Wheel", "Filter 1 Cutoff", 0.4);
                p.mono (45.0).dist ("Sine Fold", 0.5, 0.55).delay ("1/8", 0.3, 0.15).reverb (0.7, 0.5, 0.2);
                p.macros (0.2, 0.2, 0.3, 0.1).level (-11.0);
                break;
            case Arch::PumpChord:
                p.an (0, "Saw", 0.7).uni (0, 7, 0.38, 0.95).an (1, "Square", 0.35).uni (1, 3, 0.2, 0.7).tune (1, -1);
                p.filt (0, "MG Low 24", jit (3800, 0.3), 0.2);
                p.env (0, 0.01, 0.5, 0.9, 0.4);
                p.trem ("Sine", "1/4", 0.7).reverb (0.8, 0.45, 0.28).stereo (1.3, true);
                p.macros (0.1, 0.0, 0.0, 0.15).level (-9.0);
                break;

            case Arch::EskiLead:
                p.an (0, "Square", 0.8).tight (0).an (1, "Pulse", 0.35).set (ids::osc (1, "pw"), jit (0.25, 0.3)).tune (1, 1);
                p.filt (0, "Low Pass 12", jit (4200, 0.3), 0.2);
                p.env (0, 0.001, jit (0.25, 0.3), 0.35, 0.12);
                p.mono (jit (25, 0.5), false).delay ("1/8 D", 0.35, 0.2, true, 0.45).reverb (0.6, 0.6, 0.18);
                p.macros (0.0, 0.0, 0.0, 0.15).level (-10.0);
                break;
            case Arch::GrimeSub:
                p.an (0, "Sine", 0.95).tight (0).tune (0, -1);
                p.filt (0, "Low Pass 12", 400, 0.0);
                p.env (0, 0.003, 0.3, 1.0, jit (0.15, 0.4));
                p.mono (jit (70, 0.4), true).sat (jit (0.15, 0.5)).stereo (0.0, true);
                p.macros (0.0, 0.0, 0.0, 0.0).level (-7.0);
                break;
            case Arch::GrowlBass:
                p.an (0, "Square", 0.7).tight (0).tune (0, -1).an (1, "Pulse", 0.4).set (ids::osc (1, "pw"), 0.2).tune (1, -1, 0, 7);
                p.sub ("Sine -1", 0.6);
                p.filt (0, "MG Low 24", jit (1100, 0.3), 0.35, 0.4);
                p.env (0, 0.002, 0.3, 0.9, 0.15).env (1, 0.002, jit (0.25, 0.3), 0.2, 0.2).mod ("ENV 2", "Filter 1 Cutoff", 0.35);
                p.lfoSync (0, "Sine", r.nextBool() ? "1/8" : "1/4").mod ("LFO 1", "Filter 1 Cutoff", 0.18);
                p.mono (30.0).dist ("Hard Clip", jit (0.35, 0.3), 0.5).comp (-24, 5, 5).stereo (0.9, true);
                p.macros (0.0, 0.2, 0.2, 0.0).level (-10.0);
                break;
            case Arch::IceBell:
                p.wt (0, "Glass Halo", jit (0.8, 0.2), 0.75).an (1, "Sine", 0.35).tune (1, 2);
                p.filt (0, "Low Pass 12", jit (7000, 0.3), 0.1);
                p.env (0, 0.001, jit (0.9, 0.3), 0.0, jit (1.0, 0.3), -0.3);
                p.set (fxids::ringOn, 1).set (fxids::ringFreq, jit (1100, 0.3)).set (fxids::ringMix, 0.18);
                p.delay ("1/8 D", 0.32, 0.18).reverb (0.7, 0.35, 0.28);
                p.macros (0.0, 0.0, 0.0, 0.2).level (-8.0);
                break;
            case Arch::ColdStab:
                p.wt (0, "Hollow Organ", jit (0.4, 0.4), 0.75).uni (0, 3, 0.15, 0.6).an (1, "Saw", 0.35).uni (1, 3, 0.25, 0.8);
                p.filt (0, "MG Low 24", jit (2800, 0.3), 0.2);
                p.env (0, 0.001, jit (0.32, 0.3), 0.0, 0.25).env (1, 0.001, 0.2, 0.0, 0.2).mod ("ENV 2", "Filter 1 Cutoff", 0.4);
                p.reverb (0.75, 0.6, 0.3).delay ("1/4", 0.25, 0.1, false, 0.4);
                p.macros (0.0, 0.0, 0.0, 0.2).level (-8.0);
                break;
            case Arch::MetalPluck:
                p.an (0, "Pulse", 0.8).set (ids::osc (0, "pw"), jit (0.3, 0.3));
                p.filt (0, "Comb", jit (900, 0.3), 0.6, 0.0, 1.0).filt (1, "Low Pass 12", 6000, 0.1);
                p.env (0, 0.001, jit (0.35, 0.3), 0.0, 0.3).env (1, 0.001, 0.12, 0.0, 0.1).mod ("ENV 2", "Filter 2 Cutoff", 0.4);
                p.delay ("1/16 D", 0.3, 0.15).reverb (0.6, 0.5, 0.2);
                p.macros (0.0, 0.0, 0.1, 0.15).level (-9.0);
                break;
            case Arch::CrushLead:
                p.an (0, "Square", 0.8).tight (0).an (1, "Square", 0.3).tune (1, 1, 0, 6);
                p.filt (0, "Low Pass 12", 9000, 0.1);
                p.env (0, 0.001, 0.2, 0.6, 0.1);
                p.crush (jit (6, 0.3), jit (8, 0.3), 0.7).mono (jit (20, 0.5), false).delay ("1/8", 0.3, 0.15, false, 0.5);
                p.macros (0.0, 0.0, 0.3, 0.1).level (-12.0);
                break;
            case Arch::Grime808:
                p.an (0, "Sine", 0.9).tight (0).tune (0, -1).an (1, "Triangle", 0.25).tight (1).tune (1, 0);
                p.filt (0, "Off", 12000, 0.0);
                p.env (0, 0.001, jit (0.55, 0.3), 0.0, 0.2).env (2, 0.001, 0.04, 0.0, 0.04, -0.6).mod ("ENV 3", "Osc A Pitch", 0.75).mod ("ENV 3", "Osc B Pitch", 0.75);
                p.mono (jit (60, 0.4), false).dist ("Hard Clip", jit (0.5, 0.3), 0.55, 0.5).stereo (0.0, true);
                p.macros (0.0, 0.0, 0.2, 0.0).level (-9.0);
                break;
            case Arch::VoidPad:
                p.wt (0, "Hollow Organ", jit (0.3, 0.5), 0.65).uni (0, 4, 0.3, 0.85).tune (0, -1).wt (1, "Harmonic Sweep", jit (0.2, 0.5), 0.4).uni (1, 4, 0.4, 0.9);
                p.noise ("Dark", 0.1);
                p.filt (0, "MG Low 24", jit (1300, 0.3), 0.3, 0.15);
                p.env (0, jit (1.8, 0.3), 2.0, 0.9, jit (3.5, 0.3));
                p.lfo (0, "Sine", 0.07).mod ("LFO 1", "Filter 1 Cutoff", 0.25).mod ("LFO 1", "Osc B WT Pos", 0.3);
                p.reverb (0.95, 0.65, 0.5).delay ("1/4 D", 0.5, 0.15, true, 0.3);
                p.macros (0.0, 0.2, 0.0, 0.35).level (-8.0);
                break;
            case Arch::GrimeArp:
                p.an (0, "Square", 0.75).tight (0).an (1, "Pulse", 0.3).set (ids::osc (1, "pw"), 0.2).tune (1, 1);
                p.filt (0, "Low Pass 12", jit (3000, 0.3), 0.25, 0.0, 0.4);
                p.env (0, 0.001, 0.18, 0.1, 0.1);
                p.arp (r.nextBool() ? "Up" : "As Played", r.nextBool() ? "1/16" : "1/8T", 1, jit (0.45, 0.2));
                p.delay ("1/8 D", 0.3, 0.16).reverb (0.6, 0.6, 0.18);
                p.macros (0.0, 0.0, 0.0, 0.15).level (-10.0);
                break;
            case Arch::Siren:
                p.an (0, r.nextBool() ? "Saw" : "Square", 0.6).uni (0, 3, 0.2, 0.8).noise ("White", 0.2);
                p.filt (0, "Band Pass", jit (1500, 0.3), 0.5);
                p.env (0, 0.4, 1.0, 1.0, 1.5).env (1, 3.0, 1.0, 1.0, 1.0);
                p.lfo (0, "Triangle", jit (0.8, 0.4)).mod ("LFO 1", "Osc A Pitch", 0.25).mod ("ENV 2", "Filter 1 Cutoff", 0.5).mod ("ENV 2", "Osc A Pitch", 0.5);
                p.set (fxids::fshiftOn, 1).set (fxids::fshiftShift, jit (0.25, 0.4)).set (fxids::fshiftMix, 0.35);
                p.delay ("1/4 D", 0.6, 0.3).reverb (0.95, 0.4, 0.45);
                p.macros (0.0, 0.3, 0.0, 0.4).level (-11.0);
                break;
        }
    }

    struct Spec { Arch arch; const char* name; const char* traits; };

    // 110 wave + 90 neogrime. Traits are applied in order after the archetype.
    const Spec specs[] = {
        // ---- WAVE: supersaw leads (10)
        { Arch::SuperLead, "Into The Void", "wide shimmer" },
        { Arch::SuperLead, "Always Elsewhere", "lush slow" },
        { Arch::SuperLead, "Afterglow Saw", "bright detuned" },
        { Arch::SuperLead, "Frostbite Lead", "dark gritty" },
        { Arch::SuperLead, "Last Light", "airy wide" },
        { Arch::SuperLead, "Nightdrive", "warm glide" },
        { Arch::SuperLead, "Halo Drop", "octave bright" },
        { Arch::SuperLead, "Grief Engine", "dark crushed" },
        { Arch::SuperLead, "Heavenfall", "shimmer detuned" },
        { Arch::SuperLead, "Static Prayer", "gritty sweep" },
        // ---- WAVE: ethereal pads (12)
        { Arch::EtherPad, "Seraph Bloom", "lush shimmer" },
        { Arch::EtherPad, "Weightless", "airy slow" },
        { Arch::EtherPad, "Ghost In Reverb", "dark moving" },
        { Arch::EtherPad, "Soft Apocalypse", "warm wide" },
        { Arch::EtherPad, "Winter Solstice", "glass slow" },
        { Arch::EtherPad, "Tender Static", "crushed airy" },
        { Arch::EtherPad, "Cathedral Mist", "vox lush" },
        { Arch::EtherPad, "Lucid Ache", "moving hollow" },
        { Arch::EtherPad, "Pale Horizon", "bright wide" },
        { Arch::EtherPad, "Underwater Choir", "dark vox" },
        { Arch::EtherPad, "Dream Residue", "drunk lush" },
        { Arch::EtherPad, "Silver Lining", "shimmer glass" },
        // ---- WAVE: reese basses (10)
        { Arch::Reese, "Black Reese", "dark squash" },
        { Arch::Reese, "Wave Reese", "wide" },
        { Arch::Reese, "Hollow Reese", "hollow" },
        { Arch::Reese, "Abyss Reese", "dark gritty" },
        { Arch::Reese, "Teeth Reese", "gritty bright" },
        { Arch::Reese, "Fog Reese", "moving warm" },
        { Arch::Reese, "Crushed Reese", "crushed squash" },
        { Arch::Reese, "Slow Burn Reese", "slow dark" },
        { Arch::Reese, "Phantom Reese", "metallic" },
        { Arch::Reese, "Tidal Reese", "detuned wide" },
        // ---- WAVE: 808s (10)
        { Arch::Wave808, "Void 808", "" },
        { Arch::Wave808, "Grief 808", "warm" },
        { Arch::Wave808, "Slide 808", "glide" },
        { Arch::Wave808, "Blown 808", "gritty" },
        { Arch::Wave808, "Deep End 808", "dark" },
        { Arch::Wave808, "Lo-Fi 808", "crushed" },
        { Arch::Wave808, "Thump 808", "tight squash" },
        { Arch::Wave808, "Ghost 808", "dark warm" },
        { Arch::Wave808, "Long Fall 808", "slow" },
        { Arch::Wave808, "Ruined 808", "gritty crushed" },
        // ---- WAVE: vocal leads (8)
        { Arch::VoxLead, "Angel Chop", "shimmer" },
        { Arch::VoxLead, "Whisper Lead", "airy" },
        { Arch::VoxLead, "Choir Ghost", "lush dark" },
        { Arch::VoxLead, "Vowel Drift", "moving" },
        { Arch::VoxLead, "Broken Voice", "crushed" },
        { Arch::VoxLead, "Hymn Lead", "slow wide" },
        { Arch::VoxLead, "Siren Song", "glide bright" },
        { Arch::VoxLead, "Mouth Of Heaven", "octave lush" },
        // ---- WAVE: glass plucks (10)
        { Arch::GlassPluck, "Crystal Rain", "bright" },
        { Arch::GlassPluck, "Snowglobe", "shimmer" },
        { Arch::GlassPluck, "Ice Pick", "tight bright" },
        { Arch::GlassPluck, "Night Pluck", "dark" },
        { Arch::GlassPluck, "Tearjerk Pluck", "lush" },
        { Arch::GlassPluck, "Lo-Fi Glass", "crushed" },
        { Arch::GlassPluck, "Starfield", "wide shimmer" },
        { Arch::GlassPluck, "Hollow Drop", "hollow" },
        { Arch::GlassPluck, "Prism Pluck", "glass bright" },
        { Arch::GlassPluck, "Paper Wings", "airy" },
        // ---- WAVE: bells (8)
        { Arch::DarkBell, "Mourning Bell", "dark" },
        { Arch::DarkBell, "Requiem Bell", "lush" },
        { Arch::DarkBell, "Dust Bell", "crushed" },
        { Arch::DarkBell, "Moon Bell", "shimmer" },
        { Arch::DarkBell, "Iron Halo", "metallic" },
        { Arch::DarkBell, "Distant Bell", "slow wide" },
        { Arch::DarkBell, "Chapel Ghost", "hollow dark" },
        { Arch::DarkBell, "Glass Funeral", "glass" },
        // ---- WAVE: atmospheres (10)
        { Arch::Atmos, "Event Horizon", "dark" },
        { Arch::Atmos, "Static Heaven", "crushed" },
        { Arch::Atmos, "Tidepool", "moving" },
        { Arch::Atmos, "Northern Lights", "shimmer" },
        { Arch::Atmos, "Radio Ghosts", "metallic" },
        { Arch::Atmos, "Breathing Walls", "hollow" },
        { Arch::Atmos, "Cold Ascension", "bright airy" },
        { Arch::Atmos, "Sleep Paralysis", "dark drunk" },
        { Arch::Atmos, "Endless Hall", "lush" },
        { Arch::Atmos, "Vapour Trail", "wide" },
        // ---- WAVE: gated pads (8)
        { Arch::GatePad, "Strobe Heaven", "bright" },
        { Arch::GatePad, "Shutter Pad", "" },
        { Arch::GatePad, "Pulse Of Grief", "dark" },
        { Arch::GatePad, "Laser Choir", "vox" },
        { Arch::GatePad, "Flicker", "crushed" },
        { Arch::GatePad, "Heartbeat Gate", "warm" },
        { Arch::GatePad, "Glass Stutter", "glass wide" },
        { Arch::GatePad, "Rave Ghost", "detuned shimmer" },
        // ---- WAVE: arps (10)
        { Arch::WaveArp, "Falling Stars", "shimmer" },
        { Arch::WaveArp, "Night Cycle", "dark" },
        { Arch::WaveArp, "Glass Stairs", "bright" },
        { Arch::WaveArp, "Memory Loop", "crushed" },
        { Arch::WaveArp, "Halo Arp", "wide" },
        { Arch::WaveArp, "Swung Tears", "swing lush" },
        { Arch::WaveArp, "Satellite", "metallic" },
        { Arch::WaveArp, "Rainfall Arp", "airy" },
        { Arch::WaveArp, "Hollow Steps", "hollow swing" },
        { Arch::WaveArp, "Lightspeed", "octave bright" },
        // ---- WAVE: screech leads (8)
        { Arch::Screech, "Banshee", "bright" },
        { Arch::Screech, "Razor Halo", "gritty" },
        { Arch::Screech, "Wire Scream", "metallic" },
        { Arch::Screech, "Rust Lead", "crushed" },
        { Arch::Screech, "Feral Angel", "wide" },
        { Arch::Screech, "Distress Call", "glide" },
        { Arch::Screech, "Saw Wound", "dark gritty" },
        { Arch::Screech, "Overdrive Prayer", "squash" },
        // ---- WAVE: pumping chords (6)
        { Arch::PumpChord, "Pulse Of Heaven", "bright" },
        { Arch::PumpChord, "Sidechain Sorrow", "dark" },
        { Arch::PumpChord, "Breathe In", "lush" },
        { Arch::PumpChord, "Wave Chords", "wide" },
        { Arch::PumpChord, "Ducked Light", "airy" },
        { Arch::PumpChord, "Weight Of It", "warm sub" },

        // ---- NEOGRIME: eski square leads (10)
        { Arch::EskiLead, "Eskimo Ice", "" },
        { Arch::EskiLead, "Cold Square", "dark" },
        { Arch::EskiLead, "Frozen Wire", "metallic" },
        { Arch::EskiLead, "Tower Block", "hollow" },
        { Arch::EskiLead, "Pirate Radio", "crushed" },
        { Arch::EskiLead, "Square Ghost", "lush" },
        { Arch::EskiLead, "Glacier Lead", "bright" },
        { Arch::EskiLead, "Slide Square", "glide" },
        { Arch::EskiLead, "Sub Zero", "dark tight" },
        { Arch::EskiLead, "Neon Eski", "wide shimmer" },
        // ---- NEOGRIME: subs (8)
        { Arch::GrimeSub, "Pure Sub", "" },
        { Arch::GrimeSub, "Warm Sub", "warm" },
        { Arch::GrimeSub, "Slide Sub", "glide" },
        { Arch::GrimeSub, "Pressure Sub", "squash" },
        { Arch::GrimeSub, "Grit Sub", "gritty" },
        { Arch::GrimeSub, "Tight Sub", "tight" },
        { Arch::GrimeSub, "Long Sub", "slow" },
        { Arch::GrimeSub, "Deep Pressure", "warm squash" },
        // ---- NEOGRIME: growl basses (8)
        { Arch::GrowlBass, "Concrete Growl", "" },
        { Arch::GrowlBass, "Estate Bass", "dark" },
        { Arch::GrowlBass, "Bark Bass", "bright tight" },
        { Arch::GrowlBass, "Rust Bass", "crushed" },
        { Arch::GrowlBass, "Chrome Growl", "metallic" },
        { Arch::GrowlBass, "Wobble Block", "moving" },
        { Arch::GrowlBass, "Hollow Growl", "hollow" },
        { Arch::GrowlBass, "Pressure Growl", "gritty" },
        // ---- NEOGRIME: ice bells (10)
        { Arch::IceBell, "Ice Bell", "" },
        { Arch::IceBell, "Frost Chime", "bright" },
        { Arch::IceBell, "Steel Tear", "dark" },
        { Arch::IceBell, "Tin Halo", "crushed" },
        { Arch::IceBell, "Cold Glass", "shimmer" },
        { Arch::IceBell, "Night Bus Bell", "hollow" },
        { Arch::IceBell, "Shard Bell", "tight" },
        { Arch::IceBell, "Silver Grime", "wide" },
        { Arch::IceBell, "Bleak Bell", "dark slow" },
        { Arch::IceBell, "Crystal Grime", "glass" },
        // ---- NEOGRIME: stabs (8)
        { Arch::ColdStab, "Cold Stab", "" },
        { Arch::ColdStab, "Grey Stab", "dark" },
        { Arch::ColdStab, "Organ Hit", "hollow" },
        { Arch::ColdStab, "Rave Stab", "bright detuned" },
        { Arch::ColdStab, "Broken Stab", "crushed" },
        { Arch::ColdStab, "Chord Shard", "metallic" },
        { Arch::ColdStab, "Gloom Stab", "lush dark" },
        { Arch::ColdStab, "Steel Chord", "gritty" },
        // ---- NEOGRIME: metal plucks (8)
        { Arch::MetalPluck, "Clang Pluck", "" },
        { Arch::MetalPluck, "Pipe Pluck", "hollow" },
        { Arch::MetalPluck, "Tuned Metal", "bright" },
        { Arch::MetalPluck, "Rust Pluck", "crushed" },
        { Arch::MetalPluck, "Shank", "tight" },
        { Arch::MetalPluck, "Cage Pluck", "metallic" },
        { Arch::MetalPluck, "Chain Link", "dark" },
        { Arch::MetalPluck, "Wire Harp", "lush" },
        // ---- NEOGRIME: crushed leads (8)
        { Arch::CrushLead, "8-Bit Ghost", "" },
        { Arch::CrushLead, "Game Over", "bright" },
        { Arch::CrushLead, "Pixel Grime", "dark" },
        { Arch::CrushLead, "Cartridge", "hollow" },
        { Arch::CrushLead, "Low Battery", "drunk" },
        { Arch::CrushLead, "Chip Siren", "glide" },
        { Arch::CrushLead, "Dead Pixel", "metallic" },
        { Arch::CrushLead, "Arcade Smoke", "lush" },
        // ---- NEOGRIME: 808s (8)
        { Arch::Grime808, "Grime 808", "" },
        { Arch::Grime808, "Knock 808", "tight" },
        { Arch::Grime808, "Gravel 808", "gritty" },
        { Arch::Grime808, "Glide 808", "glide" },
        { Arch::Grime808, "Concrete 808", "squash" },
        { Arch::Grime808, "Tape 808", "crushed" },
        { Arch::Grime808, "Dark 808", "dark" },
        { Arch::Grime808, "Punch 808", "warm tight" },
        // ---- NEOGRIME: pads (8)
        { Arch::VoidPad, "Grey Skies", "" },
        { Arch::VoidPad, "Estate Fog", "dark" },
        { Arch::VoidPad, "Underpass", "hollow" },
        { Arch::VoidPad, "Bleak House", "moving" },
        { Arch::VoidPad, "Night Shift", "crushed" },
        { Arch::VoidPad, "Concrete Choir", "vox" },
        { Arch::VoidPad, "Cold Comfort", "lush" },
        { Arch::VoidPad, "Last Train", "shimmer" },
        // ---- NEOGRIME: arps (8)
        { Arch::GrimeArp, "Tick Tock", "" },
        { Arch::GrimeArp, "Ice Steps", "bright" },
        { Arch::GrimeArp, "Square Runner", "swing" },
        { Arch::GrimeArp, "Pixel Arp", "crushed" },
        { Arch::GrimeArp, "Metal Arp", "metallic" },
        { Arch::GrimeArp, "Night Arp", "dark" },
        { Arch::GrimeArp, "Hollow Arp", "hollow swing" },
        { Arch::GrimeArp, "Frozen Arp", "shimmer" },
        // ---- NEOGRIME: FX (6)
        { Arch::Siren, "Pirate Siren", "" },
        { Arch::Siren, "Riser From Below", "dark" },
        { Arch::Siren, "Air Raid", "bright" },
        { Arch::Siren, "Static Riser", "crushed" },
        { Arch::Siren, "Metal Siren", "metallic" },
        { Arch::Siren, "Ascension FX", "shimmer" },
    };

    double levelTrim (const juce::String& name)
    {
        static const std::map<juce::String, double> trims {
           #include "FactoryPresetsWaveLevels.inc"
        };
        auto it = trims.find (name);
        return it != trims.end() ? it->second : 0.0;
    }

    const Family& familyFor (Arch a)
    {
        for (auto& f : families) if (f.arch == a) return f;
        return families[0];
    }
}

//==============================================================================
void addWaveAndGrimePresets (std::vector<FactoryPreset>& list)
{
    for (const auto& s : specs)
    {
        const auto& fam = familyFor (s.arch);
        Patch p;
        juce::Random r ((juce::int64) juce::String (s.name).hashCode64());
        buildArchetype (p, s.arch, r);

        const auto traits = juce::StringArray::fromTokens (s.traits, " ", "");
        juce::StringArray tags { fam.style }, phrases;
        for (auto& t : traits)
        {
            if (t.isEmpty()) continue;
            applyTrait (p, t);
            if (auto it = traitTable().find (t); it != traitTable().end())
            {
                tags.add (it->second.tag);
                phrases.add (it->second.phrase);
            }
        }

        // loudness match across the library (measured, see FactoryPresetsWaveLevels.inc)
        p.set (ids::master, juce::jlimit (-60.0, 6.0, p.get (ids::master, -6.0) + levelTrim (s.name)));

        FactoryPreset fp;
        fp.name = s.name;
        fp.category = fam.category;
        fp.tags = tags.joinIntoString (", ");
        fp.description = juce::String (fam.base);
        if (! phrases.isEmpty())
        {
            auto line = phrases.joinIntoString (", ");
            fp.description << " This one: " << line << ".";
        }
        for (auto& id : p.order)
        {
            const auto& v = p.v[id];
            if (v.isString()) fp.choice (id, v.toString().toRawUTF8());
            else fp.set (id, (double) v);
        }
        list.push_back (std::move (fp));
    }
}

} // namespace divinity
