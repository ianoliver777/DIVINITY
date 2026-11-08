// HOLY DIRT — DIVINITY expansion pack generator (TRIPLE SEVEN AUDIO).
//
//   60 presets:  HATS 20 (12 one-shots + 8 hold-a-key loops)  .  PERC 10  .  DISTORTED LEADS 30
//
//   DivinityPackMaker <outDir> [--wavs]
//
// Builds every preset on the real engine, renders a test phrase, trims the master
// volume so the whole pack sits at one loudness, flags silent / clipping patches,
// then writes <outDir>/HOLY DIRT/<group>/<name>.divpreset (+ <outDir>/previews/*.wav).
// Drop the "HOLY DIRT" folder into Documents/TRIPLE SEVEN AUDIO/DIVINITY/Presets.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "DSP/FxCatalog.h"
#include <map>
#include <cstring>
#include <fstream>
#include <cstdio>

using namespace divinity;

namespace
{
constexpr double kSr = 48000.0;
const char* kPack = "HOLY DIRT";

//==============================================================================
struct P
{
    std::map<juce::String, juce::var> v;
    std::vector<juce::String> order;
    juce::String name, category, group, tags, desc;
    enum Kind { OneShot, Loop, Lead, PolyLead } kind = OneShot;

    P& set (const juce::String& id, double x) { if (! v.count (id)) order.push_back (id); v[id] = x; return *this; }
    P& choice (const juce::String& id, const char* n) { if (! v.count (id)) order.push_back (id); v[id] = juce::String (n); return *this; }
    bool has (const juce::String& id) const { return v.count (id) > 0; }

    // sources
    P& oscOff (int o) { return set (ids::osc (o, "on"), 0); }
    P& wt (int o, const char* t, double pos, double lvl = 0.8)
    { return set (ids::osc (o, "on"), 1).choice (ids::osc (o, "engine"), "Wavetable").choice (ids::osc (o, "table"), t).set (ids::osc (o, "pos"), pos).set (ids::osc (o, "level"), lvl); }
    P& an (int o, const char* w, double lvl = 0.8)
    { return set (ids::osc (o, "on"), 1).choice (ids::osc (o, "engine"), "Analog").choice (ids::osc (o, "wave"), w).set (ids::osc (o, "level"), lvl); }
    P& uni (int o, int n, double det, double wid) { return set (ids::osc (o, "unison"), n).set (ids::osc (o, "detune"), det).set (ids::osc (o, "width"), wid); }
    P& tune (int o, int oct, int semi = 0, double fine = 0) { return set (ids::osc (o, "oct"), oct).set (ids::osc (o, "semi"), semi).set (ids::osc (o, "fine"), fine); }
    P& fold (int o, double f) { return set (ids::osc (o, "fold"), f); }
    P& warp (int o, const char* m, double amt) { return choice (ids::osc (o, "warp"), m).set (ids::osc (o, "warp_amt"), amt); }
    P& tight (int o) { return set (ids::osc (o, "randphase"), 0).set (ids::osc (o, "phase"), 0.25); }
    P& sub (const char* m, double l) { return set (ids::subOn, 1).choice (ids::subMode, m).set (ids::subLevel, l); }
    P& noise (const char* t, double l) { return set (ids::noiseOn, 1).choice (ids::noiseType, t).set (ids::noiseLevel, l); }
    P& smp (const char* s, double lvl, bool keytrack, int tuneSemis = 0, double start = 0.0)
    { return set (ids::smpOn, 1).choice (ids::smpSample, s).set (ids::smpLevel, lvl).set (ids::smpKeytrack, keytrack ? 1 : 0).set (ids::smpTune, tuneSemis).set (ids::smpStart, start); }

