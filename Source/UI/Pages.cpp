#include "Pages.h"
#include "MidiGenPanel.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

//==============================================================================
// SpectrumAnalyzer
//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer (DivinityProcessor& p) : proc (p)
{
    for (auto* c : { &post, &pre }) { c->ring.assign ((size_t) kSize, 0.0f); c->smooth.assign ((size_t) kSize / 2, -120.0f); }
    pull.assign ((size_t) ScopeFifo::kSize, 0.0f);
    fftData.assign ((size_t) kSize * 2, 0.0f);
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void SpectrumAnalyzer::analyse (Chan& c, ScopeFifo& fifo, juce::int64& cursor)
{
    const int n = fifo.pull (cursor, pull.data(), (int) pull.size());
    for (int i = 0; i < n; ++i) { c.ring[(size_t) c.writePos] = pull[(size_t) i]; c.writePos = (c.writePos + 1) % kSize; }
    for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = c.ring[(size_t) ((c.writePos + i) % kSize)];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    for (int b = 0; b < kSize / 2; ++b)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * 4.0f / kSize, -120.0f);
        auto& s = c.smooth[(size_t) b];
        s = db > s ? s + 0.6f * (db - s) : s + 0.15f * (db - s);
    }
}

void SpectrumAnalyzer::timerCallback()
{
    if (! visibleInTree (*this)) return;   // every display has its own cursor: nothing to drain
    analyse (post, proc.scope, postCursor);
    analyse (pre, proc.scopePre, preCursor);
    repaint();
}

juce::Path SpectrumAnalyzer::curveFor (const Chan& c, juce::Rectangle<float> r, double sr) const
{
    juce::Path curve;
    const int points = juce::jmax (64, (int) r.getWidth());
    const float bottom = r.getBottom() - 16.0f, top = r.getY() + 6.0f;
    for (int i = 0; i <= points; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / points);
        const double bin = f / sr * kSize;
        const int b0 = juce::jlimit (1, kSize / 2 - 2, (int) bin);
        const float frac = (float) juce::jlimit (0.0, 1.0, bin - b0);
        const float db = c.smooth[(size_t) b0] + frac * (c.smooth[(size_t) b0 + 1] - c.smooth[(size_t) b0]) + 4.5f * (float) std::log2 (f / 1000.0);  // +4.5 dB/oct tilt
        const float y = juce::jmap (juce::jlimit (-84.0f, 0.0f, db), -84.0f, 0.0f, bottom, top);
        const float x = r.getX() + r.getWidth() * (float) i / points;
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    return curve;
}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const double sr = juce::jmax (8000.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);

    g.setFont (font (11.5f, Weight::Medium));
    const double freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* labels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    auto xFor = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    for (int i = 0; i < 10; ++i)
    {
        const float x = juce::jlimit (r.getX() + 10.0f, r.getRight() - 12.0f, xFor (freqs[i]));
        g.setColour (accent.withAlpha (0.05f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom() - 16.0f);
        g.setColour (text.withAlpha (0.9f));
        g.drawText (labels[i], (int) x - 20, (int) r.getBottom() - 15, 40, 13, juce::Justification::centred);
    }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) (r.getY() + (r.getHeight() - 14.0f) * i / 4.0f), r.getX(), r.getRight());
    }
    const float bottom = r.getBottom() - 16.0f, top = r.getY() + 6.0f;
    auto drawCurve = [&] (const Chan& c, juce::Colour col, bool filled)
    {
        auto curve = curveFor (c, r, sr);
        if (filled)
        {
            juce::Path fill (curve);
            fill.lineTo (r.getRight(), bottom); fill.lineTo (r.getX(), bottom); fill.closeSubPath();
            juce::ColourGradient grad (col.withAlpha (0.75f), 0, top, col.withAlpha (0.04f), 0, bottom, false);
            grad.addColour (0.35, col.withAlpha (0.45f));
            g.setGradientFill (grad);
            g.fillPath (fill);
            strokeGlow (g, curve, 1.3f, col);
            return;
        }
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (1.5f));
    };
    if (mode == 1) drawCurve (pre, accent, true);
    else if (mode == 2) { drawCurve (post, accent, true); drawCurve (pre, juce::Colour (0xffb58cff), false); }
    else drawCurve (post, accent, true);
}

