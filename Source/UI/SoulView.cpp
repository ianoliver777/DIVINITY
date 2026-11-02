#include "SoulView.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

//==============================================================================
// XYPad
//==============================================================================
XYPad::XYPad (DivinityProcessor& p, bool c)
    : proc (p), compact (c),
      px (*p.apvts.getParameter (ids::soulX)), py (*p.apvts.getParameter (ids::soulY)),
      ax (px, [this] (float v) { x = v; repaint(); }, p.apvts.undoManager),
      ay (py, [this] (float v) { y = v; repaint(); }, p.apvts.undoManager)
{
    ax.sendInitialUpdate();
    ay.sendInitialUpdate();
    setTooltip ("SOUL ENGINE: drag to shape the emotion of the sound. Left = soft, right = aggressive, down = melancholy, up = euphoria. Double-click to return to neutral.");
    setRepaintsOnMouseActivity (true);
    startTimerHz (30);
}

juce::Rectangle<float> XYPad::padArea() const
{
    return getLocalBounds().toFloat().reduced (compact ? 4.0f : 10.0f);
}

void XYPad::timerCallback()
{
    const float gx = proc.getSoulX(), gy = proc.getSoulY();
    if (std::abs (gx - ghostX) > 1.0e-4f || std::abs (gy - ghostY) > 1.0e-4f || ! trail.empty())
    {
        ghostX = gx; ghostY = gy;
        const auto a = padArea();
        trail.push_back ({ a.getX() + (ghostX * 0.5f + 0.5f) * a.getWidth(), a.getBottom() - (ghostY * 0.5f + 0.5f) * a.getHeight() });
        while (trail.size() > 24) trail.pop_front();
        if (trail.size() > 1 && trail.front() == trail.back()) trail.pop_front();
        repaint();
    }
}

void XYPad::paint (juce::Graphics& g)
{
    const auto a = padArea();
    const float r = compact ? 6.0f : 10.0f;

    // emotional field: warm violet toward euphoria, deep blue-grey toward melancholy, heat toward aggression
    g.setColour (juce::Colour (0xff0c0d10));
    g.fillRoundedRectangle (a, r);
    const auto puckPos = juce::Point<float> (a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight());
    g.setGradientFill (juce::ColourGradient (accentStrong.withAlpha (0.22f), puckPos.x, puckPos.y,
                                             juce::Colours::transparentBlack, puckPos.x + a.getWidth() * 0.55f, puckPos.y, true));
    g.fillRoundedRectangle (a, r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x18ffffff), a.getCentreX(), a.getY(), juce::Colours::transparentBlack, a.getCentreX(), a.getCentreY(), false));
    g.fillRoundedRectangle (a, r);

    // grid
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    const int lines = compact ? 4 : 8;
    for (int i = 1; i < lines; ++i)
    {
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * i / lines), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * i / lines), a.getX(), a.getRight());
    }
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawVerticalLine ((int) a.getCentreX(), a.getY(), a.getBottom());
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());
    // centre target
    g.drawEllipse (juce::Rectangle<float> (compact ? 10.0f : 16.0f, compact ? 10.0f : 16.0f).withCentre (a.getCentre()), 1.0f);
    g.setColour (border);
    g.drawRoundedRectangle (a, r, 1.0f);

    // axis labels
    g.setColour (textDim);
    g.setFont (font (compact ? 9.0f : 11.5f, Weight::Medium, 0.25f));
    const int pad = compact ? 4 : 12;
    auto ai = a.toNearestInt().reduced (pad);
    g.drawText ("EUPHORIA", ai, juce::Justification::centredTop);
    g.drawText ("MELANCHOLY", ai, juce::Justification::centredBottom);
    g.drawText ("SOFT", ai, juce::Justification::centredLeft);
    g.drawText ("AGGRESSIVE", ai, juce::Justification::centredRight);
    if (! compact)
    {
        g.setColour (textFaint);
        g.setFont (font (10.0f, Weight::Medium, 0.25f));
        g.drawText ("SERENITY", ai, juce::Justification::topLeft);
        g.drawText ("ECSTASY", ai, juce::Justification::topRight);
        g.drawText ("LONGING", ai, juce::Justification::bottomLeft);
        g.drawText ("RAGE", ai, juce::Justification::bottomRight);
    }

    // trail of the position the engine is really using
    if (trail.size() > 1)
    {
        juce::Path tp;
        tp.startNewSubPath (trail.front());
        for (size_t i = 1; i < trail.size(); ++i) tp.lineTo (trail[i]);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (tp, juce::PathStrokeType (compact ? 1.5f : 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const juce::Point<float> ghost (a.getX() + (ghostX * 0.5f + 0.5f) * a.getWidth(), a.getBottom() - (ghostY * 0.5f + 0.5f) * a.getHeight());
    if (ghost.getDistanceFrom (puckPos) > 2.0f)
    {
        g.setColour (accent.withAlpha (0.5f));
        g.drawEllipse (juce::Rectangle<float> (compact ? 8.0f : 12.0f, compact ? 8.0f : 12.0f).withCentre (ghost), 1.2f);
    }

    // puck
    const float pr = compact ? 9.0f : 15.0f;
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f), puckPos.x, puckPos.y, juce::Colours::transparentBlack, puckPos.x + pr * 3.0f, puckPos.y, true));
    g.fillEllipse (juce::Rectangle<float> (pr * 6.0f, pr * 6.0f).withCentre (puckPos));
    g.setColour (juce::Colour (0xff15161b));
    g.fillEllipse (juce::Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (puckPos));
    g.setColour (accent);
    g.drawEllipse (juce::Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (puckPos), compact ? 1.5f : 2.0f);
    g.fillEllipse (juce::Rectangle<float> (pr * 0.6f, pr * 0.6f).withCentre (puckPos));
}

