#include "GuardianPanel.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

//==============================================================================
// AngelArt
//==============================================================================
AngelArt::AngelArt()
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void AngelArt::timerCallback()
{
    phase += 1.0f / 30.0f;
    if (phase > 1000.0f) phase = 0.0f;
    talk = juce::jmax (0.0f, talk - 0.025f);
    repaint();
}

namespace
{
    juce::Path wing (bool left)
    {
        // designed for the left side, mirrored for the right
        juce::Path p;
        p.startNewSubPath (88, 206);
        p.cubicTo (60, 168, 26, 160, 8, 176);
        p.cubicTo (18, 182, 22, 186, 20, 192);
        p.cubicTo (6, 198, 4, 210, 8, 218);
        p.cubicTo (20, 214, 24, 220, 22, 226);
        p.cubicTo (12, 236, 16, 248, 24, 254);
        p.cubicTo (34, 246, 40, 250, 40, 258);
        p.cubicTo (40, 270, 52, 274, 62, 268);
        p.cubicTo (72, 262, 84, 254, 92, 242);
        p.closeSubPath();
        if (! left) p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (250.0f, 0.0f));
        return p;
    }

    juce::Path wingFeathers (bool left)
    {
        juce::Path p;
        p.startNewSubPath (84, 214); p.quadraticTo (52, 200, 22, 192);
        p.startNewSubPath (86, 226); p.quadraticTo (52, 222, 22, 226);
        p.startNewSubPath (88, 236); p.quadraticTo (60, 246, 40, 258);
        if (! left) p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (250.0f, 0.0f));
        return p;
    }

    void plushFill (juce::Graphics& g, const juce::Path& p, juce::Point<float> light, float radius)
    {
        juce::ColourGradient grad (juce::Colour (0xfff6f8fc), light.x, light.y, juce::Colour (0xff7d8aa1), light.x + radius, light.y + radius, true);
        grad.addColour (0.55, juce::Colour (0xffc7d0de));
        g.setGradientFill (grad);
        g.fillPath (p);
    }
}

