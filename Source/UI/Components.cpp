#include "Components.h"
#include "../PluginProcessor.h"
#include "../DSP/DSPCore.h"
#include <complex>

namespace divinity::ui
{
using namespace colours;

//==============================================================================
float modAmountFor (APVTS& apvts, ModDest dest)
{
    float sum = 0.0f;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) std::lround (apvts.getRawParameterValue (ids::mod (i, "src"))->load());
        const int dst = (int) std::lround (apvts.getRawParameterValue (ids::mod (i, "dst"))->load());
        if (src > 0 && dst == (int) dest)
            sum += apvts.getRawParameterValue (ids::mod (i, "amt"))->load();
    }
    return sum;
}

//==============================================================================
namespace icons
{
    juce::Path heart()
    {
        juce::Path p;
        p.startNewSubPath (12.0f, 20.5f);
        p.cubicTo (5.0f, 15.5f, 2.5f, 12.0f, 2.5f, 8.5f);
        p.cubicTo (2.5f, 5.5f, 4.8f, 3.5f, 7.4f, 3.5f);
        p.cubicTo (9.4f, 3.5f, 11.0f, 4.6f, 12.0f, 6.3f);
        p.cubicTo (13.0f, 4.6f, 14.6f, 3.5f, 16.6f, 3.5f);
        p.cubicTo (19.2f, 3.5f, 21.5f, 5.5f, 21.5f, 8.5f);
        p.cubicTo (21.5f, 12.0f, 19.0f, 15.5f, 12.0f, 20.5f);
        p.closeSubPath();
        return p;
    }
    juce::Path save()
    {
        juce::Path p;
        p.startNewSubPath (4, 4); p.lineTo (16.5f, 4); p.lineTo (20, 7.5f); p.lineTo (20, 20); p.lineTo (4, 20); p.closeSubPath();
        p.addRectangle (7.5f, 4.0f, 8.0f, 5.0f);
        p.addRectangle (7.0f, 13.0f, 10.0f, 7.0f);
        return p;
    }
    juce::Path gear()
    {
        juce::Path p;
        const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (teeth * 2);
            const float r = (i % 2 == 0) ? 10.0f : 7.8f;
            const float a0 = a - 0.17f, a1 = a + 0.17f;
            juce::Point<float> p0 (12 + r * std::sin (a0), 12 - r * std::cos (a0)), p1 (12 + r * std::sin (a1), 12 - r * std::cos (a1));
            if (i == 0) p.startNewSubPath (p0); else p.lineTo (p0);
            p.lineTo (p1);
        }
        p.closeSubPath();
        p.addEllipse (8.6f, 8.6f, 6.8f, 6.8f);
        return p;
    }
    juce::Path power (juce::Rectangle<float> r)
    {
        juce::Path p;
        const auto c = r.getCentre();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.38f;
        p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
        p.startNewSubPath (c.x, c.y - rad * 1.25f);
        p.lineTo (c.x, c.y - rad * 0.2f);
        return p;
    }
    juce::Path link (juce::Rectangle<float> r)
    {
        juce::Path p;
        const float w = r.getWidth() * 0.5f, h = r.getHeight() * 0.42f;
        p.addRoundedRectangle (r.getX(), r.getCentreY() - h * 0.5f, w + 2, h, h * 0.5f);
        p.addRoundedRectangle (r.getRight() - w - 2, r.getCentreY() - h * 0.5f, w + 2, h, h * 0.5f);
        return p;
    }
    juce::Path nav (int index, juce::Rectangle<float> r)
    {
        juce::Path p;
        switch (index)
        {
            case 0: // house
                p.startNewSubPath (3, 11); p.lineTo (12, 3.5f); p.lineTo (21, 11);
                p.startNewSubPath (5.5f, 9.5f); p.lineTo (5.5f, 20.5f); p.lineTo (18.5f, 20.5f); p.lineTo (18.5f, 9.5f);
                p.startNewSubPath (10, 20.5f); p.lineTo (10, 14.5f); p.lineTo (14, 14.5f); p.lineTo (14, 20.5f);
                break;
            case 1: // synth wave
                p.startNewSubPath (2, 17); p.cubicTo (5, 17, 6, 6, 9, 6); p.cubicTo (12, 6, 12, 17, 15, 17); p.cubicTo (18, 17, 19, 6, 22, 6);
                break;
            case 2: // motion
                p.startNewSubPath (2, 18); p.cubicTo (6, 18, 7, 6, 11, 6); p.cubicTo (14, 6, 15, 14, 18, 14); p.lineTo (22, 8);
                break;
            case 3: // fx layers
                p.startNewSubPath (12, 3); p.lineTo (21, 8); p.lineTo (12, 13); p.lineTo (3, 8); p.closeSubPath();
                p.startNewSubPath (3, 12); p.lineTo (12, 17); p.lineTo (21, 12);
                p.startNewSubPath (3, 16); p.lineTo (12, 21); p.lineTo (21, 16);
                break;
            case 4: // soul sparkle
                p.startNewSubPath (12, 2); p.lineTo (12, 22); p.startNewSubPath (2, 12); p.lineTo (22, 12);
                p.startNewSubPath (8, 8); p.lineTo (16, 16); p.startNewSubPath (16, 8); p.lineTo (8, 16);
                p.addEllipse (10, 10, 4, 4);
                break;
            default: // lab grid
                p.addRoundedRectangle (3, 3, 7.5f, 7.5f, 1.2f); p.addRoundedRectangle (13.5f, 3, 7.5f, 7.5f, 1.2f);
                p.addRoundedRectangle (3, 13.5f, 7.5f, 7.5f, 1.2f); p.addRoundedRectangle (13.5f, 13.5f, 7.5f, 7.5f, 1.2f);
                break;
        }
        p.applyTransform (p.getTransformToScaleToFit (r, true));
        return p;
    }
}

