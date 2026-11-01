#include "PresetManager.h"

namespace divinity
{
namespace
{
    using FP = FactoryPreset;

    // osc helpers -----------------------------------------------------------
    FP& oscOn  (FP& p, int o, bool on)                { return p.set (ids::osc (o, "on"), on ? 1 : 0); }
    FP& wt     (FP& p, int o, const char* table, double pos)
    {
        oscOn (p, o, true);
        return p.choice (ids::osc (o, "engine"), "Wavetable").choice (ids::osc (o, "table"), table).set (ids::osc (o, "pos"), pos);
    }
    FP& analog (FP& p, int o, const char* wave)
    {
        oscOn (p, o, true);
        return p.choice (ids::osc (o, "engine"), "Analog").choice (ids::osc (o, "wave"), wave);
    }
    FP& uni    (FP& p, int o, int voices, double detune, double width)
    {
        return p.set (ids::osc (o, "unison"), voices).set (ids::osc (o, "detune"), detune).set (ids::osc (o, "width"), width);
    }
    FP& lvl    (FP& p, int o, double level) { return p.set (ids::osc (o, "level"), level); }
    FP& tuneO  (FP& p, int o, int oct, int semi = 0, double fine = 0)
    {
        return p.set (ids::osc (o, "oct"), oct).set (ids::osc (o, "semi"), semi).set (ids::osc (o, "fine"), fine);
    }
    FP& tight  (FP& p, int o, double phase = 0.0) { return p.set (ids::osc (o, "randphase"), 0).set (ids::osc (o, "phase"), phase); }

    FP& filt   (FP& p, int f, const char* type, double cutoff, double res, double drive = 0.0)
    {
        return p.choice (ids::filt (f, "type"), type).set (ids::filt (f, "cutoff"), cutoff)
                .set (ids::filt (f, "res"), res).set (ids::filt (f, "drive"), drive);
    }
    FP& env    (FP& p, int e, double a, double d, double s, double r, double curve = 0.0)
    {
        return p.set (ids::env (e, "attack"), a).set (ids::env (e, "decay"), d).set (ids::env (e, "sustain"), s)
                .set (ids::env (e, "release"), r).set (ids::env (e, "curve"), curve);
    }
    FP& lfo    (FP& p, int l, const char* shape, double rate, double smooth = 0.0)
    {
        return p.choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "rate"), rate).set (ids::lfo (l, "smooth"), smooth);
    }
    FP& lfoSync (FP& p, int l, const char* shape, const char* div)
    {
        return p.choice (ids::lfo (l, "shape"), shape).set (ids::lfo (l, "sync"), 1).choice (ids::lfo (l, "div"), div);
    }
    FP& mod    (FP& p, int slot, const char* src, const char* dst, double amt)
    {
        return p.choice (ids::mod (slot, "src"), src).choice (ids::mod (slot, "dst"), dst).set (ids::mod (slot, "amt"), amt);
    }
    FP& sat    (FP& p, bool on, double drive, double mix = 1.0) { return p.set (ids::satOn, on).set (ids::satDrive, drive).set (ids::satMix, mix); }
    FP& chorus (FP& p, double rate, double depth, double mix)
    {
        return p.set (ids::choOn, 1).set (ids::choRate, rate).set (ids::choDepth, depth).set (ids::choMix, mix);
    }
    FP& delay  (FP& p, const char* div, double fb, double mix, bool ping = true, double tone = 0.6)
    {
        return p.set (ids::dlyOn, 1).choice (ids::dlyDiv, div).set (ids::dlyFeedback, fb).set (ids::dlyMix, mix)
                .set (ids::dlyPingPong, ping).set (ids::dlyTone, tone);
    }
    FP& reverb (FP& p, double size, double damp, double mix)
    {
        return p.set (ids::revOn, 1).set (ids::revSize, size).set (ids::revDamp, damp).set (ids::revMix, mix);
    }
    FP& macros (FP& p, double tone, double move, double texture, double space)
    {
        return p.set (ids::macro (0), tone).set (ids::macro (1), move).set (ids::macro (2), texture).set (ids::macro (3), space);
    }
    FP& mono   (FP& p, double glideMs) { return p.choice (ids::voiceMode, "Legato").set (ids::glide, glideMs); }

    FP make (const char* name, const char* cat, const char* tags, const char* desc)
    {
        FP p; p.name = name; p.category = cat; p.tags = tags; p.description = desc; return p;
    }
}

