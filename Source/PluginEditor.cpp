#include "PluginEditor.h"
#include "DivinityAssets.h"

#if JUCE_MAC
 #include <mach/mach.h>
#elif JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
 #pragma comment (lib, "psapi.lib")
#endif

namespace divinity
{
using namespace ui;
using namespace ui::colours;

namespace
{
    juce::Path chevron (bool left)
    {
        juce::Path p;
        if (left) { p.startNewSubPath (15, 5); p.lineTo (8, 12); p.lineTo (15, 19); }
        else      { p.startNewSubPath (9, 5);  p.lineTo (16, 12); p.lineTo (9, 19); }
        return p;
    }
    juce::Path burger() { juce::Path p; for (int i = 0; i < 3; ++i) { p.startNewSubPath (3, 6.0f + i * 6.0f); p.lineTo (21, 6.0f + i * 6.0f); } return p; }
    juce::Path plus()   { juce::Path p; p.startNewSubPath (12, 5); p.lineTo (12, 19); p.startNewSubPath (5, 12); p.lineTo (19, 12); return p; }
    juce::Path folderIcon()
    {
        juce::Path p;
        p.startNewSubPath (3, 7); p.lineTo (9, 7); p.lineTo (11, 9); p.lineTo (21, 9); p.lineTo (21, 19); p.lineTo (3, 19); p.closeSubPath();
        return p;
    }
    juce::Path undoIcon (bool redo)
    {
        juce::Path p;
        p.startNewSubPath (8, 6); p.lineTo (4, 10); p.lineTo (8, 14);
        p.startNewSubPath (4, 10); p.cubicTo (12, 10, 20, 10, 20, 16); p.lineTo (20, 19);
        if (redo) p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (24.0f, 0.0f));
        return p;
    }
    juce::Path copyIcon()
    {
        juce::Path p;
        p.addRoundedRectangle (8.0f, 3.0f, 12.0f, 14.0f, 2.0f);
        p.startNewSubPath (5, 7); p.lineTo (5, 21); p.lineTo (16, 21);
        return p;
    }
    juce::Path starIcon()
    {
        juce::Path p;
        p.addStar ({ 12.0f, 12.5f }, 5, 4.2f, 9.5f, 0.0f);
        return p;
    }
    juce::Path helpIcon()
    {
        juce::Path p;
        p.addEllipse (2.5f, 2.5f, 19.0f, 19.0f);
        p.startNewSubPath (9.2f, 9.5f); p.cubicTo (9.2f, 6.5f, 14.8f, 6.5f, 14.8f, 9.5f); p.cubicTo (14.8f, 11.8f, 12.0f, 11.8f, 12.0f, 14.5f);
        p.addEllipse (11.2f, 16.6f, 1.6f, 1.6f);
        return p;
    }
    juce::Path wingIcon()
    {
        // two swept wings (Guardian Angel)
        juce::Path p;
        for (int side = -1; side <= 1; side += 2)
        {
            const float d = (float) side;
            p.startNewSubPath (12.0f + d * 1.0f, 18.0f);
            p.cubicTo (12.0f + d * 4.0f, 10.0f, 12.0f + d * 8.0f, 6.0f, 12.0f + d * 11.5f, 4.0f);
            p.cubicTo (12.0f + d * 10.5f, 9.0f, 12.0f + d * 9.0f, 12.0f, 12.0f + d * 6.5f, 14.0f);
            p.startNewSubPath (12.0f + d * 3.0f, 17.0f);
            p.cubicTo (12.0f + d * 6.0f, 15.5f, 12.0f + d * 8.5f, 15.0f, 12.0f + d * 10.5f, 13.5f);
        }
        return p;
    }
    juce::Path tabIcon (int i)
    {
        juce::Path p;
        switch (i)
        {
            case 0: p.addStar ({ 12.0f, 12.5f }, 5, 4.6f, 10.5f, 0.0f); break;                               // CREATE
            case 1: for (int k = 0; k < 3; ++k) { p.startNewSubPath (4.0f + k * 8.0f, 4); p.lineTo (4.0f + k * 8.0f, 20); p.addEllipse (2.0f + k * 8.0f, 7.0f + k * 4.0f, 4, 4); } break;   // EFFECTS
            case 2: p.startNewSubPath (7, 4); p.lineTo (19, 12); p.lineTo (7, 20); p.closeSubPath(); break;   // PLAY
            case 3: p.startNewSubPath (2, 12); p.lineTo (7, 12); p.lineTo (9, 6); p.lineTo (12, 19); p.lineTo (15, 9); p.lineTo (17, 12); p.lineTo (22, 12); break;   // SOUL
            case 4: p.startNewSubPath (9, 3); p.lineTo (15, 3); p.startNewSubPath (10, 3); p.lineTo (10, 10); p.lineTo (4, 20); p.lineTo (20, 20); p.lineTo (14, 10); p.lineTo (14, 3); break;   // LAB
            default: p = folderIcon(); break;   // BROWSER
        }
        return p;
    }

