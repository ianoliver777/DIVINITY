#include "DivinityLookAndFeel.h"
#include "DivinityAssets.h"

namespace divinity::ui
{
namespace
{
    juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr r = juce::Typeface::createSystemTypefaceFor (DivinityAssets::InterRegular_ttf, (size_t) DivinityAssets::InterRegular_ttfSize);
        static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (DivinityAssets::InterMedium_ttf, (size_t) DivinityAssets::InterMedium_ttfSize);
        static juce::Typeface::Ptr s = juce::Typeface::createSystemTypefaceFor (DivinityAssets::InterSemiBold_ttf, (size_t) DivinityAssets::InterSemiBold_ttfSize);
        return w == Weight::SemiBold ? s : (w == Weight::Medium ? m : r);
    }
}

juce::Font font (float height, Weight w, float tracking)
{
    juce::Font f { juce::FontOptions { typeface (w) }.withHeight (height) };
    if (tracking != 0.0f) f.setExtraKerningFactor (tracking);
    return f;
}

void drawPanelBackground (juce::Graphics& g, juce::Rectangle<float> r, bool highlighted)
{
    juce::ColourGradient grad (colours::panel.brighter (0.04f), r.getX(), r.getY(), colours::panel, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (highlighted ? colours::accentStrong.withAlpha (0.7f) : colours::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, highlighted ? 1.2f : 1.0f);
}

void drawCardBackground (juce::Graphics& g, juce::Rectangle<float> r, bool active)
{
    juce::ColourGradient grad (colours::card.brighter (0.05f), r.getX(), r.getY(), colours::card.darker (0.1f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (active ? colours::borderBright.withAlpha (0.8f) : colours::border.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    g.setColour (colours::text.withAlpha (0.92f));
    g.setFont (font (13.5f, Weight::Medium, 0.32f));
    g.drawText (text, area, juce::Justification::centredLeft, false);
}

//==============================================================================
DivinityLookAndFeel::DivinityLookAndFeel()
{
    regular = typeface (Weight::Regular);
    medium = typeface (Weight::Medium);
    semibold = typeface (Weight::SemiBold);

    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15171c));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDeep);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::buttonOnColourId, accentDeep);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::TextEditor::backgroundColourId, panel2);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, accentStrong);
    setColour (juce::TextEditor::highlightColourId, accentDeep);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff15171c));
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, border);
    setColour (juce::ScrollBar::thumbColourId, borderBright);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf01a1c22));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, border);
}

juce::Typeface::Ptr DivinityLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? semibold : regular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

//==============================================================================
void DivinityLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();
    const float trackR = radius - 1.5f;

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (border.withAlpha (0.9f));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar sliders draw from the centre)
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour ((enabled ? accent : textFaint).withAlpha (0.95f));
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // modulation ring (set by the owning Knob component)
    const float modAmt = (float) (double) s.getProperties().getWithDefault ("modAmount", 0.0);
    if (std::abs (modAmt) > 0.001f)
    {
        const float span = endAngle - startAngle;
        const float a2 = juce::jlimit (startAngle, endAngle, angle + modAmt * span);
        juce::Path mod;
        const float mr = radius + 2.5f;
        mod.addCentredArc (c.x, c.y, mr, mr, 0.0f, juce::jmin (angle, a2), juce::jmax (angle, a2), true);
        g.setColour (modulation.withAlpha (0.85f));
        g.strokePath (mod, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float bodyR = radius * 0.78f;
    auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 1.5f).expanded (1.0f));
    juce::ColourGradient grad (juce::Colour (0xff30333b), c.x - bodyR * 0.5f, c.y - bodyR, juce::Colour (0xff121418), c.x + bodyR * 0.4f, c.y + bodyR, true);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    juce::ColourGradient rim (juce::Colour (0xff6a6f7b).withAlpha (0.9f), c.x, body.getY(), juce::Colour (0xff1c1e23), c.x, body.getBottom(), false);
    g.setGradientFill (rim);
    g.drawEllipse (body.reduced (0.6f), 1.1f);

    // pointer
    const float p1 = bodyR * 0.25f, p2 = bodyR * 0.92f;
    const juce::Point<float> a (c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle));
    const juce::Point<float> b (c.x + p2 * std::sin (angle), c.y - p2 * std::cos (angle));
    g.setColour ((enabled ? text : textFaint).withAlpha (0.95f));
    g.drawLine ({ a, b }, juce::jmax (1.5f, radius * 0.07f));
}

void DivinityLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace colours;
    if (style == juce::Slider::LinearBarVertical || style == juce::Slider::LinearBar)
    {
        // number box (VOICES / GLIDE ...): text drawn by the textbox
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        g.setColour (panel2);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (s.isMouseOverOrDragging() ? borderBright : border);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        return;
    }

    if (style == juce::Slider::LinearHorizontal)
    {
        // thin amount bar (mod matrix): bipolar from centre
        auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 2.0f, (float) w, 4.0f);
        g.setColour (border);
        g.fillRoundedRectangle (r, 2.0f);
        const bool bipolar = s.getMinimum() < 0.0;
        const float zero = bipolar ? r.getCentreX() : r.getX();
        auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, pos), r.getY(), juce::jmax (zero, pos), r.getBottom());
        juce::ColourGradient grad (accentStrong, fill.getX(), 0, accent, fill.getRight(), 0, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);
        return;
    }

    if (style == juce::Slider::LinearVertical)
    {
        // pitch / mod wheel
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        auto wheel = r.withSizeKeepingCentre (juce::jmin (22.0f, r.getWidth()), r.getHeight());
        juce::ColourGradient grad (juce::Colour (0xff0d0e11), wheel.getX(), 0, juce::Colour (0xff2b2e35), wheel.getCentreX(), 0, true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (wheel, 4.0f);
        g.setColour (border);
        g.drawRoundedRectangle (wheel, 4.0f, 1.0f);
        // ridges
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (float yy = wheel.getY() + 4; yy < wheel.getBottom() - 4; yy += 4)
            g.drawHorizontalLine ((int) yy, wheel.getX() + 3, wheel.getRight() - 3);
        auto thumb = juce::Rectangle<float> (wheel.getWidth() - 4.0f, 6.0f).withCentre ({ wheel.getCentreX(), pos });
        g.setColour (accent);
        g.fillRoundedRectangle (thumb, 2.0f);
        juce::ignoreUnused (minPos, maxPos);
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
}

juce::Label* DivinityLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (12.5f));
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, colours::panel2);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

//==============================================================================
void DivinityLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (colours::panel2.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? colours::borderBright : colours::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // chevron
    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    juce::Path p;
    p.startNewSubPath (cx - 4.0f, cy - 2.0f);
    p.lineTo (cx, cy + 2.0f);
    p.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (colours::textDim);
    g.strokePath (p, juce::PathStrokeType (1.3f));
}

void DivinityLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void DivinityLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::border);
    g.drawRect (0, 0, w, h);
}

void DivinityLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                             bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::border);
        g.drawHorizontalLine (area.getCentreY(), (float) area.getX() + 8, (float) area.getRight() - 8);
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour (colours::accentDeep.withAlpha (0.8f));
        g.fillRect (area.reduced (2, 0));
    }
    auto r = area.reduced (12, 0);
    g.setColour (isActive ? (textColour != nullptr ? *textColour : colours::text) : colours::textFaint);
    g.setFont (getPopupMenuFont());
    if (isTicked)
    {
        g.setColour (colours::accent);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ (float) area.getX() + 9.0f, (float) area.getCentreY() }));
        g.setColour (colours::text);
    }
    g.drawFittedText (text, r.withTrimmedLeft (6), juce::Justification::centredLeft, 1);
    if (shortcut.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.drawText (shortcut, r, juce::Justification::centredRight);
    }
    if (hasSubMenu)
    {
        juce::Path p;
        const float x = (float) area.getRight() - 12.0f, y = (float) area.getCentreY();
        p.startNewSubPath (x - 2, y - 4); p.lineTo (x + 2, y); p.lineTo (x - 2, y + 4);
        g.setColour (colours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }
    juce::ignoreUnused (icon);
}

void DivinityLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, Weight::Medium, 0.2f));
    g.drawText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft);
}

void DivinityLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if ((bool) b.getProperties().getWithDefault ("transparent", false))
    {
        if (over || down) { g.setColour (colours::panel2.withAlpha (down ? 0.6f : 0.35f)); g.fillRoundedRectangle (r, 4.0f); }
        return;
    }
    const float corner = (float) (double) b.getProperties().getWithDefault ("corner", 4.0);
    if (on)
    {
        juce::ColourGradient grad (juce::Colour (0xff4e4470), r.getX(), r.getY(), juce::Colour (0xff3a3358), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);
        g.setColour (colours::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
    else
    {
        g.setColour (colours::panel2.withAlpha (down ? 1.0f : (over ? 0.9f : 0.55f)));
        g.fillRoundedRectangle (r, corner);
        if (! (bool) b.getProperties().getWithDefault ("noOutline", false))
        {
            g.setColour (over ? colours::borderBright : colours::border);
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }
}

//==============================================================================
void DivinityLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::borderBright);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, font (13.0f), colours::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 300.0f);
    tl.draw (g, r.reduced (10.0f, 7.0f));
}

juce::Rectangle<int> DivinityLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (text, font (13.0f));
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 300.0f);
    const int w = (int) std::ceil (tl.getWidth() + 22.0f), h = (int) std::ceil (tl.getHeight() + 16.0f);
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
               .constrainedWithin (parent);
}

void DivinityLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colour (0xf416181d));
    g.fillPath (path);
    g.setColour (colours::borderBright);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void DivinityLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                         int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
    g.setColour (colours::borderBright.withAlpha (over || down ? 1.0f : 0.6f));
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

void DivinityLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (colours::panel2);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
}

void DivinityLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? colours::accentStrong : colours::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 4.0f, 1.0f);
}

} // namespace divinity::ui