void AngelArt::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float scale = juce::jmin (bounds.getWidth() / 250.0f, bounds.getHeight() / 300.0f);
    const float bob = std::sin (phase * 1.6f) * 4.0f;
    g.addTransform (juce::AffineTransform::translation ((bounds.getWidth() - 250.0f * scale) * 0.5f,
                                                        (bounds.getHeight() - 300.0f * scale) * 0.5f).scaled (scale));
    const float alpha = awake ? 1.0f : 0.45f;
    const float glow = 0.35f + 0.25f * talk + 0.08f * std::sin (phase * 2.2f);

    // aura
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.20f * glow * alpha * 2.0f), 125, 150 + bob,
                                             juce::Colours::transparentBlack, 125, 20, true));
    g.fillEllipse (5, 30 + bob, 240, 260);

    g.saveState();
    g.addTransform (juce::AffineTransform::translation (0, bob));

    // wings
    for (bool left : { true, false })
    {
        auto w = wing (left);
        g.setColour (accent.withAlpha (0.25f * alpha));
        g.strokePath (w, juce::PathStrokeType (5.0f));
        juce::ColourGradient wg (juce::Colour (0xfff2f5fb).withMultipliedAlpha (alpha), left ? 80.0f : 170.0f, 210,
                                 juce::Colour (0xff8b9ab3).withMultipliedAlpha (alpha), left ? 12.0f : 238.0f, 250, false);
        g.setGradientFill (wg);
        g.fillPath (w);
        g.setColour (juce::Colour (0xff6f7f99).withAlpha (0.6f * alpha));
        g.strokePath (wingFeathers (left), juce::PathStrokeType (1.2f));
    }

    // body, arms, feet
    juce::Path body; body.addEllipse (73, 192, 104, 92);
    plushFill (g, body, { 105, 205 }, 90);
    juce::Path arms; arms.addEllipse (66, 216, 28, 40); arms.addEllipse (156, 216, 28, 40);
    plushFill (g, arms, { 80, 220 }, 40);
    juce::Path feet; feet.addEllipse (84, 266, 38, 26); feet.addEllipse (128, 266, 38, 26);
    plushFill (g, feet, { 96, 270 }, 40);
    g.setColour (juce::Colour (0xff5b6880).withAlpha (0.5f * alpha));
    g.strokePath (body, juce::PathStrokeType (1.0f));

    // head
    juce::Path head; head.addEllipse (59, 62, 132, 128);
    plushFill (g, head, { 100, 92 }, 130);
    g.setColour (juce::Colour (0xff5b6880).withAlpha (0.5f * alpha));
    g.strokePath (head, juce::PathStrokeType (1.0f));

    // barbed-wire wreath: two twisted strands + barbs
    const juce::Point<float> hc (125, 126);
    for (int strand = 0; strand < 2; ++strand)
    {
        juce::Path wire;
        for (int i = 0; i <= 240; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 240.0f;
            const float r = 72.0f + (strand == 0 ? 2.6f : -2.6f) * std::sin (a * 14.0f);
            const juce::Point<float> pt (hc.x + r * std::cos (a), hc.y + r * 0.98f * std::sin (a));
            if (i == 0) wire.startNewSubPath (pt); else wire.lineTo (pt);
        }
        g.setColour (juce::Colour (0xff1b2436).withAlpha (alpha));
        g.strokePath (wire, juce::PathStrokeType (3.6f));
        g.setColour (juce::Colour (0xffe7eefb).withAlpha (alpha));
        g.strokePath (wire, juce::PathStrokeType (1.8f));
    }
    for (int i = 0; i < 18; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 18.0f + 0.12f;
        const juce::Point<float> c (hc.x + 72.0f * std::cos (a), hc.y + 70.5f * std::sin (a));
        juce::Path barb;
        const float d = 6.0f;
        barb.startNewSubPath (c.x - d * std::cos (a + 0.8f), c.y - d * std::sin (a + 0.8f));
        barb.lineTo (c.x + d * std::cos (a + 0.8f), c.y + d * std::sin (a + 0.8f));
        barb.startNewSubPath (c.x - d * std::cos (a - 0.8f), c.y - d * std::sin (a - 0.8f));
        barb.lineTo (c.x + d * std::cos (a - 0.8f), c.y + d * std::sin (a - 0.8f));
        g.setColour (juce::Colour (0xff1b2436).withAlpha (alpha));
        g.strokePath (barb, juce::PathStrokeType (3.0f));
        g.setColour (juce::Colour (0xffe7eefb).withAlpha (alpha));
        g.strokePath (barb, juce::PathStrokeType (1.4f));
    }

    // face: X eyes + sad mouth (mouth opens a little while speaking)
    juce::Path face;
    for (float ex : { 101.0f, 149.0f })
    {
        face.startNewSubPath (ex - 8, 110); face.lineTo (ex + 8, 126);
        face.startNewSubPath (ex + 8, 110); face.lineTo (ex - 8, 126);
    }
    const float open = 6.0f * talk * (0.5f + 0.5f * std::sin (phase * 22.0f));
    face.startNewSubPath (100, 166);
    face.quadraticTo (125, 140 - open, 150, 166);
    g.setColour (juce::Colour (0xff141b29).withAlpha (alpha));
    g.strokePath (face, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();

    // halo (floats on its own rhythm)
    const float hb = std::sin (phase * 1.6f - 0.6f) * 5.0f;
    juce::Path halo; halo.addEllipse (64, 18 + hb, 122, 26);
    g.setColour (accent.withAlpha (0.12f * alpha));
    g.strokePath (halo, juce::PathStrokeType (14.0f));
    g.setColour (accent.withAlpha ((0.35f + 0.3f * glow) * alpha));
    g.strokePath (halo, juce::PathStrokeType (6.0f));
    g.setColour (juce::Colour (0xfff2fbff).withAlpha (alpha));
    g.strokePath (halo, juce::PathStrokeType (2.4f));

    // twinkle
    const float tw = std::pow (juce::jmax (0.0f, std::sin (phase * 0.9f)), 24.0f);
    if (tw > 0.01f)
    {
        juce::Path star; star.addStar ({ 196, 26 + hb }, 4, 1.5f, 9.0f * tw);
        g.setColour (juce::Colours::white.withAlpha (tw * alpha));
        g.fillPath (star);
    }
}