//==============================================================================
// StereoImager
//==============================================================================
StereoImager::StereoImager (DivinityProcessor& p) : proc (p)
{
    ringL.assign (kSize, 0.0f); ringR.assign (kSize, 0.0f);
    pullL.assign (ScopeFifo::kSize, 0.0f); pullR.assign (ScopeFifo::kSize, 0.0f);
    fl.assign (kSize * 2, 0.0f); fr.assign (kSize * 2, 0.0f);
    setTooltip ("Spectral stereo imager. Each dot is a frequency: left/right is where it sits in the stereo field, "
                "the streak is how wide it is, low end at the bottom. Correlation: +1 mono-safe, 0 very wide, below 0 phase problems.");
    startTimerHz (30);
}

juce::Rectangle<float> StereoImager::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedLeft (30.0f).withTrimmedBottom (34.0f);
}

void StereoImager::resized()
{
    auto a = plotArea();
    trail = juce::Image (juce::Image::ARGB, juce::jmax (1, (int) a.getWidth()), juce::jmax (1, (int) a.getHeight()), true);
}

void StereoImager::timerCallback()
{
    if (! visibleInTree (*this) || ! trail.isValid()) return;
    const int n = proc.scope.pull (cursor, nullptr, (int) pullL.size(), pullL.data(), pullR.data());
    for (int i = 0; i < n; ++i)
    {
        ringL[(size_t) writePos] = pullL[(size_t) i];
        ringR[(size_t) writePos] = pullR[(size_t) i];
        writePos = (writePos + 1) & (kSize - 1);
    }

    // time-domain correlation / width / level over the analysis window
    double sLR = 0, sLL = 0, sRR = 0, sM = 0, sS = 0;
    for (int i = 0; i < kSize; ++i)
    {
        const float l = ringL[(size_t) ((writePos + i) & (kSize - 1))], r = ringR[(size_t) ((writePos + i) & (kSize - 1))];
        fl[(size_t) i] = l; fr[(size_t) i] = r;
        sLR += l * r; sLL += l * l; sRR += r * r;
        const float m = 0.5f * (l + r), sd = 0.5f * (l - r);
        sM += m * m; sS += sd * sd;
    }
    const bool silent = sLL + sRR < 1.0e-9;
    const float corr = silent ? 1.0f : (float) (sLR / std::sqrt (juce::jmax (1.0e-20, sLL * sRR)));
    const float wid = silent ? 0.0f : (float) (std::sqrt (sS) / (std::sqrt (sM) + std::sqrt (sS) + 1.0e-12));
    correlation += 0.2f * (corr - correlation);
    width += 0.2f * (wid - width);
    level = silent ? 0.0f : (float) std::sqrt ((sLL + sRR) / (2.0 * kSize));

    // fade the trail, then draw this frame's bins
    trail.multiplyAllAlphas (0.78f);
    if (! silent)
    {
        std::fill (fl.begin() + kSize, fl.end(), 0.0f);
        std::fill (fr.begin() + kSize, fr.end(), 0.0f);
        window.multiplyWithWindowingTable (fl.data(), (size_t) kSize);
        window.multiplyWithWindowingTable (fr.data(), (size_t) kSize);
        fft.performRealOnlyForwardTransform (fl.data(), true);
        fft.performRealOnlyForwardTransform (fr.data(), true);

        juce::Graphics g (trail);
        const float w = (float) trail.getWidth(), h = (float) trail.getHeight(), cx = w * 0.5f;
        const double sr = juce::jmax (8000.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
        const float norm = 4.0f / (float) kSize;
        for (int b = 1; b < kSize / 2; ++b)
        {
            const double f = b * sr / kSize;
            if (f < 20.0 || f > 20000.0) continue;
            const std::complex<float> L (fl[(size_t) (2 * b)], fl[(size_t) (2 * b + 1)]), R (fr[(size_t) (2 * b)], fr[(size_t) (2 * b + 1)]);
            const float aL = std::abs (L), aR = std::abs (R);
            const float mag = 0.5f * (aL + aR) * norm;
            const float db = juce::Decibels::gainToDecibels (mag, -120.0f);
            if (db < -80.0f) continue;
            const float pan = (aR * aR - aL * aL) / (aR * aR + aL * aL + 1.0e-20f);              // -1 left .. +1 right
            const float mid = std::abs (L + R), side = std::abs (L - R);
            const float spread = side / (mid + side + 1.0e-20f);                               // 0 mono .. 1 wide / out of phase
            const float y = h - h * (float) (std::log (f / 20.0) / std::log (1000.0));
            const float x = cx + pan * cx * 0.92f;
            const float a = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -80.0f, -12.0f, 0.06f, 1.0f));
            const auto col = colours::accent.interpolatedWith (juce::Colour (0xffc58cff), juce::jlimit (0.0f, 1.0f, spread * 1.4f));
            const float half = spread * cx * 0.9f * a;   // quiet bins (window leakage) get short streaks
            if (half > 1.0f)
            {
                g.setColour (col.withAlpha (a * 0.35f));
                g.fillRect (juce::Rectangle<float> (x - half, y - 0.6f, half * 2.0f, 1.2f));
            }
            g.setColour (col.withAlpha (a));
            g.fillEllipse (x - 1.6f, y - 1.6f, 3.2f, 3.2f);
            if (a > 0.6f) { g.setColour (colours::glow.withAlpha ((a - 0.6f) * 1.5f)); g.fillEllipse (x - 0.9f, y - 0.9f, 1.8f, 1.8f); }
        }
    }
    repaint();
}

