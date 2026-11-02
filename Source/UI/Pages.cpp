#include "Pages.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

//==============================================================================
// SpectrumAnalyzer
//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer (DivinityProcessor& p) : proc (p)
{
    ring.assign ((size_t) kSize, 0.0f);
    pull.assign ((size_t) ScopeFifo::kSize, 0.0f);
    fftData.assign ((size_t) kSize * 2, 0.0f);
    smooth.assign ((size_t) kSize / 2, -120.0f);
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void SpectrumAnalyzer::timerCallback()
{
    if (! visibleInTree (*this)) return;
    const int n = proc.scope.pull (pull.data(), (int) pull.size());
    for (int i = 0; i < n; ++i) { ring[(size_t) writePos] = pull[(size_t) i]; writePos = (writePos + 1) % kSize; }
    for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = ring[(size_t) ((writePos + i) % kSize)];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    for (int b = 0; b < kSize / 2; ++b)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * 4.0f / kSize, -120.0f);
        auto& s = smooth[(size_t) b];
        s = db > s ? s + 0.6f * (db - s) : s + 0.15f * (db - s);
    }
    repaint();
}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff060c18));
    g.fillRoundedRectangle (r, 5.0f);
    const double sr = juce::jmax (8000.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);

    // grid + labels
    g.setFont (font (10.5f));
    const double freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* labels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    auto xFor = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    for (int i = 0; i < 8; ++i)
    {
        const float x = xFor (freqs[i]);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom() - 14.0f);
        g.setColour (textFaint);
        g.drawText (labels[i], (int) x - 20, (int) r.getBottom() - 14, 40, 12, juce::Justification::centred);
    }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) (r.getY() + (r.getHeight() - 14.0f) * i / 4.0f), r.getX(), r.getRight());
    }

    juce::Path curve;
    const int points = juce::jmax (64, (int) r.getWidth() / 2);
    const float bottom = r.getBottom() - 16.0f, top = r.getY() + 6.0f;
    for (int i = 0; i <= points; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / points);
        const double bin = f / sr * kSize;
        const int b0 = juce::jlimit (1, kSize / 2 - 2, (int) bin);
        const float frac = (float) juce::jlimit (0.0, 1.0, bin - b0);
        const float db = smooth[(size_t) b0] + frac * (smooth[(size_t) b0 + 1] - smooth[(size_t) b0]) + 4.5f * (float) std::log2 (f / 1000.0);  // +4.5 dB/oct tilt
        const float y = juce::jmap (juce::jlimit (-84.0f, 0.0f, db), -84.0f, 0.0f, bottom, top);
        const float x = r.getX() + r.getWidth() * (float) i / points;
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    fill.lineTo (r.getRight(), bottom); fill.lineTo (r.getX(), bottom); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (accentStrong.withAlpha (0.45f), 0, top, accentDeep.withAlpha (0.05f), 0, bottom, false));
    g.fillPath (fill);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (1.5f));
}

