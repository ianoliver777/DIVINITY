#include "FxCatalog.h"
#include "../Parameters.h"
#include <map>

namespace divinity
{
using namespace fxids;

const std::vector<FxDef>& fxCatalog()
{
    static const std::vector<FxDef> c {
        { "sat", "Saturation", "Warm, anti-aliased harmonic drive. A touch adds presence, a lot adds teeth.", ids::satOn,
          { { ids::satDrive, "Drive" }, { ids::satMix, "Mix" } }, {}, {} },
        { "dist", "Distortion", "Twelve anti-aliased waveshapers, from tape and tube warmth to fuzz, diode and square-fuzz grit, with a tone control to tame the fizz.", distOn,
          { { distDrive, "Drive" }, { distTone, "Tone" }, { distMix, "Mix" } }, { distType }, {} },
        { "crush", "Bitcrusher", "Lo-fi on purpose: fewer bits and a lower sample rate for crunchy, digital grit.", crushOn,
          { { crushBits, "Bits" }, { crushDown, "Downsample" }, { crushMix, "Mix" } }, {}, {} },
        { "eq", "EQ", "Three-band tone shaping: low shelf, sweepable bell and high shelf.", eqOn,
          { { eqLowFreq, "Low Freq" }, { eqLowGain, "Low", true }, { eqMidFreq, "Mid Freq" }, { eqMidGain, "Mid", true },
            { eqMidQ, "Mid Q" }, { eqHighFreq, "High Freq" }, { eqHighGain, "High", true } }, {}, {} },
        { "comp", "Compressor", "Evens out dynamics and adds punch. Mix below 100% for parallel compression.", compOn,
          { { compThresh, "Threshold" }, { compRatio, "Ratio" }, { compAttack, "Attack" }, { compRelease, "Release" },
            { compMakeup, "Makeup" }, { compMix, "Mix" } }, {}, {} },
        { "mb", "Multiband", "Three-band upward + downward compression in the OTT style. Move the two crossovers, then set gain, downward and upward threshold/ratio per band, lows to highs.", mbOn,
          { { mbDepth, "Depth" }, { mbTime, "Time" }, { mbOut, "Output", true }, { mbLowX, "Low Split" }, { mbHighX, "High Split" },
            { mbBand (0, "gain"), "Gain", true }, { mbBand (0, "dthr"), "Down Thr" }, { mbBand (0, "dratio"), "Down Ratio" }, { mbBand (0, "uthr"), "Up Thr" }, { mbBand (0, "uamt"), "Up Ratio" },
            { mbBand (1, "gain"), "Gain", true }, { mbBand (1, "dthr"), "Down Thr" }, { mbBand (1, "dratio"), "Down Ratio" }, { mbBand (1, "uthr"), "Up Thr" }, { mbBand (1, "uamt"), "Up Ratio" },
            { mbBand (2, "gain"), "Gain", true }, { mbBand (2, "dthr"), "Down Thr" }, { mbBand (2, "dratio"), "Down Ratio" }, { mbBand (2, "uthr"), "Up Thr" }, { mbBand (2, "uamt"), "Up Ratio" } }, {}, {} },
        { "trans", "Transient", "Sharpen or soften the front of each note, and lift or shrink the body that follows.", transOn,
          { { transAttack, "Attack", true }, { transSustain, "Sustain", true } }, {}, {} },
        { "chorus", "Chorus", "Three drifting voices per side in a dimension-style stereo chorus: width and shimmer that still sums cleanly to mono.", ids::choOn,
          { { ids::choRate, "Rate" }, { ids::choDepth, "Depth" }, { ids::choMix, "Mix" } }, {}, {} },
        { "flanger", "Flanger", "A short swept delay with feedback: jet-plane whooshes and metallic combs.", flangOn,
          { { flangRate, "Rate" }, { flangDepth, "Depth" }, { flangFb, "Feedback", true }, { flangMix, "Mix" } }, {}, {} },
        { "phaser", "Phaser", "Sweeping notches that swirl through the spectrum.", phaseOn,
          { { phaseRate, "Rate" }, { phaseDepth, "Depth" }, { phaseCentre, "Centre" }, { phaseFb, "Feedback", true }, { phaseMix, "Mix" } }, {}, {} },
        { "trem", "Tremolo", "Rhythmic volume (tremolo) or stereo movement (autopan), free or tempo-synced.", tremOn,
          { { tremRate, "Rate" }, { tremDepth, "Depth" } }, { tremMode, tremShape, tremDiv }, { { tremSync, "SYNC" } } },
        { "ring", "Ring Mod", "Multiplies the sound by a sine for bell-like, robotic and metallic tones.", ringOn,
          { { ringFreq, "Frequency" }, { ringMix, "Mix" } }, {}, {} },
        { "fshift", "Freq Shifter", "Shifts every frequency by the same amount: subtle beating, barber-pole swirls or alien detune.", fshiftOn,
          { { fshiftShift, "Shift", true }, { fshiftMix, "Mix" } }, {}, {} },
        { "delay", "Delay", "Tempo-synced echoes, ping-pong across the stereo field, darkening with each repeat.", ids::dlyOn,
          { { ids::dlyFeedback, "Feedback" }, { ids::dlyMix, "Mix" }, { ids::dlyTone, "Tone" } }, { ids::dlyDiv }, { { ids::dlyPingPong, "PING-PONG" } } },
        { "reverb", "Reverb", "A dense, modulated stereo space: early reflections into a smooth, non-metallic tail, from small rooms to endless halls.", ids::revOn,
          { { ids::revPredelay, "Pre-Delay" }, { ids::revSize, "Size" }, { ids::revDamp, "Damping" }, { ids::revWidth, "Width" }, { ids::revMix, "Mix" } }, {}, {} },
        { "stereo", "Stereo", "Width, pan, mono-bass and gain: the final polish before the limiter.", stereoOn,
          { { stereoWidth, "Width" }, { stereoPan, "Pan", true }, { stereoMonoFreq, "Mono Below" }, { stereoGain, "Gain", true } }, {}, { { stereoMonoBass, "MONO BASS" } } },
    };
    return c;
}

int fxIndexForKey (const juce::String& key)
{
    const auto& c = fxCatalog();
    for (int i = 0; i < (int) c.size(); ++i) if (key == c[(size_t) i].key) return i;
    return -1;
}

juce::StringArray defaultFxOrder()
{
    return { "eq", "comp", "sat", "dist", "crush", "trans", "mb", "chorus", "flanger", "phaser", "trem", "ring", "fshift", "delay", "reverb", "stereo" };
}

juce::StringArray sanitiseFxOrder (const juce::StringArray& keys)
{
    juce::StringArray out;
    for (auto& k : keys) if (fxIndexForKey (k.trim()) >= 0) out.addIfNotAlreadyThere (k.trim());
    for (auto& k : defaultFxOrder()) out.addIfNotAlreadyThere (k);
    return out;
}

//==============================================================================
void addFxRackParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Attr = juce::AudioParameterFloatAttributes;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
    auto parse = [] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); };
    auto pct = Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
                     .withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s) / 100.0f; });
    auto bip = Attr().withStringFromValueFunction ([] (float v, int) { auto n = juce::roundToInt (v * 100.0f); return (n > 0 ? "+" : "") + juce::String (n) + "%"; })
                     .withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s) / 100.0f; });
    auto db = Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })
                    .withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s); });
    auto hz = Attr().withStringFromValueFunction ([] (float v, int)
                    {
                        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                        return v < 10.0f ? juce::String (v, 2) + " Hz" : juce::String (juce::roundToInt (v)) + " Hz";
                    })
                    .withValueFromStringFunction ([parse] (const juce::String& s) { auto v = parse (s); return s.containsIgnoreCase ("k") ? v * 1000.0f : v; });
    auto ms = Attr().withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; })
                    .withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s); });
    auto skew = [] (float lo, float hi, float centre) { juce::NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto lin = [] (float lo, float hi) { return juce::NormalisableRange<float> (lo, hi); };

    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("fxrack", "FX Rack", " | ");

    g->addChild (std::make_unique<APB> (pid (distOn), "Distortion On", false));
    g->addChild (std::make_unique<APC> (pid (distType), "Distortion Type", juce::StringArray { "Hard Clip", "Sine Fold", "Tube", "Rectify", "Soft Cubic",
                                                                                          "Fuzz", "Diode", "Overdrive", "Tape", "Half Rectify", "Crunch", "Square Fuzz" }, 0));
    g->addChild (std::make_unique<APF> (pid (distDrive), "Distortion Drive", lin (0, 1), 0.35f, pct));
    g->addChild (std::make_unique<APF> (pid (distTone), "Distortion Tone", lin (0, 1), 0.7f, pct));
    g->addChild (std::make_unique<APF> (pid (distMix), "Distortion Mix", lin (0, 1), 1.0f, pct));

    g->addChild (std::make_unique<APB> (pid (crushOn), "Bitcrusher On", false));
    g->addChild (std::make_unique<APF> (pid (crushBits), "Bitcrusher Bits", lin (2, 16), 8.0f,
                    Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " bit"; }).withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s); })));
    g->addChild (std::make_unique<APF> (pid (crushDown), "Bitcrusher Downsample", skew (1, 40, 6), 4.0f,
                    Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + "x"; }).withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s); })));
    g->addChild (std::make_unique<APF> (pid (crushMix), "Bitcrusher Mix", lin (0, 1), 1.0f, pct));

    g->addChild (std::make_unique<APB> (pid (eqOn), "EQ On", false));
    g->addChild (std::make_unique<APF> (pid (eqLowFreq), "EQ Low Freq", skew (20, 600, 120), 120.0f, hz));
    g->addChild (std::make_unique<APF> (pid (eqLowGain), "EQ Low Gain", lin (-18, 18), 0.0f, db));
    g->addChild (std::make_unique<APF> (pid (eqMidFreq), "EQ Mid Freq", skew (100, 10000, 1000), 1000.0f, hz));
    g->addChild (std::make_unique<APF> (pid (eqMidGain), "EQ Mid Gain", lin (-18, 18), 0.0f, db));
    g->addChild (std::make_unique<APF> (pid (eqMidQ), "EQ Mid Q", skew (0.3f, 8.0f, 1.0f), 0.8f,
                    Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); }).withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s); })));
    g->addChild (std::make_unique<APF> (pid (eqHighFreq), "EQ High Freq", skew (1000, 18000, 6000), 6000.0f, hz));
    g->addChild (std::make_unique<APF> (pid (eqHighGain), "EQ High Gain", lin (-18, 18), 0.0f, db));

    g->addChild (std::make_unique<APB> (pid (compOn), "Compressor On", false));
    g->addChild (std::make_unique<APF> (pid (compThresh), "Compressor Threshold", lin (-60, 0), -18.0f, db));
    g->addChild (std::make_unique<APF> (pid (compRatio), "Compressor Ratio", skew (1, 20, 4), 4.0f,
                    Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + ":1"; }).withValueFromStringFunction ([parse] (const juce::String& s) { return parse (s); })));
    g->addChild (std::make_unique<APF> (pid (compAttack), "Compressor Attack", skew (0.1f, 100.0f, 10.0f), 8.0f, ms));
    g->addChild (std::make_unique<APF> (pid (compRelease), "Compressor Release", skew (10.0f, 1000.0f, 150.0f), 120.0f, ms));
    g->addChild (std::make_unique<APF> (pid (compMakeup), "Compressor Makeup", lin (0, 24), 4.0f, db));
    g->addChild (std::make_unique<APF> (pid (compMix), "Compressor Mix", lin (0, 1), 1.0f, pct));

    g->addChild (std::make_unique<APB> (pid (mbOn), "Multiband On", false));
    g->addChild (std::make_unique<APF> (pid (mbDepth), "Multiband Depth", lin (0, 1), 0.4f, pct));
    g->addChild (std::make_unique<APF> (pid (mbTime), "Multiband Time", lin (0, 1), 0.4f, pct));
    g->addChild (std::make_unique<APF> (pid (mbOut), "Multiband Output", lin (-12, 12), 0.0f, db));

    g->addChild (std::make_unique<APB> (pid (transOn), "Transient On", false));
    g->addChild (std::make_unique<APF> (pid (transAttack), "Transient Attack", lin (-1, 1), 0.3f, bip));
    g->addChild (std::make_unique<APF> (pid (transSustain), "Transient Sustain", lin (-1, 1), 0.0f, bip));

    g->addChild (std::make_unique<APB> (pid (flangOn), "Flanger On", false));
    g->addChild (std::make_unique<APF> (pid (flangRate), "Flanger Rate", skew (0.02f, 5.0f, 0.4f), 0.25f, hz));
    g->addChild (std::make_unique<APF> (pid (flangDepth), "Flanger Depth", lin (0, 1), 0.6f, pct));
    g->addChild (std::make_unique<APF> (pid (flangFb), "Flanger Feedback", lin (-0.95f, 0.95f), 0.5f, bip));
    g->addChild (std::make_unique<APF> (pid (flangMix), "Flanger Mix", lin (0, 1), 0.5f, pct));

    g->addChild (std::make_unique<APB> (pid (phaseOn), "Phaser On", false));
    g->addChild (std::make_unique<APF> (pid (phaseRate), "Phaser Rate", skew (0.02f, 8.0f, 0.5f), 0.3f, hz));
    g->addChild (std::make_unique<APF> (pid (phaseDepth), "Phaser Depth", lin (0, 1), 0.7f, pct));
    g->addChild (std::make_unique<APF> (pid (phaseCentre), "Phaser Centre", skew (100, 6000, 900), 900.0f, hz));
    g->addChild (std::make_unique<APF> (pid (phaseFb), "Phaser Feedback", lin (-0.95f, 0.95f), 0.4f, bip));
    g->addChild (std::make_unique<APF> (pid (phaseMix), "Phaser Mix", lin (0, 1), 0.5f, pct));

    g->addChild (std::make_unique<APB> (pid (tremOn), "Tremolo On", false));
    g->addChild (std::make_unique<APC> (pid (tremMode), "Tremolo Mode", juce::StringArray { "Tremolo", "Autopan" }, 0));
    g->addChild (std::make_unique<APC> (pid (tremShape), "Tremolo Shape", juce::StringArray { "Sine", "Triangle", "Square" }, 0));
    g->addChild (std::make_unique<APB> (pid (tremSync), "Tremolo Sync", true));
    g->addChild (std::make_unique<APF> (pid (tremRate), "Tremolo Rate", skew (0.05f, 20.0f, 3.0f), 4.0f, hz));
    g->addChild (std::make_unique<APC> (pid (tremDiv), "Tremolo Division", lfoDivisionNames(), 5));
    g->addChild (std::make_unique<APF> (pid (tremDepth), "Tremolo Depth", lin (0, 1), 0.6f, pct));

    g->addChild (std::make_unique<APB> (pid (ringOn), "Ring Mod On", false));
    g->addChild (std::make_unique<APF> (pid (ringFreq), "Ring Mod Frequency", skew (20, 5000, 400), 300.0f, hz));
    g->addChild (std::make_unique<APF> (pid (ringMix), "Ring Mod Mix", lin (0, 1), 0.5f, pct));

    g->addChild (std::make_unique<APB> (pid (fshiftOn), "Freq Shifter On", false));
    g->addChild (std::make_unique<APF> (pid (fshiftShift), "Freq Shifter Shift", lin (-1, 1), 0.05f,
                    Attr().withStringFromValueFunction ([] (float v, int)
                    {
                        const float hzv = (v < 0 ? -1.0f : 1.0f) * std::pow (std::abs (v), 3.0f) * 2000.0f;
                        return (hzv > 0.005f ? "+" : "") + juce::String (hzv, std::abs (hzv) < 10.0f ? 2 : 0) + " Hz";
                    })
                    .withValueFromStringFunction ([parse] (const juce::String& s)
                    {
                        const float hzv = parse (s);
                        return (hzv < 0 ? -1.0f : 1.0f) * std::cbrt (std::abs (hzv) / 2000.0f);
                    })));
    g->addChild (std::make_unique<APF> (pid (fshiftMix), "Freq Shifter Mix", lin (0, 1), 0.5f, pct));

    g->addChild (std::make_unique<APB> (pid (stereoOn), "Stereo On", false));
    g->addChild (std::make_unique<APF> (pid (stereoWidth), "Stereo Width", lin (0, 2), 1.0f, pct));
    g->addChild (std::make_unique<APF> (pid (stereoPan), "Stereo Pan", lin (-1, 1), 0.0f, bip));
    g->addChild (std::make_unique<APB> (pid (stereoMonoBass), "Stereo Mono Bass", true));
    g->addChild (std::make_unique<APF> (pid (stereoMonoFreq), "Stereo Mono Below", skew (40, 400, 120), 120.0f, hz));
    g->addChild (std::make_unique<APF> (pid (stereoGain), "Stereo Gain", lin (-24, 12), 0.0f, db));

    layout.add (std::move (g));
}