    // filter / env / mod
    P& filt (int f, const char* t, double cut, double res, double drive = 0, double key = 0)
    { return choice (ids::filt (f, "type"), t).set (ids::filt (f, "cutoff"), cut).set (ids::filt (f, "res"), res).set (ids::filt (f, "drive"), drive).set (ids::filt (f, "keytrack"), key).set (ids::filt (f, "mix"), 1.0); }
    P& env (int e, double a, double d, double s, double r, double curve = 0)
    { return set (ids::env (e, "attack"), a).set (ids::env (e, "decay"), d).set (ids::env (e, "sustain"), s).set (ids::env (e, "release"), r).set (ids::env (e, "curve"), curve); }
    P& lfo (int l, const char* shape, double hz, double smooth = 0)
    { return set (ids::lfo (l, "on"), 1).choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "sync"), 0).set (ids::lfo (l, "rate"), hz).set (ids::lfo (l, "depth"), 1.0).set (ids::lfo (l, "smooth"), smooth); }
    P& lfoSync (int l, const char* shape, const char* div, double smooth = 0)
    { return set (ids::lfo (l, "on"), 1).choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "sync"), 1).choice (ids::lfo (l, "div"), div).set (ids::lfo (l, "depth"), 1.0).set (ids::lfo (l, "smooth"), smooth); }
    P& mod (const char* src, const char* dst, double amt)
    {
        for (int s = 0; s < kNumModSlots; ++s)
            if (! has (ids::mod (s, "src"))) return choice (ids::mod (s, "src"), src).choice (ids::mod (s, "dst"), dst).set (ids::mod (s, "amt"), amt);
        jassertfalse; return *this;
    }
    P& mono (double glideMs) { return choice (ids::voiceMode, "Legato").set (ids::glide, glideMs); }
    P& monoRetrig() { return choice (ids::voiceMode, "Mono").set (ids::glide, 0); }
    P& poly (int n) { return choice (ids::voiceMode, "Poly").set (ids::voices, n); }

    // effects
    P& sat (double d, double mix = 1) { return set (ids::satOn, 1).set (ids::satDrive, d).set (ids::satMix, mix); }
    P& noSat() { return set (ids::satOn, 0); }
    P& chorus (double rate, double depth, double mix) { return set (ids::choOn, 1).set (ids::choRate, rate).set (ids::choDepth, depth).set (ids::choMix, mix); }
    P& delay (const char* div, double fb, double mix, bool ping = true, double tone = 0.5)
    { return set (ids::dlyOn, 1).choice (ids::dlyDiv, div).set (ids::dlyFeedback, fb).set (ids::dlyMix, mix).set (ids::dlyPingPong, ping ? 1 : 0).set (ids::dlyTone, tone); }
    P& reverb (double size, double damp, double mix, double pre = 20, double width = 1)
    { return set (ids::revOn, 1).set (ids::revSize, size).set (ids::revDamp, damp).set (ids::revMix, mix).set (ids::revPredelay, pre).set (ids::revWidth, width); }
    P& dist (const char* type, double drive, double mix, double tone = 0.55)
    { return set (fxids::distOn, 1).choice (fxids::distType, type).set (fxids::distDrive, drive).set (fxids::distMix, mix).set (fxids::distTone, tone); }
    P& crush (double bits, double down, double mix) { return set (fxids::crushOn, 1).set (fxids::crushBits, bits).set (fxids::crushDown, down).set (fxids::crushMix, mix); }
    P& eq (double lo, double midF, double midG, double hi, double loF = 120, double hiF = 6000)
    { return set (fxids::eqOn, 1).set (fxids::eqLowGain, lo).set (fxids::eqMidFreq, midF).set (fxids::eqMidGain, midG).set (fxids::eqHighGain, hi).set (fxids::eqLowFreq, loF).set (fxids::eqHighFreq, hiF); }
    P& comp (double th, double ratio, double makeup, double att = 5, double rel = 80)
    { return set (fxids::compOn, 1).set (fxids::compThresh, th).set (fxids::compRatio, ratio).set (fxids::compMakeup, makeup).set (fxids::compAttack, att).set (fxids::compRelease, rel).set (fxids::compMix, 1.0); }
    P& ott (double depth, double time = 0.35, double out = 0) { return set (fxids::mbOn, 1).set (fxids::mbDepth, depth).set (fxids::mbTime, time).set (fxids::mbOut, out); }
    P& trans (double a, double s) { return set (fxids::transOn, 1).set (fxids::transAttack, a).set (fxids::transSustain, s); }
    P& phaser (double rate, double depth, double centre, double fb, double mix)
    { return set (fxids::phaseOn, 1).set (fxids::phaseRate, rate).set (fxids::phaseDepth, depth).set (fxids::phaseCentre, centre).set (fxids::phaseFb, fb).set (fxids::phaseMix, mix); }
    P& flanger (double rate, double depth, double fb, double mix) { return set (fxids::flangOn, 1).set (fxids::flangRate, rate).set (fxids::flangDepth, depth).set (fxids::flangFb, fb).set (fxids::flangMix, mix); }
    P& ring (double hz, double mix) { return set (fxids::ringOn, 1).set (fxids::ringFreq, hz).set (fxids::ringMix, mix); }
    P& fshift (double s, double mix) { return set (fxids::fshiftOn, 1).set (fxids::fshiftShift, s).set (fxids::fshiftMix, mix); }
    P& stereo (double w, double monoHz = 150) { return set (fxids::stereoOn, 1).set (fxids::stereoWidth, w).set (fxids::stereoMonoBass, 1).set (fxids::stereoMonoFreq, monoHz); }

    /** Step sequencer in TRIGGER mode (hold a key, it plays). lvl '0'-'9', rat '1'-'4', gate s/m/l. */
    P& steps (const char* lvl, const char* rat, const char* gate, std::initializer_list<int> pitch, const char* rate, int len, double swing)
    {
        set (ids::stepOn, 1).choice (ids::stepMode, "Trigger").choice (ids::stepRate, rate).set (ids::stepLength, len).set (ids::stepSwing, swing);
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

std::vector<P> pack;

P& add (const char* group, const char* category, P::Kind kind, const char* name, const juce::String& tags, const char* desc)
{
    pack.emplace_back();
    auto& p = pack.back();
    p.group = group; p.category = category; p.kind = kind; p.name = name; p.tags = juce::String (kPack) + ", Trap, " + tags; p.desc = desc;
    return p;
}

//==============================================================================
// Shared starting points
P& hat (const char* name, const char* tags, const char* desc, double decay)
{
    auto& p = add ("01 HATS", "PERCUSSION", P::OneShot, name, tags, desc);
    p.oscOff (0).poly (16).set (ids::analog, 0).filt (0, "Off", 15000, 0).noSat()
     .env (0, 0.0005, decay, 0.0, juce::jmin (0.2, decay * 0.8), -0.4).stereo (1.15, 200);
    return p;
}

P& hatLoop (const char* name, const char* tags, const char* desc, const char* sample)
{
    auto& p = add ("01 HATS", "PERCUSSION", P::Loop, name, tags + juce::String (", Loop, Hold Key"), desc);
    p.oscOff (0).poly (16).set (ids::analog, 0).filt (0, "Off", 15000, 0).noSat()
     .smp (sample, 0.85, false).env (0, 0.0005, 0.07, 0.0, 0.05, -0.4).stereo (1.15, 200);
    return p;
}

P& perc (const char* name, const char* tags, const char* desc)
{
    auto& p = add ("02 PERC", "PERCUSSION", P::OneShot, name, tags, desc);
    p.oscOff (0).poly (16).set (ids::analog, 0).filt (0, "Off", 15000, 0).noSat()
     .env (0, 0.0005, 0.4, 0.0, 0.25, -0.3).stereo (1.1, 180);
    return p;
}

/** Mono distorted lead: legato glide, delayed-feel vibrato on aftertouch, mod wheel pushes the dirt. */
P& lead (const char* name, const char* tags, const char* desc, double glideMs = 70)
{
    auto& p = add ("03 DISTORTED LEADS", "LEAD", P::Lead, name, tags + juce::String (", Distorted, Mono"), desc);
    p.mono (glideMs).set (ids::analog, 0.15).env (0, 0.003, 0.5, 0.85, 0.28)
     .lfo (1, "Sine", 5.2, 0.2).mod ("Aftertouch", "Saturation", 0.35).mod ("LFO 2", "Osc A Pitch", 0.012)
     .stereo (1.3, 140);
    return p;
}

P& polyLead (const char* name, const char* tags, const char* desc)
{
    auto& p = add ("03 DISTORTED LEADS", "LEAD", P::PolyLead, name, tags + juce::String (", Distorted, Poly"), desc);
    p.poly (8).set (ids::analog, 0.15).env (0, 0.003, 0.5, 0.8, 0.3).stereo (1.35, 140);
    return p;
}

//==============================================================================
void buildPack()
{
    // ---------------- HATS: one-shots (play your own rolls) ----------------
    hat ("Halo Tick Hat", "Hats, Closed, Crisp", "Tight closed hat, high-passed and snapped forward with a transient boost. The everyday trap hat.", 0.06)
        .smp ("Hat Closed", 0.85, false).filt (0, "High Pass 12", 6500, 0.1).trans (0.45, -0.25).eq (0, 3000, 0, 3.0);

    hat ("Glass Needle Hat", "Hats, Closed, Metallic", "Metal hat with a Metal FM layer three octaves up: thin, glassy, cuts through a dense beat.", 0.05)
        .smp ("Hat Metal", 0.75, false, 5).wt (0, "Metal FM", 0.6, 0.18).tune (0, 3).filt (0, "High Pass 24", 6000, 0.15);

    hat ("Dusty 808 Hat", "Hats, Closed, 808, Lo-Fi", "Classic 808-style hat made from white noise through a resonant high-pass, then crushed like an old sampler.", 0.045)
        .noise ("White", 0.9).filt (0, "High Pass 12", 8000, 0.35).crush (10, 3, 0.4).stereo (0.6, 200);

    hat ("Bitter Closed Hat", "Hats, Closed, Dirty", "Closed hat pushed into hard clipping: aggressive, bright and a bit rude.", 0.08)
        .smp ("Hat Closed", 0.8, false).filt (0, "High Pass 12", 700, 0.1).dist ("Hard Clip", 0.5, 0.35, 0.65);

    hat ("Witch Hat", "Hats, Closed, Wet, Witch House", "Closed hat with a ghostly triplet echo and a small dark room behind it. Witch house hats.", 0.06)
        .smp ("Hat Closed", 0.8, false).filt (0, "High Pass 12", 4000, 0.1).delay ("1/16 T", 0.3, 0.14, true, 0.35).reverb (0.35, 0.6, 0.14, 8);

    hat ("Crystal Shaker", "Shaker, Bright", "Shaker with bright noise on top, high-passed for air. Sits between the hats.", 0.09)
        .smp ("Shaker", 0.8, false).noise ("Bright", 0.12).filt (0, "High Pass 12", 2500, 0.1).eq (0, 3000, 0, 2.5);

    hat ("Velvet Hat", "Hats, Closed, Soft", "Softer closed hat: low-passed top and tape warmth. Good for wave and slower 808 beats.", 0.09)
        .smp ("Hat Closed", 0.85, false).filt (0, "Low Pass 12", 8500, 0.1).sat (0.35).eq (0, 900, 0, -2.0);

    hat ("Snapback Open Hat", "Hats, Open, Choke", "Open hat in mono mode: the next hit chokes the last one, like a real hi-hat.", 0.35)
        .smp ("Hat Open", 0.75, false).monoRetrig().env (0, 0.0005, 0.35, 0.0, 0.18, -0.3).filt (0, "High Pass 12", 1500, 0.1);

    hat ("Sizzle Open Hat", "Hats, Open, Bright, OTT", "Open hat with a bright noise sizzle, high-passed and squashed with OTT. Chokes on retrigger.", 0.5)
        .smp ("Hat Open", 0.7, false).noise ("Bright", 0.18).monoRetrig().env (0, 0.0005, 0.5, 0.0, 0.25, -0.2)
        .filt (0, "High Pass 24", 5000, 0.1).ott (0.5);

    hat ("Rattle Metal Hat", "Hats, Closed, Metallic, Ring", "Metal hat with ring modulation: clangy, inharmonic rattle.", 0.07)
        .smp ("Hat Metal", 0.8, false).filt (0, "High Pass 12", 3000, 0.1).ring (3200, 0.25);

    hat ("Tuned Roll Hat", "Hats, Closed, Pitched, Rolls", "Key-tracked closed hat: play up the keyboard for pitch-ramp rolls. A hint of random pitch keeps repeats human.", 0.05)
        .smp ("Hat Closed", 0.85, true).filt (0, "High Pass 12", 5000, 0.1).mod ("Random", "Sampler Pitch", 0.012).trans (0.3, -0.2);

    hat ("Phantom Hat", "Hats, Closed, Wide, Moving", "Closed hat that drifts around the stereo field hit to hit. Wide but still crisp.", 0.06)
        .smp ("Hat Closed", 0.85, false).filt (0, "High Pass 12", 5000, 0.1).set (ids::rndRate, 9.0).set (ids::rndSmooth, 0.0)
        .mod ("Random", "Pan", 0.45).stereo (1.45, 200);

    // ---------------- HATS: loops (hold a key; tempo-synced) ----------------
    hatLoop ("Trap Roll Loop", "Hats, Rolls, Sixteenths", "Hold a key: sixteenth hats with 1/32 and 1/64 rolls on the last beat. Velocity shapes the accents.", "Hat Closed")
        .filt (0, "High Pass 12", 5000, 0.1)
        .steps ("9575957595759575", "1111121111113141", "s", {}, "1/16", 16, 0.0);

    hatLoop ("Triplet Rain Loop", "Hats, Triplets", "Hold a key: sixteenth-triplet hats with ratchet bursts. That classic triplet flow.", "Hat Closed")
        .filt (0, "High Pass 12", 5500, 0.1)
        .steps ("957957957957", "111121111131", "s", {}, "1/16 T", 12, 0.0);

    hatLoop ("Drill Skitter Loop", "Hats, Drill, Skitter", "Hold a key: a UK-drill skitter with gaps and quick triplet ratchets.", "Hat Metal")
        .filt (0, "High Pass 12", 4500, 0.1)
        .steps ("9070607090706070", "1113111211131114", "s", {}, "1/16", 16, 0.0);

    hatLoop ("Pitch Ramp Roll", "Hats, Rolls, Pitched", "Hold a key: a one-bar hat roll that climbs in pitch into the next bar.", "Hat Closed")
        .set (ids::smpKeytrack, 1).filt (0, "High Pass 12", 4000, 0.1)
        .steps ("9576957695769999", "1111111111112233", "s", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 7, 12 }, "1/16", 16, 0.0);

    hatLoop ("Half-Time Bounce Loop", "Hats, Half-Time, Swing", "Hold a key: a lazy half-time bounce with a swung pocket.", "Hat Closed")
        .filt (0, "High Pass 12", 5000, 0.1)
        .steps ("9056905690569076", "1111111111111121", "s", {}, "1/16", 16, 0.22);

    hatLoop ("Stutter Gate Loop", "Hats, Stutter, Lo-Fi", "Hold a key: stuttering hat bursts with gaps, bit-crushed.", "Hat Closed")
        .filt (0, "High Pass 12", 4000, 0.1).crush (8, 4, 0.45)
        .steps ("9900990099009990", "1111111111111122", "s", {}, "1/16", 16, 0.0);

    hatLoop ("Swing Hats Loop", "Hats, Swing", "Hold a key: swung sixteenths with soft ghost notes. Smooth and laid back.", "Hat Closed")
        .filt (0, "Low Pass 12", 11000, 0.1)
        .steps ("9474947494749474", "1", "s", {}, "1/16", 16, 0.38);

    hatLoop ("Open Close Loop", "Hats, Open, Gate", "Hold a key: the step gate opens and closes the hat, so one pattern plays closed hats and open hats.", "Hat Open")
        .env (0, 0.0005, 0.6, 0.55, 0.03, -0.2).filt (0, "High Pass 12", 2500, 0.1)
        .steps ("9575957595759575", "1", "ssslsssmssslsssl", {}, "1/16", 16, 0.0);

    // ---------------- PERC ----------------
    perc ("Ghost Rim", "Rim, Wet, Dark", "Rimshot with a snapped attack and a dark, short room. Plays well off the grid.")
        .smp ("Rim", 0.85, false).trans (0.35, -0.15).reverb (0.45, 0.7, 0.18, 10).filt (0, "Low Pass 12", 9000, 0.1);

    perc ("Cathedral Clap", "Clap, Reverb, Big", "Clap thrown into a huge predelayed hall: the witch house snare.")
        .smp ("Clap", 0.85, false).env (0, 0.0005, 0.5, 0.0, 0.4).reverb (0.88, 0.45, 0.32, 35).eq (-2, 1200, 1.5, 1.0);

    perc ("Snap Halo", "Snap, Shimmer", "Finger snap with a frequency-shifted halo in the reverb tail.")
        .smp ("Snap", 0.85, false).fshift (0.12, 0.15).reverb (0.7, 0.4, 0.22, 20);

    perc ("Grave Tick", "Tick, Pitched, Dark", "Perc tick pitched down five semitones and key-tracked: a dark, woody click to play melodies with.")
        .smp ("Perc Tick", 0.85, true, -5).filt (0, "Low Pass 12", 5000, 0.2).sat (0.3);

    perc ("Chain Hit", "Metal, Dirty, Echo", "Metal hit rectified into grit with a triplet echo trailing off.")
        .smp ("Metal Hit", 0.75, false).dist ("Rectify", 0.5, 0.4, 0.5).delay ("1/8 T", 0.35, 0.18, true, 0.4);

    perc ("Cowbell 808", "Cowbell, 808, Synth", "The 808 cowbell, built the original way: two square waves a fifth apart through a band-pass.")
        .an (0, "Square", 0.6).an (1, "Square", 0.6).tune (1, 0, 7).tight (0).tight (1)
        .filt (0, "Band Pass", 900, 0.35).env (0, 0.0005, 0.22, 0.0, 0.15, -0.6).sat (0.25);

    perc ("Zap Perc", "Zap, Synth, Laser", "A sine laser zap: the pitch drops a full octave in a blink.")
        .an (0, "Sine", 0.85).tune (0, 2).tight (0).env (0, 0.0005, 0.12, 0.0, 0.08, -0.5).env (1, 0.0005, 0.06, 0.0, 0.05)
        .mod ("ENV 2", "Osc A Pitch", 1.0).sat (0.3);

    perc ("Vox Chop Perc", "Vocal, Chop, Pitched", "Short vocal chop, key-tracked, with a dotted echo. Play little vocal riffs between the hats.")
        .smp ("Vox Chop Ee", 0.8, true).env (0, 0.0005, 0.16, 0.0, 0.08).delay ("1/8 D", 0.32, 0.18, true, 0.45);

    perc ("Bell Strike Perc", "Bell, Pitched, Wet", "Key-tracked bell strike with a long, bright tail. For trap bell melodies.")
        .smp ("Bell Strike", 0.75, true).env (0, 0.0005, 1.6, 0.0, 0.9).reverb (0.75, 0.3, 0.2, 25);

    { auto& p = add ("02 PERC", "PERCUSSION", P::Loop, "Shaker Loop", "Shaker, Triplets, Swing, Loop, Hold Key",
                     "Hold a key: a swung triplet shaker groove with soft ghost notes.");
      p.oscOff (0).poly (16).set (ids::analog, 0).filt (0, "High Pass 12", 1800, 0.1).noSat()
       .smp ("Shaker", 0.85, false).env (0, 0.0005, 0.08, 0.0, 0.05).stereo (1.2, 200)
       .steps ("947947947947", "1", "s", {}, "1/16 T", 12, 0.2); }

    // ---------------- DISTORTED LEADS ----------------
    lead ("Burnt Halo", "Supersaw, Hard Clip", "Five-voice supersaw clipped hard, through a dark low-pass, with a dotted-eighth echo. Mod wheel pushes the drive.")
        .an (0, "Saw", 0.75).uni (0, 5, 0.3, 0.8).filt (0, "Low Pass 24", 4500, 0.2, 0.2).dist ("Hard Clip", 0.6, 0.55)
        .delay ("1/8 D", 0.35, 0.18).reverb (0.6, 0.5, 0.15).mod ("Mod Wheel", "Filter 1 Drive", 0.6);

    lead ("Screaming Angel", "Sync, Tube, Scream", "Laser-sync table that screams open on every note, through tube distortion. Long glide.", 110)
        .wt (0, "Laser Sync", 0.3).uni (0, 2, 0.08, 0.5).env (1, 0.001, 0.45, 0.3, 0.3).mod ("ENV 2", "Osc A WT Pos", 0.55)
        .dist ("Tube", 0.7, 0.6, 0.6).delay ("1/8 D", 0.3, 0.15).mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    lead ("Fold Prophet", "Fold, Sine Fold", "Sine wavefolded until it bites. Mod wheel folds it further; sine-fold distortion on top.")
        .an (0, "Sine", 0.85).fold (0, 0.55).tight (0).sub ("Sine -1", 0.25).dist ("Sine Fold", 0.4, 0.45)
        .mod ("Mod Wheel", "Osc A Fold", 0.4).reverb (0.5, 0.5, 0.12);

    lead ("Rectified Saint", "Saw, Rectify, Octave", "Saw through a rectifier: octave-up grit on top of the note. Thin and nasty in the best way.")
        .an (0, "Saw", 0.8).uni (0, 2, 0.12, 0.25).filt (0, "High Pass 12", 220, 0.1).dist ("Rectify", 0.6, 0.55).stereo (1.0, 140)
        .delay ("1/4", 0.3, 0.14).mod ("Mod Wheel", "Saturation", 0.5);

    lead ("Crushed Seraph", "Square, Bitcrush, Lo-Fi", "Hollow square bit-crushed to six bits and downsampled. Video-game demon.")
        .an (0, "Square", 0.75).uni (0, 2, 0.06, 0.4).crush (6, 8, 0.6).sat (0.35).filt (0, "Low Pass 12", 7000, 0.2)
        .mod ("Mod Wheel", "Filter 1 Cutoff", -0.4);

    lead ("Tube Scream Lead", "Saw, Tube, Resonant", "Tube Saw table through a resonant driven ladder into tube distortion. Classic overdriven lead.")
        .wt (0, "Tube Saw", 0.5).uni (0, 3, 0.15, 0.6).filt (0, "MG Low 24", 3000, 0.35, 0.5, 0.4).dist ("Tube", 0.8, 0.6, 0.45)
        .env (1, 0.002, 0.35, 0.4, 0.3).mod ("ENV 2", "Filter 1 Cutoff", 0.3).mod ("Mod Wheel", "Filter 1 Cutoff", 0.35);

    lead ("Neuro Lead", "Neuro, Growl, OTT", "Neuro Growl table swept by an envelope, clipped and OTT'd. Aggressive and talky.")
        .wt (0, "Neuro Growl", 0.3).env (1, 0.002, 0.6, 0.2, 0.3).mod ("ENV 2", "Osc A WT Pos", 0.5)
        .dist ("Hard Clip", 0.4, 0.4).ott (0.6).mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    lead ("Feedback Prayer", "FM, Feedback, Chorus", "Feedback-FM snarl softened by cubic distortion and a wide chorus.")
        .wt (0, "Feedback FM", 0.45).dist ("Soft Cubic", 0.7, 0.6).chorus (0.5, 0.5, 0.3).reverb (0.6, 0.45, 0.16)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.45);

    lead ("Digital Sin", "Digital, Bitcrush, Echo", "Digital Grit table, crushed further, with a ping-pong sixteenth echo.")
        .wt (0, "Digital Grit", 0.45).crush (8, 3, 0.45).delay ("1/16", 0.4, 0.15).filt (0, "Low Pass 12", 8000, 0.15)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    lead ("Diode Choir", "Choir, Vocal, Witch House", "Choir formant table with a diode-clipped layer under it, tube-driven into a big dark hall. Witch house lead.", 90)
        .wt (0, "Choir Formant", 0.4, 0.75).uni (0, 3, 0.12, 0.7).wt (1, "Diode Clip", 0.5, 0.35)
        .dist ("Tube", 0.55, 0.45).reverb (0.82, 0.6, 0.25, 40).mod ("Mod Wheel", "Osc A WT Pos", 0.5);

    lead ("Siren 777", "Sync, Siren, Glide", "Hard-synced square with a huge glide: slide between notes for sirens. Hard-clipped.", 260)
        .wt (0, "Sync Square", 0.45).dist ("Hard Clip", 0.55, 0.5).delay ("1/8 D", 0.4, 0.18)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.45);

    lead ("Blown Speaker Lead", "Saw, Hard Clip, Loud", "Saw and sub through a nearly maxed hard clipper with a mid push. Sounds like a blown car speaker.")
        .an (0, "Saw", 0.8).sub ("Square -1", 0.3).filt (0, "High Pass 12", 150, 0.1)
        .dist ("Hard Clip", 0.9, 0.6, 0.5).eq (0, 1500, 5.0, -2.0).mod ("Mod Wheel", "Saturation", 0.5);

    lead ("Ghost Whistle", "Sync, Whistle, Shimmer", "Synced-sine whistle, softly distorted, with a shimmering frequency-shifted tail.", 100)
        .wt (0, "Sync Sine", 0.4).dist ("Soft Cubic", 0.5, 0.5).fshift (0.1, 0.12).reverb (0.8, 0.4, 0.25, 50)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    polyLead ("Overdrive Bell", "Bell, FM, Tube, Pluck", "Bell FM through tube overdrive with a plucky decay. Distorted trap bells.")
        .wt (0, "Bell FM", 0.4).env (0, 0.001, 0.7, 0.25, 0.4).dist ("Tube", 0.5, 0.5).reverb (0.6, 0.4, 0.16)
        .mod ("Mod Wheel", "Saturation", 0.4);

    polyLead ("Chainsaw Pluck", "Saw, Pluck, Hard Clip", "Saw stack with a snappy filter pluck, hard-clipped. Plays chords and riffs.")
        .wt (0, "Saw Stack", 0.5).filt (0, "Low Pass 24", 900, 0.3, 0.3, 0.3).env (0, 0.001, 0.35, 0.2, 0.25)
        .env (1, 0.001, 0.25, 0.0, 0.2).mod ("ENV 2", "Filter 1 Cutoff", 0.6).dist ("Hard Clip", 0.55, 0.5)
        .delay ("1/8 D", 0.3, 0.14).mod ("Mod Wheel", "Filter 1 Cutoff", 0.35);

    lead ("Lo-Fi Arcade Lead", "8-Bit, Chip, Bitcrush", "Arcade Steps table crushed to six bits: a broken console lead.")
        .wt (0, "Arcade Steps", 0.5).crush (6, 5, 0.5).delay ("1/16 D", 0.3, 0.12).mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    lead ("Talk Box Riot", "Talk Box, Vocal, Rhythmic", "Talk-box table mouthing eighth notes, tube-driven.")
        .wt (0, "Talk Box", 0.3).lfoSync (0, "Triangle", "1/8").mod ("LFO 1", "Osc A WT Pos", 0.35)
        .dist ("Tube", 0.6, 0.5).reverb (0.5, 0.5, 0.12);

    lead ("Metal Mouth", "FM, Metallic, Ring, Rectify", "Metal FM table ring-modulated and rectified: clanging, inharmonic, evil.")
        .wt (0, "Metal FM", 0.4).ring (600, 0.25).dist ("Rectify", 0.5, 0.4).filt (0, "Low Pass 12", 7000, 0.2)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    lead ("Fuzz Tri Lead", "Fuzz, Phaser", "Fuzzed triangle table swirling through a slow phaser, folded on top.")
        .wt (0, "Fuzz Tri", 0.5).phaser (0.18, 0.7, 1200, 0.5, 0.4).dist ("Sine Fold", 0.3, 0.35)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.4);

    lead ("Wobble Choir Lead", "Wobble, Vocal, Triplet", "Wobble Talk table moving on quarter-note triplets, clipped. Talks in time.")
        .wt (0, "Wobble Talk", 0.3).lfoSync (0, "Sine", "1/4 T").mod ("LFO 1", "Osc A WT Pos", 0.4)
        .dist ("Hard Clip", 0.45, 0.45).delay ("1/8 D", 0.3, 0.12);

    polyLead ("Hellfire Supersaw", "Supersaw, Tube, OTT, Wide", "Seven-voice supersaw, tube-driven and OTT'd, very wide. Plays big distorted chords.")
        .an (0, "Saw", 0.7).uni (0, 7, 0.32, 0.75).filt (0, "Low Pass 24", 6000, 0.15, 0.2)
        .dist ("Tube", 0.85, 0.55).ott (0.7).reverb (0.7, 0.5, 0.18).stereo (1.25, 160).mod ("Mod Wheel", "Filter 1 Cutoff", 0.3);

    lead ("Cheby Burn", "Bright, Hard Clip, Echo", "Chebyshev shine table clipped hard, with a quarter-note echo. Bright and burning.")
        .wt (0, "Cheby Shine", 0.5).dist ("Hard Clip", 0.6, 0.5, 0.45).delay ("1/4", 0.35, 0.15)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.45);

    lead ("PWM Grinder", "PWM, Rectify, Moving", "Pulse-width table sweeping on a slow LFO, rectified and lightly crushed.")
        .wt (0, "PWM Sweep", 0.4).uni (0, 2, 0.1, 0.25).stereo (1.0, 140).lfo (0, "Triangle", 0.6, 0.3).mod ("LFO 1", "Osc A WT Pos", 0.4)
        .dist ("Rectify", 0.5, 0.45).crush (10, 2, 0.3);

    lead ("Robot Psalm", "Vocal, Robot, Bitcrush", "Robot voice table, crushed and tube-driven. A choir of broken machines.")
        .wt (0, "Robot Voice", 0.4).crush (9, 3, 0.35).dist ("Tube", 0.55, 0.45).reverb (0.7, 0.55, 0.2, 30)
        .mod ("Mod Wheel", "Osc A WT Pos", 0.5);

    lead ("Broken Flute", "Flute, Tape, Wave", "Breathy flute pushed through soft cubic distortion and tape: dusty, sad, cloud-rap lead.", 90)
        .wt (0, "Breathy Flute", 0.45).noise ("Pink", 0.05).dist ("Soft Cubic", 0.8, 0.55).sat (0.45)
        .chorus (0.2, 0.4, 0.3).eq (1.5, 900, 1.0, -4.0).reverb (0.75, 0.6, 0.22, 40);

    lead ("Mirror Blade", "Saw, Warp, Hard Clip", "Saw mirrored into itself by an envelope-driven warp, clipped hard. Slices on every note.")
        .an (0, "Saw", 0.8).warp (0, "Mirror", 0.35).env (1, 0.001, 0.4, 0.3, 0.3).mod ("ENV 2", "Osc A Warp", 0.45)
        .dist ("Hard Clip", 0.55, 0.5).mod ("Mod Wheel", "Osc A Warp", 0.4);

    lead ("Phase Ripper", "Phase Distortion, Tube, Moving", "Phase-distorted saw with a slow LFO ripping through the warp, tube-driven.")
        .an (0, "Saw", 0.8).warp (0, "Phase Dist", 0.45).lfoSync (0, "Triangle", "1 bar", 0.3).mod ("LFO 1", "Osc A Warp", 0.35)
        .dist ("Tube", 0.65, 0.5).delay ("1/8 D", 0.3, 0.12);

    lead ("FM Snarl Lead", "FM, Growl, Resonant", "Growl FM table through a resonant low-pass that snarls open on each note, clipped.")
        .wt (0, "Growl FM", 0.45).filt (0, "Low Pass 24", 1800, 0.4, 0.3, 0.4).env (1, 0.002, 0.3, 0.35, 0.25)
        .mod ("ENV 2", "Filter 1 Cutoff", 0.45).dist ("Hard Clip", 0.5, 0.45).mod ("Mod Wheel", "Filter 1 Cutoff", 0.35);

    lead ("Seventh Heaven Burn", "777, Heavenly, Tube, Shimmer", "The Seventh Heaven table, tube-burned and haloed in a shimmering hall. The TRIPLE SEVEN lead.", 90)
        .wt (0, "Seventh Heaven", 0.5).uni (0, 3, 0.12, 0.7).dist ("Tube", 0.6, 0.5).fshift (0.12, 0.12)
        .reverb (0.85, 0.4, 0.24, 45).mod ("Mod Wheel", "Osc A WT Pos", 0.45);

    polyLead ("Static Hymn", "Organ, Witch House, Hard Clip", "Gospel organ table clipped and crushed. Distorted witch house chords.")
        .wt (0, "Organ Gospel", 0.45).dist ("Hard Clip", 0.5, 0.45).crush (10, 2, 0.3).reverb (0.75, 0.6, 0.2, 30)
        .env (0, 0.005, 0.5, 0.85, 0.35).mod ("Mod Wheel", "Saturation", 0.5);
}

