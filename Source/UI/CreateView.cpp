#include "CreateView.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

//==============================================================================
PresetBrowser::PresetBrowser (DivinityProcessor& p) : Panel ("PRESETS"), proc (p)
{
    search.setTextToShowWhenEmpty ("Search name, category or tag...", textFaint);
    search.setFont (font (13.0f));
    search.setIndents (10, 7);
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    category.setTooltip ("Filter by category or favourites.");
    category.onChange = [this] { refresh(); };
    addAndMakeVisible (category);

    list.setModel (this);
    list.setRowHeight (30);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    proc.presets.addChangeListener (this);
    refresh();
}

PresetBrowser::~PresetBrowser() { proc.presets.removeChangeListener (this); }

void PresetBrowser::refresh()
{
    const auto& all = proc.presets.getPresets();

    // rebuild category list, keeping the selection
    const auto selectedCat = category.getText();
    juce::StringArray cats { "All", "Favourites" };
    for (auto& pr : all) cats.addIfNotAlreadyThere (pr.category);
    category.clear (juce::dontSendNotification);
    category.addItemList (cats, 1);
    const int idx = cats.indexOf (selectedCat);
    category.setSelectedItemIndex (idx >= 0 ? idx : 0, juce::dontSendNotification);

    const auto q = search.getText().trim();
    const auto cat = category.getText();
    filtered.clearQuick();
    for (int i = 0; i < all.size(); ++i)
    {
        const auto& pr = all.getReference (i);
        if (cat == "Favourites" && ! proc.presets.isFavourite (pr)) continue;
        if (cat != "All" && cat != "Favourites" && pr.category != cat) continue;
        if (q.isNotEmpty() && ! (pr.name.containsIgnoreCase (q) || pr.category.containsIgnoreCase (q) || pr.tags.containsIgnoreCase (q)))
            continue;
        filtered.add (i);
    }
    list.updateContent();
    list.repaint();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, filtered.size())) return;
    const int index = filtered[row];
    const auto& pr = proc.presets.getPresets().getReference (index);
    const bool current = index == proc.presets.getCurrentIndex();
    if (current)
    {
        g.setColour (accentDeep.withAlpha (0.55f));
        g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f);
    }
    g.setFont (font (13.5f, current ? Weight::Medium : Weight::Regular));
    g.setColour (current ? text : text.withAlpha (0.85f));
    g.drawText (pr.name, 12, 0, w - 130, h, juce::Justification::centredLeft, true);
    g.setFont (font (11.0f));
    g.setColour (textDim);
    g.drawText (pr.category, w - 120, 0, 92, h, juce::Justification::centredRight, true);
    if (proc.presets.isFavourite (pr))
    {
        auto heart = icons::heart();
        heart.applyTransform (heart.getTransformToScaleToFit (juce::Rectangle<float> ((float) w - 22.0f, (float) h * 0.5f - 6.0f, 12.0f, 12.0f), true));
        g.setColour (accent);
        g.fillPath (heart);
    }
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, filtered.size())) return;
    const int index = filtered[row];
    if (e.mods.isPopupMenu())
    {
        const auto pr = proc.presets.getPresets()[index];
        juce::PopupMenu m;
        m.addItem (1, proc.presets.isFavourite (pr) ? "Remove from favourites" : "Add to favourites");
        if (pr.description.isNotEmpty()) { m.addSeparator(); m.addItem (2, pr.description, false); }
        m.showMenuAsync ({}, [this, pr] (int r) { if (r == 1) proc.presets.toggleFavourite (pr); });
        return;
    }
    proc.presets.loadPreset (index);
}

void PresetBrowser::resized()
{
    search.setBounds (14, 44, getWidth() - 28, 32);
    category.setBounds (14, 84, getWidth() - 28, 28);
    list.setBounds (8, 122, getWidth() - 16, getHeight() - 130);
}

//==============================================================================
CreateView::CreateView (DivinityProcessor& p) : proc (p), browser (p), scope (p), soulPad (p, true), macros (p, true)
{
    addAndMakeVisible (browser);
    addAndMakeVisible (scope);
    addAndMakeVisible (soulPad);
    divineSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    divineSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    divineSlider.setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    divineSlider.setDoubleClickReturnValue (true, 0.0);
    divineSlider.setTooltip (helpTextFor (ids::divineAmount) + "\n\nChoose the transformation on the SOUL page.");
    divineSlider.onValueChange = [this] { repaint (775, 330, 220, 20); };
    divineAttach = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::divineAmount, divineSlider);
    addAndMakeVisible (divineSlider);
    addAndMakeVisible (macros);

    auto& s = p.apvts;
    knobs.add (new Knob (s, ids::env (0, "attack"), "Attack"));
    knobs.add (new Knob (s, ids::env (0, "decay"), "Decay"));
    knobs.add (new Knob (s, ids::env (0, "sustain"), "Sustain"));
    knobs.add (new Knob (s, ids::env (0, "release"), "Release"));
    knobs.add (new Knob (s, ids::filt (0, "cutoff"), "Brightness", ModDest::Filter1Cutoff, 0.6f));
    knobs.add (new Knob (s, ids::filt (0, "res"), "Resonance", ModDest::Filter1Res));
    knobs.add (new Knob (s, ids::satDrive, "Intensity"));
    knobs.add (new Knob (s, ids::revMix, "Depth"));
    for (auto* k : knobs) { k->setCaptionSize (12.0f); addAndMakeVisible (k); }

    // Intensity and Depth drive the saturation and reverb modules: touching them switches those modules on.
    auto enableOnTouch = [&s] (Knob* k, const juce::String& onID)
    {
        k->getSlider().onDragStart = [&s, onID]
        {
            auto* p = s.getParameter (onID);
            if (p->getValue() < 0.5f) { p->beginChangeGesture(); p->setValueNotifyingHost (1.0f); p->endChangeGesture(); }
        };
    };
    enableOnTouch (knobs[6], ids::satOn);
    enableOnTouch (knobs[7], ids::revOn);
    startTimerHz (8);
}