juce::String fxHelpText (const juce::String& id)
{
    static const std::map<juce::String, const char*> help {
        { distType, "Distortion type: Hard Clip is bold and square, Sine Fold wraps the wave back on itself, Tube is warm and asymmetric, Rectify adds octave-up grit, Soft Cubic is gentle." },
        { distDrive, "Distortion Drive: how hard the signal hits the shaper." },
        { distTone, "Distortion Tone: rolls off fizz after the distortion. Lower is darker." },
        { distMix, "Distortion Mix: blend of distorted and clean signal." },
        { crushBits, "Bits: lower values = coarser, noisier digital steps." },
        { crushDown, "Downsample: holds samples for longer, lowering the effective sample rate for aliased grit." },
        { crushMix, "Bitcrusher Mix: blend of crushed and clean signal." },
        { eqLowFreq, "EQ low shelf frequency." }, { eqLowGain, "EQ low shelf gain: boost or cut the bass." },
        { eqMidFreq, "EQ bell frequency." }, { eqMidGain, "EQ bell gain: boost or cut around the bell frequency." },
        { eqMidQ, "EQ bell Q: low values are broad, high values are narrow." },
        { eqHighFreq, "EQ high shelf frequency." }, { eqHighGain, "EQ high shelf gain: add air or soften the top." },
        { compThresh, "Compressor Threshold: level above which compression starts." },
        { compRatio, "Compressor Ratio: how strongly loud parts are turned down." },
        { compAttack, "Compressor Attack: how fast it grabs. Slower lets transients punch through." },
        { compRelease, "Compressor Release: how fast it lets go." },
        { compMakeup, "Compressor Makeup: gain added after compression." },
        { compMix, "Compressor Mix: below 100% blends in the uncompressed signal (parallel compression)." },
        { mbDepth, "Multiband Depth: how much of the upward/downward compression you hear." },
        { mbTime, "Multiband Time: faster (low) or slower, smoother (high) reaction." },
        { mbOut, "Multiband Output gain." },
        { transAttack, "Transient Attack: positive sharpens note starts, negative softens them." },
        { transSustain, "Transient Sustain: positive lifts the body and tail, negative tightens it." },
        { flangRate, "Flanger Rate: sweep speed." }, { flangDepth, "Flanger Depth: sweep range." },
        { flangFb, "Flanger Feedback: resonance of the sweep; negative flips its character." }, { flangMix, "Flanger Mix." },
        { phaseRate, "Phaser Rate: sweep speed." }, { phaseDepth, "Phaser Depth: sweep range." },
        { phaseCentre, "Phaser Centre: where in the spectrum the notches sweep." }, { phaseFb, "Phaser Feedback: notch intensity." }, { phaseMix, "Phaser Mix." },
        { tremMode, "Tremolo Mode: Tremolo pulses the volume, Autopan moves the sound left and right." },
        { tremShape, "Tremolo Shape: smooth sine, linear triangle or choppy square." },
        { tremSync, "Tremolo Sync: lock the rate to the project tempo." }, { tremRate, "Tremolo Rate (when not synced)." },
        { tremDiv, "Tremolo Division (when synced)." }, { tremDepth, "Tremolo Depth." },
        { ringFreq, "Ring Mod Frequency: the carrier. Low = tremolo-like, high = metallic and bell-like." }, { ringMix, "Ring Mod Mix." },
        { fshiftShift, "Frequency Shift: moves every partial by the same Hz. Small values beat and swirl, large values go alien." },
        { fshiftMix, "Freq Shifter Mix." },
        { stereoWidth, "Stereo Width: 0% mono, 100% unchanged, 200% extra wide." }, { stereoPan, "Stereo Pan." },
        { stereoMonoBass, "Mono Bass: keeps everything below the Mono Below frequency centred for a solid low end." },
        { stereoMonoFreq, "Mono Below: the frequency under which Mono Bass collapses to mono." }, { stereoGain, "Stereo Gain: output trim before the limiter." },
    };
    if (id.endsWith ("_on"))
        for (auto& d : fxCatalog()) if (d.onID == id) return juce::String ("Turns the ") + d.name + " on or off. " + d.description;
    auto it = help.find (id);
    return it != help.end() ? juce::String (it->second) : juce::String();
}

} // namespace divinity
