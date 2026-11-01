#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace divinity::ui
{
namespace colours
{
    const juce::Colour background   { 0xff090a0c };
    const juce::Colour panel        { 0xff111317 };
    const juce::Colour card         { 0xff16181d };
    const juce::Colour panel2       { 0xff1d2026 };
    const juce::Colour border       { 0xff2a2e36 };
    const juce::Colour borderBright { 0xff3a3f4a };
    const juce::Colour text         { 0xfff1f1f1 };
    const juce::Colour textDim      { 0xff9a9da5 };
    const juce::Colour textFaint    { 0xff5d616b };
    const juce::Colour accent       { 0xffc9b8f2 };   // violet-white
    const juce::Colour accentStrong { 0xff9b84dc };
    const juce::Colour accentDeep   { 0xff4a3d78 };
    const juce::Colour modulation   { 0xffb89cff };
}

enum class Weight { Regular, Medium, SemiBold };

/** Inter (SIL OFL) embedded fonts with consistent tracking. */
juce::Font font (float height, Weight w = Weight::Regular, float tracking = 0.0f);

void drawPanelBackground (juce::Graphics& g, juce::Rectangle<float> r, bool highlighted = false);
void drawCardBackground (juce::Graphics& g, juce::Rectangle<float> r, bool active = true);
void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

//==============================================================================
class DivinityLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DivinityLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.5f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.5f, Weight::Medium, 0.04f); }

    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;

private:
    juce::Typeface::Ptr regular, medium, semibold;
};

} // namespace divinity::ui