    /** Resident memory of this process in bytes (0 if unknown). */
    juce::int64 processMemoryBytes()
    {
       #if JUCE_MAC
        mach_task_basic_info info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
            return (juce::int64) info.resident_size;
        return 0;
       #elif JUCE_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc {};
        if (GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc))) return (juce::int64) pmc.WorkingSetSize;
        return 0;
       #else
        juce::File statm ("/proc/self/statm");
        auto parts = juce::StringArray::fromTokens (statm.loadFileAsString(), " ", "");
        return parts.size() > 1 ? parts[1].getLargeIntValue() * 4096 : 0;
       #endif
    }

    const juce::StringArray& presetCategories()
    {
        static const juce::StringArray c { "REESE", "BASS", "LEAD", "SYNTH", "PAD", "TEXTURE", "SEQUENCE", "PERCUSSION", "FX" };
        return c;
    }
}

//==============================================================================
void GlassKeyboard::drawWhiteNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    auto r = area.reduced (0.6f, 0.0f).withTrimmedBottom (1.0f);
    juce::ColourGradient grad (juce::Colour (0xffc9d0d6), r.getX(), r.getY(), juce::Colour (0xfff4f7f9), r.getX(), r.getBottom(), false);
    grad.addColour (0.75, juce::Colour (0xffeef2f5));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.5f);
    if (isDown)
    {
        juce::ColourGradient lit (accent.withAlpha (0.55f), r.getX(), r.getY(), juce::Colour (0xffb6f1ff), r.getX(), r.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (r, 2.5f);
    }
    else if (isOver)
    {
        g.setColour (accent.withAlpha (0.15f)); g.fillRoundedRectangle (r, 2.5f);
    }
    g.setColour (juce::Colour (0xff5e6a74));
    g.drawRoundedRectangle (r.reduced (0.25f), 2.5f, 0.7f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (r.withHeight (5.0f));
    if (isDown)
    {
        // glowing marker under the key
        auto m = juce::Rectangle<float> (r.getWidth() * 0.55f, 2.0f).withCentre ({ r.getCentreX(), r.getBottom() - 3.0f });
        g.setColour (accent.withAlpha (0.4f)); g.fillRect (m.expanded (1.5f, 1.5f));
        g.setColour (glow); g.fillRect (m);
    }
}

void GlassKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.withTrimmedBottom (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 2.0f);
    juce::ColourGradient grad (juce::Colour (0xff0c0e10), r.getX(), 0, juce::Colour (0xff0c0e10), r.getRight(), 0, false);
    grad.addColour (0.5, juce::Colour (0xff353b41));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);
    if (isDown) { g.setColour (accent.withAlpha (0.7f)); g.fillRoundedRectangle (r.reduced (1.5f), 2.0f); }
    else if (isOver) { g.setColour (accent.withAlpha (0.2f)); g.fillRoundedRectangle (r.reduced (1.5f), 2.0f); }
    // key top highlight
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.18f, 0.0f).withTrimmedBottom (r.getHeight() * 0.12f).withTrimmedTop (2.0f).withWidth (r.getWidth() * 0.16f)
                                .translated (r.getWidth() * 0.06f, 0.0f), 1.0f);
}

//==============================================================================
KeyboardStrip::KeyboardStrip (DivinityProcessor& p)
    : proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), bend (p.apvts, ids::bendRange),
      bendMenu ("Pick a bend range", chevron (true)), scope (p)
{
    for (auto* s : { &pitch, &mod })
    {
        s->setSliderStyle (juce::Slider::LinearVertical);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (s);
    }
    pitch.setRange (-1.0, 1.0); pitch.setValue (0.0, juce::dontSendNotification);
    pitch.setTooltip ("Pitch wheel (springs back). Range is set by BEND.");
    pitch.onValueChange = [this]
    {
        const int v = juce::jlimit (0, 16383, juce::roundToInt ((pitch.getValue() + 1.0) * 8191.5));
        proc.uiMidi.addMessageToQueue (juce::MidiMessage::pitchWheel (1, v).withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
    };
    pitch.onDragEnd = [this] { pitch.setValue (0.0, juce::sendNotificationSync); };
    mod.setRange (0.0, 1.0); mod.setValue (0.0, juce::dontSendNotification);
    mod.setTooltip ("Mod wheel (MIDI CC 1). Every factory preset maps it to something musical.");
    mod.onValueChange = [this]
    {
        proc.uiMidi.addMessageToQueue (juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (mod.getValue() * 127.0))
                                           .withTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001));
    };
    bend.getSlider().setTooltip (helpTextFor (ids::bendRange));
    bend.getSlider().textFromValueFunction = [] (double v) { return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (juce::roundToInt (v)); };
    bend.getSlider().updateText();
    bendMenu.onClick = [this]
    {
        juce::PopupMenu m;
        const int cur = juce::roundToInt (proc.apvts.getRawParameterValue (ids::bendRange)->load());
        for (int r : { 1, 2, 3, 5, 7, 12, 24 }) m.addItem (r, juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (r) + " semitones", true, r == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bendMenu), [this] (int r)
        {
            if (r <= 0) return;
            if (auto* prm = proc.apvts.getParameter (ids::bendRange)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) r)); prm->endChangeGesture(); }
        });
    };
    addAndMakeVisible (bendMenu);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9edf4));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0d1018));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff8d96a8));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.75f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, panel2);
    keyboard.setWantsKeyboardFocus (true);
    keyboard.setKeyPressBaseOctave (4);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (bend);

    auto& s = p.apvts;
    knobs.add (new Knob (s, ids::glide, "Porta"));
    knobs.add (new Knob (s, ids::velSens, "Vel"));
    knobs.add (new Knob (s, ids::filt (0, "keytrack"), "Keytrk"));
    knobs.add (new Knob (s, ids::masterPan, "Pan", ModDest::None, 1.0f, true));
    knobs.add (new Knob (s, ids::master, "Vol"));
    knobs[0]->formatValue = [] (float v) { return juce::String (juce::roundToInt (v)) + " ms"; };
    knobs[1]->formatValue = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    knobs[2]->formatValue = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    knobs[3]->formatValue = [] (float v)
    {
        const int pc = juce::roundToInt (v * 100.0f);
        return pc == 0 ? juce::String ("C") : (pc < 0 ? "L" + juce::String (-pc) : "R" + juce::String (pc));
    };
    knobs[4]->formatValue = [] (float v) { return juce::String (v, 1) + " dB"; };
    for (auto* k : knobs) { k->setCaptionSize (12.0f); k->setValueStyle (2); k->setMaxKnobSize (40); addAndMakeVisible (k); }
    addAndMakeVisible (scope);
}