//==============================================================================
void showParameterMenu (juce::Component& owner, juce::RangedAudioParameter& param)
{
    juce::PopupMenu m;
    m.addSectionHeader (param.getName (64));
    m.addItem (1, "Enter value...");
    m.addItem (2, "Reset to default");
    const auto help = helpTextFor (param.getParameterID());
    if (help.isNotEmpty()) m.addItem (3, "What does this do?");

    juce::Component::SafePointer<juce::Component> safe (&owner);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner), [safe, &param, help] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1)
        {
            auto* w = new juce::AlertWindow ("Enter value", param.getName (64), juce::MessageBoxIconType::NoIcon, safe.getComponent());
            w->addTextEditor ("v", param.getCurrentValueAsText());
            w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([w, &param] (int res)
            {
                if (res == 1)
                {
                    const auto text = w->getTextEditorContents ("v");
                    param.beginChangeGesture();
                    param.setValueNotifyingHost (param.getValueForText (text));
                    param.endChangeGesture();
                }
            }), true);
        }
        else if (r == 2)
        {
            param.beginChangeGesture();
            param.setValueNotifyingHost (param.getDefaultValue());
            param.endChangeGesture();
        }
        else if (r == 3)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon)
                                              .withTitle (param.getName (64)).withMessage (help).withButton ("Got it")
                                              .withAssociatedComponent (safe.getComponent()), nullptr);
        }
    });
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& cap, ModDest d, float scale, bool bipolar)
    : apvts (s), param (*s.getParameter (id)), caption (cap.toUpperCase()), dest (d), modScale (scale)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // shift = fine
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (helpTextFor (id) + "\n\nShift-drag: fine  |  Double-click: reset  |  Right-click: type a value");
    slider.onPopup = [this] { showParameterMenu (slider, param); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (apvts, id, slider);
    slider.setDoubleClickReturnValue (true, (double) param.convertFrom0to1 (param.getDefaultValue()));
    startTimerHz (15);
}

juce::String Knob::valueText() const
{
    const float v = param.convertFrom0to1 (param.getValue());
    if (formatValue) return formatValue (v);
    return param.getCurrentValueAsText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom ((int) (captionSize + 6));
    if (valueStyle == 1) r.removeFromBottom (14);
    if (valueStyle == 2) r.removeFromBottom (24);
    const int size = juce::jmin (r.getWidth(), r.getHeight(), maxKnob);
    slider.setBounds (r.withSizeKeepingCentre (size, size).withY (r.getBottom() - size));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    juce::Rectangle<int> pill;
    if (valueStyle == 2) pill = r.removeFromBottom (24);
    auto cap = r.removeFromBottom ((int) (captionSize + 6));
    juce::Rectangle<int> val;
    if (valueStyle == 1) val = r.removeFromBottom (14);

    g.setFont (font (captionSize, Weight::Medium, 0.04f));
    const bool hoverValue = showingValue && valueStyle == 0;
    g.setColour (hoverValue ? accent : (isEnabled() ? text : textFaint));
    g.drawText (hoverValue ? valueText() : caption, cap.expanded (8, 0), juce::Justification::centred, false);

    if (valueStyle == 1)
    {
        g.setFont (font (10.5f, Weight::Medium));
        g.setColour (showingValue ? accent : text.withAlpha (0.9f));
        g.drawText (valueText(), val.expanded (8, 0), juce::Justification::centredBottom, false);
    }
    if (valueStyle == 2)
    {
        auto pr = pill.toFloat().withSizeKeepingCentre (juce::jmin ((float) pill.getWidth() - 6.0f, 56.0f), 20.0f).translated (0.0f, 1.0f);
        g.setColour (juce::Colour (0xff050c11)); g.fillRoundedRectangle (pr, 5.0f);
        g.setColour (showingValue ? borderBright : juce::Colour (0xff22394a)); g.drawRoundedRectangle (pr.reduced (0.5f), 5.0f, 1.0f);
        g.setFont (font (12.0f, Weight::Medium));
        g.setColour (text);
        g.drawText (valueText(), pr.toNearestInt(), juce::Justification::centred, false);
    }
}

