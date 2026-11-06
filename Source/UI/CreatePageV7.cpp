// DIVINITY 0.7 CREATE page (layout from Ian's v0.7 mockup):
//   ENGINE list | OSCILLATOR / SUB / NOISE / SAMPLER editor | FILTER | AMP (ENVELOPE) | LFO
//   MACROS (8) | ENV 1-4 / LFO 1-4 / STEP / RANDOM / MATRIX tabs
//              | MOD MATRIX | EFFECTS CHAIN | OUTPUT SPECTRUM
#include "Pages.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

namespace
{
    //==========================================================================
    // small drawing helpers
    //==========================================================================
    juce::String sourceName (int i)
    {
        static const char* n[] = { "OSC A", "OSC B", "OSC C", "OSC D", "SUB", "NOISE", "SAMPLER" };
        return n[juce::jlimit (0, 6, i)];
    }

    juce::Path wavePath (int kind, juce::Rectangle<float> r)
    {
        // 0 sine, 1 tri, 2 saw, 3 square, 4 noise, 5 sub hump, 6 sampler bars, 7 saw down, 8 random steps
        juce::Path p;
        const float x0 = r.getX(), x1 = r.getRight(), cy = r.getCentreY(), h = r.getHeight() * 0.5f, w = r.getWidth();
        switch (kind)
        {
            case 0: p.startNewSubPath (x0, cy); for (int i = 1; i <= 24; ++i) p.lineTo (x0 + w * i / 24.0f, cy - h * std::sin (juce::MathConstants<float>::twoPi * i / 24.0f)); break;
            case 1: p.startNewSubPath (x0, cy); p.lineTo (x0 + w * 0.25f, cy - h); p.lineTo (x0 + w * 0.75f, cy + h); p.lineTo (x1, cy); break;
            case 2: p.startNewSubPath (x0, cy + h); p.lineTo (x0 + w * 0.5f, cy - h); p.lineTo (x0 + w * 0.5f, cy + h); p.lineTo (x1, cy - h); break;
            case 3: p.startNewSubPath (x0, cy + h); p.lineTo (x0, cy - h); p.lineTo (x0 + w * 0.5f, cy - h); p.lineTo (x0 + w * 0.5f, cy + h); p.lineTo (x1, cy + h); p.lineTo (x1, cy - h); break;
            case 4: { juce::Random rr (7); p.startNewSubPath (x0, cy); for (int k = 1; k <= 12; ++k) p.lineTo (x0 + w * k / 12.0f, cy + (rr.nextFloat() - 0.5f) * 2.0f * h); break; }
            case 5: p.startNewSubPath (x0, cy + h * 0.7f); p.cubicTo (x0 + w * 0.3f, cy - h, x0 + w * 0.7f, cy - h, x1, cy + h * 0.7f); break;
            case 6: for (int k = 0; k < 9; ++k) { const float x = x0 + w * k / 8.0f, a = h * (0.25f + 0.75f * std::abs (std::sin (k * 1.7f))); p.startNewSubPath (x, cy - a); p.lineTo (x, cy + a); } break;
            case 7: p.startNewSubPath (x0, cy - h); p.lineTo (x0 + w * 0.5f, cy + h); p.lineTo (x0 + w * 0.5f, cy - h); p.lineTo (x1, cy + h); break;
            default: { const float st[] = { 0.2f, -0.6f, 0.8f, -0.1f, 0.5f }; p.startNewSubPath (x0, cy - st[0] * h);
                       for (int k = 0; k < 5; ++k) { p.lineTo (x0 + w * (k + 1) / 5.0f, cy - st[k] * h); if (k < 4) p.lineTo (x0 + w * (k + 1) / 5.0f, cy - st[k + 1] * h); } break; }
        }
        return p;
    }