void StereoImager::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const auto a = plotArea();

    // grid: L / C / R columns and decade rows
    g.setColour (accent.withAlpha (0.06f));
    for (int k = 1; k < 4; ++k) g.drawVerticalLine ((int) (a.getX() + a.getWidth() * k / 4.0f), a.getY(), a.getBottom());
    g.setColour (accent.withAlpha (0.16f));
    g.drawVerticalLine ((int) a.getCentreX(), a.getY(), a.getBottom());
    g.setFont (font (10.5f, Weight::Medium));
    const double freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* labels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        const float y = a.getBottom() - a.getHeight() * (float) (std::log (freqs[i] / 20.0) / std::log (1000.0));
        g.setColour (accent.withAlpha (0.05f));
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (textDim);
        g.drawText (labels[i], (int) r.getX() + 4, (int) y - 7, 32, 14, juce::Justification::centredRight);
    }
    g.drawImage (trail, a);

    // stereo labels
    g.setColour (text);
    g.setFont (font (11.0f, Weight::Medium, 0.1f));
    g.drawText ("L", (int) a.getX(), (int) a.getBottom() + 2, 20, 14, juce::Justification::centredLeft);
    g.drawText ("C", (int) a.getCentreX() - 10, (int) a.getBottom() + 2, 20, 14, juce::Justification::centred);
    g.drawText ("R", (int) a.getRight() - 20, (int) a.getBottom() + 2, 20, 14, juce::Justification::centredRight);

    // correlation bar (-1 .. +1) and readouts
    const float by = a.getBottom() + 20.0f;
    auto bar = juce::Rectangle<float> (a.getX() + 110.0f, by + 2.0f, a.getWidth() - 220.0f, 6.0f);
    g.setColour (colours::meterBg); g.fillRoundedRectangle (bar, 3.0f);
    const float cxBar = bar.getCentreX(), px = cxBar + juce::jlimit (-1.0f, 1.0f, correlation) * bar.getWidth() * 0.5f;
    const bool bad = correlation < 0.0f && level > 0.0f;
    auto lit = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cxBar, px), bar.getY(), juce::jmax (cxBar, px), bar.getBottom());
    g.setColour ((bad ? juce::Colour (0xffff6b7a) : accent).withAlpha (level > 0.0f ? 0.9f : 0.25f)); g.fillRoundedRectangle (lit, 3.0f);
    g.setColour (glow); g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ px, bar.getCentreY() }));
    g.setColour (textDim);
    g.setFont (font (10.0f, Weight::Medium));
    g.drawText ("-1", (int) bar.getX() - 20, (int) by - 3, 16, 14, juce::Justification::centredRight);
    g.drawText ("+1", (int) bar.getRight() + 4, (int) by - 3, 16, 14, juce::Justification::centredLeft);
    g.setFont (font (11.0f, Weight::Medium, 0.06f));
    g.setColour (text);
    g.drawText ("CORR " + juce::String (correlation, 2), (int) a.getX(), (int) by - 4, 96, 16, juce::Justification::centredLeft);
    g.drawText ("WIDTH " + juce::String (juce::roundToInt (width * 100.0f)) + "%", (int) a.getRight() - 96, (int) by - 4, 96, 16, juce::Justification::centredRight);
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
                    g.setGradientFill (juce::ColourGradient (colours::selTop, 0, r.getY(), colours::selBottom, 0, r.getBottom(), false));
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
            const char* names[] = { "Tone", "Movement", "Texture", "Space", "Character", "Width", "Punch", "Air" };
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
            for (int i = 0; i < kNumMacros; ++i) g.drawText (juce::String (i + 1), 16, knobs[i]->getY() + 20, 20, 24, juce::Justification::centredLeft);
        }
        void resized() override
        {
            const int h = (getHeight() - 52) / kNumMacros;
            for (int i = 0; i < kNumMacros; ++i) knobs[i]->setBounds (34, 46 + i * h, getWidth() - 44, h - 4);
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
    /** All controls for one rack module, built from the FX catalogue. */
    struct FxModuleControls
    {
        FxModuleControls (APVTS& s, const FxDef& d, int maxKnobs = 99, bool withExtras = true) : def (d), power (s, d.onID)
        {
            for (auto& k : d.knobs)
            {
                if (knobs.size() >= maxKnobs) break;
                knobs.add (new Knob (s, k.id, k.label, ModDest::None, 1.0f, k.bipolar));
                knobs.getLast()->setCaptionSize (11.5f);
            }
            if (! withExtras) return;
            for (auto& id : d.choices)
            {
                choices.add (new ChoiceBox (s, id));
                auto name = s.getParameter (id)->getName (64);
                choiceLabels.add (name.fromFirstOccurrenceOf (" ", false, false).toUpperCase());
            }
            for (auto& [id, label] : d.toggles)
            {
                auto* b = toggles.add (new juce::TextButton (label));
                b->setClickingTogglesState (true);
                b->setTooltip (helpTextFor (id));
                toggleAttach.push_back (std::make_unique<APVTS::ButtonAttachment> (s, id, *b));
            }
        }
        void addTo (juce::Component& parent)
        {
            parent.addAndMakeVisible (power);
            for (auto* k : knobs) parent.addAndMakeVisible (k);
            for (auto* c : choices) parent.addAndMakeVisible (c);
            for (auto* t : toggles) parent.addAndMakeVisible (t);
        }
        void setVisible (bool v)
        {
            power.setVisible (v);
            for (auto* k : knobs) k->setVisible (v);
            for (auto* c : choices) c->setVisible (v);
            for (auto* t : toggles) t->setVisible (v);
        }
        const FxDef& def;
        PowerButton power;
        juce::OwnedArray<Knob> knobs;
        juce::OwnedArray<ChoiceBox> choices;
        juce::StringArray choiceLabels;
        juce::OwnedArray<juce::TextButton> toggles;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> toggleAttach;   // destroyed before the buttons
    };

    class FxQuick : public Panel, private juce::Timer
    {
    public:
        explicit FxQuick (DivinityProcessor& p) : apvts (p.apvts), analyzer (p), meter (p, true)
        {
            for (auto& d : fxCatalog())
            {
                modules.add (new FxModuleControls (p.apvts, d, 8, false));
                modules.getLast()->addTo (*this);
            }
            rebuildMenu();
            selector.setTooltip ("Choose which effect to edit here. Reorder the chain and see every setting on the EFFECTS tab.");
            selector.onChange = [this] { select (selector.getSelectedId() - 1); };
            addAndMakeVisible (selector);
            addAndMakeVisible (analyzer);
            addAndMakeVisible (meter);
            select (fxIndexForKey ("reverb"));
            startTimerHz (4);
        }
        void select (int i)
        {
            current = juce::jlimit (0, modules.size() - 1, i);
            selector.setSelectedId (current + 1, juce::dontSendNotification);
            for (int k = 0; k < modules.size(); ++k) modules[k]->setVisible (k == current);
            resized(); repaint();
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            g.setColour (textDim);
            g.setFont (font (11.0f, Weight::Medium, 0.1f));
            g.drawText ("OUTPUT SPECTRUM", analyzer.getX(), 14, 200, 16, juce::Justification::centredLeft);
            const auto& d = fxCatalog()[(size_t) current];
            if ((int) d.knobs.size() > modules[current]->knobs.size() || ! d.choices.empty() || ! d.toggles.empty())
            {
                g.setColour (textFaint);
                g.setFont (font (10.5f));
                g.drawText ("More settings on the EFFECTS tab", 236, 14, 200, 16, juce::Justification::centredLeft);
            }
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
                for (int k = 0; k < m->knobs.size(); ++k)
                    m->knobs[k]->setBounds (6 + (k % 4) * 80, 42 + (k / 4) * 74, 80, 72);
            }
        }
    private:
        void rebuildMenu()
        {
            selector.clear (juce::dontSendNotification);
            const auto& c = fxCatalog();
            for (int i = 0; i < (int) c.size(); ++i)
            {
                const bool on = apvts.getRawParameterValue (c[(size_t) i].onID)->load() > 0.5f;
                selector.addItem (juce::String (c[(size_t) i].name) + (on ? juce::String (juce::CharPointer_UTF8 ("  \xe2\x97\x8f")) : juce::String()), i + 1);
            }
            selector.setSelectedId (current + 1, juce::dontSendNotification);
        }
        void timerCallback() override
        {
            int mask = 0;
            const auto& c = fxCatalog();
            for (int i = 0; i < (int) c.size(); ++i) if (apvts.getRawParameterValue (c[(size_t) i].onID)->load() > 0.5f) mask |= 1 << i;
            if (mask != lastMask && ! selector.isPopupActive()) { lastMask = mask; rebuildMenu(); }
        }
        APVTS& apvts;
        juce::ComboBox selector;
        juce::OwnedArray<FxModuleControls> modules;
        SpectrumAnalyzer analyzer;
        LevelMeter meter;
        int current = 0, lastMask = -1;
    };
}