//==============================================================================
void setChoice (DivinityProcessor& p, const juce::String& id, const juce::String& n)
{
    auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id));
    if (c == nullptr || ! c->choices.contains (n)) { std::cerr << "BAD CHOICE " << id << " = " << n << "\n"; std::exit (2); }
    c->setValueNotifyingHost (c->convertTo0to1 ((float) c->choices.indexOf (n)));
}

void apply (DivinityProcessor& p, const P& pr, double masterDb)
{
    FactoryPreset fp;
    for (auto& id : pr.order)
    {
        if (p.apvts.getParameter (id) == nullptr) { std::cerr << "UNKNOWN PARAM " << id << " in " << pr.name << "\n"; std::exit (2); }
        const auto& val = pr.v.at (id);
        if (val.isString())
        {
            auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id));
            if (c == nullptr || ! c->choices.contains (val.toString())) { std::cerr << "BAD CHOICE " << id << " = " << val.toString() << " in " << pr.name << "\n"; std::exit (2); }
            fp.choice (id, val.toString().toRawUTF8());
        }
        else
        {
            auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
            const auto rg = r->getNormalisableRange();
            const double x = (double) val;
            if (x < rg.start - 1e-6 || x > rg.end + 1e-6) { std::cerr << "OUT OF RANGE " << id << " = " << x << " in " << pr.name << "\n"; std::exit (2); }
            fp.set (id, x);
        }
    }
    fp.set (ids::master, masterDb);
    p.presets.applyFactoryPreset (fp);
}

