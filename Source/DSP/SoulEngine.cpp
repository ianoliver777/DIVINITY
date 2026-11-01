#include "SoulEngine.h"

namespace divinity
{
//==============================================================================
const std::array<SoulTarget, kNumSoulTargets>& soulTargets()
{
    static const std::array<SoulTarget, kNumSoulTargets> t { {
        { "bright",    "Brightness",  "Opens or closes the filters: brighter or darker harmonics." },
        { "drive",     "Drive",       "Adds or removes saturation and filter drive." },
        { "attack",    "Attack",      "Makes the volume attack sharper or softer." },
        { "release",   "Tail",        "Makes the release tail shorter or longer." },
        { "detune",    "Detune",      "Spreads the unison voices further apart or closer together." },
        { "width",     "Width",       "Widens or narrows the stereo image." },
        { "reverb",    "Space",       "Adds or removes reverb and room size." },
        { "drift",     "Instability", "Gentle per-note pitch wander, like a tape or an old analog synth." },
        { "motion",    "Movement",    "Scales all LFO movement up or down." },
        { "harmonics", "Harmonics",   "Adds or removes harmonic complexity via oscillator folding." },
    } };
    return t;
}

const std::vector<SoulMappingPreset>& soulMappingPresets()
{
    //                                   bright       drive        attack        release       detune       width        reverb        drift         motion       harmonics
    static const std::vector<SoulMappingPreset> p {
        { "Classic",       "Balanced mapping for most sounds.",
          { { .50f, .35f }, { .70f, .00f }, { -.60f, -.10f }, { -.30f, -.50f }, { .20f, .30f }, { .00f, .60f }, { -.30f, .00f }, { .00f, -.50f }, { .30f, .40f }, { .60f, .20f } } },
        { "Cinematic",     "Big tails and wide euphoria for pads and scores.",
          { { .30f, .25f }, { .40f, .00f }, { -.50f, -.30f }, { -.20f, -.60f }, { .00f, .30f }, { .00f, .70f }, { -.20f, .30f }, { .00f, -.60f }, { .00f, .50f }, { .40f, .00f } } },
        { "Bass Pressure", "Aggression = drive and harmonics. Keeps the low end mono.",
          { { .60f, .20f }, { 1.0f, .00f }, { -.30f, .00f }, { -.20f, .00f }, { .30f, .00f }, { .00f, .00f }, { .00f, .00f }, { .00f, -.20f }, { .50f, .00f }, { .90f, .10f } } },
        { "Dream",         "Soft, floating, unstable. Great for ambient and vocal textures.",
          { { .20f, .40f }, { .30f, .00f }, { -.80f, -.20f }, { .00f, -.80f }, { .00f, .50f }, { .00f, .80f }, { -.50f, .40f }, { .00f, -.80f }, { .00f, .60f }, { .20f, .00f } } },
        { "Subtle",        "Classic at reduced strength for finished mixes.",
          { { .20f, .14f }, { .28f, .00f }, { -.24f, -.04f }, { -.12f, -.20f }, { .08f, .12f }, { .00f, .24f }, { -.12f, .00f }, { .00f, -.20f }, { .12f, .16f }, { .24f, .08f } } },
    };
    return p;
}

juce::StringArray divineModeNames() { return { "Ascension", "Collapse", "Shatter", "Bloom", "Abyss" }; }

juce::String divineModeDescription (int mode)
{
    switch (mode)
    {
        case 0:  return "Rises into light: brighter, wider, shimmering space and longer tails.";
        case 1:  return "Falls apart: darker, driven, unstable and folding in on itself.";
        case 2:  return "Splinters: sharp attacks, heavy motion, folded harmonics and scattered echoes.";
        case 3:  return "Opens slowly: soft swelling attacks, huge tails and a wide blooming room.";
        default: return "Sinks: darkness, distance and an endless cavernous tail.";
    }
}

namespace
{
    //  brightOct drive attackL2 releaseL2 detune width reverb delay driftCt motion fold revSize
    const float kDivine[5][12] = {
        {  2.0f, 0.00f,  0.0f, 1.0f, 0.35f,  0.6f, 0.45f, 0.25f,  0.0f, 0.8f, 0.15f, 0.20f },  // Ascension
        { -2.8f, 0.70f, -1.0f, -0.5f, 0.20f, -0.4f, 0.00f, 0.00f, 35.0f, 1.5f, 0.50f, 0.00f },  // Collapse
        {  1.0f, 0.55f, -1.5f, 0.0f, 0.60f,  0.5f, 0.10f, 0.35f, 10.0f, 2.5f, 0.80f, 0.00f },  // Shatter
        { -0.4f, 0.00f,  2.5f, 2.0f, 0.30f,  0.5f, 0.60f, 0.30f,  8.0f, 0.6f, 0.00f, 0.25f },  // Bloom
        { -3.0f, 0.20f,  1.0f, 1.8f, 0.15f,  0.3f, 0.75f, 0.20f, 20.0f, 0.4f, 0.10f, 0.30f },  // Abyss
    };
}

//==============================================================================
void addSoulParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using APF = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
    auto pct = Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; });
    auto bip = Attr().withStringFromValueFunction ([] (float v, int) { auto n = juce::roundToInt (v * 100.0f); return (n > 0 ? "+" : "") + juce::String (n) + "%"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; });
    auto axis = [] (const char* neg, const char* pos)
    {
        return Attr().withStringFromValueFunction ([neg, pos] (float v, int)
                     {
                         const int n = juce::roundToInt ((v - 0.5f) * 200.0f);
                         if (n == 0) return juce::String ("Neutral");
                         return juce::String (n < 0 ? neg : pos) + " " + juce::String (std::abs (n)) + "%";
                     })
                     .withValueFromStringFunction ([] (const juce::String& s) { return 0.5f + s.retainCharacters ("-0123456789.").getFloatValue() / 200.0f; });
    };

    auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("soul", "Soul Engine", " | ");
    group->addChild (std::make_unique<APF> (pid (ids::soulX), "Soul Soft-Aggressive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, axis ("Soft", "Aggressive")));
    group->addChild (std::make_unique<APF> (pid (ids::soulY), "Soul Melancholy-Euphoria", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, axis ("Melancholy", "Euphoria")));
    group->addChild (std::make_unique<APF> (pid (ids::soulIntensity), "Soul Intensity", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    group->addChild (std::make_unique<APF> (pid (ids::soulCurve), "Soul Curve", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip));
    juce::NormalisableRange<float> sm (0.0f, 2.0f); sm.setSkewForCentre (0.3f);
    group->addChild (std::make_unique<APF> (pid (ids::soulSmooth), "Soul Smoothing", sm, 0.15f,
        Attr().withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; })));

    const auto& def = soulMappingPresets()[0];
    for (int t = 0; t < kNumSoulTargets; ++t)
    {
        const juce::String n = soulTargets()[(size_t) t].name;
        group->addChild (std::make_unique<juce::AudioParameterBool> (pid (ids::soul (t, "on")), "Soul " + n + " On", true));
        group->addChild (std::make_unique<APF> (pid (ids::soul (t, "x")), "Soul " + n + " Aggressive Amount", juce::NormalisableRange<float> (-1.0f, 1.0f), def.w[t][0], bip));
        group->addChild (std::make_unique<APF> (pid (ids::soul (t, "y")), "Soul " + n + " Euphoria Amount", juce::NormalisableRange<float> (-1.0f, 1.0f), def.w[t][1], bip));
    }
    group->addChild (std::make_unique<APF> (pid (ids::divineAmount), "Divine Intervention", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    group->addChild (std::make_unique<juce::AudioParameterChoice> (pid (ids::divineMode), "Divine Mode", divineModeNames(), 0));
    layout.add (std::move (group));
}

//==============================================================================
SoulEngine::SoulEngine (juce::AudioProcessorValueTreeState& s)
{
    x = s.getRawParameterValue (ids::soulX);
    y = s.getRawParameterValue (ids::soulY);
    intensity = s.getRawParameterValue (ids::soulIntensity);
    curve = s.getRawParameterValue (ids::soulCurve);
    smooth = s.getRawParameterValue (ids::soulSmooth);
    divAmount = s.getRawParameterValue (ids::divineAmount);
    divMode = s.getRawParameterValue (ids::divineMode);
    for (int t = 0; t < kNumSoulTargets; ++t)
    {
        on[(size_t) t] = s.getRawParameterValue (ids::soul (t, "on"));
        wx[(size_t) t] = s.getRawParameterValue (ids::soul (t, "x"));
        wy[(size_t) t] = s.getRawParameterValue (ids::soul (t, "y"));
    }
}

void SoulEngine::reset() { primed = false; }

PerformanceMods SoulEngine::compute (double seconds, float xMod, float yMod, float divMod) noexcept
{
    auto finite = [] (float v) { return std::isfinite (v) ? v : 0.0f; };
    const float tx = juce::jlimit (-1.0f, 1.0f, (x->load() - 0.5f) * 2.0f + finite (xMod));
    const float ty = juce::jlimit (-1.0f, 1.0f, (y->load() - 0.5f) * 2.0f + finite (yMod));
    const float tDiv = juce::jlimit (0.0f, 1.0f, divAmount->load() + finite (divMod));

    if (! primed) { sx = tx; sy = ty; sDiv = tDiv; primed = true; }
    // Transition smoothing: one-pole glide (time constant = smoothing / 3), minimum ~5 ms de-click.
    const double tc = juce::jmax (0.005, (double) smooth->load() / 3.0);
    const float a = (float) (1.0 - std::exp (-seconds / tc));
    sx += a * (tx - sx);
    sy += a * (ty - sy);
    sDiv += (float) (1.0 - std::exp (-seconds / 0.02)) * (tDiv - sDiv);
    effX = sx; effY = sy; effDiv = sDiv;

    const float e = std::pow (2.0f, curve->load() * 1.5f);
    auto shape = [e] (float v) { return (v < 0 ? -1.0f : 1.0f) * std::pow (std::abs (v), e); };
    const float fx = shape (sx), fy = shape (sy), k = intensity->load();

    float v[kNumSoulTargets];
    for (int t = 0; t < kNumSoulTargets; ++t)
        v[t] = on[(size_t) t]->load() > 0.5f ? (wx[(size_t) t]->load() * fx + wy[(size_t) t]->load() * fy) * k : 0.0f;

    PerformanceMods m;
    m.brightOct   = 3.0f * v[0];
    m.drive       = 0.6f * v[1];
    m.attackLog2  = 3.0f * v[2];
    m.releaseLog2 = 2.5f * v[3];
    m.detune      = 0.4f * v[4];
    m.width       = 0.5f * v[5];
    m.reverb      = 0.5f * v[6];
    m.reverbSize  = 0.2f * v[6];
    m.driftCents  = 25.0f * juce::jmax (0.0f, v[7]);
    m.motion      = v[8];
    m.fold        = 0.5f * v[9];

    // Divine Intervention
    if (sDiv > 1.0e-4f)
    {
        const int mode = juce::jlimit (0, 4, (int) std::lround (divMode->load()));
        const float s = std::pow (sDiv, 1.3f);
        const float* d = kDivine[mode];
        m.brightOct += d[0] * s;  m.drive += d[1] * s;  m.attackLog2 += d[2] * s; m.releaseLog2 += d[3] * s;
        m.detune += d[4] * s;     m.width += d[5] * s;  m.reverb += d[6] * s;     m.delay += d[7] * s;
        m.driftCents += d[8] * s; m.motion += d[9] * s; m.fold += d[10] * s;      m.reverbSize += d[11] * s;
    }
    return m;
}

} // namespace divinity
