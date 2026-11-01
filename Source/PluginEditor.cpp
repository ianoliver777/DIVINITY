#include "PluginEditor.h"

namespace divinity
{
using namespace ui;
using namespace ui::colours;

static juce::Path chevron (bool left)
{
    juce::Path p;
    if (left) { p.startNewSubPath (15, 5); p.lineTo (8, 12); p.lineTo (15, 19); }
    else      { p.startNewSubPath (9, 5);  p.lineTo (16, 12); p.lineTo (9, 19); }
    return p;
}

static juce::Path dots()
{
    juce::Path p;
    for (int i = 0; i < 3; ++i) p.addEllipse (3.0f + i * 7.0f, 10.0f, 3.2f, 3.2f);
    return p;
}

static juce::Path ring()
{
    // "stop" glyph: circle with a small square = MIDI panic
    juce::Path p;
    p.addCentredArc (12, 12, 9, 9, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
    p.addRectangle (9.5f, 9.5f, 5.0f, 5.0f);
    return p;
}

//==============================================================================
SideNav::SideNav() { setRepaintsOnMouseActivity (true); }

void SideNav::paint (juce::Graphics& g)
{
    static const char* names[] = { "PLAY", "SYNTH", "MOTION", "FX", "SOUL", "LAB" };
    for (int i = 0; i < 6; ++i)
    {
        auto r = juce::Rectangle<int> (8, kTop + i * kItemHeight, getWidth() - 16, kItemHeight - 6);
        const bool disabled = i >= 4;
        if (i == selected)
        {
            juce::ColourGradient grad (juce::Colour (0xff3b3456), (float) r.getX(), 0, juce::Colour (0x00221f30), (float) r.getRight(), 0, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r.toFloat(), 6.0f);
            g.setColour (accent);
            g.fillRoundedRectangle ((float) r.getX(), (float) r.getY() + 8.0f, 2.5f, (float) r.getHeight() - 16.0f, 1.0f);
        }
        else if (i == hover && ! disabled)
        {
            g.setColour (panel2.withAlpha (0.6f));
            g.fillRoundedRectangle (r.toFloat(), 6.0f);
        }
        auto icon = icons::nav (i, juce::Rectangle<float> ((float) r.getX() + 16.0f, (float) r.getCentreY() - 11.0f, 22.0f, 22.0f));
        const auto c = disabled ? textFaint : (i == selected ? text : textDim);
        g.setColour (c);
        g.strokePath (icon, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (font (14.5f, Weight::Medium, 0.06f));
        g.drawText (names[i], r.getX() + 56, r.getY(), 90, r.getHeight(), juce::Justification::centredLeft);
        if (disabled)
        {
            g.setFont (font (9.0f, Weight::Medium, 0.15f));
            g.drawText (i == 4 ? "PHASE 2" : "PHASE 3", r.getX() + 56, r.getY() + 38, 90, 12, juce::Justification::centredLeft);
        }
    }
    g.setColour (border);
    g.drawHorizontalLine (kTop + 6 * kItemHeight + 10, 24.0f, (float) getWidth() - 24.0f);
}

void SideNav::mouseMove (const juce::MouseEvent& e)
{
    const int h = itemAt (e.y);
    if (h != hover)
    {
        hover = h;
        setTooltip (h == 4 ? "SOUL ENGINE, DNA Fusion, Time Fracture and Divine Intervention arrive in Phase 2-3."
                  : h == 5 ? "LAB (wavetable, spectral and granular editors, Mutation Lab) arrives in Phase 3." : juce::String());
        repaint();
    }
}

void SideNav::mouseUp (const juce::MouseEvent& e)
{
    const int i = itemAt (e.y);
    if (i >= 0 && i < 4 && onSelect) onSelect (i);
}

//==============================================================================
DivinityEditor::DivinityEditor (DivinityProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      prevPreset ("Previous preset", chevron (true)), nextPreset ("Next preset", chevron (false)),
      heart ("Favourite this preset", icons::heart()), save ("Save preset", icons::save()),
      menu ("Preset options", dots(), true), panic ("MIDI panic: silence all notes", ring()), settings ("Settings", icons::gear()),
      topMeter (p, false),
      oscSection (p), filterSection (p), envSection (p), lfoSection (p), matrixSection (p), macroSection (p),
      fxSection (p), globalSection (p), createView (p), keyboardBar (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* name : { "CREATE", "DESIGN", "LAB" })
    {
        auto* b = workspaceButtons.add (new juce::TextButton (name));
        b->getProperties().set ("corner", 14.0);
        b->getProperties().set ("noOutline", true);
        content.addAndMakeVisible (b);
    }
    workspaceButtons[0]->onClick = [this] { setWorkspace ("CREATE"); };
    workspaceButtons[1]->onClick = [this] { setWorkspace ("DESIGN"); };
    workspaceButtons[2]->setEnabled (false);
    workspaceButtons[2]->setTooltip ("LAB arrives in Phase 3.");
    workspaceButtons[0]->setTooltip ("CREATE: presets, macros and a few musical controls.");
    workspaceButtons[1]->setTooltip ("DESIGN: the full synthesizer.");

    heart.filledWhenOn = true;
    for (auto* b : std::initializer_list<juce::Component*> { &prevPreset, &nextPreset, &heart, &save, &menu, &panic, &settings, &presetName, &topMeter })
        content.addAndMakeVisible (b);
    presetName.getProperties().set ("transparent", true);
    presetName.setColour (juce::TextButton::textColourOffId, text);
    presetName.setTooltip ("Click to browse presets.");

    prevPreset.onClick = [this] { proc.presets.loadPrevious(); };
    nextPreset.onClick = [this] { proc.presets.loadNext(); };
    presetName.onClick = [this] { showPresetMenu(); };
    heart.onClick = [this]
    {
        const int i = proc.presets.getCurrentIndex();
        if (juce::isPositiveAndBelow (i, proc.presets.getPresets().size()))
            proc.presets.toggleFavourite (proc.presets.getPresets()[i]);
    };
    save.onClick = [this] { savePresetDialog(); };
    menu.onClick = [this] { showFileMenu(); };
    panic.onClick = [this] { proc.panicRequested = true; proc.keyboardState.allNotesOff (0); };
    settings.onClick = [this] { showSettingsMenu(); };

    content.addAndMakeVisible (nav);
    nav.onSelect = [this] (int i) { navSelected (i); };

    for (auto* c : std::initializer_list<juce::Component*> { &oscSection, &filterSection, &envSection, &lfoSection, &matrixSection,
                                                             &macroSection, &fxSection, &globalSection, &createView, &keyboardBar })
        content.addAndMakeVisible (c);

    proc.presets.addChangeListener (this);
    updatePresetDisplay();
    setWorkspace (proc.uiWorkspace);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseWidth / kBaseHeight);
    setResizeLimits ((int) (kBaseWidth * 0.55), (int) (kBaseHeight * 0.55), (int) (kBaseWidth * 1.6), (int) (kBaseHeight * 1.6));
    setSize ((int) (kBaseWidth * proc.uiScale), (int) (kBaseHeight * proc.uiScale));
    setWantsKeyboardFocus (true);
    startTimerHz (10);
}

DivinityEditor::~DivinityEditor()
{
    proc.presets.removeChangeListener (this);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DivinityEditor::paint (juce::Graphics& g) { g.fillAll (background); }

void DivinityEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseWidth;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kBaseWidth, kBaseHeight);
    proc.uiScale = s;

    workspaceButtons[0]->setBounds (284, 23, 102, 30);
    workspaceButtons[1]->setBounds (388, 23, 102, 30);
    workspaceButtons[2]->setBounds (492, 23, 98, 30);

    prevPreset.setBounds (684, 22, 32, 32);
    nextPreset.setBounds (720, 22, 32, 32);
    presetName.setBounds (760, 22, 296, 32);
    heart.setBounds (1078, 21, 34, 34);
    save.setBounds (1128, 21, 34, 34);
    menu.setBounds (1178, 21, 34, 34);
    topMeter.setBounds (1468, 32, 80, 12);
    panic.setBounds (1560, 19, 38, 38);
    settings.setBounds (1620, 21, 34, 34);

    nav.setBounds (0, 90, 158, 440);

    oscSection.setBounds (170, 90, 1107, 298);
    filterSection.setBounds (170, 400, 432, 255);
    envSection.setBounds (613, 400, 333, 255);
    lfoSection.setBounds (958, 400, 319, 255);
    matrixSection.setBounds (1289, 90, 371, 396);
    macroSection.setBounds (1289, 494, 371, 161);
    fxSection.setBounds (170, 668, 1062, 132);
    globalSection.setBounds (1244, 668, 416, 132);
    createView.setBounds (170, 90, 1490, 710);
    keyboardBar.setBounds (12, 812, 1648, 118);
}

//==============================================================================
void DivinityEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (background);
    // subtle top glow
    g.setGradientFill (juce::ColourGradient (accentDeep.withAlpha (0.10f), 836.0f, 0.0f, juce::Colours::transparentBlack, 836.0f, 420.0f, true));
    g.fillRect (0, 0, kBaseWidth, 420);