// CreatePage: see CreatePageV7.cpp

//==============================================================================
// EffectsPage
//==============================================================================
namespace
{
    /** Drag-to-reorder chain list. Click a row to edit it, click its dot to switch it on/off. */
    class ChainList : public Panel, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit ChainList (DivinityProcessor& p) : Panel ("FX CHAIN"), proc (p)
        {
            up.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb2")));
            down.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbc")));
            up.setTooltip ("Move the selected effect earlier in the chain.");
            down.setTooltip ("Move the selected effect later in the chain.");
            up.onClick = [this] { move (-1); };
            down.onClick = [this] { move (1); };
            addAndMakeVisible (up); addAndMakeVisible (down);
            setTooltip ("Drag effects to reorder the chain. Click a dot to switch an effect on or off.");
            startTimerHz (8);
        }
        std::function<void (int)> onSelect;   // catalogue index
        int selected = 0;

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const auto order = proc.getFxOrder();
            const auto& cat = fxCatalog();
            for (int r = 0; r < order.size(); ++r)
            {
                const int idx = fxIndexForKey (order[r]);
                auto row = rowBounds (r).toFloat();
                if (dragFrom == r && dragging) { g.setColour (accent.withAlpha (0.08f)); g.fillRoundedRectangle (row, 5.0f); continue; }
                const bool on = proc.apvts.getRawParameterValue (cat[(size_t) idx].onID)->load() > 0.5f;
                const bool sel = idx == selected;
                g.setColour (sel ? colours::selBottom : panel2.withAlpha (0.55f));
                g.fillRoundedRectangle (row, 5.0f);
                g.setColour (sel ? accent : border);
                g.drawRoundedRectangle (row, 5.0f, sel ? 1.2f : 1.0f);
                // grip
                g.setColour (textFaint);
                for (int d = 0; d < 3; ++d) for (int e = 0; e < 2; ++e)
                    g.fillEllipse (row.getX() + 9.0f + e * 5.0f, row.getCentreY() - 6.0f + d * 5.0f, 2.2f, 2.2f);
                g.setFont (font (11.0f, Weight::Medium));
                g.setColour (textFaint);
                g.drawText (juce::String (r + 1), (int) row.getX() + 22, (int) row.getY(), 22, (int) row.getHeight(), juce::Justification::centredRight);
                g.setFont (font (14.0f, Weight::Medium));
                g.setColour (on ? text : textDim);
                g.drawText (cat[(size_t) idx].name, (int) row.getX() + 52, (int) row.getY(), 170, (int) row.getHeight(), juce::Justification::centredLeft);
                auto dot = dotBounds (r).toFloat();
                if (on) { g.setColour (accent.withAlpha (0.3f)); g.fillEllipse (dot.expanded (3.0f)); }
                g.setColour (on ? accent : textFaint);
                g.fillEllipse (dot);
            }
            if (dragging)
            {
                const int target = targetIndex();
                const float y = (float) rowBounds (juce::jmin (target, order.size() - 1)).getY() + (target >= order.size() ? 36.0f : 0.0f) - 2.0f;
                g.setColour (accent);
                g.fillRoundedRectangle (10.0f, y, (float) getWidth() - 20.0f, 3.0f, 1.5f);
                auto ghost = juce::Rectangle<float> (10.0f, (float) dragY - 17.0f, (float) getWidth() - 20.0f, 34.0f);
                g.setColour (juce::Colour (0xee12407a));
                g.fillRoundedRectangle (ghost, 5.0f);
                g.setColour (accent); g.drawRoundedRectangle (ghost, 5.0f, 1.2f);
                g.setColour (text); g.setFont (font (14.0f, Weight::Medium));
                g.drawText (cat[(size_t) fxIndexForKey (order[dragFrom])].name, ghost.withTrimmedLeft (52.0f), juce::Justification::centredLeft);
            }
            g.setColour (textFaint);
            g.setFont (font (11.0f));
            g.drawText ("Output -> Limiter (always last)", 16, getHeight() - 26, getWidth() - 32, 16, juce::Justification::centredLeft);
        }
        void resized() override
        {
            up.setBounds (getWidth() - 74, 9, 30, 26);
            down.setBounds (getWidth() - 40, 9, 30, 26);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            dragFrom = rowAt (e.y); dragging = false; dragY = e.y;
            if (dragFrom < 0) return;
            const auto order = proc.getFxOrder();
            const int idx = fxIndexForKey (order[dragFrom]);
            if (dotBounds (dragFrom).expanded (6).contains (e.getPosition()))
            {
                auto* prm = proc.apvts.getParameter (fxCatalog()[(size_t) idx].onID);
                prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture();
                dragFrom = -1;
            }
            selected = idx;
            if (onSelect) onSelect (idx);
            repaint();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragFrom < 0) return;
            if (! dragging && std::abs (e.getDistanceFromDragStartY()) > 4) dragging = true;
            dragY = juce::jlimit (40, getHeight() - 30, e.y);
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging && dragFrom >= 0)
            {
                auto order = proc.getFxOrder();
                int target = targetIndex();
                const auto key = order[dragFrom];
                order.remove (dragFrom);
                if (target > dragFrom) --target;
                order.insert (juce::jlimit (0, order.size(), target), key);
                proc.setFxOrder (order);
            }
            dragging = false; dragFrom = -1;
            repaint();
        }

    private:
        juce::Rectangle<int> rowBounds (int r) const { return { 10, 44 + r * 38, getWidth() - 20, 34 }; }
        juce::Rectangle<int> dotBounds (int r) const { auto rb = rowBounds (r); return { rb.getRight() - 24, rb.getCentreY() - 5, 10, 10 }; }
        int rowAt (int y) const
        {
            const int n = proc.getFxOrder().size();
            for (int r = 0; r < n; ++r) if (rowBounds (r).getY() <= y && y < rowBounds (r).getBottom() + 4) return r;
            return -1;
        }
        int targetIndex() const
        {
            const int n = proc.getFxOrder().size();
            return juce::jlimit (0, n, (dragY - 44 + 19) / 38);
        }
        void move (int delta)
        {
            auto order = proc.getFxOrder();
            const int r = order.indexOf (fxCatalog()[(size_t) selected].key);
            const int t = r + delta;
            if (r < 0 || t < 0 || t >= order.size()) return;
            order.move (r, t);
            proc.setFxOrder (order);
            repaint();
        }
        void timerCallback() override
        {
            juce::String sig = proc.getFxOrder().joinIntoString (",");
            for (auto& d : fxCatalog()) sig << (proc.apvts.getRawParameterValue (d.onID)->load() > 0.5f ? "1" : "0");
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }
        DivinityProcessor& proc;
        juce::TextButton up, down;
        int dragFrom = -1, dragY = 0;
        bool dragging = false;
        juce::String lastSig;
    };

    class FxEditor : public Panel
    {
    public:
        FxEditor (DivinityProcessor& p, const FxDef& d) : ctl (p.apvts, d)
        {
            ctl.addTo (*this);
            ctl.power.onStateChange = [this] { repaint(); };
        }
        void paint (juce::Graphics& g) override
        {
            drawPanelBackground (g, getLocalBounds().toFloat(), ctl.power.getToggleState());
            g.setColour (text);
            g.setFont (font (22.0f, Weight::SemiBold, 0.04f));
            g.drawText (juce::String (ctl.def.name).toUpperCase(), 52, 12, 400, 30, juce::Justification::centredLeft);
            g.setColour (textDim);
            g.setFont (font (13.0f));
            g.drawFittedText (ctl.def.description, 20, 48, getWidth() - 40, 36, juce::Justification::topLeft, 2);
            g.setFont (font (10.5f, Weight::Medium, 0.12f));
            for (int i = 0; i < ctl.choices.size(); ++i)
                g.drawText (ctl.choiceLabels[i], ctl.choices[i]->getX(), ctl.choices[i]->getY() - 18, 200, 16, juce::Justification::centredLeft);
            if (isMultiband())
            {
                auto& s = ctl.knobs[0]->getSlider();
                juce::ignoreUnused (s);
                const auto lo = juce::String (juce::roundToInt (ctl.knobs[3]->getSlider().getValue())),
                           hi = juce::String (juce::roundToInt (ctl.knobs[4]->getSlider().getValue()));
                const juce::String names[4] = { "GLOBAL", "LOW", "MID", "HIGH" };
                const juce::String ranges[4] = { "depth / time / splits", "below " + lo + " Hz", lo + " - " + hi + " Hz", "above " + hi + " Hz" };
                for (int row = 0; row < 4; ++row)
                {
                    const int y = mbRowY (row);
                    g.setColour (row == 0 ? textDim : accent);
                    g.setFont (font (13.0f, Weight::SemiBold, 0.12f));
                    g.drawText (names[row], 20, y + 18, 110, 18, juce::Justification::centredLeft);
                    g.setColour (textFaint);
                    g.setFont (font (10.5f, Weight::Medium));
                    g.drawText (ranges[row], 20, y + 38, 120, 16, juce::Justification::centredLeft);
                    if (row > 0) { g.setColour (border); g.drawHorizontalLine (y, 20.0f, (float) getWidth() - 20.0f); }
                }
            }
        }
        void resized() override
        {
            ctl.power.setBounds (16, 15, 26, 26);
            int x = 20;
            const int extrasY = 112;
            for (auto* c : ctl.choices) { c->setBounds (x, extrasY, 180, 30); x += 196; }
            for (auto* t : ctl.toggles) { t->setBounds (x, extrasY, 140, 30); x += 156; }
            if (isMultiband())
            {
                for (int k = 0; k < ctl.knobs.size(); ++k)
                {
                    const int row = k < 5 ? 0 : 1 + (k - 5) / 5, col = k < 5 ? k : (k - 5) % 5;
                    ctl.knobs[k]->setBounds (150 + col * 150, mbRowY (row), 140, 78);
                }
                return;
            }
            const bool hasExtras = ! ctl.choices.isEmpty() || ! ctl.toggles.isEmpty();
            const int top = hasExtras ? 164 : 104, n = ctl.knobs.size();
            const int perRow = juce::jmin (n, 7), kw = juce::jmin (128, (getWidth() - 40) / juce::jmax (1, perRow));
            for (int k = 0; k < n; ++k)
                ctl.knobs[k]->setBounds (20 + (k % perRow) * kw, top + (k / perRow) * 132, kw, 124);
        }
    private:
        bool isMultiband() const { return juce::String (ctl.def.key) == "mb" && ctl.knobs.size() >= 20; }
        static int mbRowY (int row) { return 92 + row * 81; }
        FxModuleControls ctl;
    };
}