void Knob::updateModulation()
{
    if (dest == ModDest::None) return;
    const float amt = modAmountFor (apvts, dest) * modScale;
    const double old = slider.getProperties().getWithDefault ("modAmount", 0.0);
    if (std::abs (old - amt) > 1.0e-4)
    {
        slider.getProperties().set ("modAmount", amt);
        slider.repaint();
    }
}

void Knob::timerCallback()
{
    const bool show = slider.isMouseOverOrDragging();
    if (show != showingValue || show) { showingValue = show; repaint(); }
    if (valueStyle != 0)
    {
        auto t = valueText();
        if (t != lastValueText) { lastValueText = t; repaint(); }
    }
    updateModulation();
}

//==============================================================================
PowerButton::PowerButton (APVTS& s, const juce::String& id) : attachment (s, id, *this)
{
    setTooltip ("Turns this module on or off.");
}

void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();
    juce::Path p;
    if (glyph == 1)
    {
        const auto c = r.getCentre(); const float d = juce::jmin (r.getWidth(), r.getHeight()) * 0.36f;
        p.startNewSubPath (c.x, c.y - d); p.lineTo (c.x + d, c.y); p.lineTo (c.x, c.y + d); p.lineTo (c.x - d, c.y); p.closeSubPath();
    }
    else p = icons::power (r);
    if (on)
    {
        if (glyph == 1) { g.setColour (accent.withAlpha (0.35f)); g.fillPath (p); }
        strokeGlow (g, p, 1.7f);
    }
    else
    {
        g.setColour (over ? colours::text : colours::textFaint);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
Selector::Selector (APVTS& s, const juce::String& id, bool box)
    : param (*dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))),
      attachment (param, [this] (float v) { index = juce::roundToInt (v); repaint(); if (onChange) onChange(); }, s.undoManager),
      drawBox (box)
{
    setTooltip (helpTextFor (id));
    attachment.sendInitialUpdate();
}

void Selector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (drawBox) drawGlassControl (g, r.reduced (0.5f), false, isMouseOver(), 6.0f);
    auto chevron = [&g] (float x, float y, bool left)
    {
        juce::Path p;
        const float d = left ? 1.0f : -1.0f;
        p.startNewSubPath (x + 3.0f * d, y - 6.0f); p.lineTo (x - 3.0f * d, y); p.lineTo (x + 3.0f * d, y + 6.0f);
        g.setColour (text);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    };
    chevron (r.getX() + 16.0f, r.getCentreY(), true);
    chevron (r.getRight() - 16.0f, r.getCentreY(), false);
    g.setColour (text);
    g.setFont (font (juce::jmin (14.0f, r.getHeight() * 0.56f)));
    if (centreText) g.drawText (param.choices[index], r.reduced (30.0f, 0.0f), juce::Justification::centred, true);
    else            g.drawText (param.choices[index], r.withTrimmedLeft (40.0f).withTrimmedRight (30.0f), juce::Justification::centredLeft, true);
}

void Selector::step (int delta)
{
    const int n = param.choices.size();
    attachment.setValueAsCompleteGesture ((float) ((index + delta + n) % n));
}

void Selector::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showParameterMenu (*this, param); return; }
    if (e.x < 28) { step (-1); return; }
    if (e.x > getWidth() - 28) { step (1); return; }
    juce::PopupMenu m;
    for (int i = 0; i < param.choices.size(); ++i)
        m.addItem (i + 1, param.choices[i], true, i == index);
    juce::Component::SafePointer<Selector> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->attachment.setValueAsCompleteGesture ((float) (r - 1));
    });
}

//==============================================================================
ChoiceBox::ChoiceBox (APVTS& s, const juce::String& id)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        addItemList (c->choices, 1);
    setTooltip (helpTextFor (id));
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, id, *this);
}

//==============================================================================
SegmentedChoice::SegmentedChoice (APVTS& s, const juce::String& id, juce::StringArray labels, std::vector<int> valueForButton)
    : map (std::move (valueForButton)), param (*dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))),
      attachment (param, [this] (float v)
      {
          const int idx = juce::roundToInt (v);
          for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState ((i < (int) map.size() ? map[(size_t) i] : i) == idx, juce::dontSendNotification);
      }, s.undoManager)
{
    if (map.size() != (size_t) labels.size()) { map.clear(); for (int i = 0; i < labels.size(); ++i) map.push_back (i); }
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->setTooltip (helpTextFor (id));
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) map[(size_t) i]); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void SegmentedChoice::resized()
{
    auto r = getLocalBounds();
    const int gap = (int) getProperties().getWithDefault ("gap", 0);
    const int n = juce::jmax (1, buttons.size());
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (auto* b : buttons) { b->setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); }
}