void KeyboardStrip::paint (juce::Graphics& g)
{
    // panels measured from the v0.7 mockup (strip is 1424 x 122)
    const float h = (float) getHeight();
    drawPanelBackground (g, { 0.0f, 0.0f, 119.0f, h });
    drawPanelBackground (g, { 129.0f, 0.0f, 63.0f, h });
    drawPanelBackground (g, { 201.0f, 0.0f, 783.0f, h });
    drawPanelBackground (g, { 991.0f, 0.0f, 328.0f, h });
    drawPanelBackground (g, { 1328.0f, 0.0f, 96.0f, h });
    g.setColour (text);
    g.setFont (font (12.0f, Weight::Medium, 0.03f));
    g.drawText ("PITCH", 14, getHeight() - 26, 56, 16, juce::Justification::centred);
    g.drawText ("MOD", 62, getHeight() - 26, 56, 16, juce::Justification::centred);
    g.drawText ("BEND", 129, 22, 63, 16, juce::Justification::centred);
}

void KeyboardStrip::resized()
{
    const int h = getHeight();
    pitch.setBounds (28, 14, 30, h - 46);
    mod.setBounds (76, 14, 30, h - 46);
    bend.setBounds (140, 44, 41, 24);
    bendMenu.setBounds (148, 78, 26, 24);
    keyboard.setBounds (207, 6, 771, h - 12);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    keyboard.setBlackNoteLengthProportion (0.62f);
    const int kx = 997, kw = 63;
    for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (kx + i * kw, 8, kw, h - 14);
    scope.setBounds (1334, 8, 84, h - 16);
}