std::vector<FactoryPreset> createFactoryPresets()
{
    std::vector<FactoryPreset> list;

    // ---------------------------------------------------------------- PAD
    {
        auto p = make ("Celestial Pad", "Pad", "Ethereal, Euphoric, Wide", "Formant wavetable choir layered with glass partials and a soft sine floor.");
        wt (p, 0, "Formant Vox", 0.35); uni (p, 0, 5, 0.35, 0.85); lvl (p, 0, 0.95);
        wt (p, 1, "Glass Halo", 0.2); uni (p, 1, 3, 0.25, 0.9); tuneO (p, 1, 1); lvl (p, 1, 0.45);
        analog (p, 2, "Sine"); tuneO (p, 2, -1); lvl (p, 2, 0.4);
        filt (p, 0, "MG Low 24", 2400, 0.2);
        env (p, 0, 0.9, 1.5, 0.8, 2.6, 0.2);
        env (p, 1, 1.4, 2.5, 0.35, 2.5);
        lfo (p, 0, "Sine", 0.15); lfo (p, 1, "Sine", 0.09);
        mod (p, 0, "LFO 1", "Filter 1 Cutoff", 0.12);
        mod (p, 1, "ENV 2", "Osc A WT Pos", 0.37);
        mod (p, 2, "LFO 2", "Osc B WT Pos", -0.24);
        mod (p, 3, "Mod Wheel", "Filter 1 Drive", 0.41);
        mod (p, 4, "Velocity", "Filter 1 Cutoff", 0.15);
        sat (p, true, 0.1); chorus (p, 0.35, 0.35, 0.35); delay (p, "1/4 D", 0.4, 0.18); reverb (p, 0.85, 0.45, 0.42);
        macros (p, 0.0, 0.0, 0.0, 0.0);
        list.push_back (p);
    }
    list.push_back (make ("Init", "Basic", "Init, Clean", "A blank starting point: one wavetable oscillator through a gentle ladder filter."));
    {
        auto p = make ("Ambient Wash", "Ambient", "Serene, Wide, Evolving", "Eight-voice detuned shapes drowned in long delay and reverb.");
        wt (p, 0, "Basic Shapes", 0.12); uni (p, 0, 8, 0.55, 1.0); lvl (p, 0, 0.65);
        p.set (ids::noiseOn, 1).choice (ids::noiseType, "Pink").set (ids::noiseLevel, 0.08);
        filt (p, 0, "Low Pass 12", 1800, 0.15);
        env (p, 0, 2.5, 3.0, 0.9, 6.0, 0.3);
        lfo (p, 0, "Random", 0.25, 0.9);
        mod (p, 0, "LFO 1", "Osc A WT Pos", 0.15);
        chorus (p, 0.25, 0.5, 0.4); delay (p, "1/2 D", 0.55, 0.3, true, 0.4); reverb (p, 1.0, 0.5, 0.7);
        list.push_back (p);
    }
    {
        auto p = make ("Vox Choir", "Vocal Texture", "Angelic, Breathy, Melancholy", "Moving vowel formants with breath noise and slow vibrato.");
        wt (p, 0, "Formant Vox", 0.2); uni (p, 0, 6, 0.3, 0.9); lvl (p, 0, 0.75);
        p.set (ids::noiseOn, 1).choice (ids::noiseType, "Pink").set (ids::noiseLevel, 0.06);
        filt (p, 0, "Low Pass 12", 5000, 0.1);
        env (p, 0, 1.2, 1.0, 0.9, 2.5);
        lfo (p, 0, "Sine", 0.12); lfo (p, 1, "Sine", 4.8, 0.3);
        mod (p, 0, "LFO 1", "Osc A WT Pos", 0.35);
        mod (p, 1, "LFO 2", "Osc A Pitch", 0.006);
        mod (p, 2, "Mod Wheel", "Osc A WT Pos", 0.4);
        chorus (p, 0.4, 0.4, 0.4); reverb (p, 0.9, 0.4, 0.5);
        list.push_back (p);
    }
    {
        auto p = make ("Dark Drone", "Ambient", "Melancholy, Dark, Evolving", "Low harmonic sweep and hollow organ under a slowly breathing resonant ladder.");
        wt (p, 0, "Harmonic Sweep", 0.3); uni (p, 0, 4, 0.3, 0.8); tuneO (p, 0, -1); lvl (p, 0, 0.7);
        wt (p, 1, "Hollow Organ", 0.5); tuneO (p, 1, -2); lvl (p, 1, 0.5);
        p.set (ids::noiseOn, 1).choice (ids::noiseType, "Dark").set (ids::noiseLevel, 0.15);
        filt (p, 0, "MG Low 24", 500, 0.35, 0.2);
        env (p, 0, 3.0, 2.0, 1.0, 5.0);
        lfo (p, 0, "Sine", 0.05); lfo (p, 1, "Random", 0.3, 0.9);
        mod (p, 0, "LFO 1", "Filter 1 Cutoff", 0.3);
        mod (p, 1, "LFO 2", "Osc A WT Pos", 0.3);
        delay (p, "1/4 D", 0.6, 0.2, true, 0.3); reverb (p, 0.95, 0.6, 0.6);
        list.push_back (p);
    }
    // ---------------------------------------------------------------- BASS / 808
    {
        auto p = make ("Deep Sub", "Bass", "Clean, Heavy, Mono", "Pure sine sub with a sine octave below. Tight phase for consistent low end.");
        analog (p, 0, "Sine"); tight (p, 0); lvl (p, 0, 0.9);
        p.set (ids::subOn, 1).choice (ids::subMode, "Sine -1").set (ids::subLevel, 0.4);
        filt (p, 0, "Low Pass 12", 400, 0.0);
        env (p, 0, 0.003, 0.6, 0.75, 0.15);
        sat (p, true, 0.25);
        mono (p, 30);
        list.push_back (p);
    }
    {
        auto p = make ("808 Boom", "808", "Heavy, Aggressive, Punchy", "Sine 808 with a fast pitch drop and saturated body. Mono with glide for slides.");
        analog (p, 0, "Sine"); tight (p, 0); lvl (p, 0, 0.95);
        filt (p, 0, "Off", 12000, 0.0);
        env (p, 0, 0.001, 1.8, 0.0, 0.35);
        env (p, 2, 0.0005, 0.07, 0.0, 0.05, -0.5);
        mod (p, 0, "ENV 3", "Osc A Pitch", 0.8);
        sat (p, true, 0.55); mono (p, 60);
        list.push_back (p);
    }
    {
        auto p = make ("Reese Bass", "Bass", "Dark, Wide, Moving", "Detuned saws beating against each other through a driven ladder.");
        analog (p, 0, "Saw"); uni (p, 0, 3, 0.4, 0.2); lvl (p, 0, 0.85);
        analog (p, 1, "Saw"); tuneO (p, 1, 0, 0, 14); lvl (p, 1, 0.75);
        p.set (ids::subOn, 1).choice (ids::subMode, "Sine -1").set (ids::subLevel, 0.45);
        filt (p, 0, "MG Low 24", 700, 0.25, 0.3);
        env (p, 0, 0.005, 0.5, 0.9, 0.2);
        lfo (p, 0, "Sine", 0.3);
        mod (p, 0, "LFO 1", "Filter 1 Cutoff", 0.1);
        mod (p, 1, "Mod Wheel", "Filter 1 Cutoff", 0.5);
        sat (p, true, 0.35); mono (p, 40);
        list.push_back (p);
    }
    {
        auto p = make ("Wobble Machine", "Bass", "Aggressive, Rhythmic", "Tempo-synced 1/8 wobble on a resonant ladder over a pulse-grit wavetable.");
        wt (p, 0, "Pulse Grit", 0.4); uni (p, 0, 2, 0.2, 0.3); lvl (p, 0, 0.75);
        analog (p, 1, "Square"); tuneO (p, 1, -1); lvl (p, 1, 0.5);
        filt (p, 0, "MG Low 24", 300, 0.55, 0.3);
        env (p, 0, 0.003, 0.4, 0.9, 0.12);
        lfoSync (p, 0, "Sine", "1/8");
        mod (p, 0, "LFO 1", "Filter 1 Cutoff", 0.55);
        mod (p, 1, "LFO 1", "Osc A WT Pos", 0.3);
        sat (p, true, 0.5); mono (p, 0);
        list.push_back (p);
    }
    {
        auto p = make ("Grit Comb", "Experimental", "Metallic, Aggressive, Glitch", "Pulse-grit wavetable through a key-tracked comb filter with stepped random scanning.");
        wt (p, 0, "Pulse Grit", 0.7); lvl (p, 0, 0.75); p.set (ids::osc (0, "fold"), 0.3);
        filt (p, 0, "Comb", 261.6, 0.6); p.set (ids::filt (0, "keytrack"), 1.0).set (ids::filt (0, "mix"), 0.6);
        filt (p, 1, "Low Pass 12", 6000, 0.2);
        env (p, 0, 0.002, 0.5, 0.7, 0.3);
        lfoSync (p, 0, "Random", "1/16");
        mod (p, 0, "LFO 1", "Osc A WT Pos", 0.4);
        sat (p, true, 0.3); delay (p, "1/16", 0.3, 0.15);
        list.push_back (p);
    }
    // ---------------------------------------------------------------- LEAD
    {
        auto p = make ("Supersaw Lead", "Lead", "Euphoric, Bright, Wide", "Seven-voice supersaw with an octave layer, ready for big drops.");
        analog (p, 0, "Saw"); uni (p, 0, 7, 0.45, 0.9); lvl (p, 0, 0.75);
        analog (p, 1, "Saw"); uni (p, 1, 3, 0.3, 0.7); tuneO (p, 1, 1); lvl (p, 1, 0.35);
        filt (p, 0, "Low Pass 12", 6500, 0.1);
        env (p, 0, 0.005, 0.3, 0.9, 0.3);
        mod (p, 0, "Velocity", "Filter 1 Cutoff", 0.2);
        delay (p, "1/8 D", 0.35, 0.18); reverb (p, 0.7, 0.4, 0.2);
        list.push_back (p);
    }
    {
        auto p = make ("Hyper Lead", "Lead", "Bright, Hyperpop, Aggressive", "Folded harmonic sweep with octave square, glide and vibrato.");
        wt (p, 0, "Harmonic Sweep", 0.6); lvl (p, 0, 0.75); p.set (ids::osc (0, "fold"), 0.2);
        analog (p, 1, "Square"); tuneO (p, 1, 1); lvl (p, 1, 0.3);
        filt (p, 0, "Low Pass 12", 9000, 0.15);
        env (p, 0, 0.003, 0.4, 0.85, 0.2);
        lfo (p, 0, "Sine", 6.0, 0.2);
        mod (p, 0, "LFO 1", "Osc A Pitch", 0.012);
        mod (p, 1, "Mod Wheel", "Osc A Fold", 0.5);
        sat (p, true, 0.4); delay (p, "1/16", 0.3, 0.2); reverb (p, 0.5, 0.5, 0.15);
        mono (p, 70);
        list.push_back (p);
    }
    {
        auto p = make ("Analog Brass", "Lead", "Warm, Analog, Bold", "Two slightly detuned saws with a classic filter-envelope swell.");
        analog (p, 0, "Saw"); uni (p, 0, 2, 0.15, 0.4); lvl (p, 0, 0.9);
        analog (p, 1, "Saw"); tuneO (p, 1, 0, 0, -8); lvl (p, 1, 0.75);
        filt (p, 0, "MG Low 24", 600, 0.15, 0.1);
        env (p, 0, 0.03, 0.3, 0.85, 0.25);
        env (p, 1, 0.06, 0.5, 0.5, 0.3);
        mod (p, 0, "ENV 2", "Filter 1 Cutoff", 0.4);
        mod (p, 1, "Velocity", "Filter 1 Cutoff", 0.2);
        reverb (p, 0.5, 0.5, 0.15);
        list.push_back (p);
    }
    // ---------------------------------------------------------------- PLUCK / KEYS / BELL
    {
        auto p = make ("Glass Pluck", "Pluck", "Bright, Delicate", "Glass wavetable plucked by a snappy filter envelope.");
        wt (p, 0, "Glass Halo", 0.5); lvl (p, 0, 1.0);
        analog (p, 1, "Sine"); tuneO (p, 1, 1); lvl (p, 1, 0.45);
        filt (p, 0, "MG Low 24", 900, 0.3);
        env (p, 0, 0.001, 0.6, 0.0, 0.4, -0.3);
        env (p, 1, 0.001, 0.25, 0.0, 0.2, -0.4);
        mod (p, 0, "ENV 2", "Filter 1 Cutoff", 0.55);
        mod (p, 1, "Velocity", "Filter 1 Cutoff", 0.2);
        delay (p, "1/8 D", 0.35, 0.22); reverb (p, 0.6, 0.4, 0.28);
        list.push_back (p);
    }
    {
        auto p = make ("Halo Bells", "Bell", "Ethereal, Bright", "Glass partials ringing out with a decaying spectral shift.");
        wt (p, 0, "Glass Halo", 0.8); lvl (p, 0, 0.75);
        analog (p, 1, "Sine"); tuneO (p, 1, 2, 0, 7); lvl (p, 1, 0.22);
        filt (p, 0, "Low Pass 12", 8000, 0.1);
        env (p, 0, 0.001, 2.5, 0.0, 1.8, -0.2);
        env (p, 1, 0.001, 0.9, 0.0, 0.8);
        mod (p, 0, "ENV 2", "Osc A WT Pos", -0.4);
        reverb (p, 0.75, 0.3, 0.35);
        list.push_back (p);
    }
    {
        auto p = make ("Soft Keys", "Keys", "Warm, Intimate", "Drawbar organ table with a triangle body, velocity-sensitive brightness.");
        wt (p, 0, "Hollow Organ", 0.15); lvl (p, 0, 0.95);
        analog (p, 1, "Triangle"); lvl (p, 1, 0.55);
        filt (p, 0, "MG Low 24", 2500, 0.1);
        env (p, 0, 0.004, 1.2, 0.35, 0.5);
        mod (p, 0, "Velocity", "Filter 1 Cutoff", 0.3);
        chorus (p, 0.5, 0.3, 0.3); reverb (p, 0.55, 0.5, 0.2);
        list.push_back (p);
    }
    {
        auto p = make ("Arp Machine", "Arpeggio", "Rhythmic, Bright", "Short resonant saw blips with a slow synced filter sweep and ping-pong echoes.");
        analog (p, 0, "Saw"); uni (p, 0, 2, 0.15, 0.5); lvl (p, 0, 1.0);
        filt (p, 0, "MG Low 24", 700, 0.4);
        env (p, 0, 0.001, 0.25, 0.0, 0.2, -0.3);
        env (p, 1, 0.001, 0.18, 0.0, 0.15, -0.3);
        lfoSync (p, 0, "Triangle", "2 bars");
        mod (p, 0, "ENV 2", "Filter 1 Cutoff", 0.5);
        mod (p, 1, "LFO 1", "Filter 1 Cutoff", 0.15);
        delay (p, "1/8 D", 0.45, 0.3);
        list.push_back (p);
    }
    // ---------------------------------------------------------------- PERC / CINEMATIC / FX
    {
        auto p = make ("Punch Kick", "Percussion", "Punchy, Tight", "Sine kick with a two-octave pitch drop and saturated body.");
        analog (p, 0, "Sine"); tight (p, 0, 0.25); lvl (p, 0, 0.95);
        filt (p, 0, "Off", 12000, 0.0);
        env (p, 0, 0.0005, 0.35, 0.0, 0.1);
        env (p, 2, 0.0005, 0.045, 0.0, 0.04, -0.6);
        mod (p, 0, "ENV 3", "Osc A Pitch", 1.0);
        mod (p, 1, "ENV 3", "Osc A Pitch", 1.0);
        sat (p, true, 0.5);
        list.push_back (p);
    }
    {
        auto p = make ("Cinematic Impact", "Cinematic", "Heavy, Dark, Huge", "Saw, sub and noise slammed together with a pitch dive and big room.");
        analog (p, 0, "Saw"); tuneO (p, 0, -1); lvl (p, 0, 0.7); tight (p, 0);
        p.set (ids::subOn, 1).choice (ids::subMode, "Sine -2").set (ids::subLevel, 0.7);
        p.set (ids::noiseOn, 1).choice (ids::noiseType, "White").set (ids::noiseLevel, 0.5);
        filt (p, 0, "MG Low 24", 4000, 0.1, 0.4);
        env (p, 0, 0.001, 1.5, 0.0, 1.5);
        env (p, 1, 0.001, 0.4, 0.0, 0.4);
        env (p, 2, 0.001, 0.15, 0.0, 0.1);
        mod (p, 0, "ENV 2", "Filter 1 Cutoff", -0.4);
        mod (p, 1, "ENV 3", "Osc A Pitch", 0.5);
        mod (p, 2, "ENV 2", "Noise Level", 0.3);
        sat (p, true, 0.6); reverb (p, 0.9, 0.5, 0.45);
        list.push_back (p);
    }
    {
        auto p = make ("Noise Riser", "FX", "Tension, Bright", "Band-passed noise sweeping upward over four seconds.");
        oscOn (p, 0, false);
        p.set (ids::noiseOn, 1).choice (ids::noiseType, "White").set (ids::noiseLevel, 1.0);
        filt (p, 0, "Band Pass", 300, 0.6);
        env (p, 0, 3.5, 1.0, 1.0, 1.5);
        env (p, 1, 4.0, 1.0, 1.0, 1.5);
        mod (p, 0, "ENV 2", "Filter 1 Cutoff", 0.7);
        delay (p, "1/8", 0.4, 0.2); reverb (p, 0.8, 0.3, 0.5);
        list.push_back (p);
    }

    return list;
}

} // namespace divinity