struct EffectsPage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner)
        : chain (p), imager (p), master (p.apvts, ids::master, "Output"), limiterPower (p.apvts, ids::limOn),
          ceiling (p.apvts, ids::limCeiling, "Ceiling"), meter (p, true)
    {
        for (auto& d : fxCatalog()) { editors.add (new FxEditor (p, d)); owner.addChildComponent (editors.getLast()); }
        chain.onSelect = [this] (int i) { show (i); };
        master.setCaptionSize (12.0f); ceiling.setCaptionSize (12.0f);
        for (auto* c : std::initializer_list<juce::Component*> { &chain, &imager, &master, &limiterPower, &ceiling, &meter }) owner.addAndMakeVisible (c);
        show (fxIndexForKey ("reverb"));
        chain.selected = fxIndexForKey ("reverb");
    }
    void show (int i) { for (int k = 0; k < editors.size(); ++k) editors[k]->setVisible (k == i); }

    ChainList chain;
    juce::OwnedArray<FxEditor> editors;
    StereoImager imager;
    Knob master;
    PowerButton limiterPower;
    Knob ceiling;
    LevelMeter meter;
};

EffectsPage::EffectsPage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
EffectsPage::~EffectsPage() = default;

void EffectsPage::paint (juce::Graphics& g)
{
    drawPanelBackground (g, { 312.0f, 432.0f, 936.0f, 250.0f });
    drawSectionTitle (g, { 328, 440, 400, 24 }, "OUTPUT  -  STEREO IMAGE");
    g.setColour (textDim);
    g.setFont (font (11.0f, Weight::Medium, 0.1f));
    g.drawText ("LIMITER", 1010, 444, 100, 16, juce::Justification::centredLeft);
}

