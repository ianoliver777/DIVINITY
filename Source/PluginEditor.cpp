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
    juce::Path tabIcon (int i)
    {
        juce::Path p;
        switch (i)
        {
            case 0: p.addStar ({ 12.0f, 12.0f }, 5, 4.5f, 10.0f, 0.0f); break;                               // CREATE
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
        static const juce::StringArray c { "BASS", "808", "SUB", "LEAD", "SYNTH", "PLUCK", "PAD", "PERCUSSION", "FX" };
        return c;
    }
}

//==============================================================================
KeyboardStrip::KeyboardStrip (DivinityProcessor& p)
    : proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), bend (p.apvts, ids::bendRange), scope (p)
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

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setScrollButtonsVisible (true);
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
    for (auto* k : knobs) { k->setCaptionSize (10.5f); addAndMakeVisible (k); }
    addAndMakeVisible (scope);
}

void KeyboardStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanelBackground (g, { 0.0f, 0.0f, 120.0f, b.getHeight() });
    drawPanelBackground (g, { 128.0f, 0.0f, 62.0f, b.getHeight() });
    drawPanelBackground (g, { b.getWidth() - 444.0f, 0.0f, 330.0f, b.getHeight() });
    drawPanelBackground (g, { b.getWidth() - 106.0f, 0.0f, 106.0f, b.getHeight() });
    g.setColour (textDim);
    g.setFont (font (10.5f, Weight::Medium, 0.1f));
    g.drawText ("PITCH", 8, getHeight() - 22, 52, 16, juce::Justification::centred);
    g.drawText ("MOD", 60, getHeight() - 22, 52, 16, juce::Justification::centred);
    g.drawText ("BEND", 128, 14, 62, 16, juce::Justification::centred);
}

void KeyboardStrip::resized()
{
    const int w = getWidth(), h = getHeight();
    pitch.setBounds (22, 10, 24, h - 36);
    mod.setBounds (74, 10, 24, h - 36);
    bend.setBounds (136, 38, 46, 28);
    keyboard.setBounds (200, 8, w - 200 - 456, h - 16);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 29.0f);
    const int kx = w - 440;
    for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (kx + i * 64, 6, 64, h - 10);
    scope.setBounds (w - 100, 8, 94, h - 16);
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
        b->getProperties().set ("fontSize", 16.0);
        b->getProperties().set ("corner", 6.0);
        const juce::String tab (name);
        b->onClick = [this, tab] { showTab (tab); angel.showTabTip (tab); };
        content.addAndMakeVisible (b);
    }
    angelToggle.getProperties().set ("fontSize", 13.0);
    angelToggle.getProperties().set ("corner", 6.0);
    angelToggle.setTooltip ("Show or hide Guardian Angel. Hiding him makes the window narrower.");
    angelToggle.onClick = [this] { setAngelOpen (! proc.uiAngelOpen); };
    content.addAndMakeVisible (angelToggle);

    // preset strip
    presetName.getProperties().set ("transparent", true);
    presetName.getProperties().set ("fontSize", 15.0);
    presetName.setColour (juce::TextButton::textColourOffId, text);
    presetName.setTooltip ("Click to browse presets.");
    star.filledWhenOn = true;
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
    arpButton.getProperties().set ("corner", 5.0);
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
    angelToggle.setButtonText (proc.uiAngelOpen ? juce::String (juce::CharPointer_UTF8 ("HIDE ANGEL  \xe2\x80\xba"))
                                                : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9  SHOW ANGEL")));
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
    settings.setBounds (M - 132, 20, 34, 34);
    help.setBounds (M - 92, 20, 34, 34);
    menu.setBounds (M - 52, 20, 34, 34);

    // tabs
    const int tabW = 178, gap = 10;
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (124 + i * (tabW + gap), 96, tabW, 46);
    angelToggle.setBounds (M - 160, 96, 148, 46);

    // preset strip
    constexpr int y = 158, h = 40;
    int x = 26;
    for (auto* b : std::initializer_list<juce::Component*> { &folder, &save, &undo, &redo, &copy, &favs }) { b->setBounds (x, y, 40, h); x += 47; }
    presetField = { 400, y, 312, h };
    prevPreset.setBounds (presetField.getX() + 4, y + 4, 32, 32);
    presetName.setBounds (presetField.getX() + 38, y + 2, presetField.getWidth() - 110, h - 4);
    star.setBounds (presetField.getRight() - 70, y + 6, 28, 28);
    nextPreset.setBounds (presetField.getRight() - 36, y + 4, 32, 32);
    savePlus.setBounds (presetField.getRight() + 12, y, 42, h);
    arpButton.setBounds (786, y + 2, 66, h - 4);
    voiceMode.setBounds (872, y + 2, 228, h - 4);
    voices.setBounds (1184, y + 4, 56, h - 8);
    glideKnob.setBounds (1304, y + 1, 38, 38);
    glideBox.setBounds (1350, y + 4, 76, h - 8);

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
    logo.setSource (juce::ImageCache::getFromMemory (DivinityAssets::triple_seven_logo_png, DivinityAssets::triple_seven_logo_pngSize));
}

void DivinityEditor::Content::paintOverChildren (juce::Graphics& g)
{
    using namespace ui::colours;
    // tab icons, drawn over the tab buttons so the selected tab keeps its icon too
    for (int i = 0; i < owner.tabs.size(); ++i)
    {
        auto b = owner.tabs[i]->getBounds().toFloat();
        auto ic = tabIcon (i);
        ic.applyTransform (ic.getTransformToScaleToFit ({ b.getX() + 22.0f, b.getCentreY() - 10.0f, 20.0f, 20.0f }, true));
        g.setColour (owner.tabs[i]->getToggleState() ? text : textDim);
        g.strokePath (ic, juce::PathStrokeType (1.5f));
    }
}