//==============================================================================
NumberBox::NumberBox (APVTS& s, const juce::String& id) : attachment (s, id, slider)
{
    slider.setSliderStyle (juce::Slider::LinearBarVertical);
    slider.setTextBoxIsEditable (false);
    slider.setMouseDragSensitivity (300);
    slider.setVelocityModeParameters (0.1, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.setTooltip (helpTextFor (id) + "\n\nDrag up/down to change. Right-click to type a value.");
    auto* p = s.getParameter (id);
    slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    slider.onPopup = [this, p] { showParameterMenu (slider, *p); };
    addAndMakeVisible (slider);
}

//==============================================================================
TabRow::TabRow (juce::StringArray names)
{
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->getProperties().set ("noOutline", true);
        b->getProperties().set ("fontSize", 13.5);
        b->getProperties().set ("corner", 6.0);
        b->onClick = [this, i] { setSelected (i); };
        addAndMakeVisible (b);
    }
    setSelected (0, false);
}

void TabRow::setSelected (int i, bool notify)
{
    selected = i;
    for (int k = 0; k < tabs.size(); ++k) tabs[k]->setToggleState (k == i, juce::dontSendNotification);
    repaint();
    if (notify && onSelect && i >= 0) onSelect (i);
}

void TabRow::resized()
{
    auto r = getLocalBounds().reduced (3);
    const int w = r.getWidth() / juce::jmax (1, tabs.size());
    for (auto* b : tabs) b->setBounds (r.removeFromLeft (w));
}

void TabRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient grad (juce::Colour (0xff0e1d26), r.getX(), r.getY(), juce::Colour (0xff060d12), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (juce::Colour (0xff22394a));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    g.setColour (juce::Colour (0xff1b2e3a));
    for (int i = 1; i < tabs.size(); ++i)
        if (i != selected && i - 1 != selected)
            g.drawVerticalLine (tabs[i]->getX(), r.getY() + 7.0f, r.getBottom() - 7.0f);
}

//==============================================================================
IconButton::IconButton (const juce::String& name, juce::Path p, bool f) : juce::Button (name), path (std::move (p)), fill (f)
{
    setTooltip (name);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (juce::jmin (getWidth(), getHeight()) * (boxed ? 0.27f : 0.2f));
    auto p = path;
    p.applyTransform (p.getTransformToScaleToFit (r, true));
    const bool on = getToggleState();
    if (boxed)
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        drawGlassControl (g, b, false, over, 5.0f);
    }
    auto c = on ? colours::accent : (over ? colours::glow : colours::text);
    if (down) c = c.brighter (0.2f);
    g.setColour (c);
    if (fill || (filledWhenOn && on)) g.fillPath (p);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// OscDisplay
//==============================================================================
OscDisplay::OscDisplay (APVTS& s, const WavetableBank& b, int o) : apvts (s), bank (b), osc (o)
{
    setTooltip ("Click to choose the wavetable / waveform.");
    startTimerHz (30);
}

void OscDisplay::timerCallback()
{
    float h = 0.0f;
    for (auto* n : { "on", "engine", "table", "pos", "wave", "pw", "fold" })
        h = h * 1.37f + apvts.getRawParameterValue (ids::osc (osc, n))->load();
    if (h != lastHash) { lastHash = h; repaint(); }
}

void OscDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);

    auto get = [this] (const char* n) { return apvts.getRawParameterValue (ids::osc (osc, n))->load(); };
    const bool on = get ("on") > 0.5f;
    const bool wtMode = std::lround (get ("engine")) == 0;
    const int table = (int) std::lround (get ("table"));
    const float pos = get ("pos"), fold = get ("fold"), pw = get ("pw");
    const int wave = (int) std::lround (get ("wave"));
    const float alpha = on ? 1.0f : 0.35f;

    auto folded = [fold] (float x)
    {
        if (fold < 1.0e-4f) return x;
        const float gg = 1.0f + fold * 5.0f;
        return (1.0f - fold) * x + fold * std::sin (x * gg * juce::MathConstants<float>::halfPi);
    };

    auto area = r.reduced (10.0f, 12.0f);
    if (wtMode)
    {
        const int frames = 14, points = 90;
        const float dx = area.getWidth() * 0.018f, dy = area.getHeight() * 0.045f;
        const float w = area.getWidth() - dx * (frames - 1);
        const float amp = area.getHeight() * 0.28f;
        const int current = juce::roundToInt (pos * (frames - 1));
        for (int k = frames - 1; k >= 0; --k)
        {
            const int fi = juce::roundToInt ((float) k / (frames - 1) * (WavetableBank::kFrames - 1));
            const auto frameView = bank.getFrame (table, fi, 6);   // 128 harmonics
            const float ox = area.getX() + dx * (float) k;
            const float oy = area.getBottom() - amp - 4.0f - dy * (float) k;
            juce::Path p;
            for (int i = 0; i <= points; ++i)
            {
                const float x = ox + w * (float) i / points;
                const float y = oy - folded (frameView.at ((double) (i % points) / points)) * amp;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            const bool cur = (k == current);
            const float depth = 1.0f - (float) k / frames;
            if (cur) strokeGlow (g, p, 1.8f, accent.withMultipliedAlpha (alpha));
            else
            {
                g.setColour (accentStrong.withAlpha ((0.14f + 0.22f * depth) * alpha));
                g.strokePath (p, juce::PathStrokeType (0.8f));
            }
        }
    }
    else
    {
        juce::Path p;
        const int points = 200;
        const float mid = area.getCentreY(), amp = area.getHeight() * 0.42f;
        for (int i = 0; i <= points; ++i)
        {
            const double ph = std::fmod (2.0 * i / points, 1.0);
            float v = dsp::analogSample (wave, ph, 1.0e-6, wave == 4 ? pw : 0.5);
            v = folded (v * 0.9f) / 0.9f;
            const float x = area.getX() + area.getWidth() * (float) i / points;
            if (i == 0) p.startNewSubPath (x, mid - v * amp); else p.lineTo (x, mid - v * amp);
        }
        juce::Path fill (p);
        fill.lineTo (area.getRight(), r.getBottom()); fill.lineTo (area.getX(), r.getBottom()); fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.16f * alpha), 0, area.getY(), accent.withAlpha (0.0f), 0, r.getBottom(), false));
        g.fillPath (fill);
        strokeGlow (g, p, 1.9f, accent.withMultipliedAlpha (alpha));
    }
}

void OscDisplay::mouseUp (const juce::MouseEvent&)
{
    const bool wtMode = std::lround (apvts.getRawParameterValue (ids::osc (osc, "engine"))->load()) == 0;
    const auto id = ids::osc (osc, wtMode ? "table" : "wave");
    auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
    if (p == nullptr) return;
    juce::PopupMenu m;
    m.addSectionHeader (wtMode ? "Wavetable" : "Analog wave");
    for (int i = 0; i < p->choices.size(); ++i) m.addItem (i + 1, p->choices[i], true, i == p->getIndex());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [p] (int r)
    {
        if (r > 0) { p->beginChangeGesture(); *p = r - 1; p->endChangeGesture(); }
    });
}

//==============================================================================
// FilterResponse
//==============================================================================
FilterResponse::FilterResponse (APVTS& s, int f) : apvts (s), filter (f) { startTimerHz (30); }

void FilterResponse::timerCallback()
{
    float h = (float) filter;
    for (auto* n : { "type", "cutoff", "res", "drive", "mix" })
        h = h * 1.37f + apvts.getRawParameterValue (ids::filt (filter, n))->load();
    if (h != lastHash) { lastHash = h; repaint(); }
}