void EffectsPage::resized()
{
    auto& m = *impl;
    m.chain.setBounds (0, 0, 300, 682);
    for (auto* e : m.editors) e->setBounds (312, 0, 936, 420);
    m.imager.setBounds (328, 472, 660, 196);
    m.limiterPower.setBounds (1066, 440, 22, 22);
    m.ceiling.setBounds (1000, 480, 100, 120);
    m.master.setBounds (1100, 480, 100, 120);
    m.meter.setBounds (1210, 474, 26, 194);
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

    class ArpPanel : public Panel, private juce::Timer
    {
    public:
        explicit ArpPanel (DivinityProcessor& p)
            : Panel ("ARPEGGIATOR"), proc (p), power (p.apvts, arpids::on), mode (p.apvts, arpids::mode), rate (p.apvts, arpids::rate),
              octaves (p.apvts, arpids::octaves, "Octaves"), gate (p.apvts, arpids::gate, "Gate"), swing (p.apvts, arpids::swing, "Swing")
        {
            latch.setClickingTogglesState (true);
            latch.setTooltip (helpTextFor (arpids::latch));
            latchAttach = std::make_unique<APVTS::ButtonAttachment> (p.apvts, arpids::latch, latch);
            for (auto* k : { &octaves, &gate, &swing }) k->setCaptionSize (11.5f);
            for (auto* c : std::initializer_list<juce::Component*> { &power, &mode, &rate, &octaves, &gate, &swing, &latch }) addAndMakeVisible (c);
            mode.setTooltip (helpTextFor (arpids::mode));
            rate.setTooltip (helpTextFor (arpids::rate));
            startTimerHz (30);
        }
        void paint (juce::Graphics& g) override
        {
            drawPanelBackground (g, getLocalBounds().toFloat(), isOn());
            drawSectionTitle (g, { 46, 8, 200, 24 }, "ARPEGGIATOR");
            g.setColour (textDim);
            g.setFont (font (10.5f, Weight::Medium, 0.12f));
            g.drawText ("MODE", mode.getX(), mode.getY() - 16, 100, 14, juce::Justification::centredLeft);
            g.drawText ("RATE", rate.getX(), rate.getY() - 16, 100, 14, juce::Justification::centredLeft);

            // step lights + current note
            const auto lights = juce::Rectangle<float> (16.0f, (float) getHeight() - 34.0f, (float) getWidth() - 140.0f, 18.0f);
            const float w = lights.getWidth() / 16.0f;
            for (int i = 0; i < 16; ++i)
            {
                auto r = juce::Rectangle<float> (lights.getX() + i * w + 2.0f, lights.getY(), w - 4.0f, lights.getHeight());
                const bool lit = isOn() && note >= 0 && (step % 16) == i;
                g.setColour (lit ? accent : panel2.brighter (0.15f));
                g.fillRoundedRectangle (r, 3.0f);
                if (lit) { g.setColour (accent.withAlpha (0.3f)); g.drawRoundedRectangle (r.expanded (2.0f), 4.0f, 2.0f); }
            }
            g.setFont (font (16.0f, Weight::SemiBold));
            g.setColour (isOn() && note >= 0 ? text : textFaint);
            g.drawText (isOn() ? (note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 4) : (keys > 0 ? "..." : "hold keys")) : "off",
                        getWidth() - 118, getHeight() - 40, 102, 30, juce::Justification::centredRight);
        }
        void resized() override
        {
            power.setBounds (14, 9, 24, 24);
            latch.setBounds (getWidth() - 86, 8, 72, 26);
            mode.setBounds (16, 62, 150, 30);
            rate.setBounds (16, 124, 150, 30);
            const int kx = 180, kw = (getWidth() - kx - 10) / 3;
            octaves.setBounds (kx, 44, kw, 130);
            gate.setBounds (kx + kw, 44, kw, 130);
            swing.setBounds (kx + 2 * kw, 44, kw, 130);
        }
    private:
        bool isOn() const { return proc.apvts.getRawParameterValue (arpids::on)->load() > 0.5f; }
        void timerCallback() override
        {
            const int n = proc.arp.displayNote.load(), st = proc.arp.displayStep.load(), k = proc.arp.displayCount.load();
            const bool on = isOn();
            if (n != note || st != step || k != keys || on != wasOn) { note = n; step = st; keys = k; wasOn = on; repaint(); }
        }
        DivinityProcessor& proc;
        PowerButton power;
        ChoiceBox mode, rate;
        Knob octaves, gate, swing;
        juce::TextButton latch { "LATCH" };
        std::unique_ptr<APVTS::ButtonAttachment> latchAttach;
        int note = -1, step = 0, keys = 0;
        bool wasOn = false;
    };
}

struct PlayPage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner)
        : gen (p), arp (p), perf (p), keys (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &gen, &arp, &perf, &keys })
            owner.addAndMakeVisible (c);
    }
    MidiGenPanel gen;
    ArpPanel arp;
    PerformancePanel perf;
    KeyboardBar keys;
};

PlayPage::PlayPage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
PlayPage::~PlayPage() = default;
void PlayPage::paint (juce::Graphics&) {}
void PlayPage::resized()
{
    auto& m = *impl;
    m.gen.setBounds (0, 0, 742, 530);
    m.arp.setBounds (754, 0, 494, 265);
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