struct Note { double start, len; int note; float vel; };

std::vector<Note> phraseFor (P::Kind k)
{
    std::vector<Note> n;
    const double s16 = 60.0 / 140.0 / 4.0;   // 140 bpm sixteenth
    if (k == P::OneShot)
    {
        const float vel[] = { 1.0f, 0.55f, 0.8f, 0.55f };
        for (int i = 0; i < 24; ++i) n.push_back ({ i * s16 * 2, s16 * 1.5, 60, vel[i % 4] });
        for (int i = 0; i < 8; ++i)  n.push_back ({ 48 * s16 + i * s16 / 2, s16 / 3, 60 + (i / 2), 0.6f + 0.05f * i });   // roll
        n.push_back ({ 56 * s16, s16 * 4, 60, 1.0f });
    }
    else if (k == P::Loop)
        n.push_back ({ 0.0, 4.5, 60, 0.95f });
    else if (k == P::Lead)
    {
        const int mel[] = { 60, 63, 67, 70, 72, 70, 67, 75 };
        for (int i = 0; i < 8; ++i) n.push_back ({ i * s16 * 3, s16 * 3.2, mel[i], 0.9f });   // overlapping = legato glides
        n.push_back ({ 24 * s16, s16 * 8, 72, 1.0f });
    }
    else
    {
        const int ch[][3] = { { 60, 63, 67 }, { 58, 62, 65 }, { 56, 60, 63 }, { 55, 59, 62 } };
        for (int c = 0; c < 4; ++c) for (int k2 = 0; k2 < 3; ++k2) n.push_back ({ c * s16 * 8, s16 * 6, ch[c][k2], 0.9f });
    }
    return n;
}

