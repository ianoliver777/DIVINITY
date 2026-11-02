#include "Panels.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

static juce::Path dotsPath()
{
    juce::Path p;
    for (int i = 0; i < 3; ++i) p.addEllipse (3.0f + i * 7.0f, 10.0f, 3.2f, 3.2f);
    return p;
}

//==============================================================================
void Panel::paint (juce::Graphics& g)
{
    drawPanelBackground (g, getLocalBounds().toFloat(), highlighted);
    if (title.isNotEmpty())
        drawSectionTitle (g, { 16, 8, getWidth() - 32, 26 }, title);
}

//==============================================================================
// OscCard
//==============================================================================
OscCard::OscCard (DivinityProcessor& p, int i)
    : proc (p), index (i),
      power (p.apvts, ids::osc (i, "on")),
      engine (p.apvts, ids::osc (i, "engine")),
      display (p.apvts, p.getWavetables(), i),
      more ("More oscillator settings", dotsPath(), true)
{
    auto& s = p.apvts;
    const auto posDest = (ModDest) ((int) ModDest::OscAPos + i);
    const auto lvlDest = (ModDest) ((int) ModDest::OscALevel + i);
    const auto foldDest = (ModDest) ((int) ModDest::OscAFold + i);
    const auto pwmDest = (ModDest) ((int) ModDest::OscAPwm + i);
    const auto detDest = (ModDest) ((int) ModDest::OscADetune + i);

    wtKnobs.add (new Knob (s, ids::osc (i, "pos"), "Pos", posDest));
    wtKnobs.add (new Knob (s, ids::osc (i, "fold"), "Fold", foldDest));
    wtKnobs.add (new Knob (s, ids::osc (i, "unison"), "Unison"));
    wtKnobs.add (new Knob (s, ids::osc (i, "detune"), "Detune", detDest));
    wtKnobs.add (new Knob (s, ids::osc (i, "level"), "Level", lvlDest));

    anKnobs.add (new Knob (s, ids::osc (i, "oct"), "Oct", ModDest::None, 1.0f, true));
    anKnobs.add (new Knob (s, ids::osc (i, "pw"), "PWM", pwmDest, 0.5f));
    anKnobs.add (new Knob (s, ids::osc (i, "detune"), "Detune", detDest));
    anKnobs.add (new Knob (s, ids::osc (i, "phase"), "Phase"));
    anKnobs.add (new Knob (s, ids::osc (i, "level"), "Level", lvlDest));

    for (auto* k : wtKnobs) addChildComponent (k);
    for (auto* k : anKnobs) addChildComponent (k);
    addAndMakeVisible (power);
    addAndMakeVisible (engine);
    addAndMakeVisible (display);
    addAndMakeVisible (more);
    more.onClick = [this] { showMore(); };
    engine.onChange = [this] { updateEngine(); };
    power.onStateChange = [this] { repaint(); };
    updateEngine();
}

void OscCard::updateEngine()
{
    const bool wt = engine.getIndex() == 0;
    for (auto* k : wtKnobs) k->setVisible (wt);
    for (auto* k : anKnobs) k->setVisible (! wt);
    display.repaint();
}

