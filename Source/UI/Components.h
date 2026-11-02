#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "DivinityLookAndFeel.h"
#include "../Parameters.h"

namespace divinity
{
class DivinityProcessor;
class WavetableBank;
}

namespace divinity::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** True when this component and all its parents are visible (works off-screen too, unlike isShowing). */
/** Draws a large source image crisply at small sizes: the image is resampled in halving
    steps to the exact physical pixel size it will be shown at, and cached until that size changes. */
class CrispImage
{
public:
    void setSource (juce::Image img) { source = std::move (img); cached = {}; }
    bool isValid() const { return source.isValid(); }
    int getWidth() const  { return source.getWidth(); }
    int getHeight() const { return source.getHeight(); }

    void draw (juce::Graphics& g, juce::Rectangle<float> area, float opacity = 1.0f)
    {
        if (! source.isValid() || area.isEmpty()) return;
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int w = juce::jmax (1, juce::roundToInt (area.getWidth() * scale));
        const int h = juce::jmax (1, juce::roundToInt (area.getHeight() * scale));
        if (! cached.isValid() || std::abs (cached.getWidth() - w) > 1 || std::abs (cached.getHeight() - h) > 1)
        {
            auto img = source;
            while (img.getWidth() > w * 2 && img.getHeight() > h * 2)
                img = img.rescaled (img.getWidth() / 2, img.getHeight() / 2, juce::Graphics::highResamplingQuality);
            cached = img.rescaled (w, h, juce::Graphics::highResamplingQuality);
        }
        g.setOpacity (opacity);
        g.drawImage (cached, area, juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
    }
private:
    juce::Image source, cached;
};

inline bool visibleInTree (const juce::Component& c)
{
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (! p->isVisible()) return false;
    return true;
}

/** Sum of mod-matrix amounts currently routed to `dest` (ignores empty sources). */
float modAmountFor (APVTS& apvts, ModDest dest);

namespace icons
{
    juce::Path heart();
    juce::Path save();
    juce::Path gear();
    juce::Path power (juce::Rectangle<float> r);
    juce::Path link (juce::Rectangle<float> r);
    juce::Path nav (int index, juce::Rectangle<float> r);   // PLAY, SYNTH, MOTION, FX, SOUL, LAB
}

//==============================================================================
/** Slider with shift-drag fine adjustment and a right-click hook. */
class FineSlider : public juce::Slider
{
public:
    std::function<void()> onPopup;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onPopup) { onPopup(); return; }
        juce::Slider::mouseDown (e);
    }
};

/** Shows a small menu: Enter value / Reset / Help for any parameter. */
void showParameterMenu (juce::Component& owner, juce::RangedAudioParameter& param);

//==============================================================================
/** Rotary knob + caption, attached to a parameter. Caption shows the value while hovered or dragged. */
class Knob : public juce::Component, private juce::Timer
{
public:
    Knob (APVTS& apvts, const juce::String& paramID, const juce::String& caption,
          ModDest dest = ModDest::None, float modScale = 1.0f, bool bipolar = false);

    void resized() override;
    void paint (juce::Graphics&) override;
    void setCaptionSize (float s) { captionSize = s; repaint(); }
    void updateModulation();
    juce::Slider& getSlider() { return slider; }

private:
    void timerCallback() override;

    APVTS& apvts;
    juce::RangedAudioParameter& param;
    FineSlider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::String caption;
    ModDest dest;
    float modScale, captionSize = 10.5f;
    bool showingValue = false;
};

//==============================================================================
class PowerButton : public juce::ToggleButton
{
public:
    PowerButton (APVTS& apvts, const juce::String& paramID);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    APVTS::ButtonAttachment attachment;
};

//==============================================================================
/** "<  Wavetable  >" selector bound to a choice parameter. Click the name for a menu. */
class Selector : public juce::Component, public juce::SettableTooltipClient
{
public:
    Selector (APVTS& apvts, const juce::String& paramID, bool drawBox = true);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void step (int delta);
    std::function<void()> onChange;
    int getIndex() const noexcept { return index; }

private:
    juce::AudioParameterChoice& param;
    juce::ParameterAttachment attachment;
    int index = 0;
    bool drawBox;
};

/** Dropdown-style choice box bound to a choice parameter. */
class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox (APVTS& apvts, const juce::String& paramID);
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

/** Segmented buttons ("POLY | LEGATO") bound to a choice parameter. */
class SegmentedChoice : public juce::Component
{
public:
    /** valueForButton maps button i to choice index (default: identity), so buttons can be shown in any order. */
    SegmentedChoice (APVTS& apvts, const juce::String& paramID, juce::StringArray labels, std::vector<int> valueForButton = {});
    void resized() override;
private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::vector<int> map;
    juce::AudioParameterChoice& param;
    juce::ParameterAttachment attachment;
};

/** Small drag/number box (VOICES, GLIDE, BEND...). */
class NumberBox : public juce::Component
{
public:
    NumberBox (APVTS& apvts, const juce::String& paramID);
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider& getSlider() { return slider; }
private:
    FineSlider slider;
    APVTS::SliderAttachment attachment;
};

/** Tab row (FILTER 1 / FILTER 2, ENV 1..3, LFO 1..4). */
class TabRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TabRow (juce::StringArray names);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setSelected (int i, bool notify = true);
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onSelect;
private:
    juce::OwnedArray<juce::TextButton> tabs;
    int selected = 0;
};

/** Icon button drawn from a path. */
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, juce::Path p, bool fillPath = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setPath (juce::Path p) { path = std::move (p); repaint(); }
    bool filledWhenOn = false;
private:
    juce::Path path;
    bool fill;
};

//==============================================================================
// Displays
//==============================================================================
class OscDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    OscDisplay (APVTS& apvts, const WavetableBank& bank, int oscIndex);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    APVTS& apvts;
    const WavetableBank& bank;
    int osc;
    float lastHash = -1.0f;
};

class FilterResponse : public juce::Component, private juce::Timer
{
public:
    FilterResponse (APVTS& apvts, int filterIndex);
    void paint (juce::Graphics&) override;
    void setFilter (int i) { filter = i; repaint(); }
private:
    void timerCallback() override;
    APVTS& apvts;
    int filter;
    float lastHash = -1.0f;
};

class EnvelopeDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    EnvelopeDisplay (APVTS& apvts, int envIndex);
    void paint (juce::Graphics&) override;
    void setEnvelope (int i) { env = i; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    struct Geometry { juce::Point<float> start, peak, decayEnd, releaseStart, end; float unit; juce::Rectangle<float> area; };
    Geometry geometry() const;
    APVTS& apvts;
    int env;
    int dragNode = -1;
    float lastHash = -1.0f;
};

class LfoDisplay : public juce::Component, private juce::Timer
{
public:
    LfoDisplay (DivinityProcessor& p, int lfoIndex);
    void paint (juce::Graphics&) override;
    void setLfo (int i) { lfo = i; repaint(); }
private:
    void timerCallback() override { repaint(); }
    DivinityProcessor& proc;
    int lfo;
};

class ScopeDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ScopeDisplay (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    DivinityProcessor& proc;
    std::vector<float> history, pullBuf;
    int writePos = 0;
};

class LevelMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LevelMeter (DivinityProcessor& p, bool vertical);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    DivinityProcessor& proc;
    bool vertical;
    float l = 0, r = 0;
};

} // namespace divinity::ui