    // logo
    g.setColour (text);
    g.setFont (font (25.0f, Weight::Medium, 0.62f));
    g.drawText ("DIVINITY", 36, 12, 220, 32, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (font (10.0f, Weight::Medium, 0.62f));
    g.drawText ("TRIPLE SEVEN AUDIO", 42, 42, 220, 16, juce::Justification::centredLeft);

    // workspace pill + preset bar outlines
    g.setColour (panel2.withAlpha (0.55f));
    g.fillRoundedRectangle (280.0f, 19.0f, 315.0f, 38.0f, 19.0f);
    g.setColour (border);
    g.drawRoundedRectangle (280.5f, 19.5f, 314.0f, 37.0f, 19.0f, 1.0f);
    g.drawRoundedRectangle (676.5f, 18.5f, 388.0f, 39.0f, 19.5f, 1.0f);

    // status
    g.setFont (font (12.5f, Weight::Medium, 0.04f));
    g.setColour (textDim);
    g.drawText (owner.cpuText, 1250, 24, 80, 28, juce::Justification::centredLeft);
    g.drawText (owner.voiceText, 1336, 24, 120, 28, juce::Justification::centredLeft);

    g.setColour (border);
    g.drawHorizontalLine (76, 0.0f, (float) kBaseWidth);
    g.drawVerticalLine (158, 76.0f, 806.0f);
}

void DivinityEditor::timerCallback()
{
    cpuText = "CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%";
    voiceText = "VOICES " + juce::String (proc.getActiveVoices()) + " / "
              + juce::String ((int) proc.apvts.getRawParameterValue (ids::voices)->load());
    content.repaint (1240, 18, 230, 40);
    updatePresetDisplay();
}

void DivinityEditor::updatePresetDisplay()
{
    auto& pm = proc.presets;
    const auto name = pm.getCurrentName() + (pm.isModified() ? " *" : "");
    if (presetName.getButtonText() != name) presetName.setButtonText (name);
    const int i = pm.getCurrentIndex();
    const bool fav = juce::isPositiveAndBelow (i, pm.getPresets().size()) && pm.isFavourite (pm.getPresets()[i]);
    if (heart.getToggleState() != fav) heart.setToggleState (fav, juce::dontSendNotification);
    heart.setEnabled (i >= 0);
}

//==============================================================================
void DivinityEditor::setWorkspace (const juce::String& ws)
{
    const bool create = ws == "CREATE";
    proc.uiWorkspace = create ? "CREATE" : "DESIGN";
    workspaceButtons[0]->setToggleState (create, juce::dontSendNotification);
    workspaceButtons[1]->setToggleState (! create, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &oscSection, &filterSection, &envSection, &lfoSection,
                                                             &matrixSection, &macroSection, &fxSection, &globalSection })
        c->setVisible (! create);
    createView.setVisible (create);
    nav.setSelected (create ? 0 : 1);
    navSelected (create ? 0 : 1);
}