void OscCard::paint (juce::Graphics& g)
{
    drawCardBackground (g, getLocalBounds().toFloat(), power.getToggleState());
    g.setColour (power.getToggleState() ? text : textDim);
    g.setFont (font (14.0f, Weight::Medium, 0.12f));
    g.drawText ("OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + index)), 44, 8, 120, 22, juce::Justification::centredLeft);
}

void OscCard::resized()
{
    const int w = getWidth();
    power.setBounds (12, 7, 24, 24);
    more.setBounds (w - 42, 6, 32, 26);
    engine.setBounds (10, 40, w - 20, 26);
    display.setBounds (10, 72, w - 20, 98);
    auto row = juce::Rectangle<int> (6, 174, w - 12, 64);
    const int kw = row.getWidth() / 5;
    for (int k = 0; k < 5; ++k)
    {
        auto cell = row.withX (row.getX() + k * kw).withWidth (kw);
        wtKnobs[k]->setBounds (cell);
        anKnobs[k]->setBounds (cell);
    }
}

namespace
{
    /** Container used inside call-out boxes: a row of knobs (+ optional extra widgets). */
    class KnobStrip : public juce::Component
    {
    public:
        explicit KnobStrip (juce::String t) : title (std::move (t)) {}
        juce::OwnedArray<juce::Component> items;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAttachments;   // destroyed before items
        juce::String title;
        void add (juce::Component* c, int width = 72) { items.add (c); widths.add (width); addAndMakeVisible (c); }
        void paint (juce::Graphics& g) override
        {
            g.setColour (textDim);
            g.setFont (font (11.5f, Weight::Medium, 0.2f));
            g.drawText (title.toUpperCase(), 14, 6, getWidth() - 28, 18, juce::Justification::centredLeft);
        }
        void resized() override
        {
            int x = 10;
            for (int i = 0; i < items.size(); ++i)
            {
                const int w = widths[i];
                auto* c = items[i];
                if (dynamic_cast<Knob*> (c) != nullptr) c->setBounds (x, 28, w, 72);
                else c->setBounds (x, 50, w, 26);
                x += w + 6;
            }
        }
        int preferredWidth() const { int w = 14; for (auto v : widths) w += v + 6; return w + 4; }
    private:
        juce::Array<int> widths;
    };
}

void OscCard::showMore()
{
    auto& s = proc.apvts;
    auto strip = std::make_unique<KnobStrip> ("Osc " + juce::String::charToString ((juce::juce_wchar) ('A' + index)) + " - more");
    strip->add (new Knob (s, ids::osc (index, "semi"), "Semi", ModDest::None, 1.0f, true));
    strip->add (new Knob (s, ids::osc (index, "fine"), "Fine", ModDest::None, 1.0f, true));
    strip->add (new Knob (s, ids::osc (index, "unison"), "Unison"));
    strip->add (new Knob (s, ids::osc (index, "width"), "Width"));
    strip->add (new Knob (s, ids::osc (index, "fold"), "Fold", (ModDest) ((int) ModDest::OscAFold + index)));
    strip->add (new Knob (s, ids::osc (index, "phase"), "Phase"));
    strip->add (new Knob (s, ids::osc (index, "oct"), "Oct", ModDest::None, 1.0f, true));
    auto* rp = new juce::TextButton ("RAND PHASE");
    rp->setClickingTogglesState (true);
    rp->setTooltip (helpTextFor (ids::osc (index, "randphase")));
    strip->add (rp, 100);
    strip->buttonAttachments.push_back (std::make_unique<APVTS::ButtonAttachment> (s, ids::osc (index, "randphase"), *rp));
    strip->setSize (strip->preferredWidth(), 108);
    juce::CallOutBox::launchAsynchronously (std::move (strip), more.getScreenBounds(), nullptr);
}

//==============================================================================
LayerCard::LayerCard (DivinityProcessor& p, bool isNoise)
    : noise (isNoise),
      power (p.apvts, isNoise ? ids::noiseOn : ids::subOn),
      mode (p.apvts, isNoise ? ids::noiseType : ids::subMode),
      level (p.apvts, isNoise ? ids::noiseLevel : ids::subLevel, "Level", isNoise ? ModDest::NoiseLevel : ModDest::SubLevel)
{
    addAndMakeVisible (power);
    addAndMakeVisible (mode);
    addAndMakeVisible (level);
    power.onStateChange = [this] { repaint(); };
}

void LayerCard::paint (juce::Graphics& g)
{
    drawCardBackground (g, getLocalBounds().toFloat(), power.getToggleState());
    g.setColour (power.getToggleState() ? text : textDim);
    g.setFont (font (12.5f, Weight::Medium, 0.12f));
    g.drawText (noise ? "NOISE" : "SUB", 38, 6, 100, 20, juce::Justification::centredLeft);
}

void LayerCard::resized()
{
    power.setBounds (10, 5, 22, 22);
    mode.setBounds (8, 32, getWidth() - 16, 22);
    level.setBounds ((getWidth() - 60) / 2, 58, 60, getHeight() - 62);
}

//==============================================================================
OscillatorSection::OscillatorSection (DivinityProcessor& p)
    : Panel ("OSCILLATORS"), sub (p, false), noise (p, true)
{
    for (int i = 0; i < kNumOscs; ++i) addAndMakeVisible (oscs.add (new OscCard (p, i)));
    addAndMakeVisible (sub);
    addAndMakeVisible (noise);
}

void OscillatorSection::resized()
{
    oscs[0]->setBounds (13, 41, 300, 242);
    oscs[1]->setBounds (326, 41, 298, 242);
    oscs[2]->setBounds (637, 41, 297, 242);
    sub.setBounds (947, 41, 148, 115);
    noise.setBounds (947, 162, 148, 121);
}

//==============================================================================
// FilterSection
//==============================================================================
FilterSection::FilterSection (DivinityProcessor& p)
    : Panel(), proc (p),
      tabs ({ "FILTER 1", "FILTER 2" }),
      routing ("Filter routing: lit = Parallel, dim = Serial (Filter 1 into Filter 2)", icons::link ({ 0, 0, 24, 24 })),
      routingAttachment (*p.apvts.getParameter (ids::filterRouting),
                         [this] (float v) { routing.setToggleState (v > 0.5f, juce::dontSendNotification); }, p.apvts.undoManager),
      response (p.apvts, 0)
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (routing);
    addAndMakeVisible (response);
    routing.onClick = [this] { routingAttachment.setValueAsCompleteGesture (routing.getToggleState() ? 0.0f : 1.0f); };
    routingAttachment.sendInitialUpdate();

    for (int f = 0; f < 2; ++f)
    {
        auto& s = p.apvts;
        addChildComponent (types.add (new ChoiceBox (s, ids::filt (f, "type"))));
        addChildComponent (keytrack.add (new NumberBox (s, ids::filt (f, "keytrack"))));
        const auto cut = f == 0 ? ModDest::Filter1Cutoff : ModDest::Filter2Cutoff;
        const auto res = f == 0 ? ModDest::Filter1Res : ModDest::Filter2Res;
        const auto drv = f == 0 ? ModDest::Filter1Drive : ModDest::Filter2Drive;
        knobs[f].add (new Knob (s, ids::filt (f, "cutoff"), "Cutoff", cut, 0.6f));
        knobs[f].add (new Knob (s, ids::filt (f, "res"), "Res", res));
        knobs[f].add (new Knob (s, ids::filt (f, "drive"), "Drive", drv));
        knobs[f].add (new Knob (s, ids::filt (f, "mix"), "Mix"));
        for (auto* k : knobs[f]) addChildComponent (k);
    }
    tabs.onSelect = [this] (int i) { select (i); };
    select (0);
}