void FilterResponse::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);

    // grid
    g.setColour (accent.withAlpha (0.05f));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8.0f), r.getY(), r.getBottom());
    for (int i = 1; i < 6; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 6.0f), r.getX(), r.getRight());

    auto get = [this] (const char* n) { return apvts.getRawParameterValue (ids::filt (filter, n))->load(); };
    const int type = (int) std::lround (get ("type"));
    const double fc = get ("cutoff"), res = get ("res"), mix = get ("mix");
    using C = std::complex<double>;

    auto response = [&] (double f) -> double
    {
        if (type == 0) return 1.0;
        const double x = f / fc;
        const C s (0.0, x);
        const double k = 2.0 - 1.96 * res;
        const C den = s * s + k * s + 1.0;
        C h (1.0);
        switch (type)
        {
            case 1: { const double K = 3.95 * res; h = (1.0 + 0.3 * K) / (std::pow (1.0 + s, 4) + K); break; }
            case 2: h = 1.0 / den; break;
            case 3: h = s * s / den; break;
            case 4: h = k * s / den; break;
            case 5: h = (s * s + 1.0) / den; break;
            case 6: h = (1.0 - s * s) / den; break;
            case 7: { const double fb = 0.97 * res; h = 0.6 / (1.0 - fb * std::exp (C (0.0, -2.0 * juce::MathConstants<double>::pi * f / fc))); break; }
            case 8: { const C d2 = s * s + 1.414 * s + 1.0; h = 1.0 / (den * d2); break; }               // LP 24
            case 9: { const C d2 = s * s + 1.414 * s + 1.0; h = (s * s) * (s * s) / (den * d2); break; } // HP 24
            case 10: { const double fb = -0.97 * res; h = 0.6 / (1.0 - fb * std::exp (C (0.0, -2.0 * juce::MathConstants<double>::pi * f / fc))); break; }
            case 11:
            {
                // formant: three vowel peaks chosen by the cutoff (same mapping as the engine)
                static const double vowels[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 300, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
                static const double gains[3] = { 1.0, 0.6, 0.35 };
                const double x = juce::jlimit (0.0, 1.0, std::log2 (fc / 200.0) / std::log2 (25.0)) * 4.0;
                const int a = juce::jmin (3, (int) x); const double fr = x - a;
                const double kk = 2.0 - 1.96 * (0.55 + 0.42 * res);
                C sum (0.0);
                for (int q = 0; q < 3; ++q)
                {
                    const double hz = vowels[a][q] + (vowels[a + 1][q] - vowels[a][q]) * fr;
                    const C sq (0.0, f / hz);
                    sum += gains[q] * kk * 1.6 * kk * sq / (sq * sq + kk * sq + 1.0);
                }
                h = sum; break;
            }
            default: break;
        }
        h = 1.0 + mix * (h - 1.0);
        return std::abs (h);
    };

    const float top = r.getY() + 6.0f, bottom = r.getBottom() - 4.0f;
    auto dbToY = [&] (double db) { return juce::jmap ((float) juce::jlimit (-36.0, 18.0, db), -36.0f, 18.0f, bottom, top); };

    juce::Path curve;
    const int points = juce::jmax (64, (int) r.getWidth());
    for (int i = 0; i <= points; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / points);
        const float x = r.getX() + r.getWidth() * (float) i / points;
        const float y = dbToY (juce::Decibels::gainToDecibels (response (f), -60.0));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.3f), 0, top, accent.withAlpha (0.0f), 0, r.getBottom(), false));
    g.fillPath (fill);
    strokeGlow (g, curve, 1.9f);
}

//==============================================================================
// EnvelopeDisplay
//==============================================================================
namespace
{
    constexpr float kSegFrac = 0.27f, kHoldFrac = 0.16f;
    const float kTimeScale = 30.0f;
    float timeToFrac (float t, float maxT) { return std::log1p (t * kTimeScale) / std::log1p (maxT * kTimeScale); }
    float fracToTime (float f, float maxT) { return std::expm1 (f * std::log1p (maxT * kTimeScale)) / kTimeScale; }
}

EnvelopeDisplay::EnvelopeDisplay (APVTS& s, int e) : apvts (s), env (e)
{
    setTooltip ("Drag the points to shape the envelope. The knobs below show exact values.");
    startTimerHz (30);
}

void EnvelopeDisplay::timerCallback()
{
    float h = (float) env;
    for (auto* n : { "attack", "decay", "sustain", "release", "curve" })
        h = h * 1.37f + apvts.getRawParameterValue (ids::env (env, n))->load();
    if (h != lastHash) { lastHash = h; repaint(); }
}

EnvelopeDisplay::Geometry EnvelopeDisplay::geometry() const
{
    auto get = [this] (const char* n) { return apvts.getRawParameterValue (ids::env (env, n))->load(); };
    Geometry geo;
    geo.area = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    geo.unit = geo.area.getWidth();
    const float W = geo.unit * kSegFrac;
    const float a = std::max (3.0f, W * timeToFrac (get ("attack"), 20.0f));
    const float d = std::max (3.0f, W * timeToFrac (get ("decay"), 20.0f));
    const float rr = std::max (3.0f, W * timeToFrac (get ("release"), 30.0f));
    const float sY = geo.area.getBottom() - get ("sustain") * geo.area.getHeight();
    geo.start = { geo.area.getX(), geo.area.getBottom() };
    geo.peak = { geo.start.x + a, geo.area.getY() };
    geo.decayEnd = { geo.peak.x + d, sY };
    geo.releaseStart = { geo.decayEnd.x + geo.unit * kHoldFrac, sY };
    geo.end = { geo.releaseStart.x + rr, geo.area.getBottom() };
    return geo;
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const auto geo = geometry();
    const float curve = apvts.getRawParameterValue (ids::env (env, "curve"))->load();
    const float aExp = std::pow (2.0f, curve * 2.0f), dExp = 3.0f * std::pow (2.0f, curve * 1.5f);

    juce::Path p;
    p.startNewSubPath (geo.start);
    const int N = 24;
    for (int i = 1; i <= N; ++i)
    {
        const float t = (float) i / N;
        p.lineTo (geo.start.x + (geo.peak.x - geo.start.x) * t, geo.start.y + (geo.peak.y - geo.start.y) * std::pow (t, aExp));
    }
    for (int i = 1; i <= N; ++i)
    {
        const float t = (float) i / N;
        const float lvl = std::pow (1.0f - t, dExp);
        p.lineTo (geo.peak.x + (geo.decayEnd.x - geo.peak.x) * t, geo.decayEnd.y + (geo.peak.y - geo.decayEnd.y) * lvl);
    }
    p.lineTo (geo.releaseStart);
    for (int i = 1; i <= N; ++i)
    {
        const float t = (float) i / N;
        const float lvl = std::pow (1.0f - t, dExp);
        p.lineTo (geo.releaseStart.x + (geo.end.x - geo.releaseStart.x) * t, geo.end.y + (geo.releaseStart.y - geo.end.y) * lvl);
    }

    juce::Path fill (p);
    fill.lineTo (geo.end); fill.lineTo (geo.start); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.32f), 0, geo.area.getY(), accent.withAlpha (0.02f), 0, geo.area.getBottom(), false));
    g.fillPath (fill);
    strokeGlow (g, p, 1.9f);

    for (auto pt : { geo.start, geo.peak, geo.decayEnd, geo.releaseStart, geo.end })
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (pt));
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt));
        g.setColour (glow);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (pt));
    }
}

void EnvelopeDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto geo = geometry();
    const juce::Point<float> pts[] = { geo.peak, geo.decayEnd, geo.releaseStart, geo.end };
    dragNode = -1;
    float best = 14.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float dist = pts[i].getDistanceFrom (e.position);
        if (dist < best) { best = dist; dragNode = i; }
    }
    for (auto* n : { "attack", "decay", "sustain", "release" })
        if (dragNode >= 0) apvts.getParameter (ids::env (env, n))->beginChangeGesture();
}

void EnvelopeDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    const auto geo = geometry();
    const float W = geo.unit * kSegFrac;
    auto set = [this] (const char* n, float v)
    {
        auto* p = apvts.getParameter (ids::env (env, n));
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    const float sustain = juce::jlimit (0.0f, 1.0f, (geo.area.getBottom() - e.position.y) / geo.area.getHeight());
    switch (dragNode)
    {
        case 0: set ("attack", fracToTime (juce::jlimit (0.0f, 1.0f, (e.position.x - geo.start.x) / W), 20.0f)); break;
        case 1: set ("decay", fracToTime (juce::jlimit (0.0f, 1.0f, (e.position.x - geo.peak.x) / W), 20.0f)); set ("sustain", sustain); break;
        case 2: set ("sustain", sustain); break;
        case 3: set ("release", fracToTime (juce::jlimit (0.0f, 1.0f, (e.position.x - geo.releaseStart.x) / W), 30.0f)); break;
        default: break;
    }
}

void EnvelopeDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0)
        for (auto* n : { "attack", "decay", "sustain", "release" })
            apvts.getParameter (ids::env (env, n))->endChangeGesture();
    dragNode = -1;
}

//==============================================================================
// LfoDisplay
//==============================================================================
LfoDisplay::LfoDisplay (DivinityProcessor& p, int l) : proc (p), lfo (l) { startTimerHz (30); }

void LfoDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    auto& s = proc.apvts;
    const int shape = (int) std::lround (s.getRawParameterValue (ids::lfo (lfo, "shape"))->load());
    const float depth = s.getRawParameterValue (ids::lfo (lfo, "depth"))->load();
    const float phase = s.getRawParameterValue (ids::lfo (lfo, "phase"))->load();

    auto area = r.reduced (10.0f, 10.0f);
    const float mid = area.getCentreY(), amp = area.getHeight() * 0.45f * juce::jmax (0.08f, depth);
    juce::Path p;
    const int points = 240;
    dsp::Rng rng; rng.seed (77u + (uint32_t) lfo);
    float held = rng.bipolar();
    int lastStep = -1;
    for (int i = 0; i <= points; ++i)
    {
        double ph = 2.0 * i / points + phase;
        const int step = (int) std::floor (ph);
        ph -= std::floor (ph);
        float v = 0.0f;
        switch (shape)
        {
            case 0: v = (float) std::sin (ph * juce::MathConstants<double>::twoPi); break;
            case 1: v = ph < 0.5 ? 1.0f : -1.0f; break;
            case 2: v = (float) (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph); break;
            default: if (step != lastStep) { held = rng.bipolar(); lastStep = step; } v = held; break;
        }
        const float x = area.getX() + area.getWidth() * (float) i / points;
        if (i == 0) p.startNewSubPath (x, mid - v * amp); else p.lineTo (x, mid - v * amp);
    }
    strokeGlow (g, p, 1.9f);

    // live output marker
    const float live = juce::jlimit (-1.0f, 1.0f, proc.getLfoValue (lfo));
    const float y = mid - live * area.getHeight() * 0.45f;
    g.setColour (accent.withAlpha (0.3f));
    g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ area.getRight() - 3.0f, y }));
    g.setColour (glow);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ area.getRight() - 3.0f, y }));
}