void XYPad::setFromMouse (juce::Point<float> pos)
{
    const auto a = padArea();
    ax.setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, (pos.x - a.getX()) / a.getWidth()));
    ay.setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, (a.getBottom() - pos.y) / a.getHeight()));
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Return to neutral");
        m.addItem (2, "Soft X: type a value...");
        m.addItem (3, "Euphoria Y: type a value...");
        juce::Component::SafePointer<XYPad> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) { safe->ax.setValueAsCompleteGesture (0.5f); safe->ay.setValueAsCompleteGesture (0.5f); }
            if (r == 2) showParameterMenu (*safe, safe->px);
            if (r == 3) showParameterMenu (*safe, safe->py);
        });
        return;
    }
    ax.beginGesture(); ay.beginGesture();
    setFromMouse (e.position);
}

void XYPad::mouseDrag (const juce::MouseEvent& e) { if (! e.mods.isPopupMenu()) setFromMouse (e.position); }
void XYPad::mouseUp (const juce::MouseEvent& e)   { if (! e.mods.isPopupMenu()) { ax.endGesture(); ay.endGesture(); } }

void XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    ax.setValueAsCompleteGesture (0.5f);
    ay.setValueAsCompleteGesture (0.5f);
}

//==============================================================================
// SoulView
//==============================================================================
SoulView::SoulView (DivinityProcessor& p)
    : proc (p), pad (p, false),
      intensity (p.apvts, ids::soulIntensity, "Intensity"),
      curve (p.apvts, ids::soulCurve, "Curve", ModDest::None, 1.0f, true),
      smooth (p.apvts, ids::soulSmooth, "Smoothing"),
      divine (p.apvts, ids::divineAmount, "Divine Intervention"),
      divineMode (p.apvts, ids::divineMode, divineModeNames())
{
    addAndMakeVisible (pad);
    for (auto* k : { &intensity, &curve, &smooth }) { k->setCaptionSize (11.5f); addAndMakeVisible (k); }
    divine.setCaptionSize (12.5f);
    addAndMakeVisible (divine);
    addAndMakeVisible (divineMode);

    for (auto& m : soulMappingPresets()) mappingBox.addItem (m.name, mappingBox.getNumItems() + 1);
    mappingBox.setTextWhenNothingSelected ("Custom");
    mappingBox.setTooltip ("Mapping presets set how each target responds to the XY axes. Edit any row to make it your own; it's saved with the patch.");
    mappingBox.onChange = [this] { if (mappingBox.getSelectedItemIndex() >= 0) applyMapping (mappingBox.getSelectedItemIndex()); };
    addAndMakeVisible (mappingBox);

    auto& s = p.apvts;
    rows.resize ((size_t) kNumSoulTargets);
    for (int t = 0; t < kNumSoulTargets; ++t)
    {
        auto& r = rows[(size_t) t];
        r.on = std::make_unique<juce::ToggleButton>();
        r.on->setColour (juce::ToggleButton::tickColourId, accent);
        r.on->setColour (juce::ToggleButton::tickDisabledColourId, textFaint);
        r.on->setTooltip (helpTextFor (ids::soul (t, "on")));
        r.onAttach = std::make_unique<APVTS::ButtonAttachment> (s, ids::soul (t, "on"), *r.on);
        r.on->onStateChange = [this] { repaint(); };
        addAndMakeVisible (*r.on);
        for (int axis = 0; axis < 2; ++axis)
        {
            auto sl = std::make_unique<FineSlider>();
            sl->setSliderStyle (juce::Slider::LinearHorizontal);
            sl->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sl->setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            sl->setDoubleClickReturnValue (true, 0.0);
            const auto id = ids::soul (t, axis == 0 ? "x" : "y");
            sl->setTooltip (helpTextFor (id) + "\n\nDouble-click: zero  |  Right-click: type a value");
            auto* raw = sl.get();
            auto* prm = s.getParameter (id);
            sl->onPopup = [raw, prm] { showParameterMenu (*raw, *prm); };
            sl->onValueChange = [this] { repaint(); };
            addAndMakeVisible (*sl);
            (axis == 0 ? r.xAttach : r.yAttach) = std::make_unique<APVTS::SliderAttachment> (s, id, *sl);
            (axis == 0 ? r.x : r.y) = std::move (sl);
        }
    }
    startTimerHz (20);
}