//==============================================================================
namespace
{
    juce::String oscName (int i) { return "OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + i)); }

    void enableOnTouch (APVTS& s, Knob* k, const juce::String& onID)
    {
        k->getSlider().onDragStart = [&s, onID]
        {
            auto* p = s.getParameter (onID);
            if (p->getValue() < 0.5f) { p->beginChangeGesture(); p->setValueNotifyingHost (1.0f); p->endChangeGesture(); }
        };
    }

    void layoutKnobRow (juce::OwnedArray<Knob>& knobs, juce::Rectangle<int> row)
    {
        const int n = knobs.size();
        if (n == 0) return;
        const int w = row.getWidth() / n;
        for (int i = 0; i < n; ++i) knobs[i]->setBounds (row.getX() + i * w, row.getY(), w, row.getHeight());
    }

    //--------------------------------------------------------------------------
    class EngineList : public Panel, private juce::Timer
    {
    public:
        explicit EngineList (APVTS& s) : Panel ("ENGINE"), apvts (s) { startTimerHz (6); }
        std::function<void (int)> onSelect;
        int selected = 0;

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            static const char* names[] = { "OSC A", "OSC B", "OSC C", "SUB", "NOISE" };
            for (int i = 0; i < 5; ++i)
            {
                auto r = item (i).toFloat();
                const bool sel = i == selected;
                if (sel)
                {
                    g.setColour (accent.withAlpha (0.16f)); g.drawRoundedRectangle (r.expanded (1.0f), 7.0f, 3.0f);
                    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff12407a), 0, r.getY(), juce::Colour (0xff0a2547), 0, r.getBottom(), false));
                    g.fillRoundedRectangle (r, 6.0f);
                    g.setColour (accent); g.drawRoundedRectangle (r, 6.0f, 1.2f);
                }
                else
                {
                    g.setColour (panel2.withAlpha (i == hover ? 0.95f : 0.55f)); g.fillRoundedRectangle (r, 6.0f);
                    g.setColour (border); g.drawRoundedRectangle (r, 6.0f, 1.0f);
                }
                juce::Path w;
                const float x0 = r.getX() + 14, cy = r.getCentreY();
                if (i < 3) { w.startNewSubPath (x0, cy); w.cubicTo (x0 + 4, cy - 7, x0 + 8, cy - 7, x0 + 10, cy); w.cubicTo (x0 + 12, cy + 7, x0 + 16, cy + 7, x0 + 20, cy); }
                else if (i == 3) { w.startNewSubPath (x0, cy + 5); w.cubicTo (x0 + 6, cy - 8, x0 + 14, cy - 8, x0 + 20, cy + 5); }
                else { juce::Random rr (7); w.startNewSubPath (x0, cy); for (int k = 1; k <= 10; ++k) w.lineTo (x0 + k * 2.0f, cy + (rr.nextFloat() - 0.5f) * 12.0f); }
                const bool on = apvts.getRawParameterValue (onIds[i])->load() > 0.5f;
                g.setColour (on ? text : textFaint);
                g.strokePath (w, juce::PathStrokeType (1.5f));
                g.setFont (font (15.0f, Weight::Medium));
                g.drawText (names[i], (int) r.getX() + 44, (int) r.getY(), 80, (int) r.getHeight(), juce::Justification::centredLeft);
                g.setColour (on ? accent : textFaint.withAlpha (0.6f));
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getRight() - 14.0f, cy }));
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 5; ++i)
                if (item (i).contains (e.getPosition())) { selected = i; repaint(); if (onSelect) onSelect (i); }
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            int h = -1;
            for (int i = 0; i < 5; ++i) if (item (i).contains (e.getPosition())) h = i;
            if (h != hover) { hover = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    private:
        juce::Rectangle<int> item (int i) const { return { 8, 46 + i * 46, getWidth() - 16, 38 }; }
        void timerCallback() override
        {
            int mask = 0;
            for (int i = 0; i < 5; ++i) if (apvts.getRawParameterValue (onIds[i])->load() > 0.5f) mask |= 1 << i;
            if (mask != lastMask) { lastMask = mask; repaint(); }
        }
        APVTS& apvts;
        int hover = -1, lastMask = -1;
        const juce::String onIds[5] { ids::osc (0, "on"), ids::osc (1, "on"), ids::osc (2, "on"), ids::subOn, ids::noiseOn };
    };

    //--------------------------------------------------------------------------
    class OscEditor : public Panel
    {
    public:
        OscEditor (DivinityProcessor& p, int i)
            : index (i), power (p.apvts, ids::osc (i, "on")), engine (p.apvts, ids::osc (i, "engine")),
              table (p.apvts, ids::osc (i, "table")), wave (p.apvts, ids::osc (i, "wave")),
              display (p.apvts, p.getWavetables(), i)
        {
            auto& s = p.apvts;
            const auto off = [i] (ModDest d) { return (ModDest) ((int) d + i); };
            wt.add (new Knob (s, ids::osc (i, "pos"), "Position", off (ModDest::OscAPos)));
            wt.add (new Knob (s, ids::osc (i, "unison"), "Unison"));
            wt.add (new Knob (s, ids::osc (i, "detune"), "Detune", off (ModDest::OscADetune)));
            wt.add (new Knob (s, ids::osc (i, "width"), "Width"));
            wt.add (new Knob (s, ids::osc (i, "level"), "Blend", off (ModDest::OscALevel)));
            an.add (new Knob (s, ids::osc (i, "oct"), "Octave", ModDest::None, 1.0f, true));
            an.add (new Knob (s, ids::osc (i, "pw"), "PWM", off (ModDest::OscAPwm), 0.5f));
            an.add (new Knob (s, ids::osc (i, "unison"), "Unison"));
            an.add (new Knob (s, ids::osc (i, "detune"), "Detune", off (ModDest::OscADetune)));
            an.add (new Knob (s, ids::osc (i, "level"), "Blend", off (ModDest::OscALevel)));
            for (auto* k : wt) { k->setCaptionSize (11.5f); addChildComponent (k); }
            for (auto* k : an) { k->setCaptionSize (11.5f); addChildComponent (k); }
            for (auto* c : std::initializer_list<juce::Component*> { &power, &engine, &table, &wave, &display }) addAndMakeVisible (c);
            engine.onChange = [this] { update(); };
            power.onStateChange = [this] { repaint(); };
            update();
        }
        void update()
        {
            const bool isWt = engine.getIndex() == 0;
            for (auto* k : wt) k->setVisible (isWt);
            for (auto* k : an) k->setVisible (! isWt);
            table.setVisible (isWt); wave.setVisible (! isWt);
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            drawPanelBackground (g, getLocalBounds().toFloat());
            drawSectionTitle (g, { 44, 10, 200, 24 }, engine.getIndex() == 0 ? "WAVETABLE" : "ANALOG");
            g.setColour (power.getToggleState() ? accent : textFaint);
            g.setFont (font (11.5f, Weight::Medium, 0.15f));
            g.drawText (oscName (index), 150, 10, 60, 24, juce::Justification::centredLeft);
        }
        void resized() override
        {
            const int w = getWidth();
            power.setBounds (12, 10, 24, 24);
            engine.setBounds (w - 150, 9, 138, 26);
            table.setBounds (12, 44, w - 24, 30);
            wave.setBounds (12, 44, w - 24, 30);
            display.setBounds (12, 80, w - 24, 110);
            layoutKnobRow (wt, { 6, 196, w - 12, 84 });
            layoutKnobRow (an, { 6, 196, w - 12, 84 });
        }
    private:
        int index;
        PowerButton power;
        Selector engine, table, wave;
        OscDisplay display;
        juce::OwnedArray<Knob> wt, an;
    };

    class LayerEditor : public Panel
    {
    public:
        LayerEditor (DivinityProcessor& p, bool isNoise)
            : Panel (isNoise ? "NOISE" : "SUB"), noise (isNoise),
              power (p.apvts, isNoise ? ids::noiseOn : ids::subOn),
              mode (p.apvts, isNoise ? ids::noiseType : ids::subMode),
              level (p.apvts, isNoise ? ids::noiseLevel : ids::subLevel, "Level", isNoise ? ModDest::NoiseLevel : ModDest::SubLevel)
        {
            level.setCaptionSize (12.5f);
            for (auto* c : std::initializer_list<juce::Component*> { &power, &mode, &level }) addAndMakeVisible (c);
        }
        void paint (juce::Graphics& g) override
        {
            drawPanelBackground (g, getLocalBounds().toFloat());
            drawSectionTitle (g, { 44, 10, 200, 24 }, noise ? "NOISE" : "SUB OSCILLATOR");
            g.setColour (textDim);
            g.setFont (font (12.5f));
            g.drawFittedText (noise ? "Adds breath, air or grit on top of the oscillators. Pink is soft, Dark rumbles, Bright is airy."
                                    : "A clean wave one or two octaves under the played note. Weight under basses, keys and pads.",
                              16, getHeight() - 52, getWidth() - 32, 40, juce::Justification::centredLeft, 2);
        }
        void resized() override
        {
            power.setBounds (12, 10, 24, 24);
            mode.setBounds (12, 44, getWidth() - 24, 30);
            level.setBounds (getWidth() / 2 - 60, 84, 120, 140);
        }
    private:
        bool noise;
        PowerButton power;
        Selector mode;
        Knob level;
    };

    //--------------------------------------------------------------------------
    class FilterQuick : public Panel
    {
    public:
        explicit FilterQuick (DivinityProcessor& p) : Panel ("FILTER"), tabs ({ "1", "2" }), response (p.apvts, 0)
        {
            auto& s = p.apvts;
            for (int f = 0; f < 2; ++f)
            {
                types.add (new ChoiceBox (s, ids::filt (f, "type")));
                addChildComponent (types.getLast());
                const auto cut = f == 0 ? ModDest::Filter1Cutoff : ModDest::Filter2Cutoff;
                const auto res = f == 0 ? ModDest::Filter1Res : ModDest::Filter2Res;
                const auto drv = f == 0 ? ModDest::Filter1Drive : ModDest::Filter2Drive;
                knobs[f].add (new Knob (s, ids::filt (f, "cutoff"), "Cutoff", cut, 0.6f));
                knobs[f].add (new Knob (s, ids::filt (f, "res"), "Resonance", res));
                knobs[f].add (new Knob (s, ids::filt (f, "drive"), "Drive", drv));
                for (auto* k : knobs[f]) { k->setCaptionSize (11.5f); addChildComponent (k); }
            }
            tabs.setTooltip ("Filter 1 / Filter 2. Routing, mix and key tracking live in LAB.");
            tabs.onSelect = [this] (int i) { select (i); };
            addAndMakeVisible (tabs);
            addAndMakeVisible (response);
            select (0);
        }
        void select (int f)
        {
            for (int i = 0; i < 2; ++i) { types[i]->setVisible (i == f); for (auto* k : knobs[i]) k->setVisible (i == f); }
            response.setFilter (f);
        }
        void resized() override
        {
            const int w = getWidth();
            tabs.setBounds (w - 84, 8, 72, 28);
            for (auto* t : types) t->setBounds (12, 44, w - 24, 30);
            response.setBounds (12, 80, w - 24, 110);
            for (auto& k : knobs) layoutKnobRow (k, { 6, 196, w - 12, 84 });
        }
    private:
        TabRow tabs;
        juce::OwnedArray<ChoiceBox> types;
        FilterResponse response;
        juce::OwnedArray<Knob> knobs[2];
    };

    class AmpQuick : public Panel
    {
    public:
        explicit AmpQuick (DivinityProcessor& p) : Panel ("AMP (ENVELOPE)"), display (p.apvts, 0)
        {
            auto& s = p.apvts;
            for (auto [id, cap] : { std::pair { "attack", "A" }, { "decay", "D" }, { "sustain", "S" }, { "release", "R" } })
            {
                knobs.add (new Knob (s, ids::env (0, id), cap));
                knobs.getLast()->setCaptionSize (12.5f);
                addAndMakeVisible (knobs.getLast());
            }
            addAndMakeVisible (display);
        }
        void resized() override
        {
            display.setBounds (12, 44, getWidth() - 24, 146);
            layoutKnobRow (knobs, { 6, 196, getWidth() - 12, 84 });
        }
    private:
        EnvelopeDisplay display;
        juce::OwnedArray<Knob> knobs;
    };

    /** Shape / sync / division / knobs for one LFO. */
    struct LfoControls
    {
        LfoControls (APVTS& s, int l)
            : shape (s, ids::lfo (l, "shape")), division (s, ids::lfo (l, "div"))
        {
            sync.setButtonText ("SYNC");
            sync.setClickingTogglesState (true);
            sync.setTooltip (helpTextFor (ids::lfo (l, "sync")));
            syncAttach = std::make_unique<APVTS::ButtonAttachment> (s, ids::lfo (l, "sync"), sync);
            knobs.add (new Knob (s, ids::lfo (l, "rate"), "Rate", (ModDest) ((int) ModDest::Lfo1Rate + l), 0.3f));
            knobs.add (new Knob (s, ids::lfo (l, "depth"), "Amount"));
            knobs.add (new Knob (s, ids::lfo (l, "phase"), "Phase"));
            knobs.add (new Knob (s, ids::lfo (l, "smooth"), "Smooth"));
            for (auto* k : knobs) k->setCaptionSize (11.5f);
            sync.onStateChange = [this]
            {
                const bool on = sync.getToggleState();
                knobs[0]->setAlpha (on ? 0.35f : 1.0f);
                division.setEnabled (on);
                division.setAlpha (on ? 1.0f : 0.45f);
            };
            sync.onStateChange();
        }
        void addTo (juce::Component& parent, bool visible)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &shape, &sync, &division }) { parent.addChildComponent (c); c->setVisible (visible); }
            for (auto* k : knobs) { parent.addChildComponent (k); k->setVisible (visible); }
        }
        void setVisible (bool v)
        {
            shape.setVisible (v); sync.setVisible (v); division.setVisible (v);
            for (auto* k : knobs) k->setVisible (v);
        }
        ChoiceBox shape, division;
        juce::TextButton sync;
        std::unique_ptr<APVTS::ButtonAttachment> syncAttach;
        juce::OwnedArray<Knob> knobs;
    };

    class LfoQuick : public Panel
    {
    public:
        explicit LfoQuick (DivinityProcessor& p) : Panel ("LFO 1"), display (p, 0), ctl (p.apvts, 0)
        {
            addAndMakeVisible (display);
            ctl.addTo (*this, true);
            ctl.knobs[3]->setVisible (false);
        }
        void resized() override
        {
            const int w = getWidth();
            display.setBounds (12, 44, w - 24, 76);
            const int cw = (w - 32) / 3;
            ctl.shape.setBounds (12, 128, cw, 26);
            ctl.sync.setBounds (16 + cw, 128, cw, 26);
            ctl.division.setBounds (20 + 2 * cw, 128, cw, 26);
            const int kw = (w - 12) / 3;
            for (int k = 0; k < 3; ++k) ctl.knobs[k]->setBounds (6 + k * kw, 168, kw, 108);
        }
    private:
        LfoDisplay display;
        LfoControls ctl;
    };

    class MacroColumn : public Panel
    {
    public:
        explicit MacroColumn (DivinityProcessor& p) : Panel ("MACROS")
        {
            const char* names[] = { "Tone", "Movement", "Texture", "Space" };
            for (int i = 0; i < kNumMacros; ++i)
            {
                knobs.add (new Knob (p.apvts, ids::macro (i), names[i]));
                knobs.getLast()->setCaptionSize (12.0f);
                addAndMakeVisible (knobs.getLast());
            }
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            g.setColour (text);
            g.setFont (font (16.0f, Weight::Medium));
            for (int i = 0; i < 4; ++i) g.drawText (juce::String (i + 1), 16, knobs[i]->getY() + 20, 20, 24, juce::Justification::centredLeft);
        }
        void resized() override
        {
            const int h = (getHeight() - 52) / 4;
            for (int i = 0; i < 4; ++i) knobs[i]->setBounds (34, 46 + i * h, getWidth() - 44, h - 4);
        }
    private:
        juce::OwnedArray<Knob> knobs;
    };

    //--------------------------------------------------------------------------
    class ModStrip : public Panel
    {
    public:
        explicit ModStrip (DivinityProcessor& p)
            : proc (p), tabs ({ "ENV 1", "ENV 2", "ENV 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "MATRIX" }),
              envDisplay (p.apvts, 0), lfoDisplay (p, 0)
        {
            auto& s = p.apvts;
            addAndMakeVisible (tabs);
            addChildComponent (envDisplay);
            addChildComponent (lfoDisplay);
            for (int e = 0; e < kNumEnvs; ++e)
            {
                for (auto [id, cap] : { std::pair { "attack", "A" }, { "decay", "D" }, { "sustain", "S" }, { "release", "R" }, { "curve", "Curve" } })
                    envKnobs[e].add (new Knob (s, ids::env (e, id), cap, ModDest::None, 1.0f, juce::String (id) == "curve"));
                for (auto* k : envKnobs[e]) { k->setCaptionSize (11.0f); addChildComponent (k); }
            }
            for (int l = 0; l < kNumLfos; ++l) { lfos.add (new LfoControls (s, l)); lfos.getLast()->addTo (*this, false); }
            for (int i = 0; i < kNumModSlots; ++i)
            {
                auto& r = rows[i];
                r.src = std::make_unique<ChoiceBox> (s, ids::mod (i, "src"));
                r.dst = std::make_unique<ChoiceBox> (s, ids::mod (i, "dst"));
                r.amt = std::make_unique<FineSlider>();
                r.amt->setSliderStyle (juce::Slider::LinearHorizontal);
                r.amt->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                r.amt->setDoubleClickReturnValue (true, 0.0);
                r.amt->setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
                r.amt->setTooltip ("Modulation amount. Double-click to zero; right-click to type a value.");
                auto* raw = r.amt.get(); auto* prm = s.getParameter (ids::mod (i, "amt"));
                r.amt->onPopup = [raw, prm] { showParameterMenu (*raw, *prm); };
                r.amt->onValueChange = [this] { repaint(); };
                r.attach = std::make_unique<APVTS::SliderAttachment> (s, ids::mod (i, "amt"), *r.amt);
                addChildComponent (*r.src); addChildComponent (*r.dst); addChildComponent (*r.amt);
            }
            tabs.onSelect = [this] (int i) { select (i); };
            select (1);
            tabs.setSelected (1, false);
        }
        void select (int t)
        {
            current = t;
            const bool env = t < 3, lfo = t >= 3 && t < 7, matrix = t == 7;
            envDisplay.setVisible (env);
            lfoDisplay.setVisible (lfo);
            if (env) envDisplay.setEnvelope (t);
            if (lfo) lfoDisplay.setLfo (t - 3);
            for (int e = 0; e < kNumEnvs; ++e) for (auto* k : envKnobs[e]) k->setVisible (env && e == t);
            for (int l = 0; l < kNumLfos; ++l) lfos[l]->setVisible (lfo && l == t - 3);
            for (auto& r : rows) { r.src->setVisible (matrix); r.dst->setVisible (matrix); r.amt->setVisible (matrix); }
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            if (current != 7) return;
            g.setFont (font (11.0f, Weight::Medium));
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const float v = proc.apvts.getRawParameterValue (ids::mod (i, "amt"))->load();
                const int pct = juce::roundToInt (v * 100.0f);
                g.setColour (pct != 0 ? text : textFaint);
                auto b = rows[i].amt->getBounds();
                g.drawText ((pct > 0 ? "+" : "") + juce::String (pct) + "%", b.getRight() + 4, b.getY(), 44, b.getHeight(), juce::Justification::centredLeft);
            }
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            tabs.setBounds (8, 8, w - 16, 32);
            const auto area = juce::Rectangle<int> (12, 48, w - 24, h - 56);
            envDisplay.setBounds (area.withWidth ((int) (area.getWidth() * 0.6f)));
            lfoDisplay.setBounds (area.withWidth ((int) (area.getWidth() * 0.45f)));
            const int kx = envDisplay.getRight() + 10;
            const int kw = (area.getRight() - kx) / 5;
            for (int e = 0; e < kNumEnvs; ++e)
                for (int k = 0; k < 5; ++k) envKnobs[e][k]->setBounds (kx + k * kw, area.getY() + 6, kw, area.getHeight() - 6);
            for (auto* L : lfos)
            {
                const int lx = lfoDisplay.getRight() + 12;
                L->shape.setBounds (lx, area.getY(), 110, 26);
                L->sync.setBounds (lx, area.getY() + 34, 110, 26);
                L->division.setBounds (lx, area.getY() + 68, 110, 26);
                const int kx2 = lx + 122, kw2 = (area.getRight() - kx2) / 4;
                for (int k = 0; k < 4; ++k) L->knobs[k]->setBounds (kx2 + k * kw2, area.getY() + 4, kw2, area.getHeight() - 4);
            }
            const int colW = area.getWidth() / 2;
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const int col = i / 4, row = i % 4;
                const int x = area.getX() + col * colW, y = area.getY() + row * 30;
                rows[i].src->setBounds (x, y, 120, 26);
                rows[i].dst->setBounds (x + 126, y, 150, 26);
                rows[i].amt->setBounds (x + 284, y + 4, colW - 344, 18);
            }
        }
    private:
        DivinityProcessor& proc;
        TabRow tabs;
        EnvelopeDisplay envDisplay;
        LfoDisplay lfoDisplay;
        juce::OwnedArray<Knob> envKnobs[kNumEnvs];
        juce::OwnedArray<LfoControls> lfos;
        struct Row { std::unique_ptr<ChoiceBox> src, dst; std::unique_ptr<FineSlider> amt; std::unique_ptr<APVTS::SliderAttachment> attach; };
        Row rows[kNumModSlots];
        int current = 1;
    };

    //--------------------------------------------------------------------------
    struct FxModuleDef
    {
        const char* name; juce::String onID; const char* description;
        std::vector<std::pair<juce::String, const char*>> knobs;
        juce::String choiceID; juce::String toggleID; const char* toggleLabel;
    };

    const std::vector<FxModuleDef>& fxModules()
    {
        static const std::vector<FxModuleDef> m {
            { "Saturation", ids::satOn, "Warm, anti-aliased harmonic drive. A touch adds presence, a lot adds teeth.",
              { { ids::satDrive, "Drive" }, { ids::satMix, "Mix" } }, {}, {}, nullptr },
            { "Chorus", ids::choOn, "Detuned copies drifting around the dry signal for width and shimmer.",
              { { ids::choRate, "Rate" }, { ids::choDepth, "Depth" }, { ids::choMix, "Mix" } }, {}, {}, nullptr },
            { "Delay", ids::dlyOn, "Tempo-synced echoes, ping-pong across the stereo field, darkening with each repeat.",
              { { ids::dlyFeedback, "Feedback" }, { ids::dlyMix, "Mix" }, { ids::dlyTone, "Tone" } }, ids::dlyDiv, ids::dlyPingPong, "PING-PONG" },
            { "Reverb", ids::revOn, "A lush stereo room, from small chambers to endless halls.",
              { { ids::revSize, "Size" }, { ids::revDamp, "Damping" }, { ids::revWidth, "Width" }, { ids::revMix, "Mix" } }, {}, {}, nullptr },
            { "Limiter", ids::limOn, "The safety net: catches peaks so nothing ever clips your DAW.",
              { { ids::limCeiling, "Ceiling" } }, {}, {}, nullptr },
        };
        return m;
    }

    /** All controls for one FX module (used by the CREATE quick panel and the EFFECTS page). */
    struct FxModuleControls
    {
        FxModuleControls (APVTS& s, const FxModuleDef& d) : def (d), power (s, d.onID)
        {
            for (auto& [id, cap] : d.knobs) { knobs.add (new Knob (s, id, cap)); knobs.getLast()->setCaptionSize (11.5f); }
            if (d.choiceID.isNotEmpty()) choice = std::make_unique<ChoiceBox> (s, d.choiceID);
            if (d.toggleID.isNotEmpty())
            {
                toggle = std::make_unique<juce::TextButton> (d.toggleLabel);
                toggle->setClickingTogglesState (true);
                toggle->setTooltip (helpTextFor (d.toggleID));
                toggleAttach = std::make_unique<APVTS::ButtonAttachment> (s, d.toggleID, *toggle);
            }
        }
        void addTo (juce::Component& parent)
        {
            parent.addAndMakeVisible (power);
            for (auto* k : knobs) parent.addAndMakeVisible (k);
            if (choice) parent.addAndMakeVisible (*choice);
            if (toggle) parent.addAndMakeVisible (*toggle);
        }
        void setVisible (bool v)
        {
            power.setVisible (v);
            for (auto* k : knobs) k->setVisible (v);
            if (choice) choice->setVisible (v);
            if (toggle) toggle->setVisible (v);
        }
        const FxModuleDef& def;
        PowerButton power;
        juce::OwnedArray<Knob> knobs;
        std::unique_ptr<ChoiceBox> choice;
        std::unique_ptr<juce::TextButton> toggle;
        std::unique_ptr<APVTS::ButtonAttachment> toggleAttach;
    };

    class FxQuick : public Panel
    {
    public:
        explicit FxQuick (DivinityProcessor& p) : analyzer (p), meter (p, true)
        {
            for (auto& d : fxModules())
            {
                modules.add (new FxModuleControls (p.apvts, d));
                modules.getLast()->addTo (*this);
                selector.addItem (d.name, selector.getNumItems() + 1);
            }
            selector.setTooltip ("Choose which effect to edit here. The full chain is on the EFFECTS tab.");
            selector.onChange = [this] { select (selector.getSelectedItemIndex()); };
            addAndMakeVisible (selector);
            addAndMakeVisible (analyzer);
            addAndMakeVisible (meter);
            selector.setSelectedItemIndex (3, juce::sendNotificationSync);
        }
        void select (int i)
        {
            current = juce::jmax (0, i);
            for (int k = 0; k < modules.size(); ++k) modules[k]->setVisible (k == current);
            resized(); repaint();
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            drawSectionTitle (g, { 236, 10, 300, 24 }, juce::String (fxModules()[(size_t) current].name).toUpperCase());
            g.setColour (textDim);
            g.setFont (font (11.0f, Weight::Medium, 0.1f));
            g.drawText ("OUTPUT SPECTRUM", analyzer.getX(), 14, 200, 16, juce::Justification::centredLeft);
        }
        void resized() override
        {
            const int w = getWidth(), h = getHeight();
            selector.setBounds (44, 9, 180, 28);
            analyzer.setBounds (336, 36, w - 392, h - 46);
            meter.setBounds (w - 44, 36, 26, h - 46);
            for (auto* m : modules)
            {
                m->power.setBounds (12, 11, 24, 24);
                int x = 8, y = 46;
                const int kw = 96, kh = 74;
                for (auto* k : m->knobs)
                {
                    k->setBounds (x, y, kw, kh);
                    x += kw + 8;
                    if (x + kw > 330) { x = 8; y += kh + 4; }
                }
                int cy = h - 38;
                if (m->choice) { m->choice->setBounds (12, cy, 140, 26); }
                if (m->toggle) { m->toggle->setBounds (160, cy, 130, 26); }
            }
        }
    private:
        juce::ComboBox selector;
        juce::OwnedArray<FxModuleControls> modules;
        SpectrumAnalyzer analyzer;
        LevelMeter meter;
        int current = 0;
    };
}

