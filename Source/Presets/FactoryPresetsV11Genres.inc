// DIVINITY 1.1 genre banks: WAVE, TRAP, TECHNO, DUBSTEP, GARAGE, DARK GARAGE, DNB, CINEMATIC.
// Hand-designed (no recipe spread): every preset is its own core sound, built warm and thick
// (detuned unison, analog drift, tape / tube colour, rounded tops, mono low end on basses).
// Included inside FactoryPresetsV7.cpp's anonymous namespace (uses Patch / Spec).

static void addGenreBanks (std::vector<Spec>& out)
{
    auto add = [&out] (const char* cat, const char* name, const char* tags, const char* desc, Patch p)
    {
        Spec s; s.name = name; s.category = cat; s.tags = tags; s.description = desc; s.patch = std::move (p);
        out.push_back (std::move (s));
    };
    auto P = [] { Patch p; p.drift (0.35); return p; };
    // shared warm finishing moves
    auto bassSpace = [] (Patch& p) { p.reverb (0.45, 0.6, 0.12, 10.0, 0.6).stereo (1.1, true, 140.0); };
    auto leadSpace = [] (Patch& p) { p.delay ("1/8 D", 0.35, 0.22).reverb (0.7, 0.45, 0.3, 25.0); };
    auto padSpace  = [] (Patch& p) { p.chorus (0.25, 0.45, 0.3).reverb (0.88, 0.5, 0.48, 50.0); };

    //==========================================================================
    // WAVE
    {
        const char* C = "WAVE";
        { auto p = P(); p.wt (0, "Saw Stack", 0.35, 0.8).uni (0, 7, 0.28, 0.9).wt (1, "Glass Halo", 0.5, 0.45).uni (1, 4, 0.18, 1.0).tune (1, 1)
            .filt (0, "MG Low 24", 2600, 0.15, 0.2).env (0, 0.9, 1.5, 0.85, 2.4).sat (0.25, 0.6); padSpace (p);
          add (C, "Frozen Horizon Pad", "Pad Wave Warm Wide", "seven-voice saw stack and an octave of glass, slow attack, tape-warm and wide", p); }
        { auto p = P(); p.wt (0, "Reese Core", 0.3, 0.85).uni (0, 4, 0.22, 0.5).sub ("Sine -1", 0.5).filt (0, "MG Low 24", 900, 0.2, 0.35)
            .env (0, 0.01, 0.8, 0.9, 0.3).mono (40).dist ("Tube", 0.3, 0.4, 0.45); bassSpace (p);
          add (C, "Velvet Undertow Bass", "Bass Wave Mono Warm", "detuned Reese-core bass over a sine sub, tube-warmed, low end mono", p); }
        { auto p = P(); p.wt (0, "Choir Formant", 0.45, 0.8).uni (0, 5, 0.2, 1.0).noise ("Pink", 0.06).filt (0, "Low Pass 12", 3200, 0.1)
            .env (0, 1.4, 2.0, 0.8, 3.0); padSpace (p); p.reverb (0.95, 0.4, 0.55, 70.0);
          add (C, "Ghost Choir Veil", "Pad Choir Wave Airy", "a five-voice formant choir with a breath of pink noise in a huge room", p); }
        { auto p = P(); p.an (0, "Saw", 0.75).uni (0, 3, 0.12, 0.7).an (1, "Square", 0.4).tune (1, -1).filt (0, "MG Low 24", 1700, 0.35, 0.25, 0.4)
            .env (0, 0.005, 0.5, 0.55, 0.6).env (1, 0.0, 0.35, 0.0, 0.3).mod ("ENV 2", "Filter 1 Cutoff", 0.45).mono (70); leadSpace (p);
          add (C, "Neon Mirage Lead", "Lead Wave Analog Glide", "detuned analog saw + square an octave down, a filter blip on every note, gliding legato", p); }
        { auto p = P(); p.wt (0, "E-Piano Tine", 0.4, 0.8).uni (0, 3, 0.1, 0.8).filt (0, "Low Pass 12", 4200, 0.1)
            .env (0, 0.005, 1.2, 0.3, 1.0).chorus (0.35, 0.5, 0.35).delay ("1/4", 0.4, 0.25).reverb (0.8, 0.5, 0.35, 30.0).sat (0.2);
          add (C, "Moonlit Keys", "Keys Wave Chorus Warm", "a chorused electric-piano tine with a quarter-note delay: soft and nostalgic", p); }
        { auto p = P(); p.wt (0, "Glass FM", 0.55, 0.7).wt (1, "Kalimba", 0.3, 0.45).tune (1, 1).filt (0, "Low Pass 12", 5200, 0.15)
            .env (0, 0.002, 0.6, 0.0, 0.7).delay ("1/8 D", 0.5, 0.35).reverb (0.85, 0.4, 0.4, 40.0);
          add (C, "Crystal Rain Pluck", "Pluck Wave Glass", "a glass-FM pluck doubled by kalimba an octave up into dotted-eighth delays", p); }
        { auto p = P(); p.wt (0, "Vowel Sweep", 0.6, 0.8).uni (0, 2, 0.1, 0.5).tune (0, 1).sub ("Triangle -1", 0.5).filt (0, "Formant", 1200, 0.45, 0.3)
            .env (0, 0.01, 0.6, 0.85, 0.35).mono (55).lfoSync (0, "Triangle", "1/4", 1.0).mod ("LFO 1", "Osc A WT Pos", 0.6).dist ("Overdrive", 0.3, 0.4); bassSpace (p);
          add (C, "Weeping Vowel Bass", "Bass Wave Vowel Mono", "a vowel-table bass an octave up through a formant filter, saying 'oo-ah' every beat over a triangle sub", p); }
        { auto p = P(); p.wt (0, "String Ensemble", 0.5, 0.8).uni (0, 6, 0.2, 1.0).filt (0, "Low Pass 12", 2400, 0.1).env (0, 0.6, 1.0, 0.9, 2.0)
            .sat (0.3, 0.6).eq (2.0, 600, 1.0, -3.0); padSpace (p);
          add (C, "Ethereal Strings", "Strings Wave Warm Wide", "a six-voice string ensemble, warmed with tape and a rounded top", p); }
        { auto p = P(); p.an (0, "Triangle", 0.8).uni (0, 3, 0.08, 0.6).an (1, "Saw", 0.25).tune (1, 1, 7).filt (0, "MG Low 24", 2200, 0.2)
            .env (0, 0.08, 0.9, 0.7, 1.4).lfo (0, "Triangle", 0.2, 1.0).mod ("LFO 1", "Pan", 0.3); padSpace (p);
          add (C, "Slow Memory Haze", "Synth Wave Soft Analog", "a soft triangle chord-synth with a fifth on top, drifting gently across the stereo field", p); }
        { auto p = P(); p.wt (0, "Talk Box", 0.4, 0.8).uni (0, 3, 0.12, 0.8).filt (0, "Formant", 1400, 0.4).env (0, 0.02, 0.7, 0.75, 0.6)
            .mono (90).sat (0.3); leadSpace (p);
          add (C, "Witch Vox Lead", "Lead Wave Vocal Glide", "a talk-box formant lead with a long glide: the witch-house vocal synth", p); }
    }

    //==========================================================================
    // TRAP
    {
        const char* C = "TRAP";
        { auto p = P(); p.an (0, "Sine", 0.9).tight (0, 0.25).env (1, 0.0, 0.08, 0.0, 0.05).mod ("ENV 2", "Osc A Pitch", 1.0)
            .env (0, 0.001, 2.0, 0.0, 0.7).dist ("Tube", 0.4, 0.55).sub ("Sine -1", 0.15).mono (35, false); bassSpace (p);
          add (C, "Long 808", "808 Bass Trap Mono", "a long sine 808 with a fast pitch drop and tube warmth, glides between notes", p); }
        { auto p = P(); p.an (0, "Sine", 0.9).tight (0, 0.25).env (1, 0.0, 0.05, 0.0, 0.05).mod ("ENV 2", "Osc A Pitch", 1.0)
            .env (0, 0.001, 1.2, 0.0, 0.4).dist ("Hard Clip", 0.55, 0.7).sat (0.3).mono (25, false); bassSpace (p);
          add (C, "Punchy 808 Clip", "808 Bass Trap Distorted", "a short, clipped 808: punchy click, buzzy tail, mono", p); }
        { auto p = P(); p.wt (0, "Bell FM", 0.4, 0.75).wt (1, "Music Box", 0.3, 0.35).tune (1, 1).filt (0, "Low Pass 12", 6000, 0.1)
            .env (0, 0.002, 1.4, 0.0, 1.2).delay ("1/4 T", 0.4, 0.28).reverb (0.8, 0.45, 0.35, 20.0);
          add (C, "Haunted Bell Melody", "Bell Trap Melody", "an FM bell with a music-box octave, triplet delays: the dark trap melody bell", p); }
        { auto p = P(); p.wt (0, "Pipe Organ", 0.5, 0.75).uni (0, 3, 0.1, 0.8).filt (0, "Low Pass 12", 3000, 0.1).env (0, 0.02, 0.6, 0.8, 0.8)
            .sat (0.3, 0.6); padSpace (p);
          add (C, "Church Organ Chords", "Organ Trap Chords", "a three-voice pipe organ for minor-key trap chords", p); }
        { auto p = P(); p.wt (0, "Breathy Flute", 0.4, 0.8).uni (0, 2, 0.08, 0.5).filt (0, "Low Pass 12", 4500, 0.15).env (0, 0.06, 0.6, 0.85, 0.7)
            .lfo (0, "Sine", 5.0, 0.3).mod ("LFO 1", "Osc A Pitch", 0.03); leadSpace (p);
          add (C, "Flute Loop Lead", "Flute Trap Lead", "a breathy flute with gentle vibrato: the classic trap flute loop", p); }
        { auto p = P(); p.wt (0, "Mallet", 0.4, 0.8).filt (0, "Low Pass 12", 5000, 0.1).env (0, 0.002, 0.45, 0.0, 0.5)
            .delay ("1/8", 0.3, 0.2).reverb (0.6, 0.5, 0.25, 15.0);
          add (C, "Marimba Plucks", "Pluck Trap Mallet", "a woody mallet pluck with short eighth delays", p); }
        { auto p = P(); p.an (0, "Saw", 0.75).uni (0, 5, 0.2, 0.9).filt (0, "MG Low 24", 1400, 0.3, 0.2).env (0, 0.4, 1.0, 0.85, 1.6)
            .eq (1.5, 700, 0, -3.0); padSpace (p);
          add (C, "Smoky Saw Pad", "Pad Trap Analog Warm", "a five-voice analog saw pad, low-passed and smoky", p); }
        { auto p = P(); p.smp ("Hat Closed", 0.8, false).set (ids::osc (0, "on"), 0).set (ids::smpOn, 1)
            .steps ("9393993993939939", "1121112111211121", "s", {}, "1/16", 16, 0.08, true).reverb (0.3, 0.6, 0.1, 0.0).trans (0.5, -0.2);
          add (C, "Ratchet Hat Loop", "Hats Trap Loop", "tempo-synced closed hats with ratchet rolls: hold one key", p); }
        { auto p = P(); p.wt (0, "Laser Sync", 0.5, 0.8).uni (0, 3, 0.15, 0.8).filt (0, "MG Low 24", 3800, 0.25).env (0, 0.005, 0.4, 0.7, 0.4)
            .mono (60).dist ("Overdrive", 0.3, 0.4); leadSpace (p);
          add (C, "Rage Sync Lead", "Lead Trap Sync", "a gritty hard-sync lead with overdrive, made for rage-style melodies", p); }
        { auto p = P(); p.wt (0, "Choir Formant", 0.6, 0.8).uni (0, 4, 0.15, 1.0).filt (0, "Low Pass 12", 2600, 0.1).env (0, 0.5, 1.2, 0.9, 2.2);
            padSpace (p); p.reverb (0.92, 0.45, 0.5, 60.0);
          add (C, "Angel Choir Hook", "Choir Trap Pad", "a lush formant choir for hooks and intros", p); }
    }

    //==========================================================================
    // TECHNO
    {
        const char* C = "TECHNO";
        { auto p = P(); p.an (0, "Saw", 0.8).filt (0, "MG Low 24", 700, 0.55, 0.35, 0.3).env (0, 0.002, 0.25, 0.0, 0.15).env (1, 0.0, 0.18, 0.0, 0.1)
            .mod ("ENV 2", "Filter 1 Cutoff", 0.55).sub ("Sine -1", 0.35).mono (0, false).dist ("Tube", 0.35, 0.45).delay ("1/16 D", 0.3, 0.12).reverb (0.4, 0.6, 0.1);
          add (C, "Acid Rumble Bass", "Bass Techno Acid", "a resonant ladder saw with a snappy filter envelope over a sub: the rolling techno bass", p); }
        { auto p = P(); p.an (0, "Square", 0.8).filt (0, "Low Pass 24", 500, 0.75, 0.4, 0.5).env (0, 0.002, 0.2, 0.3, 0.1).env (1, 0.0, 0.22, 0.0, 0.1)
            .mod ("ENV 2", "Filter 1 Cutoff", 0.65).mono (45).dist ("Diode", 0.4, 0.5).delay ("1/8 D", 0.35, 0.15).reverb (0.4, 0.6, 0.1);
          add (C, "Acid Squelch Line", "Bass Techno Acid Mono", "a square-wave acid line: high resonance, filter envelope, diode grit and slides", p); }
        { auto p = P(); p.wt (0, "Odd Harmonics", 0.5, 0.8).uni (0, 3, 0.12, 0.8).filt (0, "Band Pass", 1300, 0.4).env (0, 0.002, 0.18, 0.0, 0.2)
            .delay ("1/8 D", 0.55, 0.32).reverb (0.75, 0.4, 0.3, 20.0);
          add (C, "Warehouse Stab", "Stab Techno Chord", "a band-passed hollow chord stab into a long dotted delay", p); }
        { auto p = P(); p.an (0, "Saw", 0.75).uni (0, 7, 0.3, 1.0).filt (0, "MG Low 24", 900, 0.3).env (0, 2.0, 2.0, 0.9, 3.0)
            .lfoSync (0, "Sine", "4 bars", 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.35).sat (0.3, 0.6); padSpace (p);
          add (C, "Industrial Fog Pad", "Pad Techno Wide", "a seven-voice saw wall that opens and closes over four bars", p); }
        { auto p = P(); p.wt (0, "Reso PD", 0.4, 0.8).filt (0, "MG Low 24", 1800, 0.45).env (0, 0.002, 0.3, 0.0, 0.25)
            .arp ("Up/Down", "1/16", 2, 0.45).delay ("1/8 D", 0.4, 0.2).reverb (0.6, 0.5, 0.2);
          add (C, "Hypnotic Sequence", "Sequence Techno Arp", "a resonant phase-distortion blip arpeggiated in sixteenths: hold a chord", p); }
        { auto p = P(); p.an (0, "Sine", 0.85).tight (0, 0.25).env (1, 0.0, 0.04, 0.0, 0.03).mod ("ENV 2", "Osc A Pitch", 0.8)
            .env (0, 0.001, 0.5, 0.0, 0.3).dist ("Overdrive", 0.5, 0.6).mono (0, false).reverb (0.7, 0.5, 0.18, 10.0).eq (2.0, 400, -2.0, -2.0);
          add (C, "Rumble Kick Bass", "Bass Techno Kick", "a tuned sine kick-bass with overdrive and a rumbling reverb tail", p); }
        { auto p = P(); p.wt (0, "Pulse Grit", 0.5, 0.75).uni (0, 2, 0.08, 0.6).filt (0, "High Pass 12", 600, 0.2).env (0, 0.002, 0.12, 0.0, 0.1)
            .steps ("9070907090709070", "1", "s", { 0, 0, 12, 0, 7, 0, 0, 12 }, "1/16", 16, 0.0, true).delay ("1/16 D", 0.45, 0.25).reverb (0.5, 0.5, 0.18);
          add (C, "Metallic Tick Loop", "Percussion Techno Loop", "a gritty pulse ticking in a sixteenth pattern with pitched accents: hold one key", p); }
        { auto p = P(); p.wt (0, "Harmonic Sweep", 0.6, 0.8).uni (0, 3, 0.12, 0.8).filt (0, "MG Low 24", 2200, 0.3).env (0, 0.01, 0.6, 0.6, 0.5)
            .mono (30).dist ("Crunch", 0.35, 0.4); leadSpace (p);
          add (C, "Rave Signal Lead", "Lead Techno Rave", "a crunchy harmonic lead cutting through a peak-time mix", p); }
        { auto p = P(); p.smp ("Air Breath", 0.7, true).noise ("Pink", 0.15).set (ids::osc (0, "on"), 0).filt (0, "Band Pass", 1600, 0.3)
            .env (0, 1.0, 1.0, 0.9, 2.5).reverb (0.92, 0.35, 0.55, 40.0);
          add (C, "Concrete Air Drone", "Texture Techno Drone", "breath and pink noise through a band-pass, washed in reverb", p); }
        { auto p = P(); p.wt (0, "Hollow Organ", 0.4, 0.8).filt (0, "Low Pass 12", 2400, 0.25).env (0, 0.002, 0.25, 0.2, 0.3)
            .chorus (0.3, 0.4, 0.25).delay ("1/8 D", 0.5, 0.3).reverb (0.7, 0.5, 0.25);
          add (C, "Detroit Organ Stab", "Stab Techno Organ", "a short hollow organ chord stab, chorused into dotted delays", p); }
    }

    //==========================================================================
    // DUBSTEP
    {
        const char* C = "DUBSTEP";
        { auto p = P(); p.wt (0, "Neuro Growl", 0.4, 0.85).uni (0, 3, 0.15, 0.4).sub ("Sine -1", 0.5).filt (0, "MG Low 24", 1600, 0.35, 0.4)
            .env (0, 0.005, 0.5, 0.9, 0.2).mono (20).lfoSync (0, "Sine", "1/8", 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.55).mod ("LFO 1", "Osc A WT Pos", 0.3)
            .dist ("Fuzz", 0.4, 0.5).ott (0.45); bassSpace (p);
          add (C, "Classic Wub", "Bass Dubstep Wobble Mono", "an eighth-note wub: neuro table + filter LFO, fuzz and OTT over a sine sub", p); }
        { auto p = P(); p.wt (0, "Growl FM", 0.55, 0.85).sub ("Sine -1", 0.45).filt (0, "Formant", 900, 0.5, 0.3)
            .env (0, 0.005, 0.4, 0.9, 0.2).mono (15).lfoSync (0, "Saw Down", "1/4", 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.5)
            .dist ("Square Fuzz", 0.35, 0.45).ott (0.5); bassSpace (p);
          add (C, "Yoi Talker", "Bass Dubstep Vocal Mono", "a formant FM growl that talks on the beat: the 'yoi' bass", p); }
        { auto p = P(); p.wt (0, "Saw Fold", 0.5, 0.85).fold (0, 0.4).sub ("Sine -1", 0.5).filt (0, "MG Low 24", 2400, 0.25, 0.4)
            .env (0, 0.002, 0.3, 0.8, 0.15).mono (10).dist ("Hard Clip", 0.45, 0.5).ott (0.55).comp (-18, 4, 4); bassSpace (p);
          add (C, "Riddim Fold Bass", "Bass Dubstep Riddim", "a wavefolded saw, clipped and squashed: tight riddim bass", p); }
        { auto p = P(); p.an (0, "Sine", 0.95).tight (0, 0.25).sub ("Sine -1", 0.3).filt (0, "Low Pass 12", 300, 0.1).env (0, 0.005, 0.5, 1.0, 0.2)
            .mono (0, false).sat (0.2).reverb (0.3, 0.7, 0.1);
          add (C, "Clean Sub Drop", "Sub Bass Dubstep Mono", "a pure, round sine sub for the drop", p); }
        { auto p = P(); p.wt (0, "Odd Harmonics", 0.7, 0.8).uni (0, 2, 0.1, 0.4).tune (0, 1).sub ("Sine -1", 0.4).filt (0, "Comb +", 1800, 0.6, 0.3)
            .env (0, 0.002, 0.35, 0.85, 0.2).mono (12).lfoSync (0, "Square", "1/16", 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.5).dist ("Rectify", 0.35, 0.4).ott (0.45); bassSpace (p);
          add (C, "Metallic Screech", "Bass Dubstep Comb", "an octave-up odd-harmonic tone through a comb filter, chopped in sixteenths: screechy and metallic", p); }
        { auto p = P(); p.an (0, "Saw", 0.8).uni (0, 5, 0.25, 0.9).filt (0, "MG Low 24", 3000, 0.2).env (0, 0.005, 0.3, 0.7, 0.4)
            .mono (40).dist ("Overdrive", 0.3, 0.35); leadSpace (p);
          add (C, "Melodic Super Lead", "Lead Dubstep Melodic", "a five-voice supersaw lead for melodic dubstep", p); }
        { auto p = P(); p.wt (0, "Glass Halo", 0.5, 0.75).uni (0, 6, 0.22, 1.0).filt (0, "Low Pass 12", 3500, 0.1).env (0, 0.5, 1.2, 0.85, 2.0); padSpace (p);
          add (C, "Emotional Halo Pad", "Pad Dubstep Melodic", "a six-voice glass pad for melodic breakdowns", p); }
        { auto p = P(); p.wt (0, "Square Stack", 0.5, 0.8).uni (0, 4, 0.2, 0.9).filt (0, "Low Pass 12", 4000, 0.15).env (0, 0.002, 0.35, 0.0, 0.3)
            .delay ("1/8", 0.35, 0.2).reverb (0.7, 0.45, 0.3);
          add (C, "Chord Pluck Stack", "Pluck Dubstep Chords", "a stacked square chord pluck for melodic drops", p); }
        { auto p = P(); p.noise ("White", 0.6).set (ids::osc (0, "on"), 0).filt (0, "Band Pass", 600, 0.5).env (0, 2.5, 0.5, 1.0, 1.0)
            .env (1, 3.0, 0.1, 1.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.85).reverb (0.85, 0.35, 0.45, 30.0);
          add (C, "Build Noise Lift", "FX Dubstep Build", "white noise lifting from a band-pass up into the drop", p); }
        { auto p = P(); p.wt (0, "Talk Box", 0.6, 0.85).sub ("Sine -1", 0.4).filt (0, "Formant", 1100, 0.45, 0.35).env (0, 0.005, 0.4, 0.9, 0.2)
            .mono (25).lfoSync (0, "Triangle", "1/4 T", 1.0).mod ("LFO 1", "Osc A WT Pos", 0.5).dist ("Half Rectify", 0.3, 0.4).ott (0.4); bassSpace (p);
          add (C, "Mouth Bass Triplet", "Bass Dubstep Vocal Triplet", "a talk-box bass that changes vowels in quarter triplets", p); }
    }

    //==========================================================================
    // GARAGE (UKG / 2-step)
    {
        const char* C = "GARAGE";
        { auto p = P(); p.wt (0, "Organ Gospel", 0.4, 0.8).filt (0, "Low Pass 12", 3000, 0.2).env (0, 0.002, 0.3, 0.2, 0.25)
            .chorus (0.35, 0.4, 0.25).delay ("1/8 D", 0.35, 0.18).reverb (0.6, 0.5, 0.2);
          add (C, "Two-Step Organ Stab", "Stab Garage Organ", "a short gospel-organ chord stab with swing-friendly delays", p); }
        { auto p = P(); p.an (0, "Sine", 0.85).tight (0, 0.25).an (1, "Triangle", 0.35).tune (1, 1).sub ("Sine -1", 0.2).filt (0, "Low Pass 12", 800, 0.15)
            .env (0, 0.002, 0.45, 0.6, 0.2).mono (60).sat (0.3).reverb (0.3, 0.7, 0.1);
          add (C, "Speed Garage Sub", "Bass Garage Sub Mono", "a bouncy sine + triangle sub bass with glide", p); }
        { auto p = P(); p.wt (0, "Reese Core", 0.45, 0.8).uni (0, 3, 0.18, 0.4).sub ("Sine -1", 0.4).filt (0, "MG Low 24", 1200, 0.3, 0.3)
            .env (0, 0.003, 0.4, 0.75, 0.2).mono (50).lfoSync (0, "Sine", "1/8 D", 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.35).dist ("Tube", 0.3, 0.4); bassSpace (p);
          add (C, "Bumping Warp Bass", "Bass Garage Bouncy", "a detuned bass with a dotted-eighth filter bounce: speed garage swing", p); }
        { auto p = P(); p.wt (0, "Vowel Sweep", 0.5, 0.8).filt (0, "Formant", 1500, 0.4).env (0, 0.002, 0.2, 0.0, 0.2)
            .delay ("1/8", 0.3, 0.2).reverb (0.55, 0.5, 0.2);
          add (C, "Vocal Chop Pluck", "Pluck Garage Vocal", "a short formant 'chop' pluck, like a cut-up vocal", p); }
        { auto p = P(); p.wt (0, "Seventh Heaven", 0.4, 0.8).uni (0, 3, 0.12, 0.8).filt (0, "Low Pass 12", 3500, 0.15).env (0, 0.01, 0.8, 0.6, 1.0)
            .chorus (0.3, 0.45, 0.3).reverb (0.75, 0.5, 0.35, 25.0).sat (0.2);
          add (C, "Late Night Rhodes Chords", "Keys Garage Chords", "lush seventh-chord keys with chorus: the UKG chord sound", p); }
        { auto p = P(); p.smp ("Snap", 0.8, false).set (ids::osc (0, "on"), 0)
            .steps ("0090009000900099", "1", "s", {}, "1/16", 16, 0.18, true).reverb (0.5, 0.5, 0.15, 5.0).trans (0.4, 0.0);
          add (C, "Shuffled Snap Loop", "Percussion Garage Loop", "swung 2-step snaps with a double hit: hold one key", p); }
        { auto p = P(); p.wt (0, "PWM Wide", 0.5, 0.75).uni (0, 4, 0.2, 1.0).filt (0, "MG Low 24", 2600, 0.15).env (0, 0.35, 1.0, 0.85, 1.6); padSpace (p);
          add (C, "Silk PWM Pad", "Pad Garage Soft", "a wide pulse-width pad, soft and silky", p); }
        { auto p = P(); p.wt (0, "Glass FM", 0.4, 0.75).filt (0, "Low Pass 12", 6000, 0.1).env (0, 0.002, 0.8, 0.0, 0.8)
            .arp ("Up", "1/16", 2, 0.4, 0.15).delay ("1/8 D", 0.4, 0.22).reverb (0.7, 0.45, 0.3);
          add (C, "Shuffle Glass Arp", "Sequence Garage Arp", "a glassy FM arp with swing: hold a chord", p); }
        { auto p = P(); p.an (0, "Square", 0.7).pw (0, 0.35).uni (0, 2, 0.08, 0.6).filt (0, "MG Low 24", 2000, 0.25).env (0, 0.005, 0.4, 0.6, 0.4)
            .mono (45).sat (0.25); leadSpace (p);
          add (C, "UKG Whistle Lead", "Lead Garage Square", "a narrow pulse lead with a little glide, for top-line hooks", p); }
        { auto p = P(); p.wt (0, "Brass Swell", 0.5, 0.8).uni (0, 3, 0.15, 0.8).filt (0, "MG Low 24", 1800, 0.2).env (0, 0.08, 0.4, 0.6, 0.4)
            .chorus (0.3, 0.35, 0.2).reverb (0.6, 0.5, 0.25);
          add (C, "Brass Hit Chords", "Stab Garage Brass", "warm brass chord hits for garage drops", p); }
    }

    //==========================================================================
    // DARK GARAGE
    {
        const char* C = "DARK GARAGE";
        { auto p = P(); p.an (0, "Sine", 0.85).tight (0, 0.25).sub ("Sine -1", 0.3).filt (0, "Low Pass 12", 500, 0.2).env (0, 0.003, 0.6, 0.7, 0.25)
            .mono (80).dist ("Tape", 0.4, 0.5).reverb (0.35, 0.7, 0.1);
          add (C, "Shadow Sub Slide", "Bass Garage Sub Mono", "a saturated sine sub with long slides between notes", p); }
        { auto p = P(); p.wt (0, "Reese Core", 0.6, 0.8).uni (0, 4, 0.25, 0.5).sub ("Sine -1", 0.45).filt (0, "MG Low 24", 700, 0.35, 0.4)
            .env (0, 0.01, 0.5, 0.9, 0.3).mono (40).dist ("Diode", 0.35, 0.45); bassSpace (p);
          add (C, "Underpass Warp", "Bass Garage Detuned Mono", "a low, detuned bass growling under a 700 Hz ladder", p); }
        { auto p = P(); p.wt (0, "Choir Formant", 0.3, 0.75).uni (0, 3, 0.12, 1.0).filt (0, "Low Pass 12", 1800, 0.2).env (0, 0.8, 1.5, 0.8, 2.5)
            .fshift (-0.08, 0.15); padSpace (p); p.reverb (0.93, 0.5, 0.55, 80.0);
          add (C, "Night Bus Choir", "Pad Choir Garage", "a low choir, slightly frequency-shifted, in a long reverb", p); }
        { auto p = P(); p.wt (0, "Bell FM", 0.6, 0.7).filt (0, "Low Pass 12", 3000, 0.2).env (0, 0.002, 1.0, 0.0, 1.2)
            .delay ("1/4 D", 0.45, 0.3).reverb (0.85, 0.5, 0.4, 30.0).crush (12, 2, 0.3);
          add (C, "Rusty Bell", "Bell Garage Lo-Fi", "a low-passed FM bell, lightly bitcrushed, into dotted delays", p); }
        { auto p = P(); p.smp ("Rim", 0.8, false).set (ids::osc (0, "on"), 0)
            .steps ("0900090009000909", "1", "s", { 0, 0, 0, 3 }, "1/16", 16, 0.2, true).delay ("1/8 D", 0.3, 0.12).reverb (0.45, 0.6, 0.1).trans (0.4, -0.2);
          add (C, "Skippy Rim Loop", "Percussion Garage Loop", "skippy swung rimshots with a pitched ghost: hold one key", p); }
        { auto p = P(); p.wt (0, "Hollow Organ", 0.6, 0.8).filt (0, "Band Pass", 900, 0.4).env (0, 0.002, 0.2, 0.0, 0.2)
            .delay ("1/8 D", 0.55, 0.3).reverb (0.7, 0.5, 0.25).dist ("Tape", 0.3, 0.4);
          add (C, "Tunnel Organ Stab", "Stab Garage Organ", "a narrow band-passed organ stab echoing off concrete", p); }
        { auto p = P(); p.wt (0, "Pulse Grit", 0.3, 0.8).uni (0, 2, 0.1, 0.6).sub ("Square -1", 0.25).filt (0, "MG Low 24", 1000, 0.4, 0.3)
            .env (0, 0.002, 0.3, 0.5, 0.2).env (1, 0.0, 0.15, 0.0, 0.1).mod ("ENV 2", "Filter 1 Cutoff", 0.4).mono (30).dist ("Crunch", 0.35, 0.45); bassSpace (p);
          add (C, "Grime Square Bass", "Bass Grime Square", "a gritty pulse bass with a square sub: hard grime-garage energy", p); }
        { auto p = P(); p.noise ("Dark", 0.25).smp ("Vinyl Crackle", 0.4, false).set (ids::osc (0, "on"), 0).filt (0, "Low Pass 12", 2500, 0.1)
            .env (0, 1.5, 1.0, 1.0, 2.0).reverb (0.8, 0.5, 0.4);
          add (C, "Rainy Street Hiss", "Texture Garage Vinyl", "vinyl crackle over low noise: rainy-night atmosphere", p); }
        { auto p = P(); p.an (0, "Triangle", 0.8).uni (0, 3, 0.1, 0.7).an (1, "Saw", 0.25).tune (1, 0, 3).filt (0, "MG Low 24", 1500, 0.2)
            .env (0, 0.2, 1.0, 0.8, 1.5); padSpace (p);
          add (C, "Minor Third Haze", "Pad Garage Minor", "a soft triangle pad with a minor third layered in", p); }
        { auto p = P(); p.wt (0, "Laser Sync", 0.3, 0.7).filt (0, "MG Low 24", 2200, 0.35).env (0, 0.005, 0.3, 0.5, 0.4).mono (60).dist ("Overdrive", 0.25, 0.3); leadSpace (p);
          add (C, "Smoke Signal Lead", "Lead Garage Sync", "a restrained sync lead with glide, sitting low in the mix", p); }
    }

    //==========================================================================
    // DNB
    {
        const char* C = "DNB";
        { auto p = P(); p.wt (0, "Reese Core", 0.5, 0.85).uni (0, 5, 0.3, 0.5).sub ("Sine -1", 0.5).filt (0, "MG Low 24", 1400, 0.3, 0.4)
            .env (0, 0.005, 0.5, 0.9, 0.25).mono (20).phaser (0.15, 0.6, 700, 0.4, 0.3).dist ("Tube", 0.35, 0.45); bassSpace (p);
          add (C, "Neuro Pipe Bass", "Bass DnB Detuned Mono", "a five-voice detuned bass with a slow phaser over a sine sub", p); }
        { auto p = P(); p.wt (0, "Neuro Growl", 0.6, 0.85).sub ("Sine -1", 0.5).filt (0, "Comb -", 400, 0.5, 0.35).env (0, 0.002, 0.3, 0.85, 0.15)
            .mono (10).lfoSync (0, "Saw Up", "1/16", 1.0).mod ("LFO 1", "Osc A WT Pos", 0.45).dist ("Fuzz", 0.4, 0.5).ott (0.5); bassSpace (p);
          add (C, "Neurofunk Growl", "Bass DnB Neuro", "a comb-filtered neuro growl rippling in sixteenths", p); }
        { auto p = P(); p.an (0, "Sine", 0.9).tight (0, 0.25).sub ("Sine -1", 0.2).filt (0, "Low Pass 12", 350, 0.1).env (0, 0.003, 0.5, 0.9, 0.15)
            .mono (35).sat (0.35).reverb (0.3, 0.7, 0.1);
          add (C, "Liquid Sub", "Sub Bass DnB Mono", "a warm, slightly saturated sine sub for liquid rollers", p); }
        { auto p = P(); p.wt (0, "E-Piano Tine", 0.5, 0.8).uni (0, 2, 0.08, 0.7).filt (0, "Low Pass 12", 4500, 0.1).env (0, 0.005, 1.0, 0.4, 1.2)
            .chorus (0.3, 0.5, 0.3).reverb (0.85, 0.5, 0.42, 30.0);
          add (C, "Liquid Keys", "Keys DnB Liquid", "soft chorused e-piano for liquid drum and bass", p); }
        { auto p = P(); p.wt (0, "String Ensemble", 0.4, 0.8).uni (0, 6, 0.22, 1.0).filt (0, "Low Pass 12", 3000, 0.1).env (0, 0.7, 1.5, 0.9, 2.5); padSpace (p);
          add (C, "Atmos Strings", "Pad DnB Strings", "wide string pad for atmospheric intros", p); }
        { auto p = P(); p.an (0, "Saw", 0.8).uni (0, 3, 0.15, 0.8).filt (0, "MG Low 24", 2800, 0.3).env (0, 0.002, 0.25, 0.6, 0.3)
            .mono (35).dist ("Overdrive", 0.3, 0.35); leadSpace (p);
          add (C, "Jump Up Lead", "Lead DnB Saw", "a punchy detuned saw lead for jump-up hooks", p); }
        { auto p = P(); p.smp ("Shaker", 0.7, false).set (ids::osc (0, "on"), 0)
            .steps ("9595959595959595", "1111111111112111", "s", {}, "1/16", 16, 0.0, true).reverb (0.35, 0.6, 0.1).trans (0.5, -0.2);
          add (C, "Tight Shaker Loop", "Percussion DnB Loop", "a tight 174-ready sixteenth shaker with accents: hold one key", p); }
        { auto p = P(); p.wt (0, "Glass Halo", 0.4, 0.7).filt (0, "Low Pass 12", 5000, 0.1).env (0, 0.002, 0.5, 0.0, 0.5)
            .arp ("Up", "1/16", 3, 0.4).delay ("1/8 D", 0.45, 0.25).reverb (0.75, 0.45, 0.3);
          add (C, "Halo Arp", "Sequence DnB Arp", "a three-octave glass arp sparkling over the breaks: hold a chord", p); }
        { auto p = P(); p.wt (0, "Growl FM", 0.3, 0.85).uni (0, 2, 0.12, 0.4).sub ("Sine -1", 0.45).filt (0, "MG Low 24", 1800, 0.35, 0.4)
            .env (0, 0.002, 0.25, 0.8, 0.15).env (1, 0.0, 0.12, 0.0, 0.1).mod ("ENV 2", "Filter 1 Cutoff", 0.5).mono (15).dist ("Square Fuzz", 0.35, 0.45).ott (0.4); bassSpace (p);
          add (C, "Foghorn Stab Bass", "Bass DnB Stab", "a short FM stab bass with a filter snap, for halftime and jump-up", p); }
        { auto p = P(); p.noise ("Bright", 0.5).set (ids::osc (0, "on"), 0).filt (0, "High Pass 12", 300, 0.3).env (0, 0.002, 0.6, 0.0, 0.8)
            .env (1, 0.0, 0.6, 0.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.6).reverb (0.9, 0.3, 0.5, 20.0);
          add (C, "Snare Crash Fill", "FX DnB Noise", "a bright noise hit with a long reverb tail, for fills and transitions", p); }
    }

    //==========================================================================
    // CINEMATIC
    {
        const char* C = "CINEMATIC";
        { auto p = P(); p.an (0, "Sine", 0.8).sub ("Sine -2", 0.5).noise ("Dark", 0.2).filt (0, "Low Pass 12", 300, 0.2).env (0, 0.002, 2.5, 0.0, 2.5)
            .dist ("Tape", 0.4, 0.5).reverb (0.95, 0.4, 0.45, 20.0).env (1, 0.0, 0.4, 0.0, 0.3).mod ("ENV 2", "Osc A Pitch", 0.8);
          add (C, "Trailer Boom", "FX Cinematic Boom", "a sub boom with a pitch drop and dark noise into a huge reverb", p); }
        { auto p = P(); p.wt (0, "Brass Swell", 0.6, 0.85).uni (0, 5, 0.18, 0.9).sub ("Sine -1", 0.2).filt (0, "MG Low 24", 1600, 0.2).env (0, 0.25, 1.0, 0.9, 1.8)
            .sat (0.3, 0.6); padSpace (p); p.reverb (0.9, 0.45, 0.42, 30.0);
          add (C, "Epic Brass Ensemble", "Brass Cinematic Epic", "a five-voice brass ensemble with a slow bloom", p); }
        { auto p = P(); p.wt (0, "String Ensemble", 0.6, 0.8).uni (0, 7, 0.25, 1.0).filt (0, "Low Pass 12", 2800, 0.1).env (0, 1.2, 2.0, 0.9, 3.0)
            .lfo (0, "Sine", 0.08, 1.0).mod ("LFO 1", "Filter 1 Cutoff", 0.2); padSpace (p); p.reverb (0.95, 0.45, 0.55, 60.0);
          add (C, "Rising Tension Strings", "Strings Cinematic Pad", "a seven-voice string section with slow filter motion", p); }
        { auto p = P(); p.wt (0, "Choir Formant", 0.7, 0.8).uni (0, 6, 0.18, 1.0).filt (0, "Low Pass 12", 2400, 0.1).env (0, 1.5, 2.0, 0.9, 3.5); padSpace (p); p.reverb (0.97, 0.4, 0.6, 80.0);
          add (C, "Requiem Voices", "Choir Cinematic Pad", "a vast six-voice choir in an endless reverb", p); }
        { auto p = P(); p.wt (0, "Music Box", 0.4, 0.75).filt (0, "Low Pass 12", 5000, 0.1).env (0, 0.002, 1.6, 0.0, 1.6)
            .delay ("1/4", 0.35, 0.22).reverb (0.88, 0.4, 0.45, 30.0).chorus (0.2, 0.3, 0.2);
          add (C, "Haunted Music Box", "Bell Cinematic Music Box", "a fragile music box in a big reverb: horror and fairy-tale", p); }
        { auto p = P(); p.wt (0, "Organ Gospel", 0.6, 0.75).wt (1, "Pipe Organ", 0.5, 0.45).tune (0, 1).filt (0, "Low Pass 12", 7000, 0.1).env (0, 0.04, 1.0, 1.0, 2.0)
            .chorus (0.6, 0.5, 0.3).reverb (0.92, 0.4, 0.5, 40.0);
          add (C, "Gothic Organ", "Organ Cinematic Gothic", "a bright, high organ over a pipe organ, with a fast rotary-style chorus in a vast room", p); }
        { auto p = P(); p.noise ("Pink", 0.6).set (ids::osc (0, "on"), 0).filt (0, "Low Pass 12", 300, 0.3).env (0, 4.0, 0.5, 1.0, 1.5)
            .env (1, 4.0, 0.1, 1.0, 0.5).mod ("ENV 2", "Filter 1 Cutoff", 0.9).reverb (0.9, 0.35, 0.45, 30.0);
          add (C, "Tension Riser", "FX Cinematic Riser", "pink noise rising from a low-pass up over four seconds", p); }
        { auto p = P(); p.an (0, "Saw", 0.75).uni (0, 3, 0.1, 0.8).sub ("Sine -1", 0.5).filt (0, "MG Low 24", 600, 0.3, 0.3)
            .env (0, 0.01, 0.4, 0.9, 0.5).mono (0, false).lfoSync (0, "Square", "1/16", 1.0).mod ("LFO 1", "Amp", 0.6).reverb (0.6, 0.5, 0.15); p.stereo (1.0, true, 140.0);
          add (C, "Ostinato Pulse Bass", "Bass Cinematic Ostinato", "a sixteenth-note pulsing saw bass: the action-trailer ostinato", p); }
        { auto p = P(); p.wt (0, "Kalimba", 0.4, 0.75).wt (1, "Glass Halo", 0.5, 0.35).tune (1, 1).filt (0, "Low Pass 12", 6000, 0.1).env (0, 0.002, 0.9, 0.0, 1.2)
            .arp ("Up/Down", "1/8", 2, 0.5).delay ("1/8 D", 0.45, 0.25).reverb (0.85, 0.45, 0.4);
          add (C, "Wonder Arp", "Sequence Cinematic Arp", "a kalimba + glass arp, light and magical: hold a chord", p); }
        { auto p = P(); p.smp ("Impact Boom", 0.9, false).set (ids::osc (0, "on"), 0).reverb (0.92, 0.4, 0.45, 10.0).eq (2.0, 500, -1.0, -2.0);
          add (C, "Final Impact", "FX Cinematic Impact", "a heavy impact hit with a long tail", p); }
    }
}