void FilterSection::select (int f)
{
    current = f;
    for (int i = 0; i < 2; ++i)
    {
        types[i]->setVisible (i == f);
        keytrack[i]->setVisible (i == f);
        for (auto* k : knobs[i]) k->setVisible (i == f);
    }
    response.setFilter (f);
}

void FilterSection::paint (juce::Graphics& g)
{
    Panel::paint (g);
    g.setColour (textDim);
    g.setFont (font (10.5f, Weight::Medium, 0.08f));
    g.drawText ("KEY", 296, 13, 30, 22, juce::Justification::centredRight);
}

void FilterSection::resized()
{
    tabs.setBounds (14, 8, 200, 32);
    routing.setBounds (222, 10, 30, 28);
    for (auto* k : keytrack) k->setBounds (332, 12, 62, 24);
    for (auto* t : types) t->setBounds (25, 60, 236, 30);
    response.setBounds (15, 100, 247, 140);
    const int cols[2] = { 286, 360 }, rows[2] = { 60, 150 };
    for (int f = 0; f < 2; ++f)
        for (int k = 0; k < 4; ++k)
            knobs[f][k]->setBounds (cols[k % 2], rows[k / 2], 66, 82);
}

//==============================================================================
EnvelopeSection::EnvelopeSection (DivinityProcessor& p)
    : Panel ("ENVELOPE"), tabs ({ "ENV 1", "ENV 2", "ENV 3" }), display (p.apvts, 0)
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (display);
    for (int e = 0; e < kNumEnvs; ++e)
    {
        auto& s = p.apvts;
        knobs[e].add (new Knob (s, ids::env (e, "attack"), "A"));
        knobs[e].add (new Knob (s, ids::env (e, "decay"), "D"));
        knobs[e].add (new Knob (s, ids::env (e, "sustain"), "S"));
        knobs[e].add (new Knob (s, ids::env (e, "release"), "R"));
        knobs[e].add (new Knob (s, ids::env (e, "curve"), "Curve", ModDest::None, 1.0f, true));
        for (auto* k : knobs[e]) addChildComponent (k);
    }
    tabs.setTooltip ("ENV 1 always shapes the volume. ENV 2 and ENV 3 are free modulation sources for the matrix.");
    tabs.onSelect = [this] (int i) { select (i); };
    select (0);
}

