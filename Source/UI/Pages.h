#pragma once

#include "Panels.h"
#include "CreateView.h"
#include "SoulView.h"
#include <juce_dsp/juce_dsp.h>

namespace divinity::ui
{
//==============================================================================
/** Live output spectrum (FFT of the scope feed), drawn as a glowing curve. */
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    DivinityProcessor& proc;
    static constexpr int kOrder = 11, kSize = 1 << kOrder;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring, pull, fftData, smooth;
    int writePos = 0;
};

/** Small titled panel used across pages. */
class TitledPanel : public Panel
{
public:
    explicit TitledPanel (juce::String t) : Panel (std::move (t)) {}
};

//==============================================================================
class CreatePage : public juce::Component
{
public:
    explicit CreatePage (DivinityProcessor& p);
    ~CreatePage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};

class EffectsPage : public juce::Component
{
public:
    explicit EffectsPage (DivinityProcessor& p);
    ~EffectsPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};

class PlayPage : public juce::Component
{
public:
    explicit PlayPage (DivinityProcessor& p);
    ~PlayPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};

class LabPage : public juce::Component
{
public:
    explicit LabPage (DivinityProcessor& p);
    ~LabPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};

} // namespace divinity::ui