void DivinityEditor::navSelected (int i)
{
    if (i == 0 && proc.uiWorkspace != "CREATE") { setWorkspace ("CREATE"); return; }
    if (i > 0 && proc.uiWorkspace == "CREATE") { setWorkspace ("DESIGN"); }
    nav.setSelected (i);
    oscSection.setHighlighted (i == 1);
    filterSection.setHighlighted (i == 1);
    envSection.setHighlighted (i == 2);
    lfoSection.setHighlighted (i == 2);
    matrixSection.setHighlighted (i == 2);
    fxSection.setHighlighted (i == 3);
}

bool DivinityEditor::keyPressed (const juce::KeyPress& k)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    if (k == juce::KeyPress ('z', cmd, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0) || k == juce::KeyPress ('y', cmd, 0)) { proc.undo.redo(); return true; }
    return false;
}

//==============================================================================
void DivinityEditor::showPresetMenu()
{
    auto& pm = proc.presets;
    const auto& all = pm.getPresets();
    juce::PopupMenu m, favs;
    juce::StringArray cats;
    for (auto& p : all) cats.addIfNotAlreadyThere (p.category);
    for (int i = 0; i < all.size(); ++i)
        if (pm.isFavourite (all[i])) favs.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex());
    if (favs.getNumItems() > 0) { m.addSubMenu ("Favourites", favs); m.addSeparator(); }
    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        bool containsCurrent = false;
        for (int i = 0; i < all.size(); ++i)
            if (all[i].category == c)
            {
                sub.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex());
                containsCurrent |= i == pm.getCurrentIndex();
            }
        m.addSubMenu (c, sub, true, nullptr, containsCurrent);
    }
    m.addSeparator();
    m.addItem (100000, "Init patch");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName), [this] (int r)
    {
        if (r == 100000) proc.presets.loadInit();
        else if (r > 0) proc.presets.loadPreset (r - 1);
    });
}

void DivinityEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Saves the current sound to your user presets.", juce::MessageBoxIconType::NoIcon, this);
    auto name = proc.presets.getCurrentName();
    w->addTextEditor ("name", name == "Init" ? juce::String() : name, "Name");
    juce::StringArray cats { "User", "Bass", "808", "Lead", "Pad", "Pluck", "Keys", "Bell", "Arpeggio", "Sequence",
                             "Percussion", "Cinematic", "Ambient", "Vocal Texture", "Experimental", "FX" };
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

void DivinityEditor::showFileMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Init patch");
    m.addItem (2, "Save preset...");
    m.addSeparator();
    m.addItem (3, "Import preset...");
    m.addItem (4, "Export current sound...");
    m.addItem (5, "Show user preset folder");
    m.addSeparator();
    m.addItem (6, "Undo " + proc.undo.getUndoDescription(), proc.undo.canUndo(), false);
    m.addItem (7, "Redo " + proc.undo.getRedoDescription(), proc.undo.canRedo(), false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menu), [this] (int r)
    {
        auto& pm = proc.presets;
        switch (r)
        {
            case 1: pm.loadInit(); break;
            case 2: savePresetDialog(); break;
            case 3:
                chooser = std::make_unique<juce::FileChooser> ("Import DIVINITY preset", PresetManager::getUserPresetFolder(), "*.divpreset");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() == juce::File()) return;
                    const auto err = proc.presets.importPreset (fc.getResult());
                    if (err.isNotEmpty())
                        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Import failed").withMessage (err).withButton ("OK")
                                                          .withAssociatedComponent (this), nullptr);
                });
                break;
            case 4:
                chooser = std::make_unique<juce::FileChooser> ("Export DIVINITY preset",
                                                               juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                                                   .getChildFile (juce::File::createLegalFileName (pm.getCurrentName()) + ".divpreset"),
                                                               "*.divpreset");
                chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() == juce::File()) return;
                    const auto err = proc.presets.exportCurrent (fc.getResult().withFileExtension ("divpreset"));
                    if (err.isNotEmpty())
                        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Export failed").withMessage (err).withButton ("OK")
                                                          .withAssociatedComponent (this), nullptr);
                });
                break;
            case 5: { auto f = PresetManager::getUserPresetFolder(); f.createDirectory(); f.revealToUser(); break; }
            case 6: proc.undo.undo(); break;
            case 7: proc.undo.redo(); break;
            default: break;
        }
    });
}

void DivinityEditor::setUiScale (float s)
{
    setSize ((int) (kBaseWidth * s), (int) (kBaseHeight * s));
}

void DivinityEditor::showSettingsMenu()
{
    juce::PopupMenu m, size;
    const float scales[] = { 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.15f, 1.3f };
    for (int i = 0; i < 7; ++i)
        size.addItem (10 + i, juce::String (juce::roundToInt (scales[i] * 100)) + "%", true, std::abs (proc.uiScale - scales[i]) < 0.02f);
    m.addSubMenu ("Interface size", size);
    m.addItem (2, "MIDI panic (all notes off)");
    m.addSeparator();
    m.addItem (3, "DIVINITY 0.1.0  -  Phase 1 foundation", false);
    m.addItem (4, "TRIPLE SEVEN AUDIO  -  triple7.world");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settings), [this, scales] (int r)
    {
        if (r >= 10 && r < 17) setUiScale (scales[r - 10]);
        else if (r == 2) { proc.panicRequested = true; proc.keyboardState.allNotesOff (0); }
        else if (r == 4) juce::URL ("https://www.triple7.world").launchInDefaultBrowser();
    });
}

} // namespace divinity