    juce::Path dots3()
    {
        juce::Path p;
        for (int i = 0; i < 3; ++i) p.addEllipse (5.0f + i * 6.0f, 10.5f, 3.0f, 3.0f);
        return p;
    }
    juce::Path plusPath() { juce::Path p; p.startNewSubPath (12, 5); p.lineTo (12, 19); p.startNewSubPath (5, 12); p.lineTo (19, 12); return p; }
    juce::Path crossPath() { juce::Path p; p.startNewSubPath (7, 7); p.lineTo (17, 17); p.startNewSubPath (17, 7); p.lineTo (7, 17); return p; }
    juce::Path folderPath()
    {
        juce::Path p;
        p.startNewSubPath (3, 7); p.lineTo (9, 7); p.lineTo (11, 9); p.lineTo (21, 9); p.lineTo (21, 19); p.lineTo (3, 19); p.closeSubPath();
        return p;
    }
    juce::Path gripPath()
    {
        juce::Path p;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 2; ++c) p.addEllipse (8.0f + c * 5.0f, 6.0f + r * 5.0f, 2.2f, 2.2f);
        return p;
    }

    juce::Path filterGlyph()
    {
        juce::Path p;
        p.startNewSubPath (2, 8); p.lineTo (12, 8); p.cubicTo (16, 8, 17, 6, 19, 4);
        p.startNewSubPath (12, 8); p.cubicTo (15, 8, 16, 14, 22, 20);
        p.addRoundedRectangle (2.0f, 13.0f, 7.0f, 7.0f, 2.0f);
        return p;
    }
    juce::Path gridGlyph()
    {
        juce::Path p;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) p.addEllipse (4.0f + c * 6.0f, 4.0f + r * 6.0f, 3.0f, 3.0f);
        return p;
    }
    juce::Path targetGlyph()
    {
        juce::Path p;
        p.addRoundedRectangle (3.0f, 3.0f, 18.0f, 18.0f, 4.0f);
        p.addEllipse (8.0f, 8.0f, 8.0f, 8.0f);
        return p;
    }
    juce::Path windowGlyph()
    {
        juce::Path p;
        p.addRoundedRectangle (3.0f, 4.0f, 18.0f, 16.0f, 3.0f);
        p.startNewSubPath (3, 9); p.lineTo (21, 9);
        return p;
    }
    juce::Path dialGlyph()
    {
        juce::Path p;
        p.addCentredArc (10, 12, 7, 7, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        p.startNewSubPath (10, 12); p.lineTo (14, 8);
        p.startNewSubPath (19, 11); p.lineTo (21, 13); p.lineTo (23, 11);
        return p;
    }
    juce::Path resetGlyph()
    {
        juce::Path p;
        p.addCentredArc (12, 12, 7, 7, 0.0f, -2.2f, 2.4f, true);
        p.startNewSubPath (4.5f, 5.0f); p.lineTo (5.2f, 9.5f); p.lineTo (9.6f, 8.2f);
        return p;
    }

    /** Panel with a title row like the mockup: optional glyph or power button, title, chip text. */
    class V7Panel : public juce::Component
    {
    public:
        explicit V7Panel (juce::String t) : title (std::move (t)) {}
        enum Glyph { NoGlyph, FilterGlyph, StarGlyph, GearGlyph, DiamondGlyph };
        void paint (juce::Graphics& g) override
        {
            drawPanelBackground (g, getLocalBounds().toFloat());
            g.setColour (text);
            g.setFont (titleFont());
            g.drawText (title, titleX, 9, getWidth() - titleX - 12, 26, juce::Justification::centredLeft, false);
            const juce::Rectangle<float> ib (14.0f, 12.0f, 20.0f, 20.0f);
            switch (glyph)
            {
                case FilterGlyph: { auto p = filterGlyph(); p.applyTransform (p.getTransformToScaleToFit (ib.reduced (1.0f), true)); strokeGlow (g, p, 1.5f); break; }
                case StarGlyph:
                {
                    juce::Path p; p.addStar (ib.getCentre(), 5, 3.6f, 8.6f, 0.0f);
                    g.setColour (accent.withAlpha (0.25f)); g.fillPath (p, juce::AffineTransform::scale (1.25f, 1.25f, ib.getCentreX(), ib.getCentreY()));
                    g.setColour (accent); g.fillPath (p);
                    break;
                }
                case GearGlyph: { auto p = icons::gear(); p.applyTransform (p.getTransformToScaleToFit (ib.reduced (1.0f), true)); g.setColour (text); g.fillPath (p); break; }
                case DiamondGlyph:
                {
                    juce::Path p; const auto c = ib.getCentre();
                    p.startNewSubPath (c.x, c.y - 7); p.lineTo (c.x + 7, c.y); p.lineTo (c.x, c.y + 7); p.lineTo (c.x - 7, c.y); p.closeSubPath();
                    g.setColour (text); g.strokePath (p, juce::PathStrokeType (1.4f));
                    break;
                }
                default: break;
            }
            if (chip.isNotEmpty())
            {
                auto c = chipBounds().toFloat();
                g.setColour (accentDeep.withAlpha (0.45f)); g.fillRoundedRectangle (c, 5.0f);
                g.setColour (accent.withAlpha (0.7f)); g.drawRoundedRectangle (c.reduced (0.5f), 5.0f, 1.0f);
                g.setColour (accent);
                g.setFont (font (10.5f, Weight::Medium, 0.06f));
                g.drawText (chip, c, juce::Justification::centred);
            }
        }
        static juce::Font titleFont() { return font (16.0f, Weight::Medium, 0.02f); }
        juce::Rectangle<int> chipBounds() const
        {
            const int tw = (int) titleFont().getStringWidthFloat (title);
            return { titleX + tw + 14, 12, 46, 20 };
        }
        void setTitle (const juce::String& t) { title = t; repaint(); }
        juce::String title, chip;
        int titleX = 16;
        Glyph glyph = NoGlyph;
    };

    IconButton* headerButton (juce::OwnedArray<IconButton>& store, juce::Component& parent, const juce::String& tip, juce::Path p, bool filled = false)
    {
        auto* b = store.add (new IconButton (tip, std::move (p), filled));
        b->boxed = true;
        parent.addAndMakeVisible (b);
        return b;
    }

    void layoutRow (juce::Array<juce::Component*> items, juce::Rectangle<int> row)
    {
        int n = 0;
        for (auto* c : items) if (c->isVisible()) ++n;
        if (n == 0) return;
        const int w = row.getWidth() / n;
        int i = 0;
        for (auto* c : items) if (c->isVisible()) c->setBounds (row.getX() + (i++) * w, row.getY(), w, row.getHeight());
    }

    Knob* makeKnob (APVTS& s, const juce::String& id, const char* cap, ModDest d = ModDest::None, float scale = 1.0f, bool bipolar = false)
    {
        auto* k = new Knob (s, id, cap, d, scale, bipolar);
        k->setCaptionSize (11.5f);
        return k;
    }

    void bindToggle (juce::TextButton& b, APVTS& s, const juce::String& id, std::vector<std::unique_ptr<APVTS::ButtonAttachment>>& store)
    {
        b.setClickingTogglesState (true);
        b.setTooltip (helpTextFor (id));
        b.getProperties().set ("fontSize", 11.5);
        store.push_back (std::make_unique<APVTS::ButtonAttachment> (s, id, b));
    }

    //==========================================================================
    // ENGINE list (7 sources)
    //==========================================================================
    class EngineListV7 : public V7Panel, private juce::Timer
    {
    public:
        explicit EngineListV7 (APVTS& s) : V7Panel ("ENGINE"), apvts (s), more ("Engine options", dots3(), true)
        {
            more.setTooltip ("Click a source to edit it. Click its dot to switch it on or off.");
            addAndMakeVisible (more);
            startTimerHz (6);
        }
        std::function<void (int)> onSelect;
        int selected = 0;

        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            static const int icon[] = { 0, 0, 0, 0, 0, 4, 6 };
            for (int i = 0; i < 7; ++i)
            {
                auto r = item (i).toFloat();
                const bool sel = i == selected;
                drawGlassControl (g, r, sel, i == hover, 7.0f);
                if (! sel) { g.setColour (juce::Colour (0xff010305).withAlpha (0.6f)); g.fillRoundedRectangle (r.reduced (1.0f), 6.0f); }
                const bool on = isOn (i);
                auto ic = wavePath (icon[i], { r.getX() + 14.0f, r.getCentreY() - 6.0f, 24.0f, 12.0f });
                if (sel) strokeGlow (g, ic, 1.6f, glow);
                else { g.setColour (on ? text : textFaint); g.strokePath (ic, juce::PathStrokeType (1.4f)); }
                g.setFont (font (14.5f, Weight::Medium));
                g.setColour (on || sel ? text : textDim);
                g.drawText (sourceName (i), (int) r.getX() + 56, (int) r.getY(), 90, (int) r.getHeight(), juce::Justification::centredLeft);
                auto dot = dotBounds (i);
                if (on)
                {
                    g.setColour ((sel ? glow : accent).withAlpha (0.3f)); g.fillEllipse (dot.expanded (3.0f));
                    g.setColour (sel ? juce::Colours::white : accent); g.fillEllipse (dot);
                }
                else
                {
                    g.setColour (textFaint); g.drawEllipse (dot.reduced (0.5f), 1.0f);
                }
            }
        }
        void resized() override { more.setBounds (getWidth() - 34, 8, 26, 26); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 7; ++i)
            {
                if (dotBounds (i).expanded (8.0f).contains (e.position))
                {
                    if (auto* p = apvts.getParameter (onId (i))) { p->beginChangeGesture(); p->setValueNotifyingHost (isOn (i) ? 0.0f : 1.0f); p->endChangeGesture(); }
                    repaint();
                    return;
                }
                if (item (i).contains (e.getPosition())) { selected = i; repaint(); if (onSelect) onSelect (i); }
            }
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            int h = -1;
            for (int i = 0; i < 7; ++i) if (item (i).contains (e.getPosition())) h = i;
            if (h != hover) { hover = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    private:
        juce::Rectangle<int> item (int i) const { const int h = (getHeight() - 46) / 7; return { 9, 40 + i * h, getWidth() - 18, h - 4 }; }
        juce::Rectangle<float> dotBounds (int i) const { auto r = item (i).toFloat(); return juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getRight() - 16.0f, r.getCentreY() }); }
        juce::String onId (int i) const
        {
            if (i < 4) return ids::osc (i, "on");
            return i == 4 ? ids::subOn : i == 5 ? ids::noiseOn : ids::smpOn;
        }
        bool isOn (int i) const { return apvts.getRawParameterValue (onId (i))->load() > 0.5f; }
        void timerCallback() override
        {
            int mask = 0;
            for (int i = 0; i < 7; ++i) if (isOn (i)) mask |= 1 << i;
            if (mask != lastMask) { lastMask = mask; repaint(); }
        }
        APVTS& apvts;
        IconButton more;
        int hover = -1, lastMask = -1;
    };

    //==========================================================================
    // Source editors
    //==========================================================================
    /** Thin vertical level fader drawn beside a display. */
    class LevelFader : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        LevelFader (APVTS& s, const juce::String& id) : param (*s.getParameter (id)), attach (param, [this] (float) { repaint(); })
        {
            attach.sendInitialUpdate();
            setTooltip (helpTextFor (id) + "\n\nDrag up/down. Double-click to reset.");
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (3.0f, 2.0f);
            g.setColour (panel2); g.fillRoundedRectangle (r, 3.0f);
            const float v = param.getValue();
            auto f = r.withTop (r.getBottom() - r.getHeight() * v);
            g.setGradientFill (juce::ColourGradient (accent, 0, f.getY(), accentDeep, 0, r.getBottom(), false));
            g.fillRoundedRectangle (f, 3.0f);
            g.setColour (text); g.fillRect (r.getX(), f.getY() - 1.0f, r.getWidth(), 2.0f);
        }
        void mouseDown (const juce::MouseEvent& e) override { attach.beginGesture(); startV = param.getValue(); startY = e.position.y; }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float scale = e.mods.isShiftDown() ? 0.15f : 1.0f;
            attach.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, startV - (e.position.y - startY) / (float) getHeight() * scale)));
        }
        void mouseUp (const juce::MouseEvent&) override { attach.endGesture(); }
        void mouseDoubleClick (const juce::MouseEvent&) override { attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
    private:
        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attach;
        float startV = 0, startY = 0;
    };

    /** Deep settings shown in a callout ("..." on the oscillator). */
    class OscDeepCallout : public juce::Component
    {
    public:
        OscDeepCallout (APVTS& s, int i)
        {
            knobs.add (makeKnob (s, ids::osc (i, "level"), "Level", (ModDest) oscModDest (2, i)));
            knobs.add (makeKnob (s, ids::osc (i, "fine"), "Fine", ModDest::None, 1.0f, true));
            knobs.add (makeKnob (s, ids::osc (i, "fold"), "Fold", (ModDest) oscModDest (3, i)));
            knobs.add (makeKnob (s, ids::osc (i, "width"), "Width"));
            knobs.add (makeKnob (s, ids::osc (i, "pw"), "PW", (ModDest) oscModDest (4, i), 0.5f));
            knobs.add (makeKnob (s, ids::osc (i, "phase"), "Phase"));
            for (auto* k : knobs) addAndMakeVisible (k);
            rand.setButtonText ("RANDOM PHASE");
            bindToggle (rand, s, ids::osc (i, "randphase"), attach);
            addAndMakeVisible (rand);
            setSize (390, 210);
        }
        void resized() override
        {
            for (int k = 0; k < knobs.size(); ++k) knobs[k]->setBounds ((k % 3) * 130, (k / 3) * 86, 130, 84);
            rand.setBounds (100, 178, 190, 26);
        }
    private:
        juce::OwnedArray<Knob> knobs;
        juce::TextButton rand;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> attach;
    };

    class OscSourceV7 : public V7Panel, private juce::Timer
    {
    public:
        OscSourceV7 (DivinityProcessor& p, int i)
            : V7Panel ("OSCILLATOR " + juce::String::charToString ((juce::juce_wchar) ('A' + i))), apvts (p.apvts), index (i),
              power (p.apvts, ids::osc (i, "on")), engine (p.apvts, ids::osc (i, "engine")),
              table (p.apvts, ids::osc (i, "table")), wave (p.apvts, ids::osc (i, "wave")),
              display (p.apvts, p.getWavetables(), i), level (p.apvts, ids::osc (i, "level")),
              more ("More oscillator settings (level, fine, fold, width, pulse width, phase)", targetGlyph()),
              link ("Copy this oscillator's settings to another oscillator", juce::Path())
        {
            chip = sourceName (i);
            titleX = 42;
            more.boxed = true; link.boxed = true;
            {
                juce::Path lp; lp.addRoundedRectangle (2.0f, 9.0f, 11.0f, 7.0f, 3.5f); lp.addRoundedRectangle (11.0f, 9.0f, 11.0f, 7.0f, 3.5f);
                link.setPath (lp);
            }
            link.onClick = [this]
            {
                juce::PopupMenu m;
                m.addSectionHeader ("Copy " + sourceName (index) + " to");
                for (int t = 0; t < kNumOscs; ++t) if (t != index) m.addItem (t + 1, sourceName (t));
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&link), [this] (int r)
                {
                    if (r <= 0) return;
                    for (auto* n : { "on", "engine", "table", "wave", "pos", "oct", "semi", "fine", "level", "unison", "detune", "width", "blend", "fold", "pw", "phase", "randphase" })
                        if (auto* from = apvts.getParameter (ids::osc (index, n)))
                            if (auto* to = apvts.getParameter (ids::osc (r - 1, n)))
                                { to->beginChangeGesture(); to->setValueNotifyingHost (from->getValue()); to->endChangeGesture(); }
                });
            };
            auto& s = p.apvts;
            oct = makeKnob (s, ids::osc (i, "oct"), "Octave", ModDest::None, 1.0f, true);
            semi = makeKnob (s, ids::osc (i, "semi"), "Semi", ModDest::None, 1.0f, true);
            fine = makeKnob (s, ids::osc (i, "fine"), "Fine", ModDest::None, 1.0f, true);
            pw = makeKnob (s, ids::osc (i, "pw"), "PW", (ModDest) oscModDest (4, i), 0.5f);
            pos = makeKnob (s, ids::osc (i, "pos"), "Position", (ModDest) oscModDest (1, i));
            uni = makeKnob (s, ids::osc (i, "unison"), "Unison");
            det = makeKnob (s, ids::osc (i, "detune"), "Detune", (ModDest) oscModDest (5, i));
            blend = makeKnob (s, ids::osc (i, "blend"), "Blend");
            for (auto* k : { oct, semi, fine, pw, pos, uni, det, blend }) { owned.add (k); addChildComponent (k); k->setValueStyle (1); }
            auto pct = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
            auto num = [] (float v) { return juce::String (juce::roundToInt (v)); };
            oct->formatValue = num; semi->formatValue = num; uni->formatValue = num;
            fine->formatValue = num; det->formatValue = pct; blend->formatValue = pct; pos->formatValue = pct; pw->formatValue = pct;
            for (auto* c : std::initializer_list<juce::Component*> { &power, &engine, &table, &wave, &display, &more, &link }) addAndMakeVisible (c);
            addChildComponent (level);   // level lives in the deep settings now (target button)
            engine.onChange = [this] { update(); };
            wave.onChange = [this] { update(); };
            table.centreText = true; wave.centreText = true;
            more.onClick = [this, &s]
            {
                auto content = std::make_unique<OscDeepCallout> (s, index);
                juce::CallOutBox::launchAsynchronously (std::move (content), more.getScreenBounds(), nullptr);
            };
            update();
            startTimerHz (4);
        }
        void update()
        {
            const bool isWt = engine.getIndex() == 0;
            const bool isPulse = ! isWt && wave.getIndex() == (int) AnalogWave::Pulse;
            table.setVisible (isWt); wave.setVisible (! isWt);
            pos->setVisible (isWt);
            fine->setVisible (! isWt && ! isPulse);
            pw->setVisible (isPulse);
            oct->setVisible (true); semi->setVisible (true); uni->setVisible (true); det->setVisible (true); blend->setVisible (true);
            resized(); repaint();
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            power.setBounds (11, 9, 26, 26);
            more.setBounds (w - 36, 9, 26, 24);
            link.setBounds (w - 66, 9, 26, 24);
            engine.setBounds (12, 41, w - 24, 28);
            table.setBounds (12, 74, w - 24, 28);
            wave.setBounds (12, 74, w - 24, 28);
            display.setBounds (14, 106, w - 28, h - 200);
            layoutRow ({ oct, semi, fine, pw, pos, uni, det, blend }, { 4, h - 90, w - 8, 86 });
        }
    private:
        void timerCallback() override { const bool on = power.getToggleState(); if (on != lastOn) { lastOn = on; display.setAlpha (on ? 1.0f : 0.45f); } }
        APVTS& apvts;
        int index;
        PowerButton power;
        Selector engine, table, wave;
        OscDisplay display;
        LevelFader level;
        IconButton more, link;
        juce::OwnedArray<Knob> owned;
        Knob *oct, *semi, *fine, *pw, *pos, *uni, *det, *blend;
        bool lastOn = true;
    };

    /** Draws the sub / noise waveform for those sources. */
    class SimpleWaveView : public juce::Component, private juce::Timer
    {
    public:
        SimpleWaveView (APVTS& s, bool noiseView) : apvts (s), noise (noiseView) { startTimerHz (5); setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawDisplayBackground (g, r);
            r = r.reduced (14.0f, 16.0f);
            juce::Path p;
            if (noise)
            {
                const int type = (int) apvts.getRawParameterValue (ids::noiseType)->load();
                juce::Random rr (5 + type);
                float lp = 0;
                p.startNewSubPath (r.getX(), r.getCentreY());
                for (int i = 1; i <= 160; ++i)
                {
                    float v = rr.nextFloat() * 2.0f - 1.0f;
                    if (type == 3) { lp += 0.15f * (v - lp); v = lp * 2.5f; }
                    if (type == 1) { lp += 0.5f * (v - lp); v = lp * 1.4f; }
                    p.lineTo (r.getX() + r.getWidth() * i / 160.0f, r.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * r.getHeight() * 0.45f);
                }
            }
            else
            {
                const int mode = (int) apvts.getRawParameterValue (ids::subMode)->load();
                const int kind = mode / 2 == 0 ? 0 : mode / 2 == 1 ? 1 : 3;
                const int cycles = mode % 2 == 0 ? 2 : 1;
                for (int c = 0; c < cycles; ++c)
                {
                    auto seg = wavePath (kind, { r.getX() + r.getWidth() * c / cycles, r.getY(), r.getWidth() / cycles, r.getHeight() });
                    p.addPath (seg);
                }
            }
            g.setColour (accent.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (5.0f));
            g.setColour (accent); g.strokePath (p, juce::PathStrokeType (1.6f));
        }
    private:
        void timerCallback() override
        {
            const float h = apvts.getRawParameterValue (noise ? ids::noiseType : ids::subMode)->load();
            if (h != last) { last = h; repaint(); }
        }
        APVTS& apvts;
        bool noise;
        float last = -1.0f;
    };

    class LayerSourceV7 : public V7Panel
    {
    public:
        LayerSourceV7 (DivinityProcessor& p, bool isNoise)
            : V7Panel (isNoise ? "NOISE" : "SUB OSCILLATOR"), noise (isNoise),
              power (p.apvts, isNoise ? ids::noiseOn : ids::subOn),
              mode (p.apvts, isNoise ? ids::noiseType : ids::subMode), view (p.apvts, isNoise)
        {
            titleX = 42;
            level.reset (makeKnob (p.apvts, isNoise ? ids::noiseLevel : ids::subLevel, "Level", isNoise ? ModDest::NoiseLevel : ModDest::SubLevel));
            for (auto* c : std::initializer_list<juce::Component*> { &power, &mode, &view, level.get() }) addAndMakeVisible (c);
        }
        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            g.setColour (textDim);
            g.setFont (font (11.5f));
            g.drawFittedText (noise ? "Breath, air or grit on top of the oscillators. Pink is soft, Dark rumbles, Bright is airy."
                                    : "A clean wave one or two octaves under the played note: weight under basses, keys and pads.",
                              16, getHeight() - 82, getWidth() - 120, 74, juce::Justification::centredLeft, 4);
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            power.setBounds (12, 9, 24, 24);
            mode.setBounds (12, 40, w - 24, 26);
            view.setBounds (12, 72, w - 24, h - 160);
            level->setBounds (w - 100, h - 86, 90, 82);
        }
    private:
        bool noise;
        PowerButton power;
        Selector mode;
        SimpleWaveView view;
        std::unique_ptr<Knob> level;
    };

    /** Waveform of the selected sample with start / loop markers. Accepts dropped audio files. */
    class SampleView : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit SampleView (DivinityProcessor& p) : proc (p)
        {
            setTooltip ("Drop a WAV, AIFF, FLAC or OGG here to play it in the sampler (it becomes \"User Sample\").");
            startTimerHz (10);
        }
        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            for (auto& f : files) if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
                                      || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".ogg")) return true;
            return false;
        }
        void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }
        void filesDropped (const juce::StringArray& files, int, int) override
        {
            dragging = false;
            if (! files.isEmpty()) message = proc.loadUserSample (juce::File (files[0]));
            cached = nullptr;
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawDisplayBackground (g, r);
            if (dragging) { g.setColour (accent); g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 2.0f); }
            const auto* sd = current();
            auto inner = r.reduced (8.0f, 10.0f);
            if (sd == nullptr || sd->length() < 2)
            {
                g.setColour (textDim);
                g.setFont (font (12.5f));
                g.drawFittedText (message.isNotEmpty() ? message : "No user sample loaded.\nDrop a WAV / AIFF here or press LOAD.", inner.toNearestInt(), juce::Justification::centred, 3);
                return;
            }
            const int cols = juce::jmax (8, (int) inner.getWidth());
            const int len = sd->length();
            juce::Path p;
            for (int c = 0; c < cols; ++c)
            {
                const int a = (int) ((juce::int64) len * c / cols), b = juce::jmax (a + 1, (int) ((juce::int64) len * (c + 1) / cols));
                float pk = 0;
                for (int i = a; i < b; i += juce::jmax (1, (b - a) / 32)) pk = juce::jmax (pk, std::abs (sd->left[(size_t) i]));
                const float x = inner.getX() + (float) c, hh = pk * inner.getHeight() * 0.48f;
                p.addRectangle (x, inner.getCentreY() - hh, 1.0f, juce::jmax (1.0f, hh * 2.0f));
            }
            g.setColour (accent.withAlpha (0.85f)); g.fillPath (p);
            auto xAt = [&] (float frac) { return inner.getX() + inner.getWidth() * frac; };
            const float st = proc.apvts.getRawParameterValue (ids::smpStart)->load();
            g.setColour (text); g.drawVerticalLine ((int) xAt (st), inner.getY(), inner.getBottom());
            if (proc.apvts.getRawParameterValue (ids::smpLoop)->load() > 0.5f)
            {
                const float a = proc.apvts.getRawParameterValue (ids::smpLoopStart)->load(), b = proc.apvts.getRawParameterValue (ids::smpLoopEnd)->load();
                g.setColour (accentStrong.withAlpha (0.16f));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (a), inner.getY(), xAt (juce::jmax (a, b)), inner.getBottom()));
            }
            g.setColour (textDim);
            g.setFont (font (11.0f, Weight::Medium));
            g.drawText (sd->name, inner.toNearestInt().removeFromTop (14), juce::Justification::topRight);
        }
        juce::String message;
    private:
        const SampleData* current() const
        {
            const int idx = (int) proc.apvts.getRawParameterValue (ids::smpSample)->load();
            return idx >= proc.getFactorySamples().size() ? proc.getUserSample().get() : proc.getFactorySamples().get (idx);
        }
        void timerCallback() override
        {
            const auto* sd = current();
            float h = (float) (juce::pointer_sized_int) sd;
            for (auto* id : { &ids::smpStart, &ids::smpLoop, &ids::smpLoopStart, &ids::smpLoopEnd }) h = h * 1.37f + proc.apvts.getRawParameterValue (*id)->load();
            if (h != lastHash) { lastHash = h; cached = sd; repaint(); }
        }
        DivinityProcessor& proc;
        const SampleData* cached = nullptr;
        bool dragging = false;
        float lastHash = -1.0f;
    };

    class SamplerSourceV7 : public V7Panel
    {
    public:
        explicit SamplerSourceV7 (DivinityProcessor& p)
            : V7Panel ("SAMPLER"), proc (p), power (p.apvts, ids::smpOn), sample (p.apvts, ids::smpSample), view (p),
              load ("Load your own sample (WAV / AIFF / FLAC / OGG)", folderPath())
        {
            titleX = 42;
            auto& s = p.apvts;
            loop.setButtonText ("LOOP"); rev.setButtonText ("REV"); key.setButtonText ("KEY");
            bindToggle (loop, s, ids::smpLoop, attach); bindToggle (rev, s, ids::smpReverse, attach); bindToggle (key, s, ids::smpKeytrack, attach);
            knobs.add (makeKnob (s, ids::smpTune, "Tune", ModDest::SamplerPitch, 1.0f, true));
            knobs.add (makeKnob (s, ids::smpFine, "Fine", ModDest::None, 1.0f, true));
            knobs.add (makeKnob (s, ids::smpStart, "Start", ModDest::SamplerStart));
            knobs.add (makeKnob (s, ids::smpLoopStart, "Loop S"));
            knobs.add (makeKnob (s, ids::smpLoopEnd, "Loop E"));
            knobs.add (makeKnob (s, ids::smpLevel, "Level", ModDest::SamplerLevel));
            for (auto* k : knobs) addAndMakeVisible (k);
            for (auto* c : std::initializer_list<juce::Component*> { &power, &sample, &view, &load, &loop, &rev, &key }) addAndMakeVisible (c);
            load.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Load a sample", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult().existsAsFile()) view.message = proc.loadUserSample (fc.getResult());
                    view.repaint();
                });
            };
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            power.setBounds (12, 9, 24, 24);
            key.setBounds (w - 156, 10, 44, 22); rev.setBounds (w - 108, 10, 44, 22); loop.setBounds (w - 60, 10, 48, 22);
            sample.setBounds (12, 40, w - 58, 26);
            load.setBounds (w - 42, 40, 30, 26);
            view.setBounds (12, 72, w - 24, h - 160);
            for (int k = 0; k < knobs.size(); ++k) knobs[k]->setBounds (4 + k * (w - 8) / 6, h - 86, (w - 8) / 6, 82);
        }
    private:
        DivinityProcessor& proc;
        PowerButton power;
        Selector sample;
        SampleView view;
        IconButton load;
        juce::TextButton loop, rev, key;
        juce::OwnedArray<Knob> knobs;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> attach;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    //==========================================================================
    // FILTER
    //==========================================================================
    class FilterV7 : public V7Panel, private juce::Timer
    {
    public:
        explicit FilterV7 (DivinityProcessor& p)
            : V7Panel ("FILTER"), apvts (p.apvts), routing ("Routing: serial / parallel", juce::Path()),
              routeAttach (*p.apvts.getParameter (ids::filterRouting), [this] (float v) { routing.setToggleState (v > 0.5f, juce::dontSendNotification); updateRouteIcon(); }),
              response (p.apvts, 0)
        {
            titleX = 42;
            glyph = FilterGlyph;
            auto& s = p.apvts;
            for (int f = 0; f < 2; ++f)
            {
                types.add (new Selector (s, ids::filt (f, "type")));
                addChildComponent (types.getLast());
                const auto cut = f == 0 ? ModDest::Filter1Cutoff : ModDest::Filter2Cutoff;
                const auto res = f == 0 ? ModDest::Filter1Res : ModDest::Filter2Res;
                const auto drv = f == 0 ? ModDest::Filter1Drive : ModDest::Filter2Drive;
                knobs[f].add (makeKnob (s, ids::filt (f, "cutoff"), "Cutoff", cut, 0.6f));
                knobs[f].add (makeKnob (s, ids::filt (f, "res"), "Res", res));
                knobs[f].add (makeKnob (s, ids::filt (f, "drive"), "Drive", drv));
                knobs[f].add (makeKnob (s, ids::filt (f, "mix"), "Mix"));
                for (auto* k : knobs[f]) addChildComponent (k);
            }
            // header: [filter 1/2 menu] [routing] [deep settings]
            auto* pick = headerButton (header, *this, "Choose Filter 1 or Filter 2", dialGlyph());
            pick->onClick = [this, pick]
            {
                juce::PopupMenu m;
                m.addItem (1, "Filter 1", true, current == 0);
                m.addItem (2, "Filter 2", true, current == 1);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (pick), [this] (int r) { if (r > 0) select (r - 1); });
            };
            routing.boxed = true;
            routing.setClickingTogglesState (false);
            routing.onClick = [this] { routeAttach.setValueAsCompleteGesture (routing.getToggleState() ? 0.0f : 1.0f); };
            routeAttach.sendInitialUpdate();
            addAndMakeVisible (routing);
            auto* deep = headerButton (header, *this, "Key tracking for this filter", gridGlyph(), true);
            deep->onClick = [this, deep]
            {
                auto* k = makeKnob (apvts, ids::filt (current, "keytrack"), "Keytrack");
                struct Holder : juce::Component { std::unique_ptr<Knob> k; void resized() override { if (k) k->setBounds (getLocalBounds().reduced (5, 4)); } };
                auto holder = std::make_unique<Holder>();
                holder->k.reset (k);
                holder->addAndMakeVisible (k);
                holder->setSize (110, 96);
                juce::CallOutBox::launchAsynchronously (std::move (holder), deep->getScreenBounds(), nullptr);
            };
            // inside the display: F1 / F2 quick select + routing state
            for (int f = 0; f < 2; ++f)
            {
                juce::Path br;
                br.startNewSubPath (3, 8); br.lineTo (3, 3); br.lineTo (8, 3);
                br.startNewSubPath (16, 3); br.lineTo (21, 3); br.lineTo (21, 8);
                br.startNewSubPath (21, 16); br.lineTo (21, 21); br.lineTo (16, 21);
                br.startNewSubPath (8, 21); br.lineTo (3, 21); br.lineTo (3, 16);
                if (f == 1) br.addRectangle (10.0f, 10.0f, 4.0f, 4.0f);
                auto* b = quick.add (new IconButton (f == 0 ? "Edit Filter 1" : "Edit Filter 2", br));
                b->onClick = [this, f] { select (f); };
                addAndMakeVisible (b);
            }
            {
                juce::Path lp; lp.addRoundedRectangle (2.0f, 8.0f, 11.0f, 8.0f, 4.0f); lp.addRoundedRectangle (11.0f, 8.0f, 11.0f, 8.0f, 4.0f);
                auto* b = quick.add (new IconButton ("Filter routing: serial / parallel", lp));
                b->onClick = [this] { routing.triggerClick(); };
                addAndMakeVisible (b);
            }
            for (auto* c : std::initializer_list<juce::Component*> { &response }) addAndMakeVisible (c);
            for (auto* b : quick) b->toFront (false);
            select (0);
            startTimerHz (5);
        }
        void select (int f)
        {
            current = f;
            for (int i = 0; i < 2; ++i) { types[i]->setVisible (i == f); for (auto* k : knobs[i]) k->setVisible (i == f); }
            for (int i = 0; i < 2; ++i) quick[i]->setToggleState (i == f, juce::dontSendNotification);
            response.setFilter (f);
            setTitle (f == 0 ? "FILTER" : "FILTER 2");
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            header[0]->setBounds (w - 98, 9, 30, 24);
            routing.setBounds (w - 64, 9, 26, 24);
            header[1]->setBounds (w - 34, 9, 26, 24);
            for (auto* t : types) t->setBounds (12, 41, w - 24, 28);
            response.setBounds (12, 76, w - 24, h - 170);
            for (int i = 0; i < quick.size(); ++i) quick[i]->setBounds (response.getRight() - 24, response.getY() + 6 + i * 20, 18, 18);
            for (auto& k : knobs) layoutRow ({ k[0], k[1], k[2], k[3] }, { 4, h - 90, w - 8, 86 });
        }
    private:
        void updateRouteIcon()
        {
            juce::Path p;
            p.addRoundedRectangle (2.0f, 9.0f, 11.0f, 7.0f, 3.5f); p.addRoundedRectangle (11.0f, 9.0f, 11.0f, 7.0f, 3.5f);
            if (routing.getToggleState()) { p.clear(); p.addRoundedRectangle (4.0f, 6.0f, 16.0f, 4.0f, 2.0f); p.addRoundedRectangle (4.0f, 14.0f, 16.0f, 4.0f, 2.0f); }
            routing.setPath (p);
            routing.setTooltip (routing.getToggleState() ? "Filters run in PARALLEL. Click for serial." : "Filters run in SERIAL (1 into 2). Click for parallel.");
        }
        void timerCallback() override
        {
            const float v = apvts.getRawParameterValue (ids::filt (current, "type"))->load();
            if (v != lastType) { lastType = v; repaint(); }
        }
        APVTS& apvts;
        juce::OwnedArray<IconButton> header, quick;
        IconButton routing;
        juce::ParameterAttachment routeAttach;
        juce::OwnedArray<Selector> types;
        FilterResponse response;
        juce::OwnedArray<Knob> knobs[2];
        int current = 0;
        float lastType = -1.0f;
    };

    //==========================================================================
    // AMP (ENVELOPE)
    //==========================================================================
    class CurveButtons : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        CurveButtons (APVTS& s, int env) : param (*s.getParameter (ids::env (env, "curve"))), attach (param, [this] (float) { repaint(); })
        {
            attach.sendInitialUpdate();
            setTooltip ("Envelope curve: punchy, snappy, linear, swelling. Fine-tune with the CURVE knob on the ENV tab.");
        }
        static constexpr int kCount = 4;
        void paint (juce::Graphics& g) override
        {
            static const float values[kCount] = { -0.8f, -0.35f, 0.0f, 0.6f };
            auto box = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff050c11).withAlpha (0.9f)); g.fillRoundedRectangle (box, 6.0f);
            g.setColour (juce::Colour (0xff22394a)); g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
            const float cur = param.convertFrom0to1 (param.getValue());
            int best = 0; for (int i = 1; i < kCount; ++i) if (std::abs (cur - values[i]) < std::abs (cur - values[best])) best = i;
            for (int i = 0; i < kCount; ++i)
            {
                auto r = slot (i).toFloat();
                const bool sel = i == best && std::abs (cur - values[i]) < 0.2f;
                if (sel) drawGlassControl (g, r.reduced (2.0f), true, false, 4.0f);
                juce::Path p;
                auto a = r.reduced (r.getWidth() * 0.26f, r.getHeight() * 0.3f);
                if (i == 3)
                {
                    p.startNewSubPath (a.getX(), a.getY() + a.getHeight() * 0.3f);
                    p.lineTo (a.getX() + a.getWidth() * 0.4f, a.getBottom());
                    p.lineTo (a.getRight(), a.getY());
                }
                else
                {
                    const float bend = values[i] * 1.2f;
                    p.startNewSubPath (a.getX(), a.getBottom());
                    p.quadraticTo (a.getCentreX() + bend * a.getWidth() * 0.5f, a.getCentreY() - bend * a.getHeight() * 0.5f, a.getRight(), a.getY());
                }
                g.setColour (sel ? glow : text.withAlpha (0.85f)); g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            static const float values[kCount] = { -0.8f, -0.35f, 0.0f, 0.6f };
            for (int i = 0; i < kCount; ++i) if (slot (i).contains (e.getPosition())) attach.setValueAsCompleteGesture (values[i]);
        }
    private:
        juce::Rectangle<int> slot (int i) const { const int w = getWidth() / kCount; return { i * w, 0, w, getHeight() }; }
        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attach;
    };

    class AmpV7 : public V7Panel
    {
    public:
        explicit AmpV7 (DivinityProcessor& p) : V7Panel ("AMP (ENVELOPE)"), apvts (p.apvts), display (p.apvts, 0), curves (p.apvts, 0)
        {
            titleX = 40;
            glyph = StarGlyph;
            auto& s = p.apvts;
            for (auto [id, cap] : { std::pair { "attack", "A" }, { "decay", "D" }, { "sustain", "S" }, { "release", "R" } })
                { knobs.add (makeKnob (s, ids::env (0, id), cap)); addAndMakeVisible (knobs.getLast()); }
            addAndMakeVisible (display);
            addAndMakeVisible (curves);
            auto* open = new IconButton ("Open the full envelope editor (ENV 1, with CURVE)", resetGlyph());
            open->setPath ([] { juce::Path q; q.addCentredArc (11, 13, 7, 7, 0.0f, -2.6f, 2.0f, true); q.startNewSubPath (15, 4); q.lineTo (20, 4); q.lineTo (20, 9); q.startNewSubPath (20, 4); q.lineTo (13, 11); return q; }());
            open->onClick = [this] { if (onOpenEditor) onOpenEditor(); };
            auto* reset = new IconButton ("Reset the amp envelope", gridGlyph(), true);
            reset->onClick = [this]
            {
                for (auto* n : { "attack", "decay", "sustain", "release", "curve" })
                    if (auto* prm = apvts.getParameter (ids::env (0, n))) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
            };
            for (auto* b : { open, reset }) { header.add (b); addAndMakeVisible (b); }
        }
        std::function<void()> onOpenEditor;
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            header[0]->setBounds (w - 58, 10, 24, 24);
            header[1]->setBounds (w - 32, 10, 24, 24);
            display.setBounds (12, 41, w - 24, h - 135);
            curves.setBounds (display.getRight() - 120, display.getY() + 8, 112, 26);
            layoutRow ({ knobs[0], knobs[1], knobs[2], knobs[3] }, { 4, h - 90, w - 8, 86 });
        }
    private:
        APVTS& apvts;
        EnvelopeDisplay display;
        CurveButtons curves;
        juce::OwnedArray<Knob> knobs;
        juce::OwnedArray<IconButton> header;
    };

    //==========================================================================
    // LFO controls (used by the LFO panel and the LFO tabs)
    //==========================================================================
    class ShapeButtons : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ShapeButtons (APVTS& s, int lfo) : param (*s.getParameter (ids::lfo (lfo, "shape"))), attach (param, [this] (float) { repaint(); })
        {
            attach.sendInitialUpdate();
            setTooltip ("LFO shape: sine, triangle, square, saw up, saw down, random.");
        }
        void paint (juce::Graphics& g) override
        {
            static const int order[6] = { 0, 2, 1, 4, 5, 3 };       // parameter index per button
            static const int icon[6]  = { 0, 1, 3, 2, 7, 8 };
            const int cur = (int) std::lround (param.convertFrom0to1 (param.getValue()));
            auto box = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff050c11)); g.fillRoundedRectangle (box, 7.0f);
            g.setColour (juce::Colour (0xff22394a)); g.drawRoundedRectangle (box.reduced (0.5f), 7.0f, 1.0f);
            for (int i = 0; i < 6; ++i)
            {
                auto r = slot (i).toFloat();
                const bool sel = order[i] == cur;
                if (sel) drawGlassControl (g, r.reduced (3.0f, 3.0f), true, false, 5.0f);
                auto ic = wavePath (icon[i], r.reduced (r.getWidth() * 0.27f, r.getHeight() * 0.33f));
                if (sel) strokeGlow (g, ic, 1.5f, glow);
                else { g.setColour (text.withAlpha (0.85f)); g.strokePath (ic, juce::PathStrokeType (1.4f)); }
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            static const int order[6] = { 0, 2, 1, 4, 5, 3 };
            for (int i = 0; i < 6; ++i) if (slot (i).contains (e.getPosition())) attach.setValueAsCompleteGesture ((float) order[i]);
        }
    private:
        juce::Rectangle<int> slot (int i) const { const int w = (getWidth() - 4) / 6; return { 2 + i * w, 0, w, getHeight() }; }
        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attach;
    };

    struct LfoBlock
    {
        LfoBlock (DivinityProcessor& p, int l) : power (p.apvts, ids::lfo (l, "on")), shapes (p.apvts, l), display (p, l), division (p.apvts, ids::lfo (l, "div"))
        {
            auto& s = p.apvts;
            power.glyph = 1;
            power.setTooltip (helpTextFor (ids::lfo (l, "on")));
            division.getProperties().set ("centred", true);
            route.setTextWhenNothingSelected ("Route");
            route.setTooltip ("Send this LFO somewhere: picks a destination and adds it to the next free mod-matrix slot (and switches the LFO on).");
            if (auto* dst = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (ids::mod (0, "dst"))))
                for (int i = 1; i < dst->choices.size(); ++i) route.addItem (dst->choices[i], i);
            route.onChange = [this, &s, l]
            {
                const int r = route.getSelectedId();
                route.setSelectedId (0, juce::dontSendNotification);
                if (r <= 0) return;
                for (int slot = 0; slot < kNumModSlots; ++slot)
                    if (s.getRawParameterValue (ids::mod (slot, "src"))->load() < 0.5f)
                    {
                        auto set = [&s] (const juce::String& id, float real) { if (auto* q = s.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (real)); q->endChangeGesture(); } };
                        auto* src = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (ids::mod (slot, "src")));
                        set (ids::mod (slot, "src"), (float) src->choices.indexOf ("LFO " + juce::String (l + 1)));
                        set (ids::mod (slot, "dst"), (float) r);
                        set (ids::mod (slot, "amt"), 0.25f);
                        set (ids::lfo (l, "on"), 1.0f);
                        return;
                    }
            };
            sync.setButtonText ("SYNC");
            bindToggle (sync, s, ids::lfo (l, "sync"), attach);
            knobs.add (makeKnob (s, ids::lfo (l, "rate"), "Rate", (ModDest) ((int) ModDest::Lfo1Rate + l), 0.3f));
            knobs.add (makeKnob (s, ids::lfo (l, "depth"), "Amount"));
            knobs.add (makeKnob (s, ids::lfo (l, "phase"), "Phase"));
            knobs.add (makeKnob (s, ids::lfo (l, "smooth"), "Smooth"));
            sync.onStateChange = [this]
            {
                const bool on = sync.getToggleState();
                knobs[0]->setAlpha (on ? 0.35f : 1.0f);
                division.setEnabled (on); division.setAlpha (on ? 1.0f : 0.45f);
            };
            sync.onStateChange();
        }
        void addTo (juce::Component& parent)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &power, &shapes, &display, &sync, &division, &route }) parent.addChildComponent (c);
            for (auto* k : knobs) parent.addChildComponent (k);
        }
        void setVisible (bool v)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &power, &shapes, &display, &sync, &division, &route }) c->setVisible (v);
            for (auto* k : knobs) k->setVisible (v);
        }
        PowerButton power;
        ShapeButtons shapes;
        LfoDisplay display;
        juce::TextButton sync;
        juce::ComboBox route;
        ChoiceBox division;
        juce::OwnedArray<Knob> knobs;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> attach;
    };

    class LfoV7 : public V7Panel
    {
    public:
        explicit LfoV7 (DivinityProcessor& p) : V7Panel ("LFO 1")
        {
            titleX = 40;
            for (int l = 0; l < kNumLfos; ++l) { blocks.add (new LfoBlock (p, l)); blocks.getLast()->addTo (*this); }
            juce::Path win; win.addRoundedRectangle (3.0f, 3.0f, 8.0f, 8.0f, 1.5f); win.addRoundedRectangle (13.0f, 3.0f, 8.0f, 8.0f, 1.5f);
            win.addRoundedRectangle (3.0f, 13.0f, 8.0f, 8.0f, 1.5f); win.addRoundedRectangle (13.0f, 13.0f, 8.0f, 8.0f, 1.5f);
            auto* pick = headerButton (header, *this, "Choose which LFO to edit here (1-4)", win);
            pick->onClick = [this, pick]
            {
                juce::PopupMenu m;
                for (int l = 0; l < kNumLfos; ++l) m.addItem (l + 1, "LFO " + juce::String (l + 1), true, l == current);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (pick), [this] (int r) { if (r > 0) select (r - 1); });
            };
            auto* more = headerButton (header, *this, "Open this LFO in the editor below", gridGlyph(), true);
            more->onClick = [this] { if (onOpenEditor) onOpenEditor (current); };
            select (0);
        }
        std::function<void (int)> onOpenEditor;
        void select (int l)
        {
            current = l;
            for (int i = 0; i < blocks.size(); ++i) blocks[i]->setVisible (i == l);
            setTitle ("LFO " + juce::String (l + 1));
            resized();
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            header[0]->setBounds (w - 64, 9, 26, 24);
            header[1]->setBounds (w - 34, 9, 26, 24);
            for (auto* b : blocks)
            {
                b->power.setBounds (11, 9, 26, 26);
                b->shapes.setBounds (12, 41, w - 24, 30);
                b->display.setBounds (12, 77, w - 24, h - 213);
                const int y = h - 128, cw = (w - 24 - 12) / 3;
                b->route.setBounds (12, y, cw - 8, 28);
                b->sync.setBounds (12 + cw - 2, y, cw + 4, 28);
                b->division.setBounds (12 + cw * 2 + 8, y, w - 24 - cw * 2 - 8, 28);
                layoutRow ({ b->knobs[0], b->knobs[1], b->knobs[2], b->knobs[3] }, { 4, h - 90, w - 8, 86 });
            }
        }
    private:
        juce::OwnedArray<IconButton> header;
        juce::OwnedArray<LfoBlock> blocks;
        int current = 0;
    };

    //==========================================================================
    // MACROS (8)
    //==========================================================================
    class MacrosV7 : public V7Panel
    {
    public:
        explicit MacrosV7 (DivinityProcessor& p) : V7Panel ("MACROS"), apvts (p.apvts), reset ("Macro options", dots3(), true), add ("Assign a macro in the mod matrix", plusPath())
        {
            reset.boxed = true; add.boxed = true;
            add.onClick = [this] { if (onAssign) onAssign(); };
            addAndMakeVisible (add);
            for (int i = 0; i < kNumMacros; ++i)
            {
                auto* s = sliders.add (new FineSlider());
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s->setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
                s->setDoubleClickReturnValue (true, 0.0);
                s->setTooltip (helpTextFor (ids::macro (i)));
                auto* prm = p.apvts.getParameter (ids::macro (i));
                s->onPopup = [s, prm] { showParameterMenu (*s, *prm); };
                s->onValueChange = [this] { repaint(); };
                attach.add (new APVTS::SliderAttachment (p.apvts, ids::macro (i), *s));
                addAndMakeVisible (s);
            }
            reset.onClick = [this]
            {
                juce::PopupMenu m;
                m.addItem (1, "Reset all macros to 0%");
                m.addItem (2, "Assign a macro (mod matrix)");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&reset), [this] (int r)
                {
                    if (r == 1) for (int i = 0; i < kNumMacros; ++i) if (auto* p = apvts.getParameter (ids::macro (i))) p->setValueNotifyingHost (0.0f);
                    if (r == 2 && onAssign) onAssign();
                });
            };
            addAndMakeVisible (reset);
        }
        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            static const char* names[] = { "TONE", "MOVEMENT", "TEXTURE", "SPACE", "CHARACTER", "WIDTH", "PUNCH", "AIR" };
            for (int i = 0; i < kNumMacros; ++i)
            {
                auto r = row (i).toFloat();
                drawGlassControl (g, r, false, sliders[i]->isMouseOverOrDragging(), 6.0f);
                g.setColour (text);
                g.setFont (font (15.0f, Weight::Medium));
                g.drawText (juce::String (i + 1), (int) r.getX() + 10, (int) r.getY(), 16, (int) r.getHeight(), juce::Justification::centredLeft);
                g.setFont (font (13.0f, Weight::Medium, 0.03f));
                g.drawText (names[i], (int) r.getX() + 92, (int) r.getY(), (int) r.getWidth() - 96, (int) r.getHeight(), juce::Justification::centredLeft);
                if (sliders[i]->isMouseOverOrDragging())
                {
                    g.setColour (accent);
                    g.drawText (juce::String (juce::roundToInt (sliders[i]->getValue() * 100.0)) + "%", r.toNearestInt().reduced (8, 0), juce::Justification::centredRight);
                }
            }
        }
        void resized() override
        {
            reset.setBounds (getWidth() - 36, 9, 26, 24);
            add.setBounds (getWidth() - 66, 9, 26, 24);
            for (int i = 0; i < kNumMacros; ++i) { auto r = row (i); sliders[i]->setBounds (juce::Rectangle<int> (44, 44).withCentre ({ r.getX() + 54, r.getCentreY() })); }
        }
        std::function<void()> onAssign;
    private:
        juce::Rectangle<int> row (int i) const { const int h = (getHeight() - 48) / kNumMacros; return { 9, 41 + i * h, getWidth() - 18, h - 6 }; }
        APVTS& apvts;
        juce::OwnedArray<FineSlider> sliders;
        juce::OwnedArray<APVTS::SliderAttachment> attach;
        IconButton reset, add;
    };

    //==========================================================================
    // STEP sequencer editor
    //==========================================================================
    class StepEditor : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit StepEditor (DivinityProcessor& p) : proc (p)
        {
            setTooltip ("Drag the bars to set each step's level (0 = rest). Lanes below: click RATCHET to cycle 1-4 hits (trap rolls), "
                        "drag GATE for note length, drag PITCH for semitones. Right-click a bar to reset it.");
            startTimerHz (20);
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawDisplayBackground (g, r);
            const int len = (int) proc.apvts.getRawParameterValue (ids::stepLength)->load();
            const int playing = proc.getStepIndex();
            const auto bars = barArea();
            const auto rat = lane (0), gate = lane (1), pitch = lane (2);
            g.setFont (font (9.5f, Weight::Medium, 0.08f));
            g.setColour (textFaint);
            g.drawText ("RATCHET", 4, rat.getY(), 52, rat.getHeight(), juce::Justification::centredLeft);
            g.drawText ("GATE", 4, gate.getY(), 52, gate.getHeight(), juce::Justification::centredLeft);
            g.drawText ("PITCH", 4, pitch.getY(), 52, pitch.getHeight(), juce::Justification::centredLeft);
            for (int s = 0; s < kNumSteps; ++s)
            {
                const bool active = s < len;
                const float lvl = value (s, "lvl"), gl = value (s, "len");
                const int rt = (int) value (s, "rat"), pt = (int) value (s, "pitch");
                auto col = cell (bars, s).reduced (2.0f, 0.0f);
                g.setColour (panel2.withAlpha (active ? 0.6f : 0.25f)); g.fillRoundedRectangle (col, 3.0f);
                auto bar = col.withTop (col.getBottom() - col.getHeight() * lvl);
                const bool now = s == playing;
                g.setGradientFill (juce::ColourGradient ((now ? text : accent).withAlpha (active ? 1.0f : 0.3f), 0, bar.getY(),
                                                         accentDeep.withAlpha (active ? 1.0f : 0.3f), 0, col.getBottom(), false));
                g.fillRoundedRectangle (bar, 3.0f);
                if (s % 4 == 0) { g.setColour (border); g.drawVerticalLine ((int) col.getX() - 2, bars.getY(), (float) pitch.getBottom()); }
                // ratchet: dots
                auto rc = cell (rat.toFloat(), s).reduced (3.0f, 3.0f);
                for (int k = 0; k < rt; ++k)
                {
                    const float dw = rc.getWidth() / 4.0f;
                    g.setColour (active ? accent : textFaint);
                    g.fillEllipse (rc.getX() + k * dw + dw * 0.2f, rc.getCentreY() - dw * 0.3f, dw * 0.6f, dw * 0.6f);
                }
                // gate: horizontal fill
                auto gc = cell (gate.toFloat(), s).reduced (3.0f, 5.0f);
                g.setColour (panel2); g.fillRect (gc);
                g.setColour (active ? accentStrong : textFaint); g.fillRect (gc.withWidth (gc.getWidth() * gl));
                // pitch: number
                g.setColour (pt != 0 ? text : textFaint);
                g.setFont (font (10.0f, Weight::Medium));
                g.drawText (pt > 0 ? "+" + juce::String (pt) : juce::String (pt), cell (pitch.toFloat(), s).toNearestInt(), juce::Justification::centred);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            dragLane = laneAt (e.getPosition());
            dragStartY = e.position.y;
            const int s = stepAt (e.position.x);
            if (s < 0) return;
            dragStartValue = value (s, dragLane == 3 ? "pitch" : "len");
            if (e.mods.isPopupMenu())
            {
                set (s, "lvl", 0.6f); set (s, "rat", 1.0f); set (s, "len", 0.5f); set (s, "pitch", 0.0f);
                return;
            }
            if (dragLane == 1) { set (s, "rat", (float) ((int) value (s, "rat") % 4 + 1)); return; }
            mouseDrag (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int s = stepAt (e.position.x);
            if (s < 0) return;
            if (dragLane == 0)
            {
                const auto b = barArea();
                set (s, "lvl", juce::jlimit (0.0f, 1.0f, (b.getBottom() - e.position.y) / b.getHeight()));
            }
            else if (dragLane == 2)
            {
                const auto c = cell (lane (1).toFloat(), s);
                set (s, "len", juce::jlimit (0.05f, 1.0f, (e.position.x - c.getX()) / c.getWidth()));
            }
            else if (dragLane == 3)
                set (s, "pitch", (float) juce::jlimit (-24, 24, (int) std::round (dragStartValue + (dragStartY - e.position.y) / 6.0f)));
        }
    private:
        juce::Rectangle<float> barArea() const { return { 58.0f, 6.0f, (float) getWidth() - 64.0f, (float) getHeight() * 0.55f }; }
        juce::Rectangle<int> lane (int i) const
        {
            const auto b = barArea();
            const int top = (int) b.getBottom() + 4, h = (getHeight() - top - 4) / 3;
            return { (int) b.getX(), top + i * h, (int) b.getWidth(), h };
        }
        juce::Rectangle<float> cell (juce::Rectangle<float> area, int s) const
        {
            const float w = area.getWidth() / kNumSteps;
            return { area.getX() + w * s, area.getY(), w, area.getHeight() };
        }
        int stepAt (float x) const
        {
            const auto b = barArea();
            const int s = (int) std::floor ((x - b.getX()) / (b.getWidth() / kNumSteps));
            return juce::isPositiveAndBelow (s, kNumSteps) ? s : -1;
        }
        int laneAt (juce::Point<int> p) const
        {
            if (p.y < lane (0).getY()) return 0;
            for (int i = 0; i < 3; ++i) if (lane (i).contains (p.withX (lane (i).getX() + 1))) return i + 1;
            return 0;
        }
        float value (int s, const char* name) const { return proc.apvts.getRawParameterValue (ids::step (s, name))->load(); }
        void set (int s, const char* name, float v)
        {
            if (auto* p = proc.apvts.getParameter (ids::step (s, name)))
            {
                p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture();
            }
            repaint();
        }
        void timerCallback() override
        {
            float h = (float) proc.getStepIndex();
            for (int s = 0; s < kNumSteps; ++s) h = h * 1.13f + value (s, "lvl") + value (s, "rat") * 3.1f + value (s, "len") * 7.3f + value (s, "pitch") * 0.37f;
            h += proc.apvts.getRawParameterValue (ids::stepLength)->load() * 11.0f;
            if (h != lastHash) { lastHash = h; repaint(); }
        }
        DivinityProcessor& proc;
        int dragLane = 0;
        float dragStartY = 0, dragStartValue = 0, lastHash = -1.0f;
    };

    class RandomView : public juce::Component, private juce::Timer
    {
    public:
        explicit RandomView (DivinityProcessor& p) : proc (p) { history.assign (160, 0.0f); startTimerHz (30); setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawDisplayBackground (g, r);
            r = r.reduced (8.0f, 10.0f);
            juce::Path p;
            for (size_t i = 0; i < history.size(); ++i)
            {
                const float v = history[(i + pos) % history.size()];
                const float x = r.getX() + r.getWidth() * (float) i / (float) (history.size() - 1), y = r.getCentreY() - v * r.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (accent.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (4.0f));
            g.setColour (accent); g.strokePath (p, juce::PathStrokeType (1.5f));
        }
    private:
        void timerCallback() override
        {
            if (! isShowing()) return;
            history[pos] = proc.getRandomValue(); pos = (pos + 1) % history.size(); repaint();
        }
        DivinityProcessor& proc;
        std::vector<float> history;
        size_t pos = 0;
    };

    //==========================================================================
    // Modulation tabs: ENV 1-4 | LFO 1-4 | STEP | RANDOM | MATRIX
    //==========================================================================
    struct MatrixRow
    {
        MatrixRow (APVTS& s, int i) : src (s, ids::mod (i, "src")), dst (s, ids::mod (i, "dst")), clear ("Clear this route", crossPath())
        {
            amt.setSliderStyle (juce::Slider::LinearHorizontal);
            amt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            amt.setDoubleClickReturnValue (true, 0.0);
            amt.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            amt.setTooltip ("Modulation amount. Double-click to zero; right-click to type a value.");
            auto* prm = s.getParameter (ids::mod (i, "amt"));
            amt.onPopup = [this, prm] { showParameterMenu (amt, *prm); };
            attach = std::make_unique<APVTS::SliderAttachment> (s, ids::mod (i, "amt"), amt);
            clear.onClick = [&s, i]
            {
                for (auto* n : { "src", "dst", "amt" })
                    if (auto* p = s.getParameter (ids::mod (i, n))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
            };
        }
        void addTo (juce::Component& parent) { for (auto* c : std::initializer_list<juce::Component*> { &src, &dst, &amt, &clear }) parent.addChildComponent (c); }
        void setVisible (bool v) { for (auto* c : std::initializer_list<juce::Component*> { &src, &dst, &amt, &clear }) c->setVisible (v); }
        ChoiceBox src, dst;
        FineSlider amt;
        IconButton clear;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };

    class ModTabsV7 : public V7Panel
    {
    public:
        explicit ModTabsV7 (DivinityProcessor& p)
            : V7Panel (""), proc (p),
              envTabs ({ "ENV 1", "ENV 2", "ENV 3", "ENV 4" }), lfoTabs ({ "LFO 1", "LFO 2", "LFO 3", "LFO 4", "STEP", "RANDOM" }), mtxTabs ({ "MATRIX" }),
              envDisplay (p.apvts, 0), stepEditor (p), randomView (p),
              stepPower (p.apvts, ids::stepOn),
              stepMode (p.apvts, ids::stepMode, { "MODULATE", "TRIGGER" }),
              stepRate (p.apvts, ids::stepRate), stepLen (p.apvts, ids::stepLength), rndDiv (p.apvts, ids::rndDiv)
        {
            auto& s = p.apvts;
            for (auto* t : { &envTabs, &lfoTabs, &mtxTabs }) addAndMakeVisible (t);
            addChildComponent (envDisplay);
            for (int e = 0; e < kNumEnvs; ++e) { envCurves.add (new CurveButtons (s, e)); addChildComponent (envCurves.getLast()); }
            {
                juce::Path lp; lp.addRoundedRectangle (2.0f, 9.0f, 11.0f, 7.0f, 3.5f); lp.addRoundedRectangle (11.0f, 9.0f, 11.0f, 7.0f, 3.5f);
                envTools.add (new IconButton ("Copy the AMP envelope (ENV 1) into this envelope", lp));
                envTools.add (new IconButton ("x2: double this envelope's attack, decay and release times", juce::Path()));
                envTools.add (new IconButton ("Reset this envelope", resetGlyph()));
                envTools[0]->onClick = [this] { copyEnv (0, current); };
                envTools[1]->onClick = [this] { scaleEnvTimes (current, 2.0f); };
                envTools[2]->onClick = [this]
                {
                    for (auto* n : { "attack", "decay", "sustain", "release", "curve" })
                        if (auto* prm = proc.apvts.getParameter (ids::env (current, n))) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
                };
                for (auto* b : envTools) { b->boxed = true; addChildComponent (b); }
            }
            for (int e = 0; e < kNumEnvs; ++e)
            {
                for (auto [id, cap] : { std::pair { "attack", "A" }, { "decay", "D" }, { "sustain", "S" }, { "release", "R" }, { "curve", "Curve" } })
                {
                    envKnobs[e].add (makeKnob (s, ids::env (e, id), cap, ModDest::None, 1.0f, juce::String (id) == "curve"));
                    envKnobs[e].getLast()->setCaptionSize (14.0f);
                }
                for (auto* k : envKnobs[e]) addChildComponent (k);
            }
            for (int l = 0; l < kNumLfos; ++l) { lfos.add (new LfoBlock (p, l)); lfos.getLast()->addTo (*this); }
            // STEP
            stepPower.setTooltip (helpTextFor (ids::stepOn));
            stepRate.setTooltip (helpTextFor (ids::stepRate));
            stepLen.getSlider().setTooltip (helpTextFor (ids::stepLength));
            stepKnobs.add (makeKnob (s, ids::stepSwing, "Swing"));
            stepKnobs.add (makeKnob (s, ids::stepSmooth, "Smooth"));
            for (auto* c : std::initializer_list<juce::Component*> { &stepEditor, &stepPower, &stepMode, &stepRate, &stepLen }) addChildComponent (c);
            for (auto* k : stepKnobs) addChildComponent (k);
            // RANDOM
            rndSync.setButtonText ("SYNC");
            bindToggle (rndSync, s, ids::rndSync, attach);
            rndKnobs.add (makeKnob (s, ids::rndRate, "Rate"));
            rndKnobs.add (makeKnob (s, ids::rndSmooth, "Smooth"));
            for (auto* c : std::initializer_list<juce::Component*> { &randomView, &rndSync, &rndDiv }) addChildComponent (c);
            for (auto* k : rndKnobs) addChildComponent (k);
            // MATRIX (all 8 slots)
            for (int i = 0; i < kNumModSlots; ++i) { rows.add (new MatrixRow (s, i)); rows.getLast()->addTo (*this); }
            envTabs.onSelect = [this] (int i) { select (i); };
            lfoTabs.onSelect = [this] (int i) { select (4 + i); };
            mtxTabs.onSelect = [this] (int) { select (10); };
            select (0);
        }
        void select (int t)
        {
            current = t;
            const bool env = t < 4, lfo = t >= 4 && t < 8, step = t == 8, rnd = t == 9, matrix = t == 10;
            envDisplay.setVisible (env);
            if (env) envDisplay.setEnvelope (t);
            for (int e = 0; e < envCurves.size(); ++e) envCurves[e]->setVisible (env && e == t);
            for (auto* b : envTools) b->setVisible (env);
            for (int e = 0; e < kNumEnvs; ++e) for (auto* k : envKnobs[e]) k->setVisible (env && e == t);
            for (int l = 0; l < kNumLfos; ++l) lfos[l]->setVisible (lfo && l == t - 4);
            for (auto* c : std::initializer_list<juce::Component*> { &stepEditor, &stepPower, &stepMode, &stepRate, &stepLen }) c->setVisible (step);
            for (auto* k : stepKnobs) k->setVisible (step);
            for (auto* c : std::initializer_list<juce::Component*> { &randomView, &rndSync, &rndDiv }) c->setVisible (rnd);
            for (auto* k : rndKnobs) k->setVisible (rnd);
            for (auto* r : rows) r->setVisible (matrix);
            envTabs.setSelected (env ? t : -1, false);
            lfoTabs.setSelected (t >= 4 && t < 10 ? t - 4 : -1, false);
            mtxTabs.setSelected (matrix ? 0 : -1, false);
            resized(); repaint();
        }
        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            if (current < 4) drawPanelBackground (g, knobBox().toFloat());
            g.setColour (textDim);
            g.setFont (font (10.5f, Weight::Medium, 0.08f));
            if (current == 8)
            {
                g.drawText ("RATE", stepRate.getX(), stepRate.getY() - 15, 80, 13, juce::Justification::centredLeft);
                g.drawText ("STEPS", stepLen.getX(), stepLen.getY() - 15, 80, 13, juce::Justification::centredLeft);
            }
            if (current == 10)
            {
                g.setFont (font (11.0f, Weight::Medium));
                for (int i = 0; i < kNumModSlots; ++i)
                {
                    const float v = proc.apvts.getRawParameterValue (ids::mod (i, "amt"))->load();
                    const int pct = juce::roundToInt (v * 100.0f);
                    auto b = rows[i]->amt.getBounds();
                    g.setColour (pct != 0 ? text : textFaint);
                    g.drawText ((pct > 0 ? "+" : "") + juce::String (pct) + "%", b.getRight() + 2, b.getY() - 2, 40, b.getHeight() + 4, juce::Justification::centredLeft);
                }
            }
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            // three tab groups like the mockup: ENV 1-4 | LFO 1-4 STEP RANDOM | MATRIX
            const float u = (float) (w - 20) / 1180.0f;
            envTabs.setBounds (10, 8, juce::roundToInt (440 * u), 30);
            lfoTabs.setBounds (envTabs.getRight() + 3, 8, juce::roundToInt (636 * u), 30);
            mtxTabs.setBounds (w - 10 - juce::roundToInt (92 * u), 8, juce::roundToInt (92 * u), 30);
            const auto area = juce::Rectangle<int> (12, 46, w - 24, h - 54);
            // ENV
            envDisplay.setBounds (area.withWidth (knobBox().getX() - 12 - area.getX()));
            {
                const auto kb = knobBox().reduced (8, 4);
                const int kw = kb.getWidth() / 5;
                for (int e = 0; e < kNumEnvs; ++e)
                    for (int k = 0; k < 5; ++k) envKnobs[e][k]->setBounds (kb.getX() + k * kw, kb.getY(), kw, kb.getHeight());
                for (auto* c : envCurves) c->setBounds (envDisplay.getRight() - 160, envDisplay.getY() + 8, 140, 28);
                for (int i = 0; i < envTools.size(); ++i) envTools[i]->setBounds (envDisplay.getRight() - 128 + i * 34, envDisplay.getY() + 42, 28, 26);
            }
            // LFO
            for (auto* L : lfos)
            {
                const int dw = (int) (area.getWidth() * 0.4f);
                L->power.setBounds (area.getX(), area.getY(), 24, 24);
                L->shapes.setBounds (area.getX() + 30, area.getY(), dw - 30, 24);
                L->display.setBounds (area.getX(), area.getY() + 30, dw, area.getHeight() - 30);
                const int cx = area.getX() + dw + 12;
                L->sync.setBounds (cx, area.getY() + 6, 100, 26);
                L->division.setBounds (cx, area.getY() + 40, 100, 26);
                L->route.setBounds (cx, area.getY() + 74, 100, 26);
                const int kx = cx + 112, kw = (area.getRight() - kx) / 4;
                for (int k = 0; k < 4; ++k) L->knobs[k]->setBounds (kx + k * kw, area.getY(), kw, area.getHeight());
            }
            // STEP
            {
                const int cw = 170;
                stepEditor.setBounds (area.withTrimmedRight (cw + 12 + 150));
                const int cx = stepEditor.getRight() + 10;
                stepPower.setBounds (cx, area.getY() + 2, 24, 24);
                stepMode.setBounds (cx + 30, area.getY(), cw - 30, 26);
                stepRate.setBounds (cx, area.getY() + 50, 92, 26);
                stepLen.setBounds (cx + 100, area.getY() + 50, 70, 26);
                stepKnobs[0]->setBounds (area.getRight() - 150, area.getY(), 75, area.getHeight());
                stepKnobs[1]->setBounds (area.getRight() - 75, area.getY(), 75, area.getHeight());
            }
            // RANDOM
            {
                randomView.setBounds (area.withWidth ((int) (area.getWidth() * 0.55f)));
                const int cx = randomView.getRight() + 12;
                rndSync.setBounds (cx, area.getY() + 6, 100, 26);
                rndDiv.setBounds (cx, area.getY() + 40, 100, 26);
                const int kx = cx + 112, kw = (area.getRight() - kx) / 2;
                rndKnobs[0]->setBounds (kx, area.getY(), kw, area.getHeight());
                rndKnobs[1]->setBounds (kx + kw, area.getY(), kw, area.getHeight());
            }
            // MATRIX
            {
                const int colW = area.getWidth() / 2, rowH = juce::jmax (22, area.getHeight() / 4);
                for (int i = 0; i < kNumModSlots; ++i)
                {
                    const int col = i / 4, row = i % 4;
                    const int x = area.getX() + col * colW, y = area.getY() + row * rowH;
                    auto& r = *rows[i];
                    r.src.setBounds (x, y, 130, rowH - 4);
                    r.dst.setBounds (x + 136, y, 170, rowH - 4);
                    r.amt.setBounds (x + 312, y + 3, colW - 312 - 76, rowH - 10);
                    r.clear.setBounds (x + colW - 34, y, rowH - 4, rowH - 4);
                }
            }
        }
        void showTab (int t) { select (t); }
        void paintOverChildren (juce::Graphics& g) override
        {
            if (current >= 4) return;
            auto b = envTools[1]->getBounds().toFloat();
            g.setColour (text); g.setFont (font (12.5f, Weight::Medium));
            g.drawText ("x2", b, juce::Justification::centred);
        }
    private:
        juce::Rectangle<int> knobBox() const { return { juce::roundToInt (getWidth() * 0.6375f), 46, getWidth() - 10 - juce::roundToInt (getWidth() * 0.6375f), getHeight() - 54 }; }
        void copyEnv (int from, int to)
        {
            if (from == to) return;
            for (auto* n : { "attack", "decay", "sustain", "release", "curve" })
                if (auto* a = proc.apvts.getParameter (ids::env (from, n))) if (auto* b = proc.apvts.getParameter (ids::env (to, n)))
                    { b->beginChangeGesture(); b->setValueNotifyingHost (a->getValue()); b->endChangeGesture(); }
        }
        void scaleEnvTimes (int e, float factor)
        {
            for (auto* n : { "attack", "decay", "release" })
                if (auto* prm = proc.apvts.getParameter (ids::env (e, n)))
                {
                    const float v = prm->convertFrom0to1 (prm->getValue()) * factor;
                    prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture();
                }
        }
        DivinityProcessor& proc;
        TabRow envTabs, lfoTabs, mtxTabs;
        EnvelopeDisplay envDisplay;
        juce::OwnedArray<CurveButtons> envCurves;
        juce::OwnedArray<IconButton> envTools;
        juce::OwnedArray<Knob> envKnobs[kNumEnvs];
        juce::OwnedArray<LfoBlock> lfos;
        StepEditor stepEditor;
        RandomView randomView;
        PowerButton stepPower;
        SegmentedChoice stepMode;
        ChoiceBox stepRate;
        NumberBox stepLen;
        juce::OwnedArray<Knob> stepKnobs, rndKnobs;
        juce::TextButton rndSync;
        ChoiceBox rndDiv;
        juce::OwnedArray<MatrixRow> rows;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> attach;
        int current = 0;
    };

    //==========================================================================
    // MOD MATRIX (compact list of all 8 slots, scrollable)
    //==========================================================================
    class MatrixV7 : public V7Panel, private juce::Timer
    {
    public:
        explicit MatrixV7 (DivinityProcessor& p)
            : V7Panel ("MOD MATRIX"), proc (p), add ("Add a route (next empty slot)", plusPath()), more ("Open the full matrix", dots3(), true)
        {
            titleX = 16;   // no power glyph: the matrix has no on/off
            add.boxed = true; more.boxed = true;
            inner.rows = &rows;
            for (int i = 0; i < kNumModSlots; ++i) { rows.add (new MatrixRow (p.apvts, i)); rows.getLast()->addTo (inner); }
            viewport.setViewedComponent (&inner, false);
            viewport.setScrollBarsShown (true, false);
            viewport.setScrollBarThickness (6);
            addAndMakeVisible (viewport);
            addAndMakeVisible (add);
            addAndMakeVisible (more);
            add.onClick = [this]
            {
                for (int i = 0; i < kNumModSlots; ++i)
                    if (proc.apvts.getRawParameterValue (ids::mod (i, "src"))->load() < 0.5f)
                    {
                        reveal = i; refreshRows();
                        viewport.setViewPosition (0, rows[i]->src.getY());
                        rows[i]->src.showPopup();
                        return;
                    }
            };
            more.onClick = [this] { if (onOpenFull) onOpenFull(); };
            refreshRows();
            startTimerHz (4);
        }
        std::function<void()> onOpenFull;
        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            g.setColour (text);
            g.setFont (font (11.5f, Weight::Medium, 0.04f));
            const auto c = columns();
            g.drawText ("SRC", c.src.getX(), 40, c.src.getWidth(), 14, juce::Justification::centred);
            g.drawText ("DEST", c.dst.getX(), 40, c.dst.getWidth(), 14, juce::Justification::centred);
            g.drawText ("AMT", c.amt.getX(), 40, c.amt.getWidth(), 14, juce::Justification::centred);
            juce::Path caret; caret.startNewSubPath (19, 46); caret.lineTo (22, 43); caret.lineTo (25, 46);
            g.setColour (textFaint); g.strokePath (caret, juce::PathStrokeType (1.2f));
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            add.setBounds (w - 66, 9, 26, 24);
            more.setBounds (w - 36, 9, 26, 24);
            viewport.setBounds (8, 58, w - 16, h - 64);
            layoutRows();
        }
    private:
        void refreshRows()
        {
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const bool used = proc.apvts.getRawParameterValue (ids::mod (i, "src"))->load() > 0.5f
                               || proc.apvts.getRawParameterValue (ids::mod (i, "dst"))->load() > 0.5f;
                rows[i]->setVisible (used || i == reveal);
            }
            layoutRows();
        }
        void layoutRows()
        {
            const int w = viewport.getWidth() - 4, rowH = 32;
            const auto c = columns();
            int y = 0;
            for (auto* r : rows)
            {
                if (! r->src.isVisible()) continue;
                r->src.setBounds (c.src.getX() - 8, y, c.src.getWidth(), 26);
                r->dst.setBounds (c.dst.getX() - 8, y, c.dst.getWidth(), 26);
                r->amt.setBounds (c.amt.getX() - 8, y + 3, c.amt.getWidth(), 20);
                r->clear.setBounds (c.amt.getRight() - 8 + 6, y + 5, 16, 16);
                y += rowH;
            }
            inner.setSize (w, juce::jmax (y, 10));
        }
        void timerCallback() override
        {
            int mask = 0;
            for (int i = 0; i < kNumModSlots; ++i)
                if (proc.apvts.getRawParameterValue (ids::mod (i, "src"))->load() > 0.5f || proc.apvts.getRawParameterValue (ids::mod (i, "dst"))->load() > 0.5f) mask |= 1 << i;
            if (mask != lastMask) { lastMask = mask; if (reveal >= 0 && (mask & (1 << reveal))) reveal = -1; refreshRows(); }
        }
        struct Cols { juce::Rectangle<int> src, dst, amt; };
        Cols columns() const
        {
            // proportions from the mockup (423 px wide panel)
            const float u = (float) getWidth() / 423.0f;
            auto col = [u] (int x, int w) { return juce::Rectangle<int> (juce::roundToInt (x * u), 0, juce::roundToInt (w * u), 1); };
            return { col (38, 104), col (157, 143), col (313, 66) };
        }
        struct Inner : juce::Component
        {
            juce::OwnedArray<MatrixRow>* rows = nullptr;
            void paint (juce::Graphics& g) override
            {
                if (rows == nullptr) return;
                for (auto* r : *rows)
                    if (r->src.isVisible())
                    {
                        auto grip = gripPath();
                        grip.applyTransform (grip.getTransformToScaleToFit ({ 4.0f, (float) r->src.getY() + 6.0f, 12.0f, 14.0f }, true));
                        g.setColour (textDim); g.fillPath (grip);
                    }
            }
        };
        DivinityProcessor& proc;
        juce::Viewport viewport;
        Inner inner;
        juce::OwnedArray<MatrixRow> rows;
        IconButton add, more;
        int reveal = -1, lastMask = -1;
    };

    //==========================================================================
    // EFFECTS CHAIN (enabled modules, in chain order; drag to reorder)
    //==========================================================================
    class FxChainV7 : public V7Panel, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit FxChainV7 (DivinityProcessor& p) : V7Panel ("EFFECTS CHAIN"), proc (p), add ("Add an effect", plusPath())
        {
            titleX = 42;
            glyph = GearGlyph;
            add.boxed = true;
            setTooltip ("The effects that are switched on, in processing order. Drag a row to reorder, click the power icon to bypass, "
                        "\"...\" opens it on the EFFECTS tab. + adds an effect.");
            add.onClick = [this]
            {
                juce::PopupMenu m;
                const auto& c = fxCatalog();
                for (int i = 0; i < (int) c.size(); ++i)
                    if (proc.apvts.getRawParameterValue (c[(size_t) i].onID)->load() < 0.5f) m.addItem (i + 1, c[(size_t) i].name);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add), [this] (int r)
                {
                    if (r <= 0) return;
                    if (auto* prm = proc.apvts.getParameter (fxCatalog()[(size_t) (r - 1)].onID)) prm->setValueNotifyingHost (1.0f);
                });
            };
            addAndMakeVisible (add);
            startTimerHz (5);
        }
        std::function<void()> onOpenEffects;
        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            g.setFont (font (14.0f, Weight::Medium));
            if (items.isEmpty())
            {
                g.setColour (textFaint);
                g.drawFittedText ("No effects on. Press + to add one.", 14, 46, getWidth() - 28, 40, juce::Justification::topLeft, 2);
            }
            for (int i = 0; i < items.size(); ++i)
            {
                auto r = row (i).toFloat();
                const bool on = proc.apvts.getRawParameterValue (fxCatalog()[(size_t) items[i]].onID)->load() > 0.5f;
                const bool sel = i == dragIndex || i == selectedRow;
                auto grip = gripPath(); grip.applyTransform (grip.getTransformToScaleToFit ({ (float) getX() * 0.0f + 6.0f, r.getY() + 6.0f, 12.0f, r.getHeight() - 12.0f }, true));
                g.setColour (textDim); g.fillPath (grip);
                drawGlassControl (g, r, sel, false, 6.0f);
                auto pw = icons::power (powerBounds (i));
                if (on) strokeGlow (g, pw, 1.4f); else { g.setColour (textFaint); g.strokePath (pw, juce::PathStrokeType (1.4f)); }
                g.setColour (on ? text : textDim);
                g.drawText (fxCatalog()[(size_t) items[i]].name, (int) r.getX() + 38, (int) r.getY(), (int) r.getWidth() - 68, (int) r.getHeight(), juce::Justification::centredLeft);
                auto d = dots3(); d.applyTransform (d.getTransformToScaleToFit (moreBounds (i).reduced (2.0f, 0.0f), true));
                g.setColour (text); g.fillPath (d);
            }
        }
        void resized() override { add.setBounds (getWidth() - 36, 9, 26, 24); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            dragIndex = -1;
            for (int i = 0; i < items.size(); ++i)
            {
                if (powerBounds (i).expanded (4.0f).contains (e.position))
                {
                    if (auto* prm = proc.apvts.getParameter (fxCatalog()[(size_t) items[i]].onID)) prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
                    return;
                }
                if (moreBounds (i).expanded (4.0f).contains (e.position)) { if (onOpenEffects) onOpenEffects(); return; }
                if (row (i).contains (e.getPosition()) || (e.x < 22 && e.y >= row (i).getY() && e.y < row (i).getBottom())) { dragIndex = i; selectedRow = i; repaint(); return; }
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragIndex < 0) return;
            for (int i = 0; i < items.size(); ++i)
                if (i != dragIndex && row (i).contains (e.getPosition()))
                {
                    // swap the two modules inside the full chain order
                    auto order = proc.getFxOrder();
                    const int a = order.indexOf (fxCatalog()[(size_t) items[dragIndex]].key), b = order.indexOf (fxCatalog()[(size_t) items[i]].key);
                    if (a >= 0 && b >= 0) { order.move (a, b); proc.setFxOrder (order); }
                    rebuild(); dragIndex = i; repaint();
                    return;
                }
        }
        void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; repaint(); }
    private:
        juce::Rectangle<int> row (int i) const { return { 24, 39 + i * 30, getWidth() - 36, 26 }; }
        juce::Rectangle<float> powerBounds (int i) const { auto r = row (i).toFloat(); return { r.getX() + 9.0f, r.getY() + 4.0f, 18.0f, 18.0f }; }
        juce::Rectangle<float> moreBounds (int i) const { auto r = row (i).toFloat(); return { r.getRight() - 26.0f, r.getY() + 2.0f, 22.0f, r.getHeight() - 4.0f }; }
        void rebuild()
        {
            items.clearQuick();
            const int maxRows = juce::jmax (1, (getHeight() - 42) / 30);
            for (auto& k : proc.getFxOrder())
            {
                const int idx = fxIndexForKey (k);
                if (idx >= 0 && proc.apvts.getRawParameterValue (fxCatalog()[(size_t) idx].onID)->load() > 0.5f && items.size() < maxRows) items.add (idx);
            }
        }
        void timerCallback() override
        {
            if (dragIndex >= 0) return;
            auto before = items;
            rebuild();
            if (before != items) repaint();
        }
        DivinityProcessor& proc;
        IconButton add;
        juce::Array<int> items;
        int dragIndex = -1, selectedRow = -1;
    };

    //==========================================================================
    // OUTPUT SPECTRUM
    //==========================================================================
    class SpectrumV7 : public V7Panel
    {
    public:
        explicit SpectrumV7 (DivinityProcessor& p) : V7Panel ("OUTPUT SPECTRUM"), analyzer (p), meter (p, true)
        {
            titleX = 16;   // no power glyph: the analyzer has no on/off
            const char* names[] = { "PRE", "POST", "MIX" };
            for (int i = 0; i < 3; ++i)
            {
                auto* b = modes.add (new juce::TextButton (names[i]));
                b->setClickingTogglesState (false);
                b->getProperties().set ("fontSize", 11.5);
                b->setTooltip (i == 0 ? "PRE: the raw voices, before the effects." : i == 1 ? "POST: what you hear, after the effects." : "MIX: both - post filled, pre as a line.");
                b->onClick = [this, i] { setMode (i == 0 ? 1 : i == 1 ? 0 : 2); };
                addAndMakeVisible (b);
            }
            addAndMakeVisible (analyzer);
            addAndMakeVisible (meter);
            setMode (0);
        }
        void setMode (int m)
        {
            analyzer.setMode (m);
            modes[0]->setToggleState (m == 1, juce::dontSendNotification);
            modes[1]->setToggleState (m == 0, juce::dontSendNotification);
            modes[2]->setToggleState (m == 2, juce::dontSendNotification);
        }
        void paint (juce::Graphics& g) override
        {
            V7Panel::paint (g);
            g.setColour (textFaint);
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            for (int i = 0; i < 3; ++i) modes[i]->setBounds (w - 182 + i * 48, 10, 44, 22);
            analyzer.setBounds (12, 40, w - 50, h - 50);
            meter.setBounds (w - 30, 40, 18, h - 50);
        }
    private:
        juce::OwnedArray<juce::TextButton> modes;
        SpectrumAnalyzer analyzer;
        LevelMeter meter;
    };
}

