#include "Parameters.h"
#include "DSP/SoulEngine.h"
#include "DSP/FxCatalog.h"
#include "DSP/Arpeggiator.h"

namespace divinity
{
using APF  = juce::AudioParameterFloat;
using APC  = juce::AudioParameterChoice;
using APB  = juce::AudioParameterBool;
using API  = juce::AudioParameterInt;
using Attr = juce::AudioParameterFloatAttributes;

//==============================================================================
juce::StringArray modSourceNames()
{
    return { "-", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV 1", "ENV 2", "ENV 3",
             "Velocity", "Mod Wheel", "Aftertouch", "Pitch Bend", "Note",
             "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Soul X", "Soul Y" };
}

juce::StringArray modDestNames()
{
    return { "-",
             "Osc A Pitch", "Osc B Pitch", "Osc C Pitch",
             "Osc A WT Pos", "Osc B WT Pos", "Osc C WT Pos",
             "Osc A Level", "Osc B Level", "Osc C Level",
             "Osc A Fold", "Osc B Fold", "Osc C Fold",
             "Osc A PWM", "Osc B PWM", "Osc C PWM",
             "Osc A Detune", "Osc B Detune", "Osc C Detune",
             "Sub Level", "Noise Level",
             "Filter 1 Cutoff", "Filter 1 Res", "Filter 1 Drive",
             "Filter 2 Cutoff", "Filter 2 Res", "Filter 2 Drive",
             "Amp", "Pan",
             "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "LFO 4 Rate",
             "Saturation", "Chorus Depth", "Delay Mix", "Reverb Mix",
             "Soul X", "Soul Y", "Divine Amount" };
}

juce::StringArray wavetableNames()
{
    return { "Basic Shapes", "Harmonic Sweep", "Formant Vox", "Pulse Grit", "Glass Halo", "Hollow Organ" };
}

juce::StringArray filterTypeNames()
{
    return { "Off", "MG Low 24", "Low Pass 12", "High Pass 12", "Band Pass", "Notch", "Peak", "Comb" };
}

static const double kLfoBeats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.5, 0.75, 2.0 / 3.0, 1.0 / 3.0 };

juce::StringArray lfoDivisionNames()
{
    return { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 D", "1/8 D", "1/4 T", "1/8 T" };
}

double lfoDivisionInBeats (int index)
{
    return kLfoBeats[juce::jlimit (0, (int) std::size (kLfoBeats) - 1, index)];
}

static const double kDelayBeats[] = { 0.125, 0.25, 0.5, 1.0, 0.1875, 0.375, 0.75, 1.5, 1.0 / 6.0, 1.0 / 3.0, 2.0 / 3.0 };

juce::StringArray delayDivisionNames()
{
    return { "1/32", "1/16", "1/8", "1/4", "1/16 D", "1/8 D", "1/4 D", "1/2 D", "1/16 T", "1/8 T", "1/4 T" };
}

double delayDivisionInBeats (int index)
{
    return kDelayBeats[juce::jlimit (0, (int) std::size (kDelayBeats) - 1, index)];
}

juce::StringArray subModeNames()
{
    return { "Sine -1", "Sine -2", "Triangle -1", "Triangle -2", "Square -1", "Square -2" };
}

bool isGlobalDest (ModDest d)
{
    return d == ModDest::SatDrive || d == ModDest::ChorusDepth || d == ModDest::DelayMix || d == ModDest::ReverbMix
        || d == ModDest::Lfo1Rate || d == ModDest::Lfo2Rate || d == ModDest::Lfo3Rate || d == ModDest::Lfo4Rate
        || d == ModDest::SoulX || d == ModDest::SoulY || d == ModDest::DivineAmount;
}

//==============================================================================
// Value formatting helpers — consistent across the whole plugin.
static Attr pct()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; })
                 .withLabel ("%");
}

static Attr bipolarPct()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     auto n = juce::roundToInt (v * 100.0f);
                     return (n > 0 ? "+" : "") + juce::String (n) + "%";
                 })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; });
}

static Attr seconds()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     if (v < 1.0f) return juce::String (juce::roundToInt (v * 1000.0f)) + " ms";
                     return juce::String (v, 2) + " s";
                 })
                 .withValueFromStringFunction ([] (const juce::String& s)
                 {
                     auto v = s.retainCharacters ("0123456789.").getFloatValue();
                     return s.containsIgnoreCase ("ms") ? v / 1000.0f : v;
                 });
}

