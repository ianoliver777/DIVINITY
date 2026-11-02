#include "PluginEditor.h"
#include "DivinityAssets.h"

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
}

//==============================================================================
DivinityEditor::DivinityEditor (DivinityProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      menu ("Menu", burger()), prevPreset ("Previous preset", chevron (true)), nextPreset ("Next preset", chevron (false)),
      savePlus ("Save as new preset", plus()),
      createPage (p), effectsPage (p), playPage (p), soulPage (p), labPage (p), angel (p),
      settings ("Settings", icons::gear()), folder ("Import preset", folderIcon()), save ("Save preset", icons::save()),
      undo ("Undo (Cmd/Ctrl+Z)", undoIcon (false)), redo ("Redo (Cmd/Ctrl+Shift+Z)", undoIcon (true)),
      voices (p.apvts, ids::voices), glide (p.apvts, ids::glide),
      voiceMode (p.apvts, ids::voiceMode, { "POLY", "MONO", "LEGATO" }, { 0, 2, 1 })
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* name : { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB" })
    {
        auto* b = tabs.add (new juce::TextButton (name));
        b->getProperties().set ("fontSize", 19.0);
        b->getProperties().set ("corner", 5.0);
        const juce::String tab (name);
        b->onClick = [this, tab] { showTab (tab); angel.showTabTip (tab); };
        content.addAndMakeVisible (b);
    }

    presetName.getProperties().set ("transparent", true);
    presetName.getProperties().set ("fontSize", 15.5);
    presetName.setColour (juce::TextButton::textColourOffId, text);
    presetName.setTooltip ("Click to browse presets.");
    for (auto* c : std::initializer_list<juce::Component*> { &menu, &prevPreset, &nextPreset, &presetName, &savePlus })
        content.addAndMakeVisible (c);
    menu.onClick = [this] { showMainMenu(); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    presetName.onClick = [this] { showPresetMenu (presetName); };
    savePlus.onClick = [this] { savePresetDialog(); };

    for (auto* c : std::initializer_list<juce::Component*> { &createPage, &effectsPage, &playPage, &soulPage, &labPage, &angel })
        content.addChildComponent (c);
    angel.setVisible (true);
    angel.onExplainRequested = [this]
    {
        explainOverlay.setVisible (true);
        explainOverlay.toFront (true);
        explainOverlay.grabKeyboardFocus();
    };
    content.addChildComponent (explainOverlay);

    angelToggle.setClickingTogglesState (false);
    angelToggle.getProperties().set ("fontSize", 13.0);
    angelToggle.getProperties().set ("corner", 5.0);
    angelToggle.setTooltip ("Show or hide Guardian Angel. Hiding him makes the window narrower.");
    angelToggle.onClick = [this] { setAngelOpen (! proc.uiAngelOpen); };
    content.addAndMakeVisible (angelToggle);

    arpButton.setClickingTogglesState (true);
    arpButton.getProperties().set ("fontSize", 13.5);
    arpButton.getProperties().set ("corner", 5.0);
    arpButton.setTooltip (helpTextFor (arpids::on) + "\n\nMode, rate, octaves, gate and swing are on the PLAY tab.");
    arpAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, arpids::on, arpButton);
    content.addAndMakeVisible (arpButton);
    content.addAndMakeVisible (voiceMode);

    presetCount.setVisible (false);   // count now lives in the category button
    for (auto* b : { &presetCount, &category })
    {
        b->getProperties().set ("fontSize", 15.0);
        b->getProperties().set ("corner", 6.0);
        content.addChildComponent (b);
    }
    category.setVisible (true);
    presetCount.onClick = [this] { showPresetMenu (presetCount); };
    presetCount.setTooltip ("Browse presets in the selected category.");
    category.onClick = [this] { showCategoryMenu(); };
    category.setTooltip ("Filter presets by category. Previous / next follow this filter.");
    for (auto* c : std::initializer_list<juce::Component*> { &settings, &folder, &save, &undo, &redo, &voices, &glide })
        content.addAndMakeVisible (c);
    settings.onClick = [this] { showMainMenu(); };
    folder.onClick = [this] { importPreset(); };
    save.onClick = [this] { savePresetDialog(); };
    undo.onClick = [this] { proc.undo.undo(); };
    redo.onClick = [this] { proc.undo.redo(); };

    proc.presets.addChangeListener (this);
    updatePresetDisplay();
    showTab (proc.uiWorkspace);

    setResizable (true, true);
    angel.setVisible (proc.uiAngelOpen);
    angelToggle.setToggleState (proc.uiAngelOpen, juce::dontSendNotification);
    updateAngelToggleText();
    applySizeLimits();
    setSize (juce::roundToInt (getBaseWidth() * proc.uiScale), juce::roundToInt (kBaseHeight * proc.uiScale));
    setWantsKeyboardFocus (true);
    startTimerHz (8);
}

DivinityEditor::~DivinityEditor()
{
    proc.presets.removeChangeListener (this);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DivinityEditor::paint (juce::Graphics& g) { g.fillAll (background); }

void DivinityEditor::updateAngelToggleText()
{
    angelToggle.setButtonText (proc.uiAngelOpen ? juce::String (juce::CharPointer_UTF8 ("HIDE ANGEL  \xe2\x80\xba"))
                                                : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9  SHOW ANGEL")));
}

void DivinityEditor::applySizeLimits()
{
    const int W = getBaseWidth();
    getConstrainer()->setFixedAspectRatio ((double) W / kBaseHeight);
    setResizeLimits ((int) (W * 0.5f), (int) (kBaseHeight * 0.5f), (int) (W * 1.6f), (int) (kBaseHeight * 1.6f));
}

void DivinityEditor::setAngelOpen (bool open)
{
    if (open == proc.uiAngelOpen) return;
    const int h = getHeight();
    const float s = (float) h / (float) kBaseHeight;
    proc.uiAngelOpen = open;
    angel.setVisible (open);
    angelToggle.setToggleState (open, juce::dontSendNotification);
    updateAngelToggleText();
    getConstrainer()->setFixedAspectRatio (0.0);   // drop the old ratio first so it can't stretch the height
    setResizeLimits (1, 1, 10000, 10000);
    setSize (juce::roundToInt (getBaseWidth() * s), h);
    applySizeLimits();
    resized();
    content.repaint();
}

void DivinityEditor::resized()
{
    const int W = getBaseWidth();
    const bool compact = ! proc.uiAngelOpen;
    const float s = (float) getWidth() / (float) W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, W, kBaseHeight);
    proc.uiScale = s;

    // header: logo left, title centre, menu + status right
    menu.setBounds (W - 58, 26, 40, 40);

    // row 1: workspace tabs + Guardian Angel toggle
    const int tabW = compact ? 188 : 215, tabGap = 8, tabsTotal = 5 * tabW + 4 * tabGap;
    const int tabX = compact ? 24 + (W - 24 - 140 - tabsTotal) / 2 : (W - tabsTotal) / 2;
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (tabX + i * (tabW + tabGap), 116, tabW, 44);
    angelToggle.setBounds (W - 14 - 124, 116, 124, 44);

    // row 2: file actions | category | preset browser | arp + voice mode | voices / glide
    constexpr int rowY = 182, rowH = 42;
    int x = 24;
    for (auto* b : std::initializer_list<juce::Component*> { &settings, &folder, &save, &undo, &redo })
        { b->setBounds (x, rowY, 42, rowH); x += 47; }
    const int right = W - 24;
    glide.setBounds (right - 80, rowY + 2, 80, 38);
    voices.setBounds (right - 80 - 50 - 64, rowY + 2, 64, 38);         // labels painted to their left
    const int modeW = compact ? 192 : 228;
    const int modeX = right - 80 - 50 - 64 - 64 - 14 - modeW;
    voiceMode.setBounds (modeX, rowY + 2, modeW, 38);
    arpButton.setBounds (modeX - 10 - 62, rowY + 2, 62, 38);
    const int midL = 24 + 5 * 47 + 12, midR = modeX - 10 - 62 - 16;
    const int catW = compact ? 150 : 190;
    category.setBounds (midL, rowY, catW, rowH);
    const int pbX = midL + catW + 12, plusW = 42;
    const int pbW = midR - pbX - plusW - 6;
    prevPreset.setBounds (pbX + 4, rowY + 3, 36, 36);
    presetName.setBounds (pbX + 42, rowY + 3, pbW - 84, 36);
    nextPreset.setBounds (pbX + pbW - 40, rowY + 3, 36, 36);
    savePlus.setBounds (pbX + pbW + 6, rowY, plusW, rowH);
    presetBox = { pbX, rowY, pbW, rowH };

    const juce::Rectangle<int> page (14, 240, 1248, 682);
    for (auto* c : std::initializer_list<juce::Component*> { &createPage, &effectsPage, &playPage, &soulPage, &labPage })
        c->setBounds (page);
    angel.setBounds (1272, 240, 250, 682);
    explainOverlay.setBounds (0, 0, 1266, kBaseHeight);
}

//==============================================================================
void DivinityEditor::Content::paint (juce::Graphics& g)
{
    const int W = owner.getBaseWidth();
    const float cx = (float) W * 0.5f;
    g.fillAll (background);
    // celestial glow behind the title
    g.setGradientFill (juce::ColourGradient (accentStrong.withAlpha (0.22f), cx, 0.0f, juce::Colours::transparentBlack, cx, 260.0f, true));
    g.fillRect (cx - 468.0f, 0.0f, 936.0f, 260.0f);
    juce::Path arc;
    arc.addCentredArc (cx, 150.0f, 180.0f, 145.0f, 0.0f, -0.55f, 0.55f, true);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (arc, juce::PathStrokeType (5.0f));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (arc, juce::PathStrokeType (1.4f));
    g.setColour (juce::Colours::white);
    g.fillEllipse (cx - 4.0f, 1.0f, 8.0f, 8.0f);

    // brand block: TRIPLE SEVEN AUDIO logo
    if (logo.isValid())
        logo.draw (g, { 22.0f, 14.0f, 250.0f, 250.0f * (float) logo.getHeight() / (float) logo.getWidth() });

    // title
    const int tx = (int) cx - 330;
    g.setFont (font (52.0f, Weight::Medium, 1.05f));
    g.setColour (accent.withAlpha (0.18f));
    g.drawText ("DIVINITY", tx, 28, 660, 60, juce::Justification::centred);
    g.setColour (text);
    g.drawText ("DIVINITY", tx, 26, 660, 60, juce::Justification::centred);
    g.setFont (font (15.0f, Weight::Medium, 0.95f));
    g.setColour (textDim);
    g.drawText ("SYNTHESIZER", tx, 82, 660, 20, juce::Justification::centred);

    // status (top right)
    g.setColour (textFaint);
    g.setFont (font (11.5f, Weight::Medium, 0.05f));
    g.drawText (owner.statusText, W - 426, 78, 408, 14, juce::Justification::centredRight);

    // preset / file strip (under the tabs)
    drawPanelBackground (g, { 14.0f, 176.0f, (float) W - 28.0f, 54.0f });
    const auto pb = owner.presetBox.toFloat();
    g.setColour (panel2.withAlpha (0.8f));
    g.fillRoundedRectangle (pb, 6.0f);
    g.setColour (borderBright);
    g.drawRoundedRectangle (pb.reduced (0.5f), 6.0f, 1.0f);
    g.drawRoundedRectangle (owner.savePlus.getBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (border);
    g.drawVerticalLine (24 + 5 * 47 + 4, 186.0f, 220.0f);
    g.drawVerticalLine (owner.arpButton.getX() - 9, 186.0f, 220.0f);
    g.setColour (text);
    g.setFont (font (13.0f, Weight::Medium, 0.06f));
    g.drawText ("VOICES", owner.voices.getX() - 64, 184, 58, 38, juce::Justification::centredRight);
    g.drawText ("GLIDE", owner.glide.getX() - 50, 184, 44, 38, juce::Justification::centredRight);

    // separator with glow
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), 0, 234, accent.withAlpha (0.55f), cx, 234, true));
    g.fillRect (0, 234, W, 1);
}