//==============================================================================
// CreatePage
//==============================================================================
struct CreatePage::Impl
{
    Impl (DivinityProcessor& p, CreatePage& owner)
        : engines (p.apvts), filter (p), amp (p), lfo (p), macros (p), mods (p), matrix (p), chain (p), spectrum (p)
    {
        for (int i = 0; i < kNumOscs; ++i) sources.add (new OscSourceV7 (p, i));
        sources.add (new LayerSourceV7 (p, false));
        sources.add (new LayerSourceV7 (p, true));
        sources.add (new SamplerSourceV7 (p));
        for (auto* c : sources) owner.addChildComponent (c);
        for (auto* c : std::initializer_list<juce::Component*> { &engines, &filter, &amp, &lfo, &macros, &mods, &matrix, &chain, &spectrum })
            owner.addAndMakeVisible (c);
        engines.onSelect = [this] (int i) { show (i); };
        matrix.onOpenFull = [this] { mods.showTab (10); };
        amp.onOpenEditor = [this] { mods.showTab (0); };
        lfo.onOpenEditor = [this] (int l) { mods.showTab (4 + l); };
        macros.onAssign = [this] { mods.showTab (10); };
        chain.onOpenEffects = [&owner] { if (owner.onShowTab) owner.onShowTab ("EFFECTS"); };
        show (0);
    }
    void show (int i) { for (int k = 0; k < sources.size(); ++k) sources[k]->setVisible (k == i); }