void SoulView::applyMapping (int index)
{
    const auto& m = soulMappingPresets()[(size_t) index];
    auto& s = proc.apvts;
    if (auto* um = s.undoManager) um->beginNewTransaction ("Soul mapping: " + juce::String (m.name));
    auto set = [&s] (const juce::String& id, float real)
    {
        auto* p = s.getParameter (id);
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (real)); p->endChangeGesture();
    };
    for (int t = 0; t < kNumSoulTargets; ++t)
    {
        set (ids::soul (t, "on"), 1.0f);
        set (ids::soul (t, "x"), m.w[t][0]);
        set (ids::soul (t, "y"), m.w[t][1]);
    }
}

int SoulView::matchingMapping() const
{
    auto& s = proc.apvts;
    const auto& presets = soulMappingPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        bool same = true;
        for (int t = 0; t < kNumSoulTargets && same; ++t)
            same = s.getRawParameterValue (ids::soul (t, "on"))->load() > 0.5f
                && std::abs (s.getRawParameterValue (ids::soul (t, "x"))->load() - presets[(size_t) i].w[t][0]) < 0.005f
                && std::abs (s.getRawParameterValue (ids::soul (t, "y"))->load() - presets[(size_t) i].w[t][1]) < 0.005f;
        if (same) return i;
    }
    return -1;
}

void SoulView::timerCallback()
{
    const int m = matchingMapping();
    repaint (770, 636, 460, 12);   // live Divine amount bar
    if (m != lastMatch)
    {
        lastMatch = m;
        if (m >= 0) mappingBox.setSelectedItemIndex (m, juce::dontSendNotification);
        else mappingBox.setSelectedId (0, juce::dontSendNotification);
        repaint();
    }
}