void EnvelopeSection::select (int e)
{
    current = e;
    for (int i = 0; i < kNumEnvs; ++i)
        for (auto* k : knobs[i]) k->setVisible (i == e);
    display.setEnvelope (e);
}

void EnvelopeSection::resized()
{
    tabs.setBounds (12, 38, getWidth() - 24, 30);
    display.setBounds (14, 76, getWidth() - 28, 108);
    const int kw = (getWidth() - 20) / 5;
    for (int e = 0; e < kNumEnvs; ++e)
        for (int k = 0; k < 5; ++k)
            knobs[e][k]->setBounds (10 + k * kw, 188, kw, 62);
}

//==============================================================================
namespace
{
    class ShapeButtons : public juce::Component
    {
    public:
        ShapeButtons (APVTS& s, const juce::String& id)
            : param (*s.getParameter (id)),
              attachment (param, [this] (float v)
              {
                  for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
              }, s.undoManager)
        {
            const char* names[] = { "Sine", "Square", "Triangle", "Random" };
            for (int i = 0; i < 4; ++i)
            {
                auto* b = buttons.add (new IconButton (names[i], shape (i)));
                b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
                addAndMakeVisible (b);
            }
            attachment.sendInitialUpdate();
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (panel2.withAlpha (0.6f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
            for (int i = 0; i < buttons.size(); ++i)
                if (buttons[i]->getToggleState())
                {
                    g.setColour (accentDeep.withAlpha (0.9f));
                    g.fillRoundedRectangle (buttons[i]->getBounds().toFloat().reduced (1.0f), 3.0f);
                }
        }
        void resized() override
        {
            auto r = getLocalBounds();
            const int w = r.getWidth() / 4;
            for (auto* b : buttons) b->setBounds (r.removeFromLeft (w));
        }
    private:
        static juce::Path shape (int i)
        {
            juce::Path p;
            switch (i)
            {
                case 0: p.startNewSubPath (0, 6); p.cubicTo (3, -2, 5, -2, 8, 6); p.cubicTo (11, 14, 13, 14, 16, 6); break;
                case 1: p.startNewSubPath (0, 12); p.lineTo (0, 0); p.lineTo (8, 0); p.lineTo (8, 12); p.lineTo (16, 12); p.lineTo (16, 0); break;
                case 2: p.startNewSubPath (0, 12); p.lineTo (8, 0); p.lineTo (16, 12); break;
                default: p.startNewSubPath (0, 8); p.lineTo (4, 8); p.lineTo (4, 2); p.lineTo (8, 2); p.lineTo (8, 11); p.lineTo (12, 11); p.lineTo (12, 5); p.lineTo (16, 5); break;
            }
            return p;
        }
        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;
        juce::OwnedArray<IconButton> buttons;
    };
}

LfoSection::LfoSection (DivinityProcessor& p)
    : Panel ("LFO"), proc (p), tabs ({ "LFO 1", "LFO 2", "LFO 3", "LFO 4" }), display (p, 0)
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (display);
    auto& s = p.apvts;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& L = lfos[l];
        L.shapes = std::make_unique<ShapeButtons> (s, ids::lfo (l, "shape"));
        L.sync = std::make_unique<juce::TextButton> ("SYNC");
        L.sync->setClickingTogglesState (true);
        L.sync->setTooltip (helpTextFor (ids::lfo (l, "sync")));
        L.syncAttach = std::make_unique<APVTS::ButtonAttachment> (s, ids::lfo (l, "sync"), *L.sync);
        L.division = std::make_unique<ChoiceBox> (s, ids::lfo (l, "div"));
        L.knobs.add (new Knob (s, ids::lfo (l, "rate"), "Rate", (ModDest) ((int) ModDest::Lfo1Rate + l), 0.3f));
        L.knobs.add (new Knob (s, ids::lfo (l, "depth"), "Depth"));
        L.knobs.add (new Knob (s, ids::lfo (l, "phase"), "Phase"));
        L.knobs.add (new Knob (s, ids::lfo (l, "smooth"), "Smooth"));
        auto* syncBtn = L.sync.get();
        auto* rate = L.knobs[0];
        auto* div = L.division.get();
        L.sync->onStateChange = [syncBtn, rate, div]
        {
            const bool on = syncBtn->getToggleState();
            rate->setAlpha (on ? 0.35f : 1.0f);
            div->setEnabled (on);
            div->setAlpha (on ? 1.0f : 0.45f);
        };
        L.sync->onStateChange();
        addChildComponent (*L.shapes);
        addChildComponent (*L.sync);
        addChildComponent (*L.division);
        for (auto* k : L.knobs) addChildComponent (k);
    }
    tabs.onSelect = [this] (int i) { select (i); };
    select (0);
}