//==============================================================================
DivinityEditor::DivinityEditor (DivinityProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      settings ("Settings", icons::gear()), help ("Help: click this, then any control, and Guardian Angel explains it", helpIcon()),
      menu ("Menu", burger()),
      folder ("Import preset", folderIcon()), save ("Save preset", icons::save()),
      undo ("Undo (Cmd/Ctrl+Z)", undoIcon (false)), redo ("Redo (Cmd/Ctrl+Shift+Z)", undoIcon (true)),
      copy ("Save a copy of this sound", copyIcon()), favs ("Browse by category / favourites", starIcon()),
      prevPreset ("Previous preset", chevron (true)), nextPreset ("Next preset", chevron (false)),
      star ("Favourite this preset", starIcon()), savePlus ("Save as new preset", plus()),
      voiceMode (p.apvts, ids::voiceMode, { "POLY", "MONO", "LEGATO" }, { 0, 2, 1 }),
      voices (p.apvts, ids::voices), glideBox (p.apvts, ids::glide),
      createPage (p), effectsPage (p), playPage (p), soulPage (p), labPage (p), browserPage (p), angel (p), keys (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* c : std::initializer_list<juce::Component*> { &settings, &help, &menu }) content.addAndMakeVisible (c);
    settings.onClick = [this] { showMainMenu(); };
    menu.onClick = [this] { showMainMenu(); };
    help.onClick = [this] { startExplain(); };

    for (auto* name : { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB", "BROWSER" })
    {
        auto* b = tabs.add (new juce::TextButton (name));
        b->getProperties().set ("corner", 7.0);
        b->setButtonText ({});   // label + icon are drawn by Content::paintOverChildren
        const juce::String tab (name);
        b->onClick = [this, tab] { showTab (tab); angel.showTabTip (tab); };
        content.addAndMakeVisible (b);
    }
    angelToggle.getProperties().set ("corner", 7.0);
    angelToggle.setTooltip ("Show or hide Guardian Angel. Hiding him makes the window narrower.");
    angelToggle.onClick = [this] { setAngelOpen (! proc.uiAngelOpen); };
    content.addAndMakeVisible (angelToggle);

    // preset strip
    presetName.getProperties().set ("transparent", true);
    presetName.getProperties().set ("fontSize", 15.0);
    presetName.setColour (juce::TextButton::textColourOffId, text);
    presetName.setTooltip ("Click to browse presets.");
    star.filledWhenOn = true;
    savePlus.boxed = true;
    star.setClickingTogglesState (false);
    for (auto* c : std::initializer_list<juce::Component*> { &folder, &save, &undo, &redo, &copy, &favs, &prevPreset, &presetName, &star, &nextPreset, &savePlus, &voiceMode, &voices, &glideBox })
        content.addAndMakeVisible (c);
    folder.onClick = [this] { importPreset(); };
    save.onClick = [this] { savePresetDialog(); };
    copy.onClick = [this] { savePresetDialog (true); };
    undo.onClick = [this] { proc.undo.undo(); };
    redo.onClick = [this] { proc.undo.redo(); };
    favs.onClick = [this] { showCategoryMenu (favs); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    presetName.onClick = [this] { showPresetMenu (presetName); };
    savePlus.onClick = [this] { savePresetDialog(); };
    star.onClick = [this]
    {
        const int i = proc.presets.getCurrentIndex();
        if (juce::isPositiveAndBelow (i, proc.presets.getPresets().size())) proc.presets.toggleFavourite (proc.presets.getPresets()[i]);
        updatePresetDisplay();
    };

    arpButton.setClickingTogglesState (true);
    arpButton.getProperties().set ("fontSize", 13.0);
    arpButton.getProperties().set ("corner", 6.0);
    voiceMode.getProperties().set ("gap", 7);
    arpButton.setTooltip (helpTextFor (arpids::on) + "\n\nMode, rate, octaves, gate and swing are on the PLAY tab.");
    arpAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, arpids::on, arpButton);
    content.addAndMakeVisible (arpButton);

    glideKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    glideKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    glideKnob.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    glideKnob.setTooltip (helpTextFor (ids::glide));
    glideAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::glide, glideKnob);
    content.addAndMakeVisible (glideKnob);

    // pages
    for (auto* c : std::initializer_list<juce::Component*> { &createPage, &effectsHost, &playHost, &soulHost, &labHost, &browserPage, &angel })
        content.addChildComponent (c);
    content.addAndMakeVisible (keys);
    createPage.onShowTab = [this] (const juce::String& t) { showTab (t); };
    angel.onExplainRequested = [this] { startExplain(); };
    content.addChildComponent (explainOverlay);

    proc.presets.addChangeListener (this);
    updatePresetDisplay();
    showTab (proc.uiWorkspace);

    setResizable (true, true);
    angel.setVisible (proc.uiAngelOpen);
    updateAngelToggleText();
    applySizeLimits();
    setSize (juce::roundToInt (getBaseWidth() * proc.uiScale), juce::roundToInt (kBaseHeight * proc.uiScale));
    setWantsKeyboardFocus (true);
    timerCallback();
    startTimerHz (6);
}

DivinityEditor::~DivinityEditor()
{
    proc.presets.removeChangeListener (this);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DivinityEditor::paint (juce::Graphics& g) { g.fillAll (background); }

void DivinityEditor::startExplain()
{
    if (! proc.uiAngelOpen) setAngelOpen (true);
    explainOverlay.setVisible (true);
    explainOverlay.toFront (true);
    explainOverlay.grabKeyboardFocus();
}

void DivinityEditor::updateAngelToggleText()
{
    angelToggle.setButtonText ({});   // drawn with the wing icon by Content::paintOverChildren
    angelToggle.setToggleState (true, juce::dontSendNotification);
    content.repaint();
}

void DivinityEditor::applySizeLimits()
{
    const int W = getBaseWidth();
    getConstrainer()->setFixedAspectRatio ((double) W / kBaseHeight);
    setResizeLimits ((int) (W * 0.45f), (int) (kBaseHeight * 0.45f), (int) (W * 1.6f), (int) (kBaseHeight * 1.6f));
}

void DivinityEditor::setAngelOpen (bool open)
{
    if (open == proc.uiAngelOpen) return;
    const int h = getHeight();
    const float s = (float) h / (float) kBaseHeight;
    proc.uiAngelOpen = open;
    angel.setVisible (open);
    updateAngelToggleText();
    getConstrainer()->setFixedAspectRatio (0.0);
    setResizeLimits (1, 1, 10000, 10000);
    setSize (juce::roundToInt (getBaseWidth() * s), h);
    applySizeLimits();
    resized();
    content.repaint();
}

void DivinityEditor::resized()
{
    const int W = getBaseWidth();
    const float s = (float) getWidth() / (float) W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, W, kBaseHeight);
    proc.uiScale = s;
    constexpr int M = kBaseWidth;   // main column width (angel sits to its right)

    // header icons (top right of the main column)
    settings.setBounds (1313, 17, 34, 34);
    help.setBounds (1357, 17, 34, 34);
    menu.setBounds (1402, 17, 34, 34);

    // tabs (positions measured from the v0.7 mockup)
    const int tabX[] = { 125, 311, 496, 685, 874, 1063 }, tabW[] = { 175, 174, 178, 178, 178, 180 };
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (tabX[i], 97, tabW[i], 44);
    angelToggle.setBounds (1289, 97, 144, 44);

    // preset strip
    constexpr int y = 158, h = 40;
    int x = 26;
    juce::ignoreUnused (x);
    const int iconX[] = { 38, 94, 156, 206, 265, 324 };
    int k = 0;
    for (auto* b : std::initializer_list<juce::Component*> { &folder, &save, &undo, &redo, &copy, &favs }) b->setBounds (iconX[k++], y, 38, h);
    presetField = { 402, 160, 309, 37 };
    prevPreset.setBounds (presetField.getX() + 12, 162, 32, 33);
    presetName.setBounds (presetField.getX() + 46, 162, presetField.getWidth() - 120, 33);
    star.setBounds (presetField.getRight() - 72, 164, 30, 30);
    nextPreset.setBounds (presetField.getRight() - 40, 162, 32, 33);
    savePlus.setBounds (715, 160, 40, 37);
    arpButton.setBounds (788, 163, 68, 31);
    voiceMode.setBounds (872, 163, 220, 31);
    voices.setBounds (1183, 164, 57, 30);
    glideKnob.setBounds (1311, 162, 34, 34);
    glideBox.setBounds (1357, 164, 71, 30);

    // pages
    const juce::Rectangle<int> page (12, 210, 1424, 646);
    for (auto* c : std::initializer_list<juce::Component*> { &createPage, &effectsHost, &playHost, &soulHost, &labHost, &browserPage })
        c->setBounds (page);
    keys.setBounds (12, 866, 1424, 122);
    angel.setBounds (M, 96, kAngelWidth - 12, 892);
    explainOverlay.setBounds (0, 0, M, kBaseHeight);
}

//==============================================================================
DivinityEditor::Content::Content (DivinityEditor& e) : owner (e)
{
    band.setSource (juce::ImageCache::getFromMemory (DivinityAssets::header_band_png, DivinityAssets::header_band_pngSize));
}

void DivinityEditor::Content::paintOverChildren (juce::Graphics& g)
{
    using namespace ui::colours;
    // tab labels: icon + name, centred as a group (matches the v0.7 mockup)
    auto drawLabel = [&g] (juce::Rectangle<float> b, juce::Path ic, const juce::String& name, float size, bool selected, bool fillIcon)
    {
        const auto f = font (size, Weight::Medium, 0.03f);
        const float tw = f.getStringWidthFloat (name), iw = 22.0f, gap = 14.0f;
        const float x0 = b.getCentreX() - (iw + gap + tw) * 0.5f;
        ic.applyTransform (ic.getTransformToScaleToFit ({ x0, b.getCentreY() - 11.0f, iw, 22.0f }, true));
        g.setColour (selected ? glow : text);
        if (fillIcon) { g.setColour (accent.withAlpha (0.25f)); g.fillPath (ic); g.setColour (selected ? glow : text); }
        g.strokePath (ic, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (f);
        g.setColour (text);
        g.drawText (name, juce::Rectangle<float> (x0 + iw + gap, b.getY(), tw + 4.0f, b.getHeight()), juce::Justification::centredLeft, false);
    };
    const juce::StringArray names { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB", "BROWSER" };
    for (int i = 0; i < owner.tabs.size(); ++i)
        drawLabel (owner.tabs[i]->getBounds().toFloat(), tabIcon (i), names[i], 16.5f, owner.tabs[i]->getToggleState(), false);
    // VOICES box chevron (over the number box)
    {
        const float cx = (float) owner.voices.getRight() - 12.0f, cy = (float) owner.voices.getBounds().getCentreY();
        juce::Path ch; ch.startNewSubPath (cx - 3.5f, cy - 1.8f); ch.lineTo (cx, cy + 1.8f); ch.lineTo (cx + 3.5f, cy - 1.8f);
        g.setColour (textDim); g.strokePath (ch, juce::PathStrokeType (1.3f));
    }
    drawLabel (owner.angelToggle.getBounds().toFloat(), wingIcon(), proc().uiAngelOpen ? "HIDE ANGEL" : "SHOW ANGEL", 14.0f, true, true);
}

void DivinityEditor::Content::paint (juce::Graphics& g)
{
    constexpr int M = kBaseWidth;
    g.fillAll (background);

    // header art (logo, DIVINITY wordmark, mountains) from the v0.7 mockup, 2x resolution
    if (band.isValid())
    {
        band.draw (g, { 0.0f, 0.0f, (float) M, 152.0f });
        if (getWidth() > M)
        {
            // Guardian Angel open: continue the night sky by mirroring the right edge
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (M, 0, getWidth() - M, 152);
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated ((float) M * 2.0f, 0.0f));
            band.draw (g, { 0.0f, 0.0f, (float) M, 152.0f });
        }
    }

    // stats (top right)
    g.setColour (text.withAlpha (0.92f));
    g.setFont (font (12.0f, Weight::Medium, 0.02f));
    g.drawText (owner.cpuText, 1153, 20, 90, 15, juce::Justification::centredLeft);
    g.drawText (owner.ramText, 1208, 20, 100, 15, juce::Justification::centredLeft);
    g.drawText (owner.voicesText, 1153, 37, 160, 15, juce::Justification::centredLeft);
    auto bar = juce::Rectangle<float> (1153.0f, 57.0f, 125.0f, 5.0f);
    g.setColour (juce::Colour (0xff15232c)); g.fillRoundedRectangle (bar, 2.5f);
    auto lit = bar.withWidth (juce::jmax (4.0f, bar.getWidth() * owner.voiceFill));
    g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (lit.expanded (1.5f), 3.5f);
    g.setGradientFill (juce::ColourGradient (accentStrong, lit.getX(), 0, glow, lit.getRight(), 0, false));
    g.fillRoundedRectangle (lit, 2.5f);

    // preset strip
    drawPanelBackground (g, { 14.0f, 153.0f, (float) M - 28.0f, 51.0f });
    const auto pf = owner.presetField.toFloat();
    drawGlassControl (g, pf, false, false, 7.0f);
    g.setColour (juce::Colour (0xff22394a));
    for (int x : { 146, 258, 316 }) g.drawVerticalLine (x, 165.0f, 193.0f);
    g.drawVerticalLine (owner.arpButton.getX() - 16, 165.0f, 193.0f);
    g.drawVerticalLine (owner.voiceMode.getRight() + 16, 165.0f, 193.0f);
    g.setColour (text);
    g.setFont (font (13.5f, Weight::Medium, 0.03f));
    g.drawText ("VOICES", owner.voices.getX() - 66, 158, 58, 40, juce::Justification::centredRight);
    g.drawText ("GLIDE", owner.glideKnob.getX() - 66, 158, 56, 40, juce::Justification::centredRight);
}

void DivinityEditor::ExplainOverlay::paint (juce::Graphics& g)
{
    g.fillAll (accentDeep.withAlpha (0.12f));
    auto banner = juce::Rectangle<float> ((float) getWidth() * 0.5f - 220.0f, 102.0f, 440.0f, 34.0f);
    g.setColour (juce::Colour (0xf20a1426));
    g.fillRoundedRectangle (banner, 17.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (banner, 17.0f, 1.2f);
    g.setFont (font (13.5f, Weight::Medium, 0.06f));
    g.drawText ("CLICK ANY CONTROL TO EXPLAIN IT  -  ESC TO CANCEL", banner, juce::Justification::centred);
}

void DivinityEditor::ExplainOverlay::mouseDown (const juce::MouseEvent& e)
{
    setVisible (false);
    const auto pos = e.getEventRelativeTo (&owner.content).getPosition();
    juce::String tip;
    for (auto* c = owner.content.getComponentAt (pos); c != nullptr && c != &owner.content; c = c->getParentComponent())
        if (auto* tc = dynamic_cast<juce::TooltipClient*> (c))
        {
            tip = tc->getTooltip();
            if (tip.isNotEmpty()) break;
        }
    owner.angel.explainText (tip);
}

bool DivinityEditor::ExplainOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { setVisible (false); owner.angel.showReply ({ "Explain mode off." }); return true; }
    return false;
}

//==============================================================================
void DivinityEditor::showTab (const juce::String& tab)
{
    const juce::StringArray names { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB", "BROWSER" };
    const int idx = juce::jmax (0, names.indexOf (tab));
    proc.uiWorkspace = names[idx];
    juce::Component* pages[] = { &createPage, &effectsHost, &playHost, &soulHost, &labHost, &browserPage };
    for (int i = 0; i < 6; ++i)
    {
        pages[i]->setVisible (i == idx);
        tabs[i]->setToggleState (i == idx, juce::dontSendNotification);
    }
    content.repaint();
}

void DivinityEditor::timerCallback()
{
    cpuText = "CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%";
    {
        const int q = proc.getRunningQuality();
        if (q == (int) Quality::Eco) cpuText << " ECO";
        else if (q == (int) Quality::HQ) cpuText << " HQ";
    }
    const auto mem = processMemoryBytes();
    ramText = mem > 0 ? (mem >= (juce::int64) 1 << 30 ? "RAM " + juce::String ((double) mem / (double) (1 << 30), 1) + " GB"
                                                      : "RAM " + juce::String ((int) (mem >> 20)) + " MB")
                      : juce::String();
    const int maxV = juce::jmax (1, proc.getMaxVoices());
    voicesText = "VOICES " + juce::String (proc.getActiveVoices()) + " / " + juce::String (maxV);
    voiceFill = juce::jlimit (0.0f, 1.0f, (float) proc.getActiveVoices() / (float) maxV);
    content.repaint (kBaseWidth - 404, 16, 260, 50);
    undo.setEnabled (proc.undo.canUndo());
    redo.setEnabled (proc.undo.canRedo());
    updatePresetDisplay();
}

juce::Array<int> DivinityEditor::presetsInCategory() const
{
    juce::Array<int> list;
    const auto& all = proc.presets.getPresets();
    for (int i = 0; i < all.size(); ++i)
    {
        const auto& p = all.getReference (i);
        if (categoryFilter == "ALL" || (categoryFilter == "FAVOURITES" && proc.presets.isFavourite (p))
            || (categoryFilter == "USER" && ! p.isFactory) || p.category.equalsIgnoreCase (categoryFilter))
            list.add (i);
    }
    return list;
}

void DivinityEditor::updatePresetDisplay()
{
    auto& pm = proc.presets;
    const int idx = pm.getCurrentIndex();
    juce::String name = pm.getCurrentName();
    if (idx >= 0) name = juce::String (idx + 1).paddedLeft ('0', 3) + ". " + name;
    if (pm.isModified()) name += " *";
    if (presetName.getButtonText() != name) presetName.setButtonText (name);
    const bool fav = juce::isPositiveAndBelow (idx, pm.getPresets().size()) && pm.isFavourite (pm.getPresets()[idx]);
    if (star.getToggleState() != fav) star.setToggleState (fav, juce::dontSendNotification);
    favs.setTooltip ("Browse by category / favourites (now: " + categoryFilter + ", " + juce::String (presetsInCategory().size()) + " presets). Previous / next follow this filter.");
}

void DivinityEditor::stepPreset (int delta)
{
    const auto list = presetsInCategory();
    if (list.isEmpty()) return;
    int pos = list.indexOf (proc.presets.getCurrentIndex());
    pos = pos < 0 ? (delta > 0 ? 0 : list.size() - 1) : (pos + delta + list.size()) % list.size();
    proc.presets.loadPreset (list[pos]);
}

void DivinityEditor::showPresetMenu (juce::Component& target)
{
    auto& pm = proc.presets;
    const auto& all = pm.getPresets();
    juce::PopupMenu m, favMenu, user;
    for (int i = 0; i < all.size(); ++i)
    {
        if (pm.isFavourite (all[i])) favMenu.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex());
        if (! all[i].isFactory) user.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex());
    }
    if (favMenu.getNumItems() > 0) m.addSubMenu ("Favourites", favMenu);
    if (user.getNumItems() > 0) m.addSubMenu ("User", user);
    if (m.getNumItems() > 0) m.addSeparator();
    for (auto& c : presetCategories())
    {
        juce::PopupMenu sub;
        bool cur = false;
        for (int i = 0; i < all.size(); ++i)
            if (all[i].isFactory && all[i].category.equalsIgnoreCase (c)) { sub.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex()); cur |= i == pm.getCurrentIndex(); }
        if (sub.getNumItems() > 0) m.addSubMenu (c, sub, true, nullptr, cur);
    }
    m.addSeparator();
    m.addItem (100000, "Init patch");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [this] (int r)
    {
        if (r == 100000) proc.presets.loadInit();
        else if (r > 0) proc.presets.loadPreset (r - 1);
    });
}