void DivinityEditor::Content::paint (juce::Graphics& g)
{
    constexpr int M = kBaseWidth;
    const float cx = (float) M * 0.5f;
    g.fillAll (background);

    // night-sky backdrop: deep blue haze, faint ridges, a few stars
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1d3a), cx, 40.0f, background, cx, 420.0f, true));
    g.fillRect (0, 0, M, 420);
    {
        juce::Path ridge;
        ridge.startNewSubPath (0, 150);
        juce::Random rr (777);
        for (int x = 0; x <= M; x += 48) ridge.lineTo ((float) x, 70.0f + 60.0f * rr.nextFloat() + 30.0f * std::sin (x * 0.004f));
        ridge.lineTo ((float) M, 150); ridge.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x3326466f), 0, 60, juce::Colour (0x00000000), 0, 150, false));
        g.fillPath (ridge);
        juce::Random st (7);
        for (int i = 0; i < 70; ++i)
        {
            const float sx = st.nextFloat() * M, sy = st.nextFloat() * 92.0f, a = 0.15f + 0.5f * st.nextFloat();
            g.setColour (juce::Colours::white.withAlpha (a));
            g.fillEllipse (sx, sy, 1.4f, 1.4f);
        }
    }

    // TRIPLE SEVEN AUDIO logo (top left, deliberately understated so DIVINITY leads)
    if (logo.isValid())
    {
        const float lw = 172.0f;
        logo.draw (g, { 30.0f, 20.0f, lw, lw * (float) logo.getHeight() / (float) logo.getWidth() }, 0.5f);
    }

    // DIVINITY wordmark: wide chrome-blue letters with a flare
    {
        const float tw = 540.0f, ty = 16.0f;
        const juce::Rectangle<float> tr (cx - tw * 0.5f, ty, tw, 66.0f);
        auto f = font (64.0f, Weight::SemiBold, 0.12f);
        f.setHorizontalScale (1.28f);
        juce::GlyphArrangement ga;
        ga.addFittedText (f, "DIVINITY", tr.getX(), tr.getY(), tr.getWidth(), tr.getHeight(), juce::Justification::centred, 1);
        juce::Path letters;
        ga.createPath (letters);
        g.setColour (accent.withAlpha (0.25f));
        for (float d : { 4.0f, 2.0f })
            g.strokePath (letters, juce::PathStrokeType (d * 2.0f, juce::PathStrokeType::curved));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeaf6ff), 0, tr.getY() + 6.0f, juce::Colour (0xff3d9bff), 0, tr.getBottom() - 4.0f, false));
        g.fillPath (letters);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.strokePath (letters, juce::PathStrokeType (0.8f));
        // flare above the centre
        const float fx = cx, fy = 10.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, fx, fy, juce::Colours::transparentWhite, fx + 120.0f, fy, true));
        g.fillRect (fx - 120.0f, fy - 0.8f, 240.0f, 1.6f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, fx, fy, juce::Colours::transparentWhite, fx, fy + 18.0f, true));
        g.fillRect (fx - 0.8f, fy - 18.0f, 1.6f, 36.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (fx - 3.0f, fy - 3.0f, 6.0f, 6.0f);
        g.setColour (textDim);
        g.setFont (font (13.0f, Weight::Medium, 1.1f));
        g.drawText ("SYNTHESIZER", (int) (cx - 200), 76, 400, 16, juce::Justification::centred);
    }

    // stats (top right)
    g.setColour (textDim);
    g.setFont (font (11.5f, Weight::Medium, 0.04f));
    g.drawText (owner.cpuText + "     " + owner.ramText, M - 400, 20, 250, 15, juce::Justification::centredLeft);
    g.drawText (owner.voicesText, M - 400, 38, 250, 15, juce::Justification::centredLeft);
    auto bar = juce::Rectangle<float> ((float) M - 400.0f, 58.0f, 124.0f, 4.0f);
    g.setColour (panel2); g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (accent); g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * owner.voiceFill)), 2.0f);


    // preset strip
    drawPanelBackground (g, { 12.0f, 152.0f, (float) M - 24.0f, 52.0f });
    const auto pf = owner.presetField.toFloat();
    g.setColour (juce::Colour (0xff081224)); g.fillRoundedRectangle (pf, 6.0f);
    g.setColour (borderBright); g.drawRoundedRectangle (pf.reduced (0.5f), 6.0f, 1.0f);
    g.drawRoundedRectangle (owner.savePlus.getBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (border);
    g.drawVerticalLine (owner.arpButton.getX() - 10, 162.0f, 194.0f);
    g.setColour (text);
    g.setFont (font (12.5f, Weight::Medium, 0.06f));
    g.drawText ("VOICES", owner.voices.getX() - 70, 158, 62, 40, juce::Justification::centredRight);
    g.drawText ("GLIDE", owner.glideKnob.getX() - 60, 158, 54, 40, juce::Justification::centredRight);
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
    m.addSeparator();
    m.addItem (8, "DIVINITY " + juce::String (DIVINITY_VERSION_STRING), false);
    m.addItem (9, "TRIPLE SEVEN AUDIO  -  triplesevenaudio.com");
    m.showMenuAsync (juce::PopupMenu::Options(), [this, scales] (int r)
    {
        auto& pm = proc.presets;
        if (r >= 10 && r < 17) { setUiScale (scales[r - 10]); return; }
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