void CreateView::timerCallback()
{
    const auto name = proc.presets.getCurrentName() + (proc.presets.isModified() ? "*" : "");
    if (name != lastName) { lastName = name; repaint(); }
}

void CreateView::paint (juce::Graphics& g)
{
    // sound panel
    auto sound = juce::Rectangle<int> (365, 0, 645, 390);
    drawPanelBackground (g, sound.toFloat());
    const auto& all = proc.presets.getPresets();
    const int idx = proc.presets.getCurrentIndex();
    const PresetInfo info = juce::isPositiveAndBelow (idx, all.size()) ? all[idx] : PresetInfo {};
    g.setColour (text);
    g.setFont (font (30.0f, Weight::Medium, 0.02f));
    g.drawText (lastName, sound.getX() + 24, 18, sound.getWidth() - 48, 40, juce::Justification::centredLeft);
    g.setFont (font (11.5f, Weight::Medium, 0.16f));
    g.setColour (accent);
    g.drawText ((info.category.isNotEmpty() ? info.category : juce::String ("Init")).toUpperCase()
                    + (info.tags.isNotEmpty() ? "   /   " + info.tags.toUpperCase() : juce::String()),
                sound.getX() + 24, 58, sound.getWidth() - 48, 18, juce::Justification::centredLeft);
    g.setFont (font (13.0f));
    g.setColour (textDim);
    g.drawFittedText (info.description.isNotEmpty() ? info.description : juce::String ("Play the keyboard below, then shape the sound with the macros."),
                      sound.getX() + 24, 80, sound.getWidth() - 48, 36, juce::Justification::topLeft, 2);

    // SOUL quick controls
    g.setFont (font (10.5f, Weight::Medium, 0.25f));
    g.setColour (textDim);
    g.drawText ("SOUL", 778, 118, 100, 14, juce::Justification::centredLeft);
    const int mode = juce::roundToInt (proc.apvts.getRawParameterValue (ids::divineMode)->load());
    const int amt = juce::roundToInt (proc.apvts.getRawParameterValue (ids::divineAmount)->load() * 100.0f);
    g.drawText ("DIVINE  /  " + divineModeNames()[mode].toUpperCase(), 778, 332, 170, 14, juce::Justification::centredLeft);
    g.setColour (amt > 0 ? accent : textFaint);
    g.drawText (juce::String (amt) + "%", 940, 332, 52, 14, juce::Justification::centredRight);

    // performance panel
    auto perf = juce::Rectangle<int> (365, 405, 1125, 305);
    drawPanelBackground (g, perf.toFloat());
    drawSectionTitle (g, { perf.getX() + 16, perf.getY() + 8, 400, 26 }, "SHAPE  &  CHARACTER");
    g.setFont (font (11.0f, Weight::Medium, 0.14f));
    g.setColour (textDim);
    const int colW = perf.getWidth() / 8;
    g.drawText ("VOLUME ENVELOPE", perf.getX() + 10, perf.getY() + 56, colW * 4 - 20, 18, juce::Justification::centred);
    g.drawText ("FILTER", perf.getX() + colW * 4 + 10, perf.getY() + 56, colW * 2 - 20, 18, juce::Justification::centred);
    g.drawText ("CHARACTER", perf.getX() + colW * 6 + 10, perf.getY() + 56, colW * 2 - 20, 18, juce::Justification::centred);
    g.setColour (border);
    for (int c : { 4, 6 })
        g.drawVerticalLine (perf.getX() + colW * c, (float) perf.getY() + 56, (float) perf.getBottom() - 30);
}

void CreateView::resized()
{
    browser.setBounds (0, 0, 350, 710);
    scope.setBounds (385, 128, 372, 248);
    soulPad.setBounds (774, 134, 222, 190);
    divineSlider.setBounds (774, 350, 222, 20);
    macros.setBounds (1025, 0, 465, 390);
    const auto perf = juce::Rectangle<int> (365, 405, 1125, 305);
    const int colW = perf.getWidth() / 8;
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (juce::Rectangle<int> (perf.getX() + i * colW, perf.getY() + 90, colW, 170).reduced (14, 10));
}

} // namespace divinity::ui