//==============================================================================
// ScopeDisplay
//==============================================================================
ScopeDisplay::ScopeDisplay (DivinityProcessor& p) : proc (p)
{
    history.assign (4096, 0.0f);
    pullBuf.assign (ScopeFifo::kSize, 0.0f);
    startTimerHz (30);
}

void ScopeDisplay::timerCallback()
{
    if (! visibleInTree (*this)) return;   // the scope feed has one reader at a time
    const int n = proc.scope.pull (pullBuf.data(), (int) pullBuf.size());
    for (int i = 0; i < n; ++i)
    {
        history[(size_t) writePos] = pullBuf[(size_t) i];
        writePos = (writePos + 1) % (int) history.size();
    }
    repaint();
}

void ScopeDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int size = (int) history.size();
    const int window = 1024;
    // find a rising zero crossing in the older half so the trace is stable
    int start = (writePos - window - 1 + size) % size;
    for (int k = 0; k < 1500; ++k)
    {
        const int i0 = (writePos - window - 2 - k + 2 * size) % size;
        const int i1 = (i0 + 1) % size;
        if (history[(size_t) i0] <= 0.0f && history[(size_t) i1] > 0.0f) { start = i1; break; }
    }
    float peak = 0.0f;
    for (int i = 0; i < window; ++i) peak = juce::jmax (peak, std::abs (history[(size_t) ((start + i) % size)]));
    if (peak < 1.0e-4f)
    {
        // idle: a single glowing pulse on the centre line
        juce::Path beat;
        const float mid = r.getCentreY(), cx = r.getCentreX(), u = r.getWidth() / 24.0f, hh = r.getHeight() * 0.36f;
        const float xs[] = { -12, -3, -2, -1.2f, -0.5f, 0.2f, 0.9f, 1.6f, 2.4f, 3.2f, 12 };
        const float ys[] = { 0, 0, -0.15f, 0.25f, -1.0f, 0.85f, -0.45f, 0.3f, -0.1f, 0, 0 };
        for (int i = 0; i < 11; ++i)
        {
            const juce::Point<float> pt (cx + xs[i] * u, mid + ys[i] * hh);
            if (i == 0) beat.startNewSubPath (pt); else beat.lineTo (pt);
        }
        strokeGlow (g, beat, 1.6f);
        return;
    }
    const float gain = 0.9f / juce::jmax (0.05f, peak);

    juce::Path p;
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.42f;
    for (int i = 0; i < window; i += 2)
    {
        const float v = history[(size_t) ((start + i) % size)] * gain;
        const float x = r.getX() + r.getWidth() * (float) i / window;
        if (i == 0) p.startNewSubPath (x, mid - v * amp); else p.lineTo (x, mid - v * amp);
    }
    strokeGlow (g, p, 1.6f);
}

//==============================================================================
LevelMeter::LevelMeter (DivinityProcessor& p, bool v) : proc (p), vertical (v)
{
    setTooltip ("Output level");
    startTimerHz (30);
}

void LevelMeter::timerCallback()
{
    l = proc.meterL.load(); r = proc.meterR.load();
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto norm = [] (float x) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (x, -60.0f) + 48.0f) / 48.0f); };
    auto bounds = getLocalBounds().toFloat();
    const float vals[2] = { norm (l), norm (r) };
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Rectangle<float> bar = vertical
            ? juce::Rectangle<float> (bounds.getX() + ch * bounds.getWidth() * 0.55f, bounds.getY(), bounds.getWidth() * 0.4f, bounds.getHeight())
            : juce::Rectangle<float> (bounds.getX(), bounds.getY() + ch * bounds.getHeight() * 0.55f, bounds.getWidth(), bounds.getHeight() * 0.4f);
        g.setColour (juce::Colour (0xff0b1a22));
        g.fillRoundedRectangle (bar, 2.0f);
        auto lit = vertical ? bar.withTrimmedTop (bar.getHeight() * (1.0f - vals[ch])) : bar.withWidth (bar.getWidth() * vals[ch]);
        const bool hot = (ch == 0 ? l : r) > 0.97f;
        if (hot) g.setColour (juce::Colour (0xffff6b7a));
        else g.setGradientFill (juce::ColourGradient (glow, lit.getX(), lit.getY(), accentStrong.withAlpha (0.6f), lit.getX(), bar.getBottom(), false));
        g.fillRoundedRectangle (lit, 2.0f);
        g.setColour (accent.withAlpha (0.18f));
        g.drawRoundedRectangle (lit.expanded (1.0f), 2.5f, 1.5f);
    }
}

} // namespace divinity::ui
