// MIDI GENERATOR panel: settings, piano-roll preview, audition and drag-to-DAW.
#include "MidiGenPanel.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;
using namespace midigen;

//==============================================================================
/** Piano roll of the current clip, with the audition playhead. */
class MidiGenPanel::PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (DivinityProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        drawDisplayBackground (g, r);
        const auto& st = proc.midiGen;
        if (! st.hasClip) return;
        const auto& clip = st.clip;
        auto a = r.reduced (10.0f, 10.0f).withTrimmedLeft (44.0f);
        const bool drumsPart = clip.part == Part::Drums;

        // rows
        std::vector<int> rows;
        if (drumsPart) rows = { drums::kick, drums::rim, drums::snare, drums::clap, drums::hatClosed, drums::hatPedal, drums::hatOpen, drums::perc };
        else
        {
            int lo = 127, hi = 0;
            for (auto& n : clip.notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
            lo = juce::jmax (0, lo - 2); hi = juce::jmin (127, hi + 2);
            if (hi - lo < 12) { const int mid = (lo + hi) / 2; lo = mid - 6; hi = mid + 6; }
            for (int k = hi; k >= lo; --k) rows.push_back (k);
        }
        if (drumsPart)
        {
            // keep only lanes that are used, in a fixed order
            std::vector<int> used;
            for (int k : rows) for (auto& n : clip.notes) if (n.pitch == k) { used.push_back (k); break; }
            rows = used.empty() ? std::vector<int> { drums::kick } : used;
        }
        const float rowH = a.getHeight() / (float) rows.size();
        auto rowOf = [&rows] (int pitch) { for (size_t i = 0; i < rows.size(); ++i) if (rows[i] == pitch) return (int) i; return -1; };

        g.setFont (font (10.0f, Weight::Medium));
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const float y = a.getY() + rowH * (float) i;
            const int pc = rows[i] % 12;
            const bool black = ! drumsPart && (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            g.setColour ((black ? juce::Colours::black : accent).withAlpha (black ? 0.25f : 0.03f));
            g.fillRect (a.getX(), y, a.getWidth(), rowH);
            const bool label = drumsPart || pc == 0 || rows.size() <= 18;
            if (label)
            {
                g.setColour (textDim);
                juce::String name;
                if (drumsPart)
                {
                    switch (rows[i])
                    {
                        case drums::kick: name = "KICK"; break;     case drums::snare: name = "SNARE"; break;
                        case drums::clap: name = "CLAP"; break;     case drums::rim: name = "RIM"; break;
                        case drums::hatOpen: name = "OH"; break;    case drums::hatPedal: name = "PH"; break;
                        case drums::perc: name = "PERC"; break;     default: name = "HAT"; break;
                    }
                }
                else name = juce::MidiMessage::getMidiNoteName (rows[i], true, true, 4);
                g.drawText (name, (int) r.getX() + 6, (int) y, 44, (int) juce::jmax (10.0f, rowH), juce::Justification::centredLeft);
            }
        }
        // beat / bar grid
        const double beats = clip.lengthBeats;
        for (int b = 0; b <= (int) beats; ++b)
        {
            const float x = a.getX() + a.getWidth() * (float) (b / beats);
            g.setColour (accent.withAlpha (b % 4 == 0 ? 0.22f : 0.07f));
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        }
        // notes
        for (auto& n : clip.notes)
        {
            const int row = rowOf (n.pitch);
            if (row < 0) continue;
            const float x = a.getX() + a.getWidth() * (float) (n.start / beats);
            const float w = juce::jmax (3.0f, a.getWidth() * (float) (n.length / beats) - 1.0f);
            auto nr = juce::Rectangle<float> (x, a.getY() + rowH * (float) row + 1.0f, drumsPart ? juce::jmax (4.0f, juce::jmin (w, 8.0f)) : w, juce::jmax (2.0f, rowH - 2.0f));
            g.setColour (accent.withAlpha (0.18f)); g.fillRoundedRectangle (nr.expanded (1.5f), 3.0f);
            g.setColour (accentStrong.interpolatedWith (glow, n.velocity * 0.6f).withAlpha (0.45f + 0.55f * n.velocity));
            g.fillRoundedRectangle (nr, 2.5f);
        }
        // playhead
        if (proc.midiPreview.isPlaying())
        {
            const float x = a.getX() + a.getWidth() * (float) (std::fmod (proc.midiPreview.getPosition(), beats) / beats);
            juce::Path ph; ph.startNewSubPath (x, a.getY()); ph.lineTo (x, a.getBottom());
            strokeGlow (g, ph, 1.2f);
        }
    }
private:
    DivinityProcessor& proc;
};