void SoulView::paint (juce::Graphics& g)
{
    drawPanelBackground (g, { 0.0f, 0.0f, 560.0f, 682.0f });
    drawSectionTitle (g, { 16, 10, 300, 24 }, "SOUL ENGINE");
    g.setColour (textDim);
    g.setFont (font (12.0f));
    g.drawText ("Drag to feel. Double-click for neutral.", 230, 12, 314, 22, juce::Justification::centredRight);

    drawPanelBackground (g, { 572.0f, 0.0f, 676.0f, 446.0f });
    drawSectionTitle (g, { 588, 10, 300, 24 }, "EMOTION MAPPING");
    g.setFont (font (10.5f, Weight::Medium, 0.1f));
    g.setColour (textDim);
    g.drawText ("TARGET", 614, 50, 110, 16, juce::Justification::centredLeft);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("SOFT \xe2\x86\x90  \xe2\x86\x92 AGGRESSIVE")), 735, 50, 195, 16, juce::Justification::centred);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("MELANCHOLY \xe2\x86\x90  \xe2\x86\x92 EUPHORIA")), 985, 50, 195, 16, juce::Justification::centred);

    auto& s = proc.apvts;
    for (int t = 0; t < kNumSoulTargets; ++t)
    {
        const int y = 76 + t * 36;
        const bool on = s.getRawParameterValue (ids::soul (t, "on"))->load() > 0.5f;
        g.setFont (font (13.0f, Weight::Medium));
        g.setColour (on ? text : textFaint);
        g.drawText (soulTargets()[(size_t) t].name, 614, y, 120, 26, juce::Justification::centredLeft);
        g.setFont (font (11.0f));
        for (int axis = 0; axis < 2; ++axis)
        {
            const float v = s.getRawParameterValue (ids::soul (t, axis == 0 ? "x" : "y"))->load();
            const int n = juce::roundToInt (v * 100.0f);
            g.setColour (on && n != 0 ? text.withAlpha (0.85f) : textFaint);
            g.drawText ((n > 0 ? "+" : "") + juce::String (n), axis == 0 ? 932 : 1182, y, 40, 26, juce::Justification::centredRight);
        }
        g.setColour (border.withAlpha (0.5f));
        g.drawHorizontalLine (y + 31, 586.0f, 1234.0f);
    }

    drawPanelBackground (g, { 572.0f, 458.0f, 676.0f, 224.0f });
    drawSectionTitle (g, { 588, 466, 400, 24 }, "DIVINE INTERVENTION");
    const int mode = juce::roundToInt (s.getRawParameterValue (ids::divineMode)->load());
    const float amt = proc.getDivineAmount();
    g.setColour (text);
    g.setFont (font (22.0f, Weight::Medium, 0.04f));
    g.drawText (divineModeNames()[mode].toUpperCase(), 770, 546, 460, 30, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (font (13.0f));
    g.drawFittedText (divineModeDescription (mode), 770, 578, 460, 40, juce::Justification::topLeft, 2);
    auto bar = juce::Rectangle<float> (770.0f, 640.0f, 460.0f, 4.0f);
    g.setColour (border);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setGradientFill (juce::ColourGradient (accentStrong, bar.getX(), 0, accent, bar.getRight(), 0, false));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, amt)), 2.0f);
}

void SoulView::resized()
{
    pad.setBounds (16, 44, 528, 520);
    intensity.setBounds (60, 574, 110, 100);
    curve.setBounds (225, 574, 110, 100);
    smooth.setBounds (390, 574, 110, 100);
    mappingBox.setBounds (1036, 9, 196, 28);
    for (int t = 0; t < kNumSoulTargets; ++t)
    {
        const int y = 76 + t * 36;
        rows[(size_t) t].on->setBounds (586, y + 2, 24, 22);
        rows[(size_t) t].x->setBounds (735, y + 4, 195, 18);
        rows[(size_t) t].y->setBounds (985, y + 4, 195, 18);
    }
    divine.setBounds (590, 492, 160, 182);
    divineMode.setBounds (770, 500, 460, 32);
}

} // namespace divinity::ui