DivinityEditor::Content::Content (DivinityEditor& e) : owner (e)
{
    logo.setSource (juce::ImageCache::getFromMemory (DivinityAssets::triple_seven_logo_png, DivinityAssets::triple_seven_logo_pngSize));
}

void DivinityEditor::ExplainOverlay::paint (juce::Graphics& g)
{
    g.fillAll (accentDeep.withAlpha (0.12f));
    auto banner = juce::Rectangle<float> (433.0f, 168.0f, 400.0f, 34.0f);
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
    const juce::StringArray names { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB" };
    const int idx = juce::jmax (0, names.indexOf (tab));
    proc.uiWorkspace = names[idx];
    juce::Component* pages[] = { &createPage, &effectsPage, &playPage, &soulPage, &labPage };
    for (int i = 0; i < 5; ++i)
    {
        pages[i]->setVisible (i == idx);
        tabs[i]->setToggleState (i == idx, juce::dontSendNotification);
    }
}

void DivinityEditor::timerCallback()
{
    statusText = "CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%   /   "
               + juce::String (proc.getActiveVoices()) + " VOICES PLAYING";
    content.repaint (getBaseWidth() - 430, 76, 420, 18);
    undo.setEnabled (proc.undo.canUndo());
    redo.setEnabled (proc.undo.canRedo());
    updatePresetDisplay();
}

namespace
{
    /** Style group of a category: the wave and neogrime libraries, or the classic set. */
    juce::String styleOf (const juce::String& category)
    {
        if (category.startsWithIgnoreCase ("Wave ")) return "WAVE";
        if (category.startsWithIgnoreCase ("Grime ")) return "NEOGRIME";
        return "CLASSIC";
    }
}

juce::Array<int> DivinityEditor::presetsInCategory() const
{
    juce::Array<int> list;
    const auto& all = proc.presets.getPresets();
    for (int i = 0; i < all.size(); ++i)
    {
        const auto& p = all.getReference (i);
        if (categoryFilter == "ALL" || (categoryFilter == "FAVOURITES" && proc.presets.isFavourite (p)) || p.category.toUpperCase() == categoryFilter
            || ((categoryFilter == "WAVE" || categoryFilter == "NEOGRIME") && styleOf (p.category) == categoryFilter))
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

    const auto list = presetsInCategory();
    int factory = 0;
    for (int i : list) if (pm.getPresets()[i].isFactory) ++factory;
    const auto countText = (categoryFilter == "ALL" && factory == list.size()) ? juce::String (list.size()) + " FACTORY PRESETS"
                                                                                : juce::String (list.size()) + " PRESETS";
    juce::ignoreUnused (factory);
    if (presetCount.getButtonText() != countText) presetCount.setButtonText (countText);
    const auto catText = (categoryFilter == "ALL" ? juce::String ("ALL") : categoryFilter) + "  (" + juce::String (list.size()) + ")" + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\xba"));
    if (category.getButtonText() != catText) category.setButtonText (catText);
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
    juce::PopupMenu m, favs;
    juce::StringArray cats;
    for (auto& p : all) cats.addIfNotAlreadyThere (p.category);
    for (int i = 0; i < all.size(); ++i)
        if (pm.isFavourite (all[i])) favs.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex());
    if (favs.getNumItems() > 0) { m.addSubMenu ("Favourites", favs); m.addSeparator(); }
    for (auto style : { "WAVE", "NEOGRIME", "CLASSIC" })
    {
        m.addSectionHeader (style);
        for (auto& c : cats)
        {
            if (styleOf (c) != style) continue;
            juce::PopupMenu sub;
            bool cur = false;
            for (int i = 0; i < all.size(); ++i)
                if (all[i].category == c) { sub.addItem (i + 1, all[i].name, true, i == pm.getCurrentIndex()); cur |= i == pm.getCurrentIndex(); }
            m.addSubMenu (c, sub, true, nullptr, cur);
        }
    }
    m.addSeparator();
    m.addItem (100000, "Init patch");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [this] (int r)
    {
        if (r == 100000) proc.presets.loadInit();
        else if (r > 0) proc.presets.loadPreset (r - 1);
    });
}