juce::AudioBuffer<float> render (DivinityProcessor& p, const std::vector<Note>& notes, double seconds)
{
    const int block = 256;
    p.setRateAndBufferSizeDetails (kSr, block);
    p.prepareToPlay (kSr, block);
    const int total = (int) (seconds * kSr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        midi.clear();
        for (auto& nt : notes)
        {
            const int on = (int) (nt.start * kSr), off = (int) ((nt.start + nt.len) * kSr);
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, nt.note), off - pos);
        }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    p.releaseResources();
    return out;
}

struct Meter { double peakDb, rmsDb, monoCorr; bool finite; };

Meter measure (const juce::AudioBuffer<float>& b)
{
    double peak = 0, sum = 0, lr = 0, ll = 0, rr = 0; bool fin = true;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double L = b.getSample (0, i), R = b.getSample (1, i);
        if (! std::isfinite (L) || ! std::isfinite (R)) { fin = false; continue; }
        peak = juce::jmax (peak, std::abs (L), std::abs (R));
        sum += 0.5 * (L * L + R * R); lr += L * R; ll += L * L; rr += R * R;
    }
    // RMS over the loudest 50 ms windows (ignores silence between hits)
    const int win = (int) (0.05 * kSr);
    std::vector<double> w;
    for (int s = 0; s + win <= b.getNumSamples(); s += win)
    {
        double e = 0;
        for (int i = s; i < s + win; ++i) e += 0.5 * (b.getSample (0, i) * b.getSample (0, i) + b.getSample (1, i) * b.getSample (1, i));
        w.push_back (e / win);
    }
    std::sort (w.rbegin(), w.rend());
    double top = 0; const size_t k = juce::jmax ((size_t) 1, w.size() / 5);
    for (size_t i = 0; i < k && i < w.size(); ++i) top += w[i];
    top /= (double) k;
    return { juce::Decibels::gainToDecibels (peak, -120.0), 10.0 * std::log10 (top + 1e-12), lr / std::sqrt (ll * rr + 1e-12), fin };
}