//==============================================================================
// CreatePage
//==============================================================================
struct CreatePage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner)
        : engines (p.apvts), filter (p), amp (p), lfo (p), macros (p), mods (p), fx (p)
    {
        for (int i = 0; i < kNumOscs; ++i) sources.add (new OscEditor (p, i));
        sources.add (new LayerEditor (p, false));
        sources.add (new LayerEditor (p, true));
        for (auto* c : sources) owner.addChildComponent (c);
        for (auto* c : std::initializer_list<juce::Component*> { &engines, &filter, &amp, &lfo, &macros, &mods, &fx })
            owner.addAndMakeVisible (c);
        engines.onSelect = [this] (int i) { show (i); };
        show (0);
    }
    void show (int i) { for (int k = 0; k < sources.size(); ++k) sources[k]->setVisible (k == i); }

    EngineList engines;
    juce::OwnedArray<juce::Component> sources;
    FilterQuick filter;
    AmpQuick amp;
    LfoQuick lfo;
    MacroColumn macros;
    ModStrip mods;
    FxQuick fx;
};

CreatePage::CreatePage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
CreatePage::~CreatePage() = default;
void CreatePage::paint (juce::Graphics&) {}
void CreatePage::resized()
{
    auto& m = *impl;
    m.engines.setBounds (0, 0, 157, 285);
    for (auto* s : m.sources) s->setBounds (169, 0, 352, 285);
    m.filter.setBounds (533, 0, 252, 285);
    m.amp.setBounds (797, 0, 240, 285);
    m.lfo.setBounds (1049, 0, 199, 285);
    m.macros.setBounds (0, 297, 157, 385);
    m.mods.setBounds (169, 297, 1079, 178);
    m.fx.setBounds (169, 487, 1079, 195);
}