//==============================================================================
// Action buttons with icons
//==============================================================================
class GuardianPanel::ActionButton : public juce::Button
{
public:
    ActionButton (const juce::String& name, int iconKind, bool toggle = false) : juce::Button (name), icon (iconKind)
    {
        setClickingTogglesState (toggle);
    }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        g.setColour (on ? juce::Colour (0xff0c2c55) : panel2.withAlpha (over ? 0.95f : 0.7f));
        g.fillRoundedRectangle (r, 6.0f);
        if (on) { g.setColour (accent.withAlpha (0.2f)); g.drawRoundedRectangle (r.expanded (1.0f), 7.0f, 3.0f); }
        g.setColour (on ? accent : (over ? borderBright : border));
        g.drawRoundedRectangle (r, 6.0f, on ? 1.4f : 1.0f);

        auto ic = juce::Rectangle<float> (16.0f, r.getCentreY() - 10.0f, 20.0f, 20.0f);
        g.setColour (on || down ? colours::accent : colours::text);
        juce::Path p;
        switch (icon)
        {
            case 0: { // toggle dot
                g.setColour (on ? accent : textFaint);
                g.fillEllipse (ic.reduced (3.0f));
                if (on) { g.setColour (accent.withAlpha (0.3f)); g.fillEllipse (ic.expanded (2.0f)); g.setColour (juce::Colours::white); g.fillEllipse (ic.reduced (7.0f)); }
                break;
            }
            case 1: p.addStar (ic.getCentre().translated (-2, 2), 4, 2.0f, 8.0f); p.addStar (ic.getCentre().translated (6, -6), 4, 1.0f, 3.5f); break;
            case 2: p.addEllipse (ic.getX() + 4, ic.getY(), 12, 12); p.startNewSubPath (ic.getX() + 7, ic.getY() + 15); p.lineTo (ic.getX() + 13, ic.getY() + 15);
                    p.startNewSubPath (ic.getX() + 8, ic.getY() + 18); p.lineTo (ic.getX() + 12, ic.getY() + 18); break;
            case 3: p.startNewSubPath (ic.getCentreX(), ic.getY() + 4); p.quadraticTo (ic.getX() + 4, ic.getY() + 1, ic.getX(), ic.getY() + 4);
                    p.lineTo (ic.getX(), ic.getBottom() - 2); p.quadraticTo (ic.getX() + 4, ic.getBottom() - 5, ic.getCentreX(), ic.getBottom() - 2);
                    p.quadraticTo (ic.getRight() - 4, ic.getBottom() - 5, ic.getRight(), ic.getBottom() - 2); p.lineTo (ic.getRight(), ic.getY() + 4);
                    p.quadraticTo (ic.getRight() - 4, ic.getY() + 1, ic.getCentreX(), ic.getY() + 4); p.lineTo (ic.getCentreX(), ic.getBottom() - 2); break;
            default: p.addRoundedRectangle (ic.reduced (1.0f), 3.0f);
                    for (auto d : { juce::Point<float> (6, 6), juce::Point<float> (13, 13), juce::Point<float> (13, 6), juce::Point<float> (6, 13) })
                        p.addEllipse (ic.getX() + d.x - 1.5f, ic.getY() + d.y - 1.5f, 3.0f, 3.0f);
                    break;
        }
        if (icon != 0) g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (colours::text);
        g.setFont (font (14.5f, Weight::Medium, 0.02f));
        g.drawText (getName().toUpperCase(), 48, 0, getWidth() - 56, getHeight(), juce::Justification::centredLeft);
    }
private:
    int icon;
};