void DivinityEditor::showCategoryMenu (juce::Component& target)
{
    juce::StringArray cats { "ALL", "FAVOURITES", "USER" };
    cats.addArray (presetCategories());
    juce::PopupMenu m;
    for (int i = 0; i < cats.size(); ++i)
    {
        if (i == 3) m.addSeparator();
        m.addItem (i + 1, cats[i], true, cats[i] == categoryFilter);
    }
    m.addSeparator();
    m.addItem (100, "Open the BROWSER");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [this, cats] (int r)
    {
        if (r == 100) { showTab ("BROWSER"); return; }
        if (r <= 0) return;
        categoryFilter = cats[r - 1];
        updatePresetDisplay();
        const auto list = presetsInCategory();
        if (! list.isEmpty() && ! list.contains (proc.presets.getCurrentIndex())) proc.presets.loadPreset (list[0]);
    });
}

void DivinityEditor::savePresetDialog (bool asCopy)
{
    auto* w = new juce::AlertWindow (asCopy ? "Save a copy" : "Save preset", "Saves the current sound to your user presets.", juce::MessageBoxIconType::NoIcon, this);
    auto name = proc.presets.getCurrentName();
    if (asCopy && name != "Init") name += " Copy";
    w->addTextEditor ("name", name == "Init" ? juce::String() : name, "Name");
    juce::StringArray cats { "User" };
    for (auto& c : presetCategories()) cats.add (c);
    w->addComboBox ("category", cats, "Category");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DivinityEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto err = safe->proc.presets.saveUserPreset (w->getTextEditorContents ("name"), w->getComboBoxComponent ("category")->getText());
        if (err.isNotEmpty())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Could not save").withMessage (err).withButton ("OK")
                                              .withAssociatedComponent (safe.getComponent()), nullptr);
    }), true);
}

