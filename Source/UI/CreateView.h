#pragma once

#include "Panels.h"

namespace divinity::ui
{
//==============================================================================
/** Searchable preset browser with category / favourite filtering. */
class PresetBrowser : public Panel, private juce::ListBoxModel, private juce::ChangeListener
{
public:
    explicit PresetBrowser (DivinityProcessor& p);
    ~PresetBrowser() override;
    void resized() override;

private:
    int getNumRows() override { return filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();

    DivinityProcessor& proc;
    juce::TextEditor search;
    juce::ComboBox category;
    juce::ListBox list;
    juce::Array<int> filtered;
};

/** The CREATE workspace: instant inspiration with a handful of musical controls. */
class CreateView : public juce::Component, private juce::Timer
{
public:
    explicit CreateView (DivinityProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    DivinityProcessor& proc;
    PresetBrowser browser;
    ScopeDisplay scope;
    MacroSection macros;
    juce::OwnedArray<Knob> knobs;
    juce::String lastName;
};

} // namespace divinity::ui