//==============================================================================
/** The tile you drag into the DAW. Writes a .mid file and starts an OS-level file drag. */
class MidiGenPanel::DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragTile (MidiGenPanel& o) : owner (o)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag this onto a MIDI / instrument track in your DAW to drop the clip in. (Or use SAVE .MID.)");
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        drawGlassControl (g, r, true, isMouseOver(), 8.0f);
        // icon: a MIDI file with an arrow
        auto ic = juce::Rectangle<float> (26.0f, 30.0f).withCentre ({ r.getX() + 34.0f, r.getCentreY() });
        juce::Path f;
        f.startNewSubPath (ic.getX(), ic.getY()); f.lineTo (ic.getRight() - 8.0f, ic.getY()); f.lineTo (ic.getRight(), ic.getY() + 8.0f);
        f.lineTo (ic.getRight(), ic.getBottom()); f.lineTo (ic.getX(), ic.getBottom()); f.closeSubPath();
        for (int k = 0; k < 3; ++k) { f.startNewSubPath (ic.getX() + 6.0f, ic.getY() + 13.0f + k * 5.0f); f.lineTo (ic.getRight() - 6.0f - k * 3.0f, ic.getY() + 13.0f + k * 5.0f); }
        strokeGlow (g, f, 1.4f, glow);
        g.setColour (text);
        g.setFont (font (15.0f, Weight::SemiBold, 0.08f));
        g.drawText ("DRAG MIDI", r.withTrimmedLeft (60.0f).withTrimmedBottom (r.getHeight() * 0.45f), juce::Justification::bottomLeft);
        g.setColour (textDim);
        g.setFont (font (11.0f));
        g.drawText ("into your DAW track", r.withTrimmedLeft (60.0f).withTrimmedTop (r.getHeight() * 0.55f), juce::Justification::topLeft);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        auto f = owner.writeTempFile();
        if (! f.existsAsFile()) return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    MidiGenPanel& owner;
    bool dragging = false;
};

//==============================================================================
MidiGenPanel::MidiGenPanel (DivinityProcessor& p) : Panel ("MIDI GENERATOR"), proc (p)
{
    roll = std::make_unique<PianoRoll> (p);
    tile = std::make_unique<DragTile> (*this);
    addAndMakeVisible (*roll);
    addAndMakeVisible (*tile);

    auto& st = proc.midiGen.settings;
    for (int i = 0; i < partNames().size(); ++i)
    {
        auto* b = partButtons.add (new juce::TextButton (partNames()[i]));
        b->setClickingTogglesState (false);
        b->getProperties().set ("fontSize", 13.0);
        b->onClick = [this, i] { proc.midiGen.settings.part = (Part) i; regenerate (false); };
        addAndMakeVisible (b);
    }
    for (int bars : { 2, 4, 8 })
    {
        auto* b = barButtons.add (new juce::TextButton (juce::String (bars) + " BARS"));
        b->getProperties().set ("fontSize", 12.0);
        b->onClick = [this, bars] { proc.midiGen.settings.bars = bars; regenerate (false); };
        addAndMakeVisible (b);
    }
    auto fill = [] (juce::ComboBox& c, const juce::StringArray& items, int sel)
    {
        c.addItemList (items, 1);
        c.setSelectedItemIndex (sel, juce::dontSendNotification);
    };
    fill (key, keyNames(), st.key);
    fill (scale, scaleNames(), st.scale);
    fill (style, styleNames(), st.style);
    key.onChange = [this] { proc.midiGen.settings.key = key.getSelectedItemIndex(); regenerate (false); };
    scale.onChange = [this] { proc.midiGen.settings.scale = scale.getSelectedItemIndex(); regenerate (false); };
    style.onChange = [this] { proc.midiGen.settings.style = style.getSelectedItemIndex(); regenerate (true); };
    key.setTooltip ("Key of the clip.");
    scale.setTooltip ("Scale: chords and melodies stay inside it.");
    style.setTooltip ("Style: sets the progression pool, rhythms, bass patterns and drum grooves.");
    for (auto* c : { &key, &scale, &style }) addAndMakeVisible (c);

    newProg.setTooltip ("Roll a new chord progression for this style (chords, lead and bass all follow it).");
    vary.setTooltip ("Same progression, a new take on the selected part.");
    play.setTooltip ("Audition the clip in a loop at the host tempo (melodic parts play through the current preset, drums use built-in kit sounds).");
    save.setTooltip ("Save the clip as a .mid file.");
    sevenths.setTooltip ("Use 7th chords in the progression.");
    sevenths.setClickingTogglesState (true);
    newProg.onClick = [this] { proc.midiGen.settings.seed = juce::Random::getSystemRandom().nextInt (1 << 30) + 1; regenerate (true); };
    vary.onClick = [this] { proc.midiGen.settings.seed = juce::Random::getSystemRandom().nextInt (1 << 30) + 1; regenerate (false); };
    play.onClick = [this]
    {
        if (proc.midiPreview.isPlaying()) proc.midiPreview.stop();
        else startPreview();
        syncButtons();
    };
    save.onClick = [this] { saveAs(); };
    sevenths.onClick = [this] { proc.midiGen.progression.sevenths = sevenths.getToggleState(); regenerate (false); };
    for (auto* b : { &newProg, &vary, &play, &save, &sevenths }) { b->getProperties().set ("fontSize", 12.5); addAndMakeVisible (b); }

    if (! proc.midiGen.hasClip) regenerate (true);
    syncButtons();
    startTimerHz (30);
}

