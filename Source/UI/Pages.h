#pragma once

#include "Panels.h"
#include "CreateView.h"
#include "SoulView.h"
#include <juce_dsp/juce_dsp.h>

namespace divinity { class ScopeFifo; }

namespace divinity::ui
{
//==============================================================================
/** Live output spectrum (FFT of the scope feed), drawn as a glowing curve. */
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    /** 0 = POST (what you hear), 1 = PRE (raw voices, before the effects), 2 = MIX (both overlaid). */
    void setMode (int m) { mode = m; repaint(); }
    int getMode() const noexcept { return mode; }
private:
    void timerCallback() override;
    struct Chan { std::vector<float> ring, smooth; int writePos = 0; };
    void analyse (Chan& c, ScopeFifo& fifo, juce::int64& cursor);
    juce::Path curveFor (const Chan& c, juce::Rectangle<float> r, double sr) const;
    DivinityProcessor& proc;
    static constexpr int kOrder = 11, kSize = 1 << kOrder;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> pull, fftData;
    Chan post, pre;
    juce::int64 postCursor = -1, preCursor = -1;
    int mode = 0;
};

/** Spectral stereo imager: every frequency of the output placed by where it sits in the stereo field
    (left - centre - right) and how wide it is, low frequencies at the bottom, with a fading trail.
    Below it: phase correlation and overall width. */
class StereoImager : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit StereoImager (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    DivinityProcessor& proc;
    static constexpr int kOrder = 11, kSize = 1 << kOrder;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ringL, ringR, pullL, pullR, fl, fr;
    int writePos = 0;
    juce::int64 cursor = -1;
    juce::Image trail;
    float correlation = 1.0f, width = 0.0f, level = 0.0f;
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
    /** Set by the editor: switch workspace (the FX chain's "..." opens EFFECTS). */
    std::function<void (const juce::String&)> onShowTab;
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

/** BROWSER: full-page preset browser with category tiles, search and preset details. */
class BrowserPage : public juce::Component
{
public:
    explicit BrowserPage (DivinityProcessor& p);
    ~BrowserPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};

/** Wraps a page designed for the v0.6 1248 x 682 grid and scales it into whatever area it gets. */
class LegacyPageHost : public juce::Component
{
public:
    explicit LegacyPageHost (juce::Component& page) : inner (page) { addAndMakeVisible (inner); }
    void resized() override
    {
        const float s = juce::jmin ((float) getWidth() / 1248.0f, (float) getHeight() / 682.0f);
        inner.setBounds (0, 0, 1248, 682);
        inner.setTransform (juce::AffineTransform::scale (s).translated (((float) getWidth() - 1248.0f * s) * 0.5f, 0.0f));
    }
private:
    juce::Component& inner;
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