void LfoSection::select (int l)
{
    current = l;
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& L = lfos[i];
        const bool v = i == l;
        L.shapes->setVisible (v); L.sync->setVisible (v); L.division->setVisible (v);
        for (auto* k : L.knobs) k->setVisible (v);
    }
    display.setLfo (l);
}

void LfoSection::resized()
{
    tabs.setBounds (12, 38, getWidth() - 24, 30);
    display.setBounds (14, 74, getWidth() - 28, 82);
    for (auto& L : lfos)
    {
        L.shapes->setBounds (16, 162, 150, 24);
        L.sync->setBounds (184, 162, 52, 24);
        L.division->setBounds (240, 162, getWidth() - 254, 24);
        const int kw = (getWidth() - 20) / 4;
        for (int k = 0; k < 4; ++k) L.knobs[k]->setBounds (10 + k * kw, 190, kw, 60);
    }
}

//==============================================================================
// ModMatrixSection
//==============================================================================
ModMatrixSection::ModMatrixSection (DivinityProcessor& p) : Panel ("MODULATION MATRIX"), proc (p)
{
    add.onClick = [this] { addRouting(); };
    addAndMakeVisible (add);

    auto& s = p.apvts;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& r = rows[i];
        r.src = std::make_unique<ChoiceBox> (s, ids::mod (i, "src"));
        r.dst = std::make_unique<ChoiceBox> (s, ids::mod (i, "dst"));
        r.amount = std::make_unique<FineSlider>();
        r.amount->setSliderStyle (juce::Slider::LinearHorizontal);
        r.amount->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        r.amount->setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        r.amount->setDoubleClickReturnValue (true, 0.0);
        r.amount->setTooltip ("Amount: drag left for negative, right for positive. Double-click to zero. Right-click for options.");
        r.attach = std::make_unique<APVTS::SliderAttachment> (s, ids::mod (i, "amt"), *r.amount);
        auto* slider = r.amount.get();
        r.amount->onPopup = [this, i, slider]
        {
            juce::PopupMenu m;
            m.addItem (1, "Enter amount...");
            m.addItem (2, "Clear this routing");
            juce::Component::SafePointer<juce::Component> safe (slider);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slider), [this, i, safe] (int res)
            {
                if (safe == nullptr) return;
                auto& st = proc.apvts;
                if (res == 1) showParameterMenu (*safe, *st.getParameter (ids::mod (i, "amt")));
                if (res == 2)
                    for (auto* n : { "src", "dst", "amt" })
                    {
                        auto* prm = st.getParameter (ids::mod (i, n));
                        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture();
                    }
            });
        };
        addAndMakeVisible (*r.src);
        addAndMakeVisible (*r.dst);
        addAndMakeVisible (*r.amount);
    }
    startTimerHz (10);
}

void ModMatrixSection::addRouting()
{
    auto& s = proc.apvts;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        if (s.getRawParameterValue (ids::mod (i, "src"))->load() < 0.5f && s.getRawParameterValue (ids::mod (i, "dst"))->load() < 0.5f)
        {
            auto set = [&s, i] (const char* n, float real)
            {
                auto* p = s.getParameter (ids::mod (i, n));
                p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (real)); p->endChangeGesture();
            };
            set ("src", (float) ModSource::Lfo1);
            set ("dst", (float) ModDest::Filter1Cutoff);
            set ("amt", 0.25f);
            return;
        }
    }
}

void ModMatrixSection::paint (juce::Graphics& g)
{
    Panel::paint (g);
    g.setColour (textDim);
    g.setFont (font (10.5f, Weight::Medium, 0.1f));
    g.drawText ("SOURCE", 14, 50, 100, 16, juce::Justification::centredLeft);
    g.drawText ("DESTINATION", 133, 50, 120, 16, juce::Justification::centredLeft);
    g.drawText ("AMOUNT", 278, 50, 80, 16, juce::Justification::centredLeft);

    g.setFont (font (11.5f, Weight::Medium));
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const float v = proc.apvts.getRawParameterValue (ids::mod (i, "amt"))->load();
        const int y = 76 + i * 38;
        const int pct = juce::roundToInt (v * 100.0f);
        const bool active = proc.apvts.getRawParameterValue (ids::mod (i, "src"))->load() > 0.5f
                         && proc.apvts.getRawParameterValue (ids::mod (i, "dst"))->load() > 0.5f;
        g.setColour (active ? text : textFaint);
        g.drawText ((pct > 0 ? "+" : "") + juce::String (pct) + "%", 278, y - 1, 70, 14, juce::Justification::centredLeft);
    }
}