    EngineListV7 engines;
    juce::OwnedArray<juce::Component> sources;
    FilterV7 filter;
    AmpV7 amp;
    LfoV7 lfo;
    MacrosV7 macros;
    ModTabsV7 mods;
    MatrixV7 matrix;
    FxChainV7 chain;
    SpectrumV7 spectrum;
};

CreatePage::CreatePage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
CreatePage::~CreatePage() = default;
void CreatePage::paint (juce::Graphics&) {}

void CreatePage::resized()
{
    // proportions from the v0.7 mockup, laid out on whatever area the editor gives us (1424 x 646 by default)
    auto& m = *impl;
    const float sx = (float) getWidth() / 1424.0f, sy = (float) getHeight() / 646.0f;
    auto R = [sx, sy] (int x, int y, int w, int h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * sx), juce::roundToInt (y * sy), juce::roundToInt (w * sx), juce::roundToInt (h * sy));
    };
    m.engines.setBounds (R (0, 0, 191, 274));
    for (auto* s : m.sources) s->setBounds (R (203, 0, 374, 274));
    m.filter.setBounds (R (589, 0, 310, 274));
    m.amp.setBounds (R (911, 0, 243, 274));
    m.lfo.setBounds (R (1166, 0, 258, 274));
    m.macros.setBounds (R (0, 286, 213, 360));
    m.mods.setBounds (R (225, 286, 1199, 150));
    m.matrix.setBounds (R (225, 448, 424, 198));
    m.chain.setBounds (R (661, 448, 236, 198));
    m.spectrum.setBounds (R (909, 448, 515, 198));
}

} // namespace divinity::ui