//==============================================================================
// EffectsPage
//==============================================================================
namespace
{
    class FxModulePanel : public Panel
    {
    public:
        FxModulePanel (DivinityProcessor& p, const FxModuleDef& d) : Panel (juce::String (d.name).toUpperCase()), ctl (p.apvts, d)
        {
            ctl.addTo (*this);
            ctl.power.onStateChange = [this] { repaint(); };
        }
        void paint (juce::Graphics& g) override
        {
            drawPanelBackground (g, getLocalBounds().toFloat(), ctl.power.getToggleState());
            drawSectionTitle (g, { 44, 10, getWidth() - 56, 24 }, juce::String (ctl.def.name).toUpperCase());
            g.setColour (textDim);
            g.setFont (font (12.5f));
            g.drawFittedText (ctl.def.description, 16, 44, getWidth() - 32, 52, juce::Justification::topLeft, 3);
        }
        void resized() override
        {
            ctl.power.setBounds (12, 10, 24, 24);
            const int cols = 2, kw = (getWidth() - 24) / cols, kh = 104;
            int y = 106;
            if (ctl.choice) { ctl.choice->setBounds (16, y, getWidth() - 32, 28); y += 40; }
            for (int i = 0; i < ctl.knobs.size(); ++i)
                ctl.knobs[i]->setBounds (12 + (i % cols) * kw, y + (i / cols) * (kh + 8), kw, kh);
            if (ctl.toggle) ctl.toggle->setBounds (16, getHeight() - 46, getWidth() - 32, 30);
        }
    private:
        FxModuleControls ctl;
    };
}