//==============================================================================
void GuardianPanel::Bubble::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xf20a1426));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (accent.withAlpha (0.18f));
    g.drawRoundedRectangle (r.expanded (1.0f), 11.0f, 3.0f);
    g.setColour (accent.withAlpha (0.75f));
    g.drawRoundedRectangle (r, 10.0f, 1.2f);
    g.setColour (accent);
    g.setFont (font (10.5f, Weight::SemiBold, 0.2f));
    g.drawText ("GUARDIAN ANGEL", 12, 6, 150, 16, juce::Justification::centredLeft);
}

GuardianPanel::GuardianPanel (DivinityProcessor& p)
    : proc (p), brain (p),
      send ("Ask", [] { juce::Path s; s.startNewSubPath (3, 11); s.lineTo (21, 3); s.lineTo (14, 21); s.lineTo (11, 13); s.closeSubPath(); return s; }(), false)
{
    addAndMakeVisible (art);

    toggle    = std::make_unique<ActionButton> ("Guardian Angel", 0, true);
    create    = std::make_unique<ActionButton> ("Create a sound", 1);
    ideas     = std::make_unique<ActionButton> ("Suggest ideas", 2);
    explain   = std::make_unique<ActionButton> ("Explain a feature", 3);
    randomize = std::make_unique<ActionButton> ("Randomize a patch", 4);
    toggle->setToggleState (proc.uiAngelTips, juce::dontSendNotification);
    toggle->setTooltip ("Turn Guardian Angel's tips on or off. Off: the angel only speaks when you ask.");
    create->setTooltip ("Describe a sound in your own words and I'll build it.");
    ideas->setTooltip ("Get one idea at a time to push the current patch further.");
    explain->setTooltip ("Click this, then click any control: I'll explain what it does.");
    randomize->setTooltip ("A fresh, musically-safe random patch. Undo brings the old one back.");
    for (auto* b : { toggle.get(), create.get(), ideas.get(), explain.get(), randomize.get() }) addAndMakeVisible (b);

    toggle->onClick = [this]
    {
        proc.uiAngelTips = toggle->getToggleState();
        art.setAwake (proc.uiAngelTips);
        if (proc.uiAngelTips) showReply (brain.greet()); else hideBubble();
    };
    art.setAwake (proc.uiAngelTips);
    create->onClick = [this]
    {
        awaitingDescription = true;
        GuardianBrain::Reply r;
        r.text = "Describe the sound you want in the box below: a type (pad, bass, 808, lead, pluck, keys, bell, choir, drone, riser...) and a vibe (dark, dreamy, aggressive, wide, evolving...). Or try one of these:";
        r.actionLabel = "DREAMY PAD";      r.action    = [this] { showReply (brain.createSound ("dreamy evolving wide pad")); };
        r.secondaryLabel = "GRITTY 808";   r.secondary = [this] { showReply (brain.createSound ("aggressive gritty 808 with glide")); };
        showReply (r);
        askBox.grabKeyboardFocus();
    };
    ideas->onClick = [this] { showReply (brain.suggestIdea()); };
    explain->onClick = [this]
    {
        showReply ({ "Explain mode is on: click any control and I'll tell you what it does. (Esc to cancel.)" });
        if (onExplainRequested) onExplainRequested();
    };
    randomize->onClick = [this] { showReply (brain.randomize()); };

    askBox.setTextToShowWhenEmpty ("Ask Guardian Angel...", textFaint);
    askBox.setFont (font (14.0f));
    askBox.setIndents (12, 12);
    askBox.onReturnKey = [this] { submitAsk(); };
    addAndMakeVisible (askBox);
    send.onClick = [this] { submitAsk(); };
    addAndMakeVisible (send);

    bubble.text.setMultiLine (true, true);
    bubble.text.setReadOnly (true);
    bubble.text.setScrollbarsShown (true);
    bubble.text.setCaretVisible (false);
    bubble.text.setFont (font (13.5f));
    bubble.text.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    bubble.text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    bubble.text.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    bubble.text.setColour (juce::TextEditor::textColourId, text);
    bubble.addAndMakeVisible (bubble.text);
    for (auto* b : { &bubble.primary, &bubble.secondary }) { b->getProperties().set ("fontSize", 11.5); bubble.addChildComponent (b); }
    bubble.close.getProperties().set ("transparent", true);
    bubble.addAndMakeVisible (bubble.close);
    bubble.close.onClick = [this] { hideBubble(); };
    bubble.primary.onClick = [this]
    {
        auto act = current.action;
        if (act) act();
        if (current.actionLabel == "DO IT") showReply ({ "Done. Want another one?", "ANOTHER IDEA", [this] { showReply (brain.suggestIdea()); } });
    };
    bubble.secondary.onClick = [this]
    {
        if (current.secondaryLabel == "ANOTHER IDEA") { showReply (brain.suggestIdea()); return; }
        auto act = current.secondary;
        if (act) act();
    };
    addChildComponent (bubble);
    if (proc.uiAngelTips) showReply (brain.greet());
}

