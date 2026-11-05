#pragma once

#include <juce_dsp/juce_dsp.h>
#include "SynthEngine.h"
#include "FxCatalog.h"

namespace divinity
{
//==============================================================================

struct FxContext
{
    double sampleRate = 44100.0, bpm = 120.0;
    SynthEngine::GlobalMods mods;
    std::array<float, kNumMacros> macros {};
};

//==============================================================================
/** One rack effect. process() works in place and handles its own wet/dry mix. */
class FxModule
{
public:
    FxModule (juce::AudioProcessorValueTreeState& s, const juce::String& onID) : apvts (s), onParam (s.getRawParameterValue (onID)) {}
    virtual ~FxModule() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void process (float* L, float* R, int n, const FxContext&) noexcept = 0;
    /** True when the module should run (power on, or a macro / SOUL / matrix route is driving it). */
    virtual bool wantsToRun (const FxContext&) const noexcept { return isOn(); }
    bool isOn() const noexcept { return onParam->load() > 0.5f; }

    juce::SmoothedValue<float> enable;   // bypass crossfade (managed by the chain)
    bool running = false;

protected:
    std::atomic<float>* p (const juce::String& id) { return apvts.getRawParameterValue (id); }
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>* onParam;
};

//==============================================================================
class FXChain
{
public:
    explicit FXChain (juce::AudioProcessorValueTreeState& apvts);
    ~FXChain();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, double bpm, const SynthEngine::GlobalMods& mods,
                  const std::array<float, kNumMacros>& macros) noexcept;

    /** Message thread: set / get the chain order as module keys (see fxCatalog()). */
    void setOrder (const juce::StringArray& keys);
    juce::StringArray getOrder() const;

    /** Effective (post-macro/post-mod) values, for UI display. */
    struct Display { std::atomic<float> sat { 0 }, chorus { 0 }, delay { 0 }, reverb { 0 }; };
    Display display;

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<FxModule>> modules;          // catalogue index order
    std::array<int, 32> order {}, pendingOrder {};
    int numOrdered = 0, numPending = 0;
    juce::SpinLock orderLock;
    bool orderDirty = false;

    std::atomic<float> *limOn, *limCeiling, *master;
    double sampleRate = 44100.0;
    int maxBlock = 512;
    juce::dsp::Limiter<float> limiter;
    juce::SmoothedValue<float> masterS, widthS, airS, panS;
    std::atomic<float>* pan = nullptr;
    float airL = 0, airR = 0, sideLo = 0;
    juce::AudioBuffer<float> dry;
    bool limiterWasOn = false;
};

} // namespace divinity