static Attr hz()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                     if (v < 10.0f)    return juce::String (v, 2) + " Hz";
                     return juce::String (juce::roundToInt (v)) + " Hz";
                 })
                 .withValueFromStringFunction ([] (const juce::String& s)
                 {
                     auto v = s.retainCharacters ("0123456789.").getFloatValue();
                     return s.containsIgnoreCase ("k") ? v * 1000.0f : v;
                 })
                 .withLabel ("Hz");
}

static Attr ms()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("0123456789.").getFloatValue(); })
                 .withLabel ("ms");
}

static Attr db()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); })
                 .withLabel ("dB");
}

static Attr cents()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); });
}

static juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, kParamVersion }; };

    // ---- Oscillators --------------------------------------------------------
    for (int i = 0; i < kNumOscs; ++i)
    {
        const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + i));
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("osc" + letter, "Osc " + letter, " | ");
        const bool defaultOn = (i == 0);

        group->addChild (std::make_unique<APB> (pid (ids::osc (i, "on")), "Osc " + letter + " On", defaultOn));
        group->addChild (std::make_unique<APC> (pid (ids::osc (i, "engine")), "Osc " + letter + " Engine",
                                                juce::StringArray { "Wavetable", "Analog" }, i == 2 ? 1 : 0));
        group->addChild (std::make_unique<APC> (pid (ids::osc (i, "table")), "Osc " + letter + " Wavetable", wavetableNames(), 0));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "pos")), "Osc " + letter + " WT Position",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "fold")), "Osc " + letter + " Fold",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct()));
        group->addChild (std::make_unique<APC> (pid (ids::osc (i, "wave")), "Osc " + letter + " Analog Wave",
                                                juce::StringArray { "Sine", "Triangle", "Saw", "Square", "Pulse" }, 2));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "pw")), "Osc " + letter + " Pulse Width",
                                                juce::NormalisableRange<float> (0.05f, 0.95f), 0.5f, pct()));
        group->addChild (std::make_unique<API> (pid (ids::osc (i, "unison")), "Osc " + letter + " Unison", 1, kMaxUnison, 1));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "detune")), "Osc " + letter + " Detune",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "width")), "Osc " + letter + " Stereo Width",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "phase")), "Osc " + letter + " Phase",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 360.0f)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); })));
        group->addChild (std::make_unique<APB> (pid (ids::osc (i, "randphase")), "Osc " + letter + " Random Phase", true));
        group->addChild (std::make_unique<API> (pid (ids::osc (i, "oct")), "Osc " + letter + " Octave", -3, 3, 0));
        group->addChild (std::make_unique<API> (pid (ids::osc (i, "semi")), "Osc " + letter + " Semitone", -12, 12, 0));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "fine")), "Osc " + letter + " Fine",
                                                juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f, cents()));
        group->addChild (std::make_unique<APF> (pid (ids::osc (i, "level")), "Osc " + letter + " Level",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f, pct()));
        layout.add (std::move (group));
    }

    // ---- Sub / Noise --------------------------------------------------------
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("subnoise", "Sub / Noise", " | ");
        group->addChild (std::make_unique<APB> (pid (ids::subOn), "Sub On", false));
        group->addChild (std::make_unique<APC> (pid (ids::subMode), "Sub Mode", subModeNames(), 0));
        group->addChild (std::make_unique<APF> (pid (ids::subLevel), "Sub Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct()));
        group->addChild (std::make_unique<APB> (pid (ids::noiseOn), "Noise On", false));
        group->addChild (std::make_unique<APC> (pid (ids::noiseType), "Noise Type", juce::StringArray { "White", "Pink", "Bright", "Dark" }, 0));
        group->addChild (std::make_unique<APF> (pid (ids::noiseLevel), "Noise Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct()));
        layout.add (std::move (group));
    }

    // ---- Filters ------------------------------------------------------------
    for (int i = 0; i < 2; ++i)
    {
        const auto n = juce::String (i + 1);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("filter" + n, "Filter " + n, " | ");
        group->addChild (std::make_unique<APC> (pid (ids::filt (i, "type")), "Filter " + n + " Type", filterTypeNames(), i == 0 ? 1 : 0));
        group->addChild (std::make_unique<APF> (pid (ids::filt (i, "cutoff")), "Filter " + n + " Cutoff",
                                                skewed (20.0f, 20000.0f, 1000.0f), i == 0 ? 12000.0f : 2000.0f, hz()));
        group->addChild (std::make_unique<APF> (pid (ids::filt (i, "res")), "Filter " + n + " Resonance",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::filt (i, "drive")), "Filter " + n + " Drive",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::filt (i, "mix")), "Filter " + n + " Mix",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::filt (i, "keytrack")), "Filter " + n + " Key Track",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct()));
        layout.add (std::move (group));
    }
    layout.add (std::make_unique<APC> (pid (ids::filterRouting), "Filter Routing", juce::StringArray { "Serial", "Parallel" }, 0));

    // ---- Envelopes ----------------------------------------------------------
    for (int i = 0; i < kNumEnvs; ++i)
    {
        const auto n = juce::String (i + 1);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("env" + n, "Env " + n, " | ");
        const bool amp = (i == 0);
        group->addChild (std::make_unique<APF> (pid (ids::env (i, "attack")), "Env " + n + " Attack",
                                                skewed (0.0005f, 20.0f, 0.5f), amp ? 0.005f : 0.01f, seconds()));
        group->addChild (std::make_unique<APF> (pid (ids::env (i, "decay")), "Env " + n + " Decay",
                                                skewed (0.001f, 20.0f, 0.8f), amp ? 0.4f : 0.6f, seconds()));
        group->addChild (std::make_unique<APF> (pid (ids::env (i, "sustain")), "Env " + n + " Sustain",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), amp ? 0.8f : 0.0f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::env (i, "release")), "Env " + n + " Release",
                                                skewed (0.001f, 30.0f, 1.0f), amp ? 0.25f : 0.4f, seconds()));
        group->addChild (std::make_unique<APF> (pid (ids::env (i, "curve")), "Env " + n + " Curve",
                                                juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPct()));
        layout.add (std::move (group));
    }

    // ---- LFOs ---------------------------------------------------------------
    for (int i = 0; i < kNumLfos; ++i)
    {
        const auto n = juce::String (i + 1);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("lfo" + n, "LFO " + n, " | ");
        group->addChild (std::make_unique<APC> (pid (ids::lfo (i, "shape")), "LFO " + n + " Shape",
                                                juce::StringArray { "Sine", "Square", "Triangle", "Random" }, 0));
        group->addChild (std::make_unique<APF> (pid (ids::lfo (i, "rate")), "LFO " + n + " Rate",
                                                skewed (0.01f, 30.0f, 2.0f), 1.0f, hz()));
        group->addChild (std::make_unique<APB> (pid (ids::lfo (i, "sync")), "LFO " + n + " Sync", false));
        group->addChild (std::make_unique<APC> (pid (ids::lfo (i, "div")), "LFO " + n + " Division", lfoDivisionNames(), 4));
        group->addChild (std::make_unique<APF> (pid (ids::lfo (i, "depth")), "LFO " + n + " Depth",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::lfo (i, "phase")), "LFO " + n + " Phase",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 360.0f)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); })));
        group->addChild (std::make_unique<APF> (pid (ids::lfo (i, "smooth")), "LFO " + n + " Smooth",
                                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct()));
        layout.add (std::move (group));
    }

    // ---- Modulation matrix --------------------------------------------------
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("modmatrix", "Mod Matrix", " | ");
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const auto n = juce::String (i + 1);
            group->addChild (std::make_unique<APC> (pid (ids::mod (i, "src")), "Mod " + n + " Source", modSourceNames(), 0));
            group->addChild (std::make_unique<APC> (pid (ids::mod (i, "dst")), "Mod " + n + " Destination", modDestNames(), 0));
            group->addChild (std::make_unique<APF> (pid (ids::mod (i, "amt")), "Mod " + n + " Amount",
                                                    juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPct()));
        }
        layout.add (std::move (group));
    }

    // ---- Macros -------------------------------------------------------------
    {
        const char* names[] = { "Tone", "Movement", "Texture", "Space" };
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("macros", "Macros", " | ");
        for (int i = 0; i < kNumMacros; ++i)
            group->addChild (std::make_unique<APF> (pid (ids::macro (i)), juce::String ("Macro ") + juce::String (i + 1) + " " + names[i],
                                                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct()));
        layout.add (std::move (group));
    }

    // ---- Effects ------------------------------------------------------------
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("fx", "Effects", " | ");
        group->addChild (std::make_unique<APB> (pid (ids::satOn), "Saturation On", true));
        group->addChild (std::make_unique<APF> (pid (ids::satDrive), "Saturation Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::satMix), "Saturation Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct()));

        group->addChild (std::make_unique<APB> (pid (ids::choOn), "Chorus On", false));
        group->addChild (std::make_unique<APF> (pid (ids::choRate), "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.6f, hz()));
        group->addChild (std::make_unique<APF> (pid (ids::choDepth), "Chorus Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::choMix), "Chorus Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct()));

        group->addChild (std::make_unique<APB> (pid (ids::dlyOn), "Delay On", false));
        group->addChild (std::make_unique<APC> (pid (ids::dlyDiv), "Delay Time", delayDivisionNames(), 3));
        group->addChild (std::make_unique<APF> (pid (ids::dlyFeedback), "Delay Feedback", juce::NormalisableRange<float> (0.0f, 0.95f), 0.35f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::dlyMix), "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct()));
        group->addChild (std::make_unique<APB> (pid (ids::dlyPingPong), "Delay Ping-Pong", true));
        group->addChild (std::make_unique<APF> (pid (ids::dlyTone), "Delay Tone", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct()));

        group->addChild (std::make_unique<APB> (pid (ids::revOn), "Reverb On", false));
        group->addChild (std::make_unique<APF> (pid (ids::revSize), "Reverb Size", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::revDamp), "Reverb Damping", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::revWidth), "Reverb Width", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::revMix), "Reverb Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct()));
        group->addChild (std::make_unique<APF> (pid (ids::revPredelay), "Reverb Pre-Delay", juce::NormalisableRange<float> (0.0f, 250.0f, 0.0f, 0.6f), 18.0f, ms()));

        group->addChild (std::make_unique<APB> (pid (ids::limOn), "Limiter On", true));
        group->addChild (std::make_unique<APF> (pid (ids::limCeiling), "Limiter Ceiling", juce::NormalisableRange<float> (-12.0f, 0.0f), -0.3f, db()));
        layout.add (std::move (group));
    }

    // ---- Global -------------------------------------------------------------
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", " | ");
        group->addChild (std::make_unique<API> (pid (ids::voices), "Voices", 1, kMaxVoices, 8));
        group->addChild (std::make_unique<API> (pid (ids::bendRange), "Bend Range", 0, 24, 2));
        group->addChild (std::make_unique<APF> (pid (ids::glide), "Glide", skewed (0.0f, 2000.0f, 200.0f), 0.0f, ms()));
        group->addChild (std::make_unique<APC> (pid (ids::voiceMode), "Voice Mode", juce::StringArray { "Poly", "Legato", "Mono" }, 0));
        group->addChild (std::make_unique<APF> (pid (ids::masterTune), "Master Tune", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f, cents()));
        group->addChild (std::make_unique<APF> (pid (ids::master), "Master Volume", juce::NormalisableRange<float> (-60.0f, 6.0f, 0.0f, 2.5f), -6.0f, db()));
        group->addChild (std::make_unique<APC> (pid (ids::engineMode), "Engine", juce::StringArray { "Classic 1.0", "Divine 2.0" }, 1));
        group->addChild (std::make_unique<APF> (pid (ids::analog), "Analog Drift", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct()));
        layout.add (std::move (group));
    }

    addSoulParameters (layout);
    addFxRackParameters (layout);
    addArpParameters (layout);
    return layout;
}