void DivinityEditor::importPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Import DIVINITY preset", PresetManager::getUserPresetFolder(), "*.divpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() == juce::File()) return;
        const auto err = proc.presets.importPreset (fc.getResult());
        if (err.isNotEmpty())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Import failed").withMessage (err).withButton ("OK")
                                              .withAssociatedComponent (this), nullptr);
    });
}

void DivinityEditor::setUiScale (float s) { setSize (juce::roundToInt (getBaseWidth() * s), juce::roundToInt (kBaseHeight * s)); }

void DivinityEditor::showMainMenu()
{
    juce::PopupMenu m, size;
    const float scales[] = { 0.55f, 0.65f, 0.75f, 0.85f, 1.0f, 1.15f, 1.3f };
    for (int i = 0; i < 7; ++i)
        size.addItem (10 + i, juce::String (juce::roundToInt (scales[i] * 100)) + "%", true, std::abs (proc.uiScale - scales[i]) < 0.02f);
    m.addItem (1, "Init patch");
    m.addItem (2, "Save preset...");
    m.addItem (3, "Import preset...");
    m.addItem (4, "Export current sound...");
    m.addItem (5, "Show user preset folder");
    m.addSeparator();
    m.addSubMenu ("Interface size", size);
    m.addItem (6, "Guardian Angel tips", true, proc.uiAngelTips);
    m.addItem (7, "MIDI panic (all notes off)");
    {
        juce::PopupMenu q, mpe, bend;
        const int curQ = (int) std::lround (proc.apvts.getRawParameterValue (ids::quality)->load());
        q.addItem (40, "Eco  (lighter CPU, unison max 4)", true, curQ == 0);
        q.addItem (41, "Standard", true, curQ == 1);
        q.addItem (42, "HQ  (2x oversampled engine)", true, curQ == 2);
        q.addSeparator();
        q.addItem (43, "Always HQ when rendering / bouncing", true, proc.apvts.getRawParameterValue (ids::qualityRender)->load() > 0.5f);
        m.addSubMenu ("Quality", q);
        const bool mpeOn = proc.apvts.getRawParameterValue (ids::mpeOn)->load() > 0.5f;
        const int curBend = (int) std::lround (proc.apvts.getRawParameterValue (ids::mpeBend)->load());
        mpe.addItem (50, "MPE (lower zone, ch 2-16)", true, mpeOn);
        for (int b : { 2, 12, 24, 48, 96 }) bend.addItem (60 + b, juce::String (b) + " semitones", true, curBend == b);
        mpe.addSubMenu ("Per-note bend range", bend);
        m.addSubMenu ("MPE", mpe);
    }
    m.addSeparator();
    m.addItem (8, "DIVINITY " + juce::String (DIVINITY_VERSION_STRING), false);
    m.addItem (9, "TRIPLE SEVEN AUDIO  -  triplesevenaudio.com");
    m.showMenuAsync (juce::PopupMenu::Options(), [this, scales] (int r)
    {
        auto& pm = proc.presets;
        if (r >= 10 && r < 17) { setUiScale (scales[r - 10]); return; }
        auto setParam = [this] (const juce::String& id, float plain)
        {
            if (auto* p = proc.apvts.getParameter (id))
            { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
        };
        if (r >= 40 && r <= 42) { setParam (ids::quality, (float) (r - 40)); return; }
        if (r == 43) { setParam (ids::qualityRender, proc.apvts.getRawParameterValue (ids::qualityRender)->load() > 0.5f ? 0.0f : 1.0f); return; }
        if (r == 50) { setParam (ids::mpeOn, proc.apvts.getRawParameterValue (ids::mpeOn)->load() > 0.5f ? 0.0f : 1.0f); return; }
        if (r > 60 && r <= 156) { setParam (ids::mpeBend, (float) (r - 60)); return; }
        switch (r)
        {
            case 1: pm.loadInit(); break;
            case 2: savePresetDialog(); break;
            case 3: importPreset(); break;
            case 4:
                chooser = std::make_unique<juce::FileChooser> ("Export DIVINITY preset",
                    juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (juce::File::createLegalFileName (pm.getCurrentName()) + ".divpreset"), "*.divpreset");
                chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File()) proc.presets.exportCurrent (fc.getResult().withFileExtension ("divpreset"));
                });
                break;
            case 5: { auto f = PresetManager::getUserPresetFolder(); f.createDirectory(); f.revealToUser(); break; }
            case 6: proc.uiAngelTips = ! proc.uiAngelTips; break;
            case 7: proc.panicRequested = true; proc.keyboardState.allNotesOff (0); break;
            case 9: juce::URL ("https://triplesevenaudio.com").launchInDefaultBrowser(); break;
            default: break;
        }
    });
}

bool DivinityEditor::keyPressed (const juce::KeyPress& k)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    if (k == juce::KeyPress ('z', cmd, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0) || k == juce::KeyPress ('y', cmd, 0)) { proc.undo.redo(); return true; }
    if (k == juce::KeyPress::escapeKey && explainOverlay.isVisible()) { explainOverlay.setVisible (false); return true; }
    return false;
}

} // namespace divinity
