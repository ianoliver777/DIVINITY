#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace divinity::ui
{
namespace colours
{
    // Mutable palette: DAY (the 1.0 light skin, default) or NIGHT (the v0.7 black-glass + cyan skin).
    // Read at paint time everywhere, so switching repaints the whole UI in the other skin.
    inline juce::Colour background, panel, card, panel2, border, borderBright, text, textDim, textFaint,
                        accent, accentStrong, accentDeep, modulation, glow;
    // surfaces that used to be hard-coded dark values
    inline juce::Colour well, well2, wellBorder, panelTop, panelBottom, panelRimTop, panelRimBottom, displayTop, displayBottom,
                        displayBorder, selTop, selBottom, btnTop, btnBottom, btnHover, menuBg, trackBg, knobTop, knobBottom,
                        knobRimTop, knobRimBottom, meterBg, cardTop, cardBottom, onAccent;
    void setNightMode (bool night);
    bool isNightMode();
    /** Loads the saved skin choice (per user, not per project). */
    void loadSavedSkin();
    void saveSkin (bool night);
}

enum class Weight { Regular, Medium, SemiBold };

/** Inter (SIL OFL) embedded fonts with consistent tracking. */
juce::Font font (float height, Weight w = Weight::Regular, float tracking = 0.0f);

void drawPanelBackground (juce::Graphics& g, juce::Rectangle<float> r, bool highlighted = false);
/** Inset "screen" behind displays (waveforms, envelopes, spectrum). */
void drawDisplayBackground (juce::Graphics& g, juce::Rectangle<float> r);
/** A path drawn as a glowing cyan line (wide soft halo + bright core). */
void strokeGlow (juce::Graphics& g, const juce::Path& p, float width = 1.8f, juce::Colour c = colours::accent);
/** Rounded glass control (rows, dropdowns, tabs); selected = cyan glow. */
void drawGlassControl (juce::Graphics& g, juce::Rectangle<float> r, bool selected, bool hover = false, float corner = 6.0f);
void drawCardBackground (juce::Graphics& g, juce::Rectangle<float> r, bool active = true);
void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

//==============================================================================
class DivinityLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DivinityLookAndFeel();
    /** Re-applies the current palette (day / night) to the JUCE colour IDs. */
    void applyPalette();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.5f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        const double size = b.getProperties().getWithDefault ("fontSize", 13.5);
        return font ((float) size, Weight::Medium, size > 15.0 ? 0.02f : 0.04f);
    }

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