struct EffectsPage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner)
        : master (p.apvts, ids::master, "Output"), meter (p, true), analyzer (p)
    {
        for (auto& d : fxModules()) { panels.add (new FxModulePanel (p, d)); owner.addAndMakeVisible (panels.getLast()); }
        master.setCaptionSize (12.5f);
        for (auto* c : std::initializer_list<juce::Component*> { &master, &meter, &analyzer }) owner.addAndMakeVisible (c);
    }
    juce::OwnedArray<FxModulePanel> panels;
    Knob master;
    LevelMeter meter;
    SpectrumAnalyzer analyzer;
};

EffectsPage::EffectsPage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
EffectsPage::~EffectsPage() = default;

void EffectsPage::paint (juce::Graphics& g)
{
    drawPanelBackground (g, { 0.0f, 470.0f, 1248.0f, 212.0f });
    drawSectionTitle (g, { 16, 478, 400, 24 }, "OUTPUT");
    g.setColour (textDim);
    g.setFont (font (12.5f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Signal flow: voices \xe2\x86\x92 Saturation \xe2\x86\x92 Chorus \xe2\x86\x92 Delay \xe2\x86\x92 Reverb \xe2\x86\x92 Output \xe2\x86\x92 Limiter")),
                16, 652, 900, 20, juce::Justification::centredLeft);
    g.setColour (textFaint);
    for (int i = 0; i + 1 < impl->panels.size(); ++i)
    {
        const auto b = impl->panels[i]->getBounds();
        const float x = (float) b.getRight() + 6.0f, y = (float) b.getCentreY();
        juce::Path p; p.startNewSubPath (x - 3, y - 6); p.lineTo (x + 3, y); p.lineTo (x - 3, y + 6);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
}

void EffectsPage::resized()
{
    auto& m = *impl;
    const int n = m.panels.size(), gap = 12, w = (1248 - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i) m.panels[i]->setBounds (i * (w + gap), 0, w, 458);
    m.analyzer.setBounds (16, 508, 1000, 136);
    m.master.setBounds (1040, 500, 120, 140);
    m.meter.setBounds (1190, 510, 30, 130);
}

//==============================================================================
// PlayPage
//==============================================================================
namespace
{
    class PresetInfoPanel : public Panel, private juce::Timer
    {
    public:
        explicit PresetInfoPanel (DivinityProcessor& p) : proc (p), scope (p) { addAndMakeVisible (scope); startTimerHz (6); }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const auto& all = proc.presets.getPresets();
            const int idx = proc.presets.getCurrentIndex();
            const divinity::PresetInfo info = juce::isPositiveAndBelow (idx, all.size()) ? all[idx] : divinity::PresetInfo {};
            g.setColour (text);
            g.setFont (font (28.0f, Weight::Medium));
            g.drawText (shown, 20, 14, getWidth() - 40, 36, juce::Justification::centredLeft);
            g.setFont (font (11.5f, Weight::Medium, 0.16f));
            g.setColour (accent);
            g.drawText ((info.category.isNotEmpty() ? info.category : juce::String ("Init")).toUpperCase()
                            + (info.tags.isNotEmpty() ? "   /   " + info.tags.toUpperCase() : juce::String()), 20, 52, getWidth() - 40, 16, juce::Justification::centredLeft);
            g.setFont (font (12.5f));
            g.setColour (textDim);
            g.drawFittedText (info.description, 20, 72, getWidth() - 40, 32, juce::Justification::topLeft, 2);
        }
        void resized() override { scope.setBounds (20, 110, getWidth() - 40, getHeight() - 122); }
    private:
        void timerCallback() override
        {
            const auto n = proc.presets.getCurrentName() + (proc.presets.isModified() ? " *" : "");
            if (n != shown) { shown = n; repaint(); }
        }
        DivinityProcessor& proc;
        ScopeDisplay scope;
        juce::String shown;
    };

    class SoulMini : public Panel
    {
    public:
        explicit SoulMini (DivinityProcessor& p) : Panel ("SOUL"), proc (p), pad (p, true)
        {
            addAndMakeVisible (pad);
            divine.setSliderStyle (juce::Slider::LinearHorizontal);
            divine.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            divine.setDoubleClickReturnValue (true, 0.0);
            divine.setTooltip (helpTextFor (ids::divineAmount) + "\n\nPick the transformation on the SOUL tab.");
            divine.onValueChange = [this] { repaint(); };
            attach = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::divineAmount, divine);
            addAndMakeVisible (divine);
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const int mode = juce::roundToInt (proc.apvts.getRawParameterValue (ids::divineMode)->load());
            g.setFont (font (10.5f, Weight::Medium, 0.2f));
            g.setColour (textDim);
            g.drawText ("DIVINE  /  " + divineModeNames()[mode].toUpperCase(), 16, getHeight() - 52, 220, 14, juce::Justification::centredLeft);
            g.setColour (accent);
            g.drawText (juce::String (juce::roundToInt (divine.getValue() * 100.0)) + "%", getWidth() - 76, getHeight() - 52, 60, 14, juce::Justification::centredRight);
        }
        void resized() override
        {
            pad.setBounds (16, 40, getWidth() - 32, getHeight() - 100);
            divine.setBounds (16, getHeight() - 34, getWidth() - 32, 20);
        }
    private:
        DivinityProcessor& proc;
        XYPad pad;
        FineSlider divine;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };

    class PerformancePanel : public Panel
    {
    public:
        explicit PerformancePanel (DivinityProcessor& p) : Panel ("SHAPE & CHARACTER")
        {
            auto& s = p.apvts;
            knobs.add (new Knob (s, ids::env (0, "attack"), "Attack"));
            knobs.add (new Knob (s, ids::env (0, "decay"), "Decay"));
            knobs.add (new Knob (s, ids::env (0, "sustain"), "Sustain"));
            knobs.add (new Knob (s, ids::env (0, "release"), "Release"));
            knobs.add (new Knob (s, ids::filt (0, "cutoff"), "Brightness", ModDest::Filter1Cutoff, 0.6f));
            knobs.add (new Knob (s, ids::filt (0, "res"), "Resonance", ModDest::Filter1Res));
            knobs.add (new Knob (s, ids::satDrive, "Intensity"));
            knobs.add (new Knob (s, ids::revMix, "Depth"));
            for (auto* k : knobs) { k->setCaptionSize (11.5f); addAndMakeVisible (k); }
            enableOnTouch (s, knobs[6], ids::satOn);
            enableOnTouch (s, knobs[7], ids::revOn);
        }
        void resized() override
        {
            const int cw = (getWidth() - 16) / 4, ch = (getHeight() - 48) / 2;
            for (int i = 0; i < 8; ++i) knobs[i]->setBounds (8 + (i % 4) * cw, 42 + (i / 4) * ch, cw, ch);
        }
    private:
        juce::OwnedArray<Knob> knobs;
    };
}