MidiGenPanel::~MidiGenPanel() = default;

void MidiGenPanel::regenerate (bool newProgression)
{
    auto& st = proc.midiGen;
    if (newProgression)
    {
        st.progression = makeProgression (st.settings.style, st.settings.seed);
        sevenths.setToggleState (st.progression.sevenths, juce::dontSendNotification);
    }
    st.clip = generate (st.settings, st.progression);
    st.hasClip = true;
    if (proc.midiPreview.isPlaying()) startPreview();
    syncButtons();
    repaint();
    roll->repaint();
}

void MidiGenPanel::startPreview()
{
    if (proc.midiGen.hasClip) proc.midiPreview.play (proc.midiGen.clip);
}

void MidiGenPanel::syncButtons()
{
    const auto& st = proc.midiGen.settings;
    for (int i = 0; i < partButtons.size(); ++i) partButtons[i]->setToggleState (i == (int) st.part, juce::dontSendNotification);
    const int barsIdx = st.bars == 2 ? 0 : st.bars == 8 ? 2 : 1;
    for (int i = 0; i < barButtons.size(); ++i) barButtons[i]->setToggleState (i == barsIdx, juce::dontSendNotification);
    play.setButtonText (proc.midiPreview.isPlaying() ? "STOP" : "PLAY");
    play.setToggleState (proc.midiPreview.isPlaying(), juce::dontSendNotification);
    const bool drumsPart = st.part == Part::Drums;
    key.setEnabled (! drumsPart); scale.setEnabled (! drumsPart); sevenths.setEnabled (! drumsPart);
}

juce::File MidiGenPanel::writeTempFile()
{
    const auto& st = proc.midiGen;
    if (! st.hasClip) return {};
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DIVINITY MIDI");
    auto f = dir.getChildFile (juce::File::createLegalFileName (st.clip.fileStem) + ".mid");
    return writeMidiFile (st.clip, proc.hostBpm.load(), f) ? f : juce::File();
}

void MidiGenPanel::saveAs()
{
    if (! proc.midiGen.hasClip) return;
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (juce::File::createLegalFileName (proc.midiGen.clip.fileStem) + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI clip", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        writeMidiFile (proc.midiGen.clip, proc.hostBpm.load(), f.withFileExtension ("mid"));
    });
}

void MidiGenPanel::timerCallback()
{
    if (play.getToggleState() != proc.midiPreview.isPlaying()) syncButtons();
    if (proc.midiPreview.isPlaying()) roll->repaint();
}

void MidiGenPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const auto& st = proc.midiGen;
    g.setColour (textDim);
    g.setFont (font (11.0f, Weight::Medium, 0.08f));
    g.drawText ("KEY", 16, 88, 120, 14, juce::Justification::centredLeft);
    g.drawText ("SCALE", 112, 88, 160, 14, juce::Justification::centredLeft);
    g.drawText ("STYLE", 290, 88, 160, 14, juce::Justification::centredLeft);
    g.drawText ("LENGTH", 452, 88, 160, 14, juce::Justification::centredLeft);
    if (st.hasClip)
    {
        g.setColour (accent);
        g.setFont (font (15.0f, Weight::Medium, 0.02f));
        const auto label = st.clip.part == Part::Drums ? st.clip.summary.toUpperCase() : st.clip.summary;
        g.drawText (label, 16, 150, getWidth() - 32, 22, juce::Justification::centredLeft);
        g.setColour (textDim);
        g.setFont (font (11.5f));
        g.drawText (juce::String ((int) st.clip.notes.size()) + " notes  -  " + juce::String (juce::roundToInt (proc.hostBpm.load())) + " BPM (host)",
                    16, 150, getWidth() - 32, 22, juce::Justification::centredRight);
    }
}

void MidiGenPanel::resized()
{
    const int w = getWidth(), h = getHeight();
    const int pw = (w - 32 - 3 * 6) / 4;
    for (int i = 0; i < partButtons.size(); ++i) partButtons[i]->setBounds (16 + i * (pw + 6), 44, pw, 34);
    key.setBounds (16, 104, 86, 30);
    scale.setBounds (112, 104, 168, 30);
    style.setBounds (290, 104, 152, 30);
    const int bw = (w - 16 - 452 - 2 * 6) / 3;
    for (int i = 0; i < barButtons.size(); ++i) barButtons[i]->setBounds (452 + i * (bw + 6), 104, bw, 30);
    roll->setBounds (16, 178, w - 32, h - 178 - 70);
    const int by = h - 58;
    newProg.setBounds (16, by, 140, 44);
    vary.setBounds (162, by, 126, 44);
    sevenths.setBounds (294, by, 56, 44);
    play.setBounds (356, by, 70, 44);
    save.setBounds (432, by, 84, 44);
    tile->setBounds (522, by - 2, w - 16 - 522, 48);
}

} // namespace divinity::ui
