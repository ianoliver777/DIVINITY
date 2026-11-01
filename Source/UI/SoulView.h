#pragma once

#include "Panels.h"

namespace divinity::ui
{
//==============================================================================
/** The SOUL ENGINE XY pad. Drag to move, double-click to centre. Shows the
    smoothed / modulated position the engine is actually using as a ghost. */
class XYPad : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    XYPad (DivinityProcessor& p, bool compact);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setFromMouse (juce::Point<float>);
    juce::Rectangle<float> padArea() const;
    DivinityProcessor& proc;
    bool compact;
    juce::RangedAudioParameter &px, &py;
    juce::ParameterAttachment ax, ay;
    float x = 0.5f, y = 0.5f, ghostX = 0, ghostY = 0;
    std::deque<juce::Point<float>> trail;
};

//==============================================================================
class SoulView : public juce::Component, private juce::Timer
{
public:
    explicit SoulView (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyMapping (int presetIndex);
    int matchingMapping() const;

    DivinityProcessor& proc;
    XYPad pad;
    Knob intensity, curve, smooth;
    juce::ComboBox mappingBox;
    struct Row
    {
        std::unique_ptr<juce::ToggleButton> on;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
        std::unique_ptr<FineSlider> x, y;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAttach, yAttach;
    };
    std::vector<Row> rows;
    Knob divine;
    SegmentedChoice divineMode;
    int lastMatch = -2;
};

} // namespace divinity::ui