void ModMatrixSection::resized()
{
    add.setBounds (getWidth() - 42, 6, 32, 32);
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int y = 76 + i * 38;
        rows[i].src->setBounds (12, y, 112, 26);
        rows[i].dst->setBounds (131, y, 138, 26);
        rows[i].amount->setBounds (276, y + 10, getWidth() - 290, 18);
    }
}

//==============================================================================
MacroSection::MacroSection (DivinityProcessor& p, bool big) : Panel ("MACROS"), large (big)
{
    const char* names[] = { "Tone", "Movement", "Texture", "Space" };
    const ModDest none = ModDest::None;
    for (int i = 0; i < kNumMacros; ++i)
    {
        auto* k = knobs.add (new Knob (p.apvts, ids::macro (i), names[i], none));
        k->setCaptionSize (large ? 13.0f : 11.5f);
        addAndMakeVisible (k);
    }
}

void MacroSection::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedTop (large ? 56 : 40).withTrimmedBottom (large ? 20 : 8);
    if (large)
    {
        // 2x2 grid of big knobs
        const int cw = r.getWidth() / 2, ch = r.getHeight() / 2;
        for (int i = 0; i < 4; ++i)
            knobs[i]->setBounds (juce::Rectangle<int> (r.getX() + (i % 2) * cw, r.getY() + (i / 2) * ch, cw, ch).reduced (18, 6));
    }
    else
    {
        const int w = r.getWidth() / 4;
        for (int i = 0; i < 4; ++i) knobs[i]->setBounds (r.getX() + i * w, r.getY(), w, r.getHeight());
    }
}

//==============================================================================
// FxSection
//==============================================================================
struct FxSection::Tile : public juce::Component
{
    struct Spec { juce::String title, onID, mainID, mainLabel; int iconKind; std::vector<std::pair<juce::String, juce::String>> knobs; juce::String choiceID, toggleID, toggleLabel; };

    Tile (DivinityProcessor& p, Spec sp)
        : proc (p), spec (std::move (sp)), power (p.apvts, spec.onID),
          more ("All " + spec.title + " settings", dotsPath(), true),
          mainParam (*p.apvts.getParameter (spec.mainID))
    {
        addAndMakeVisible (power);
        addAndMakeVisible (more);
        drag.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        drag.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        drag.setAlpha (0.0f);
        drag.setMouseDragSensitivity (200);
        drag.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        drag.setTooltip (helpTextFor (spec.mainID) + "\n\nDrag to adjust, double-click to reset, ... for all settings.");
        drag.onPopup = [this] { showParameterMenu (drag, mainParam); };
        drag.onValueChange = [this] { repaint(); };
        attach = std::make_unique<APVTS::SliderAttachment> (p.apvts, spec.mainID, drag);
        drag.setDoubleClickReturnValue (true, (double) mainParam.convertFrom0to1 (mainParam.getDefaultValue()));
        addAndMakeVisible (drag);
        power.onStateChange = [this] { repaint(); };
        more.onClick = [this] { showMore(); };
    }