GuardianPanel::~GuardianPanel() = default;

void GuardianPanel::paint (juce::Graphics& g)
{
    drawPanelBackground (g, { 0.0f, 0.0f, (float) getWidth(), 322.0f });
}

void GuardianPanel::resized()
{
    const int w = getWidth();
    if (bubble.isVisible()) { art.setBounds (40, 2, w - 80, 132); bubble.setBounds (6, 136, w - 12, 182); }
    else                    { art.setBounds (6, 8, w - 12, 306);  bubble.setBounds (6, 136, w - 12, 182); }
    bubble.text.setBounds (6, 22, bubble.getWidth() - 12, bubble.getHeight() - 58);
    bubble.close.setBounds (bubble.getWidth() - 28, 2, 24, 22);
    const bool two = bubble.secondary.isVisible();
    const int bw = two ? (bubble.getWidth() - 30) / 2 : bubble.getWidth() - 20;
    bubble.primary.setBounds (10, bubble.getHeight() - 34, bw, 26);
    bubble.secondary.setBounds (20 + bw, bubble.getHeight() - 34, bw, 26);

    int y = 336;
    for (auto* b : { toggle.get(), create.get(), ideas.get(), explain.get(), randomize.get() })
    {
        b->setBounds (0, y, w, b == toggle.get() ? 44 : 42);
        y += b == toggle.get() ? 54 : 50;
    }
    askBox.setBounds (0, getHeight() - 58, w, 58);
    send.setBounds (w - 46, getHeight() - 47, 36, 36);
}

void GuardianPanel::showReply (const GuardianBrain::Reply& r)
{
    current = r;
    bubble.text.setText (r.text, false);
    bubble.text.moveCaretToTop (false);
    bubble.primary.setButtonText (r.actionLabel);
    bubble.primary.setVisible (r.actionLabel.isNotEmpty());
    bubble.secondary.setButtonText (r.secondaryLabel);
    bubble.secondary.setVisible (r.secondaryLabel.isNotEmpty());
    bubble.text.setBounds (6, 22, bubble.getWidth() - 12, bubble.getHeight() - (r.actionLabel.isNotEmpty() ? 58 : 28));
    bubble.setVisible (true);
    resized();
    bubble.toFront (false);
    art.speak();
}

void GuardianPanel::hideBubble()
{
    bubble.setVisible (false);
    awaitingDescription = false;
    resized();
}

void GuardianPanel::showTabTip (const juce::String& tab)
{
    if (! proc.uiAngelTips) return;
    const auto tip = brain.tipForTab (tab);
    if (tip.isNotEmpty()) showReply ({ tip });
}

void GuardianPanel::explainText (const juce::String& tooltip)
{
    showReply (brain.explain (tooltip));
}

void GuardianPanel::submitAsk()
{
    const auto q = askBox.getText().trim();
    if (q.isEmpty()) return;
    askBox.clear();
    showReply (awaitingDescription ? brain.createSound (q) : brain.ask (q));
    awaitingDescription = false;
}

} // namespace divinity::ui