struct PlayPage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner)
        : browser (p), info (p), soul (p), macros (p, true), perf (p), keys (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &browser, &info, &soul, &macros, &perf, &keys })
            owner.addAndMakeVisible (c);
    }
    PresetBrowser browser;
    PresetInfoPanel info;
    SoulMini soul;
    MacroSection macros;
    PerformancePanel perf;
    KeyboardBar keys;
};

PlayPage::PlayPage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
PlayPage::~PlayPage() = default;
void PlayPage::paint (juce::Graphics&) {}
void PlayPage::resized()
{
    auto& m = *impl;
    m.browser.setBounds (0, 0, 330, 530);
    m.info.setBounds (342, 0, 560, 265);
    m.soul.setBounds (914, 0, 334, 265);
    m.macros.setBounds (342, 277, 400, 253);
    m.perf.setBounds (754, 277, 494, 253);
    m.keys.setBounds (0, 542, 1248, 140);
}

//==============================================================================
// LabPage
//==============================================================================
namespace
{
    class OscDeep : public Panel
    {
    public:
        explicit OscDeep (DivinityProcessor& p) : Panel ("OSCILLATORS - DEEP SETTINGS")
        {
            auto& s = p.apvts;
            for (int o = 0; o < kNumOscs; ++o)
            {
                auto& c = cols[o];
                c.power = std::make_unique<PowerButton> (s, ids::osc (o, "on"));
                c.engine = std::make_unique<Selector> (s, ids::osc (o, "engine"));
                c.table = std::make_unique<Selector> (s, ids::osc (o, "table"));
                c.wave = std::make_unique<Selector> (s, ids::osc (o, "wave"));
                auto* eng = c.engine.get(); auto* tbl = c.table.get(); auto* wv = c.wave.get();
                c.engine->onChange = [eng, tbl, wv] { tbl->setVisible (eng->getIndex() == 0); wv->setVisible (eng->getIndex() == 1); };
                for (auto [id, cap] : { std::pair { "oct", "Oct" }, { "semi", "Semi" }, { "fine", "Fine" }, { "phase", "Phase" },
                                        { "width", "Width" }, { "fold", "Fold" }, { "pw", "PW" }, { "unison", "Unison" } })
                {
                    const juce::String sid (id);
                    c.knobs.add (new Knob (s, ids::osc (o, id), cap, ModDest::None, 1.0f, sid == "oct" || sid == "semi" || sid == "fine"));
                    c.knobs.getLast()->setCaptionSize (11.0f);
                }
                c.rand = std::make_unique<juce::TextButton> ("RANDOM PHASE");
                c.rand->setClickingTogglesState (true);
                c.rand->setTooltip (helpTextFor (ids::osc (o, "randphase")));
                c.randAttach = std::make_unique<APVTS::ButtonAttachment> (s, ids::osc (o, "randphase"), *c.rand);
                for (auto* comp : std::initializer_list<juce::Component*> { c.power.get(), c.engine.get(), c.table.get(), c.wave.get(), c.rand.get() })
                    addAndMakeVisible (comp);
                for (auto* k : c.knobs) addAndMakeVisible (k);
                c.engine->onChange();
            }
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            for (int o = 0; o < kNumOscs; ++o)
            {
                const int x = 12 + o * colW();
                g.setColour (text);
                g.setFont (font (14.0f, Weight::Medium, 0.1f));
                g.drawText (oscName (o), x + 32, 44, 100, 24, juce::Justification::centredLeft);
                if (o > 0) { g.setColour (border); g.drawVerticalLine (x - 6, 46.0f, (float) getHeight() - 12.0f); }
            }
        }
        void resized() override
        {
            for (int o = 0; o < kNumOscs; ++o)
            {
                auto& c = cols[o];
                const int x = 12 + o * colW(), w = colW() - 18;
                c.power->setBounds (x, 44, 24, 24);
                c.engine->setBounds (x + w - 130, 42, 130, 26);
                c.table->setBounds (x, 76, w, 28);
                c.wave->setBounds (x, 76, w, 28);
                const int kw = w / 4;
                for (int k = 0; k < 8; ++k) c.knobs[k]->setBounds (x + (k % 4) * kw, 112 + (k / 4) * 120, kw, 112);
                c.rand->setBounds (x, 356, w, 28);
            }
        }
    private:
        int colW() const { return (getWidth() - 12) / kNumOscs; }
        struct Col
        {
            std::unique_ptr<PowerButton> power;
            std::unique_ptr<Selector> engine, table, wave;
            juce::OwnedArray<Knob> knobs;
            std::unique_ptr<juce::TextButton> rand;
            std::unique_ptr<APVTS::ButtonAttachment> randAttach;
        };
        Col cols[kNumOscs];
    };
}