    void paint (juce::Graphics& g) override
    {
        const bool on = power.getToggleState();
        drawCardBackground (g, getLocalBounds().toFloat(), on);
        g.setColour (on ? text : textDim);
        g.setFont (font (12.0f, Weight::Medium, 0.12f));
        g.drawText (spec.title.toUpperCase(), 34, 7, getWidth() - 70, 20, juce::Justification::centredLeft);

        // icon
        auto ic = juce::Rectangle<float> (14.0f, 38.0f, 28.0f, 22.0f);
        juce::Path p;
        switch (spec.iconKind)
        {
            case 0: p.startNewSubPath (ic.getX(), ic.getBottom()); p.cubicTo (ic.getX() + 12, ic.getBottom(), ic.getRight() - 12, ic.getY(), ic.getRight(), ic.getY()); break;
            case 1: for (int i = 0; i < 3; ++i) { const float x = ic.getX() + i * 7.0f; p.startNewSubPath (x, ic.getCentreY()); p.quadraticTo (x + 3.5f, ic.getY(), x + 7, ic.getCentreY()); p.quadraticTo (x + 10.5f, ic.getBottom(), x + 14, ic.getCentreY()); } break;
            case 2: p.addEllipse (ic.getX() + 3, ic.getY() + 2, 14, 18); p.addEllipse (ic.getX() + 11, ic.getY() + 2, 14, 18); break;
            case 3: p.addStar (ic.getCentre(), 6, 5.0f, 10.0f); break;
            default: p.startNewSubPath (ic.getX(), ic.getBottom()); p.lineTo (ic.getCentreX(), ic.getY() + 4); p.lineTo (ic.getRight(), ic.getY() + 4); break;
        }
        g.setColour (on ? textDim : textFaint);
        g.strokePath (p, juce::PathStrokeType (1.3f));

        g.setFont (font (12.5f));
        g.setColour (on ? textDim : textFaint);
        g.drawText (spec.mainLabel, 54, 38, 60, 22, juce::Justification::centredLeft);
        g.setColour (on ? text : textFaint);
        g.drawText (mainParam.getCurrentValueAsText(), 110, 38, getWidth() - 120, 22, juce::Justification::centredLeft);
    }

    void resized() override
    {
        power.setBounds (8, 6, 22, 22);
        more.setBounds (getWidth() - 36, 4, 30, 24);
        drag.setBounds (50, 32, getWidth() - 56, 34);
    }

    void showMore()
    {
        auto& s = proc.apvts;
        auto strip = std::make_unique<KnobStrip> (spec.title);
        if (spec.choiceID.isNotEmpty()) strip->add (new ChoiceBox (s, spec.choiceID), 90);
        for (auto& [id, label] : spec.knobs) strip->add (new Knob (s, id, label));
        if (spec.toggleID.isNotEmpty())
        {
            auto* b = new juce::TextButton (spec.toggleLabel);
            b->setClickingTogglesState (true);
            b->setTooltip (helpTextFor (spec.toggleID));
            strip->add (b, 96);
            strip->buttonAttachments.push_back (std::make_unique<APVTS::ButtonAttachment> (s, spec.toggleID, *b));
        }
        strip->setSize (strip->preferredWidth(), 108);
        juce::CallOutBox::launchAsynchronously (std::move (strip), more.getScreenBounds(), nullptr);
    }

    DivinityProcessor& proc;
    Spec spec;
    PowerButton power;
    IconButton more;
    juce::RangedAudioParameter& mainParam;
    FineSlider drag;
    std::unique_ptr<APVTS::SliderAttachment> attach;
};

FxSection::FxSection (DivinityProcessor& p) : Panel ("EFFECTS CHAIN"), proc (p)
{
    using S = Tile::Spec;
    std::vector<S> specs;
    specs.push_back (S { "Saturation", ids::satOn, ids::satDrive, "Drive", 0, { { ids::satDrive, "Drive" }, { ids::satMix, "Mix" } }, {}, {}, {} });
    specs.push_back (S { "Chorus", ids::choOn, ids::choDepth, "Depth", 1, { { ids::choRate, "Rate" }, { ids::choDepth, "Depth" }, { ids::choMix, "Mix" } }, {}, {}, {} });
    specs.push_back (S { "Delay", ids::dlyOn, ids::dlyDiv, "Time", 2, { { ids::dlyFeedback, "Feedback" }, { ids::dlyMix, "Mix" }, { ids::dlyTone, "Tone" } }, ids::dlyDiv, ids::dlyPingPong, "PING-PONG" });
    specs.push_back (S { "Reverb", ids::revOn, ids::revMix, "Mix", 3, { { ids::revSize, "Size" }, { ids::revDamp, "Damp" }, { ids::revWidth, "Width" }, { ids::revMix, "Mix" } }, {}, {}, {} });
    specs.push_back (S { "Limiter", ids::limOn, ids::limCeiling, "Ceiling", 4, { { ids::limCeiling, "Ceiling" } }, {}, {}, {} });
    for (auto& sp : specs) addAndMakeVisible (tiles.add (new Tile (p, sp)));
}

FxSection::~FxSection() = default;