void DivinityEditor::showCategoryMenu()
{
    juce::StringArray cats { "ALL", "FAVOURITES", "WAVE", "NEOGRIME" };
    juce::StringArray real;
    for (auto& p : proc.presets.getPresets()) real.addIfNotAlreadyThere (p.category.toUpperCase());
    juce::PopupMenu m;
    m.addItem (1, "All presets", true, categoryFilter == "ALL");
    m.addItem (2, "Favourites", true, categoryFilter == "FAVOURITES");
    m.addItem (3, "All wave", true, categoryFilter == "WAVE");
    m.addItem (4, "All neogrime", true, categoryFilter == "NEOGRIME");
    for (auto style : { "WAVE", "NEOGRIME", "CLASSIC" })
    {
        m.addSectionHeader (style);
        for (auto& c : real)
            if (styleOf (c) == style) { cats.add (c); m.addItem (cats.size(), c, true, c == categoryFilter); }
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&category), [this, cats] (int r)
    {
        if (r <= 0) return;
        categoryFilter = cats[r - 1];
        updatePresetDisplay();
        const auto list = presetsInCategory();
        if (! list.isEmpty() && ! list.contains (proc.presets.getCurrentIndex())) proc.presets.loadPreset (list[0]);
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
    m.addItem (8, "DIVINITY 0.3.0", false);
    m.addItem (9, "TRIPLE SEVEN AUDIO  -  triple7.world");
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
            case 9: juce::URL ("https://www.triple7.world").launchInDefaultBrowser(); break;
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