struct LabPage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner) : matrix (p), global (p), osc (p), filter (p), lfo (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &matrix, &global, &osc, &filter, &lfo }) owner.addAndMakeVisible (c);
    }
    ModMatrixSection matrix;
    GlobalSection global;
    OscDeep osc;
    FilterSection filter;
    LfoSection lfo;
};

LabPage::LabPage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
LabPage::~LabPage() = default;

void LabPage::paint (juce::Graphics& g)
{
    auto r = juce::Rectangle<float> (0.0f, 576.0f, 416.0f, 106.0f);
    drawPanelBackground (g, r);
    drawSectionTitle (g, { 16, 584, 380, 24 }, "COMING TO THE LAB");
    g.setColour (textDim);
    g.setFont (font (12.0f));
    g.drawFittedText ("Wavetable drawing, spectral and granular engines, DNA Fusion, Time Fracture, Sonic Terrain and the Mutation Lab.",
                      16, 612, 384, 60, juce::Justification::topLeft, 3);
}

void LabPage::resized()
{
    auto& m = *impl;
    m.matrix.setBounds (0, 0, 416, 420);
    m.global.setBounds (0, 432, 416, 132);
    m.osc.setBounds (428, 0, 820, 420);
    m.filter.setBounds (428, 432, 432, 250);
    m.lfo.setBounds (872, 432, 376, 250);
}

} // namespace divinity::ui