void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (os.get(), kSr, 2, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> wr (w); wr->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
}
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto outDir = juce::File (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName() + "/holydirt");
    bool wavs = false;
    for (int i = 2; i < argc; ++i) wavs |= juce::String (argv[i]) == "--wavs";

    buildPack();
    std::cout << "HOLY DIRT: " << pack.size() << " presets\n";
    juce::StringArray names;
    for (auto& pr : pack) { if (names.contains (pr.name)) { std::cerr << "DUPLICATE NAME " << pr.name << "\n"; return 2; } names.add (pr.name); }

    DivinityProcessor proc;
    const auto presetRoot = outDir.getChildFile (kPack);
    presetRoot.deleteRecursively();
    const auto previews = outDir.getChildFile ("previews");
    if (wavs) { previews.deleteRecursively(); previews.createDirectory(); }

    std::ofstream csv (outDir.getChildFile ("levels.csv").getFullPathName().toStdString());
    csv << "group,name,master_db,peak_db,rms_db,corr\n";
    int problems = 0, idx = 0;
    for (auto& pr : pack)
    {
        ++idx;
        // loudness target (RMS of the loudest moments): hats a touch under perc, leads on top
        // drums are levelled by peak (they're transients), leads by the RMS of their loudest moments
        const bool drums = pr.category == "PERCUSSION";
        const double target = drums ? (pr.group.startsWith ("01") ? -4.0 : -3.0) : -13.0;
        const auto notes = phraseFor (pr.kind);
        const double secs = pr.kind == P::Loop ? 5.0 : 4.5;
        double master = -8.0;
        Meter m {};
        juce::AudioBuffer<float> audio;
        for (int pass = 0; pass < 3; ++pass)
        {
            apply (proc, pr, master);
            audio = render (proc, notes, secs);
            m = measure (audio);
            if (! m.finite || m.rmsDb < -80.0) break;
            const double delta = target - (drums ? m.peakDb : m.rmsDb);
            if (std::abs (delta) < 0.4) break;
            master = juce::jlimit (-40.0, 6.0, master + delta);
        }
        const bool silent = ! m.finite || m.rmsDb < -60.0;
        const bool hot = m.peakDb > -0.2;    // riding the limiter
        const bool quiet = master >= 5.9 && (drums ? m.peakDb < target - 2 : m.rmsDb < target - 2);
        const bool narrow = m.monoCorr < 0.2;   // would collapse in mono
        if (silent || hot || quiet || narrow) ++problems;
        std::printf ("%2d %-20s %-24s master %6.1f  peak %6.1f  rms %6.1f  corr %5.2f %s\n", idx, pr.group.toRawUTF8(), pr.name.toRawUTF8(),
                     master, m.peakDb, m.rmsDb, m.monoCorr, silent ? "  << SILENT" : hot ? "  << HOT" : quiet ? "  << QUIET" : narrow ? "  << NARROW" : "");
        csv << pr.group << "," << pr.name << "," << master << "," << m.peakDb << "," << m.rmsDb << "," << m.monoCorr << "\n";

        // final state -> .divpreset
        apply (proc, pr, master);
        PresetInfo info;
        info.name = pr.name; info.category = pr.category; info.author = "TRIPLE SEVEN AUDIO";
        info.tags = pr.tags; info.description = pr.desc;
        auto xml = proc.presets.createPresetXml (info);
        const auto dir = presetRoot.getChildFile (pr.group);
        dir.createDirectory();
        xml->writeTo (dir.getChildFile (pr.name + PresetManager::kExtension));

        // round trip: the file must load back to the same values
        apply (proc, pr, -30.0);
        if (auto back = juce::XmlDocument::parse (dir.getChildFile (pr.name + PresetManager::kExtension)))
        {
            proc.presets.applyPresetXml (*back);
            if (std::abs (proc.apvts.getRawParameterValue (ids::master)->load() - master) > 0.05) { std::cerr << "ROUND TRIP FAILED " << pr.name << "\n"; ++problems; }
        }
        else { std::cerr << "XML PARSE FAILED " << pr.name << "\n"; ++problems; }

        if (wavs)
        {
            auto fname = juce::String (idx).paddedLeft ('0', 2) + " " + pr.name + ".wav";
            writeWav (audio, previews.getChildFile (fname));
        }
    }
    std::cout << (problems == 0 ? "ALL GOOD\n" : "PROBLEMS: " + std::to_string (problems) + "\n");
    return problems == 0 ? 0 : 1;
}