//==============================================================================
juce::String helpTextFor (const juce::String& id)
{
    auto has = [&id] (const char* s) { return id.endsWith (s); };

    if (id.startsWith ("osc"))
    {
        if (has ("_pos"))       return "Wavetable Position: scans through the frames of the selected wavetable. Modulate it for evolving, animated tones.";
        if (has ("_table"))     return "Wavetable: picks the set of waveforms this oscillator scans through.";
        if (has ("_fold"))      return "Fold: wavefolds the oscillator, adding bright harmonics. Small amounts add bite; large amounts get metallic.";
        if (has ("_unison"))    return "Unison: stacks several detuned copies of this oscillator for a thicker, wider sound.";
        if (has ("_detune"))    return "Detune: how far apart the unison voices are tuned. Higher values sound wider and more chorused.";
        if (has ("_width"))     return "Stereo Width: spreads the unison voices across the stereo field.";
        if (has ("_level"))     return "Level: how loud this oscillator is in the mix.";
        if (has ("_oct"))       return "Octave: shifts this oscillator up or down in whole octaves.";
        if (has ("_semi"))      return "Semitone: transposes this oscillator in semitones. Try +7 for a fifth.";
        if (has ("_fine"))      return "Fine: small pitch offset in cents for subtle beating against other oscillators.";
        if (has ("_pw"))        return "Pulse Width: changes the shape of square/pulse waves. Move it for classic PWM movement.";
        if (has ("_phase"))     return "Phase: where in its cycle the waveform starts on each note. Affects punch on bass sounds.";
        if (has ("_randphase")) return "Random Phase: starts each note at a random point. Natural for pads, turn off for tight bass.";
        if (has ("_engine"))    return "Engine: choose Wavetable (scannable waveforms) or Analog (classic sine/triangle/saw/square/pulse).";
        if (has ("_wave"))      return "Analog Wave: the classic waveform this oscillator generates.";
        if (has ("_on"))        return "Turns this oscillator on or off.";
    }
    if (id.startsWith ("filter"))
    {
        if (has ("_cutoff"))   return "Controls which frequencies pass through the filter. Lower values generally create darker sounds.";
        if (has ("_res"))      return "Resonance: boosts frequencies around the cutoff. High values whistle and sing.";
        if (has ("_drive"))    return "Drive: pushes the filter input for warmth and grit.";
        if (has ("_mix"))      return "Mix: blends the filtered signal with the unfiltered signal.";
        if (has ("_keytrack")) return "Key Track: makes the cutoff follow the notes you play, so high notes stay bright.";
        if (has ("_type"))     return "Filter Type: MG Low 24 is a warm 4-pole ladder; others are clean state-variable shapes. Comb adds metallic resonance.";
    }
    if (id == ids::filterRouting) return "Filter Routing: Serial runs Filter 1 into Filter 2. Parallel runs them side by side.";
    if (id.startsWith ("env"))
    {
        if (has ("_attack"))  return "Attack: how long the sound takes to fade in after you press a key.";
        if (has ("_decay"))   return "Decay: how long it takes to fall from the peak to the sustain level.";
        if (has ("_sustain")) return "Sustain: the level held while the key is down.";
        if (has ("_release")) return "Release: how long the sound rings after you let go.";
        if (has ("_curve"))   return "Curve: bends the envelope segments. Negative = snappy, positive = slow and swelling.";
    }
    if (id.startsWith ("lfo"))
    {
        if (has ("_rate"))   return "Rate: how fast the LFO moves.";
        if (has ("_depth"))  return "Depth: scales this LFO's output everywhere it is routed.";
        if (has ("_phase"))  return "Phase: offsets where the LFO cycle starts.";
        if (has ("_smooth")) return "Smooth: rounds off sharp LFO edges to avoid clicks and make motion softer.";
        if (has ("_sync"))   return "Sync: locks the LFO to your project tempo.";
        if (has ("_shape"))  return "Shape: the waveform the LFO follows.";
        if (has ("_div"))    return "Division: the tempo-synced LFO cycle length.";
    }
    if (id.startsWith ("mod"))
    {
        if (has ("_src")) return "Mod Source: what moves the destination (an LFO, envelope, velocity, macro...).";
        if (has ("_dst")) return "Mod Destination: the parameter this slot moves.";
        if (has ("_amt")) return "Mod Amount: how much and in which direction the source moves the destination.";
    }
    if (id == ids::macro (0)) return "TONE: brightens the whole sound by opening both filters.";
    if (id == ids::macro (1)) return "MOVEMENT: increases LFO motion and adds chorus shimmer.";
    if (id == ids::macro (2)) return "TEXTURE: adds harmonic grit with wavefolding and saturation.";
    if (id == ids::macro (3)) return "SPACE: adds reverb, delay and stereo width.";
    if (id == ids::soulX)         return "SOUL ENGINE horizontal axis: left is SOFT, right is AGGRESSIVE. Centre leaves the patch untouched.";
    if (id == ids::soulY)         return "SOUL ENGINE vertical axis: down is MELANCHOLY, up is EUPHORIA. Centre leaves the patch untouched.";
    if (id == ids::soulIntensity) return "Soul Intensity: how strongly the XY position changes the sound.";
    if (id == ids::soulCurve)     return "Soul Curve: negative reacts strongly near the centre, positive keeps the centre calm and saves the drama for the edges.";
    if (id == ids::soulSmooth)    return "Soul Smoothing: how long the sound takes to follow the XY pad. Longer = slow emotional morphs.";
    if (id == ids::divineAmount)  return "DIVINE INTERVENTION: at 0% the patch is untouched; at 100% the selected transformation is fully active.";
    if (id == ids::divineMode)    return "Divine Mode: which transformation Divine Intervention performs.";
    if (id.startsWith ("soul_"))
        for (int t = 0; t < kNumSoulTargets; ++t)
        {
            const auto& tg = soulTargets()[(size_t) t];
            if (id == ids::soul (t, "on")) return juce::String (tg.name) + ": include this target in the SOUL ENGINE. " + tg.help;
            if (id == ids::soul (t, "x"))  return juce::String (tg.name) + " amount toward AGGRESSIVE (negative = toward SOFT). " + tg.help;
            if (id == ids::soul (t, "y"))  return juce::String (tg.name) + " amount toward EUPHORIA (negative = toward MELANCHOLY). " + tg.help;
        }
    if (id == ids::subOn)      return "Turns the sub oscillator on or off.";
    if (id == ids::noiseOn)    return "Turns the noise layer on or off.";
    if (id == ids::satOn)      return "Turns the saturation effect on or off.";
    if (id == ids::satMix)     return "Saturation Mix: blends the saturated signal with the clean signal.";
    if (id == ids::choOn)      return "Turns the chorus effect on or off.";
    if (id == ids::choRate)    return "Chorus Rate: how fast the chorus voices drift.";
    if (id == ids::choMix)     return "Chorus Mix: how much chorus is blended in.";
    if (id == ids::dlyOn)      return "Turns the delay effect on or off.";
    if (id == ids::dlyMix)     return "Delay Mix: how loud the echoes are.";
    if (id == ids::dlyPingPong) return "Ping-Pong: bounces echoes between left and right.";
    if (id == ids::dlyTone)    return "Delay Tone: darkens or brightens each repeat.";
    if (id == ids::revOn)      return "Turns the reverb effect on or off.";
    if (id == ids::revDamp)    return "Reverb Damping: absorbs high frequencies for a darker, softer space.";
    if (id == ids::revWidth)   return "Reverb Width: how wide the reverb spreads in stereo.";
    if (id == ids::limOn)      return "Limiter: catches peaks so the output never clips.";
    if (id == ids::subMode)    return "Sub: a clean sine/triangle/square one or two octaves below the played note. Great for weight under basses.";
    if (id == ids::subLevel)   return "Sub Level: how loud the sub oscillator is.";
    if (id == ids::noiseType)  return "Noise Type: White is bright and hissy, Pink is softer, Bright is airy, Dark is rumbly.";
    if (id == ids::noiseLevel) return "Noise Level: adds breath, air or texture.";
    if (id == ids::satDrive)   return "Saturation: warm harmonic distortion. Gentle at low values, aggressive at high values.";
    if (id == ids::choDepth)   return "Chorus Depth: how much the chorus thickens and widens the sound.";
    if (id == ids::dlyDiv)     return "Delay Time: tempo-synced echo spacing.";
    if (id == ids::dlyFeedback) return "Delay Feedback: how many echoes repeat.";
    if (id == ids::revMix)     return "Reverb Mix: how much space surrounds the sound.";
    if (id == ids::revSize)    return "Reverb Size: how large the virtual room is.";
    if (id == ids::voices)     return "Voices: maximum notes that can sound at once. Lower values save CPU.";
    if (id == ids::bendRange)  return "Bend Range: how many semitones the pitch wheel bends.";
    if (id == ids::glide)      return "Glide: slides the pitch between notes.";
    if (id == ids::voiceMode)  return "Poly plays chords. Mono plays one note at a time and restarts the envelopes on every key. Legato is also one note at a time, but overlapping notes slide without restarting the envelopes.";
    if (id == ids::master)     return "Master Volume: final output level.";
    if (id == ids::masterTune) return "Master Tune: shifts the tuning of the whole instrument in cents.";
    if (id == ids::limCeiling) return "Limiter Ceiling: the loudest the output is allowed to get.";
    if (id == ids::engineMode) return "Engine: Divine 2.0 is the overhauled sound engine (cleaner highs, living unison, warmer filters, new reverb and chorus). Classic 1.0 keeps the original sound so older projects play back exactly as they were made.";
    if (id == ids::analog)     return "Analog Drift: slow, organic wander in unison voices, pitch and filter cutoff, a little different on every note. 0% is perfectly static and digital; 30% feels alive; 100% is a worn-out tape machine. (Divine 2.0 engine.)";
    if (id == ids::revPredelay) return "Reverb Pre-Delay: a short gap before the reverb starts, so the dry note stays clear in front of the space.";
    if (auto t = arpHelpText (id); t.isNotEmpty()) return t;
    return fxHelpText (id);
}

} // namespace divinity