void FxSection::paint (juce::Graphics& g)
{
    Panel::paint (g);
    g.setColour (textFaint);
    for (int i = 0; i + 1 < tiles.size(); ++i)
    {
        const auto b = tiles[i]->getBounds();
        const float x = (float) b.getRight() + 15.0f, y = (float) b.getCentreY();
        juce::Path p; p.startNewSubPath (x - 3, y - 6); p.lineTo (x + 3, y); p.lineTo (x - 3, y + 6);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
}

void FxSection::resized()
{
    const int gap = 32, n = tiles.size();
    const int w = (getWidth() - 40 - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i) tiles[i]->setBounds (20 + i * (w + gap), 42, w, 74);
}

//==============================================================================
GlobalSection::GlobalSection (DivinityProcessor& p)
    : Panel ("GLOBAL"),
      voices (p.apvts, ids::voices), bend (p.apvts, ids::bendRange), glide (p.apvts, ids::glide), tune (p.apvts, ids::masterTune),
      mode (p.apvts, ids::voiceMode, { "POLY", "LEGATO" })
{
    for (auto* c : std::initializer_list<juce::Component*> { &voices, &bend, &glide, &tune, &mode }) addAndMakeVisible (c);
}

void GlobalSection::paint (juce::Graphics& g)
{
    Panel::paint (g);
    g.setColour (textDim);
    g.setFont (font (11.0f, Weight::Medium, 0.08f));
    g.drawText ("VOICES", 22, 40, 70, 22, juce::Justification::centredLeft);
    g.drawText ("BEND", 22, 68, 70, 22, juce::Justification::centredLeft);
    g.drawText ("GLIDE", 22, 96, 70, 22, juce::Justification::centredLeft);
    g.drawText ("TUNE", 208, 40, 50, 22, juce::Justification::centredLeft);
}

void GlobalSection::resized()
{
    voices.setBounds (90, 40, 62, 22);
    bend.setBounds (90, 68, 62, 22);
    glide.setBounds (90, 96, 62, 22);
    tune.setBounds (261, 40, 138, 24);
    mode.setBounds (207, 84, 193, 28);
}

//==============================================================================
KeyboardBar::KeyboardBar (DivinityProcessor& p)
    : proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      master (p.apvts, ids::master, "Out"),
      meter (p, true)
{
    for (auto* s : { &pitch, &mod })
    {
        s->setSliderStyle (juce::Slider::LinearVertical);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (s);
    }
    pitch.setRange (-1.0, 1.0); pitch.setValue (0.0, juce::dontSendNotification);
    pitch.setTooltip ("Pitch wheel (springs back). Range is set by BEND in Global.");
    pitch.onValueChange = [this]
    {
        const int v = juce::jlimit (0, 16383, juce::roundToInt ((pitch.getValue() + 1.0) * 8191.5));
        proc.uiMidi.addMessageToQueue (juce::MidiMessage::pitchWheel (1, v).withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
    };
    pitch.onDragEnd = [this] { pitch.setValue (0.0, juce::sendNotificationSync); };
    mod.setRange (0.0, 1.0); mod.setValue (0.0, juce::dontSendNotification);
    mod.setTooltip ("Mod wheel (MIDI CC 1). Route it in the modulation matrix.");
    mod.onValueChange = [this]
    {
        proc.uiMidi.addMessageToQueue (juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (mod.getValue() * 127.0))
                                           .withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
    };

    keyboard.setAvailableRange (36, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9e8ee));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141519));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9a98a6));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accentStrong.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, panel2);
    keyboard.setWantsKeyboardFocus (true);
    keyboard.setKeyPressBaseOctave (4);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (master);
    addAndMakeVisible (meter);
}

void KeyboardBar::paint (juce::Graphics& g)
{
    drawPanelBackground (g, getLocalBounds().toFloat());
    g.setColour (textDim);
    g.setFont (font (11.0f, Weight::Medium, 0.1f));
    g.drawText ("PITCH", 30, getHeight() - 24, 64, 16, juce::Justification::centred);
    g.drawText ("MOD", 84, getHeight() - 24, 64, 16, juce::Justification::centred);
}

void KeyboardBar::resized()
{
    const int w = getWidth();
    pitch.setBounds (50, 12, 24, getHeight() - 40);
    mod.setBounds (104, 12, 24, getHeight() - 40);
    const int kbW = w - 178 - 200;
    keyboard.setBounds (178, 10, kbW, getHeight() - 20);
    keyboard.setKeyWidth ((float) kbW / 36.0f);
    master.setBounds (w - 178, 12, 90, getHeight() - 20);
    meter.setBounds (w - 58, 12, 22, getHeight() - 24);
}

} // namespace divinity::ui
