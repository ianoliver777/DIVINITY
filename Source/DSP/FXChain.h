#pragma once

#include <juce_dsp/juce_dsp.h>
#include "SynthEngine.h"

namespace divinity
{
//==============================================================================
/** Saturation with first-order antiderivative anti-aliasing (no oversampling, no latency). */
class AdaaSaturator
{
public:
    void reset() { for (auto& x : prev) x = 0.0f; }

    float process (int ch, float x) noexcept
    {
        const float x0 = prev[ch];
        prev[ch] = x;
        const float d = x - x0;
        if (std::abs (d) < 1.0e-4f)
            return std::tanh (0.5f * (x + x0));
        return (logCosh (x) - logCosh (x0)) / d;
    }

private:
    static float logCosh (float x) noexcept
    {
        const float a = std::abs (x);
        return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f;
    }
    float prev[2] {};
};

//==============================================================================
class StereoDelay
{
public:
    void prepare (double sr, int maxSeconds);
    void reset();
    /** Adds the delay output (scaled by mix) to the buffer. */
    void process (float* L, float* R, int n, float delaySeconds, float feedback, float mix, float tone, bool pingPong) noexcept;

private:
    double sampleRate = 44100.0;
    std::vector<float> bufL, bufR;
    int writePos = 0;
    juce::SmoothedValue<float> time;
    float lpL = 0, lpR = 0;
};

//==============================================================================
class FXChain
{
public:
    explicit FXChain (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, double bpm, const SynthEngine::GlobalMods& mods,
                  const std::array<float, kNumMacros>& macros) noexcept;

    /** Effective (post-macro/post-mod) values, for UI display. */
    struct Display { std::atomic<float> sat { 0 }, chorus { 0 }, delay { 0 }, reverb { 0 }; };
    Display display;

private:
    std::atomic<float>* p (const juce::String& id) { return apvts.getRawParameterValue (id); }
    juce::AudioProcessorValueTreeState& apvts;

    std::atomic<float> *satOn, *satDrive, *satMix,
                       *choOn, *choRate, *choDepth, *choMix,
                       *dlyOn, *dlyDiv, *dlyFb, *dlyMix, *dlyPing, *dlyTone,
                       *revOn, *revSize, *revDamp, *revWidth, *revMix,
                       *limOn, *limCeiling, *master;

    double sampleRate = 44100.0;
    int maxBlock = 512;

    AdaaSaturator saturator;
    juce::SmoothedValue<float> satDriveS, satMixS, choMixS, dlyMixS, revMixS, masterS;
    juce::dsp::Chorus<float> chorus;
    StereoDelay delay;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    juce::AudioBuffer<float> wet;
    bool reverbWasActive = false, chorusWasActive = false, limiterWasOn = false;
};

} // namespace divinity
