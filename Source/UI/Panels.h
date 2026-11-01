#pragma once

#include "Components.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace divinity { class DivinityProcessor; }

namespace divinity::ui
{
//==============================================================================
/** Titled panel background shared by every section. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String t = {}) : title (std::move (t)) {}
    void paint (juce::Graphics&) override;
    void setHighlighted (bool h) { if (h != highlighted) { highlighted = h; repaint(); } }
protected:
    juce::String title;
    bool highlighted = false;
    int titleHeight = 40;
};

//==============================================================================
class OscCard : public juce::Component
{
public:
    OscCard (DivinityProcessor& p, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void updateEngine();
    void showMore();
    DivinityProcessor& proc;
    int index;
    PowerButton power;
    Selector engine;
    OscDisplay display;
    IconButton more;
    juce::OwnedArray<Knob> wtKnobs, anKnobs;
};

/** Compact card for the sub or noise layer. */
class LayerCard : public juce::Component
{
public:
    LayerCard (DivinityProcessor& p, bool isNoise);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    bool noise;
    PowerButton power;
    Selector mode;
    Knob level;
};

class OscillatorSection : public Panel
{
public:
    explicit OscillatorSection (DivinityProcessor& p);
    void resized() override;
private:
    juce::OwnedArray<OscCard> oscs;
    LayerCard sub, noise;
};

//==============================================================================
class FilterSection : public Panel
{
public:
    explicit FilterSection (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void select (int f);
    DivinityProcessor& proc;
    TabRow tabs;
    IconButton routing;
    juce::ParameterAttachment routingAttachment;
    juce::OwnedArray<ChoiceBox> types;
    juce::OwnedArray<NumberBox> keytrack;
    FilterResponse response;
    juce::OwnedArray<Knob> knobs[2];
    int current = 0;
};

class EnvelopeSection : public Panel
{
public:
    explicit EnvelopeSection (DivinityProcessor& p);
    void resized() override;
private:
    void select (int e);
    TabRow tabs;
    EnvelopeDisplay display;
    juce::OwnedArray<Knob> knobs[kNumEnvs];
    int current = 0;
};

class LfoSection : public Panel
{
public:
    explicit LfoSection (DivinityProcessor& p);
    void resized() override;
private:
    void select (int l);
    DivinityProcessor& proc;
    TabRow tabs;
    LfoDisplay display;
    struct PerLfo
    {
        std::unique_ptr<juce::Component> shapes;
        std::unique_ptr<juce::TextButton> sync;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;
        std::unique_ptr<ChoiceBox> division;
        juce::OwnedArray<Knob> knobs;
    };
    PerLfo lfos[kNumLfos];
    int current = 0;
};

//==============================================================================
class ModMatrixSection : public Panel, private juce::Timer
{
public:
    explicit ModMatrixSection (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { repaint(); }
    void addRouting();
    DivinityProcessor& proc;
    IconButton add { "Add a routing in the first empty slot", [] { juce::Path p; p.startNewSubPath (12, 4); p.lineTo (12, 20); p.startNewSubPath (4, 12); p.lineTo (20, 12); return p; }() };
    struct Row
    {
        std::unique_ptr<ChoiceBox> src, dst;
        std::unique_ptr<FineSlider> amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    Row rows[kNumModSlots];
};

class MacroSection : public Panel
{
public:
    MacroSection (DivinityProcessor& p, bool large = false);
    void resized() override;
private:
    juce::OwnedArray<Knob> knobs;
    bool large;
};

//==============================================================================
class FxSection : public Panel
{
public:
    explicit FxSection (DivinityProcessor& p);
    ~FxSection() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    struct Tile;
private:
    DivinityProcessor& proc;
    juce::OwnedArray<Tile> tiles;
};

class GlobalSection : public Panel
{
public:
    explicit GlobalSection (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    NumberBox voices, bend, glide, tune;
    SegmentedChoice mode;
};

//==============================================================================
class KeyboardBar : public juce::Component
{
public:
    explicit KeyboardBar (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DivinityProcessor& proc;
    FineSlider pitch, mod;
    juce::MidiKeyboardComponent keyboard;
    Knob master;
    LevelMeter meter;
};

} // namespace divinity::ui
