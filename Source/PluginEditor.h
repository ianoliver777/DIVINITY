#pragma once

#include "PluginProcessor.h"
#include "UI/Pages.h"
#include "UI/GuardianPanel.h"

namespace divinity
{
//==============================================================================
class DivinityEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::ChangeListener
{
public:
    static constexpr int kBaseWidth = 1536, kBaseHeight = 1024;

    explicit DivinityEditor (DivinityProcessor&);
    ~DivinityEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void showTab (const juce::String& tab);

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (DivinityEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        DivinityEditor& owner;
    };

    /** Explain mode: captures the next click and asks Guardian Angel to explain the control underneath. */
    class ExplainOverlay : public juce::Component
    {
    public:
        explicit ExplainOverlay (DivinityEditor& e) : owner (e) { setWantsKeyboardFocus (true); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        DivinityEditor& owner;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { updatePresetDisplay(); }
    void updatePresetDisplay();
    void stepPreset (int delta);
    void showPresetMenu (juce::Component& target);
    void showCategoryMenu();
    void showMainMenu();
    void savePresetDialog();
    void importPreset();
    void setUiScale (float s);
    juce::Array<int> presetsInCategory() const;

    DivinityProcessor& proc;
    ui::DivinityLookAndFeel lnf;
    Content content { *this };
    juce::TooltipWindow tooltips { &content, 600 };

    // header
    ui::IconButton menu, prevPreset, nextPreset, savePlus;
    juce::TextButton presetName;
    juce::OwnedArray<juce::TextButton> tabs;

    // pages + angel
    ui::CreatePage createPage;
    ui::EffectsPage effectsPage;
    ui::PlayPage playPage;
    ui::SoulView soulPage;
    ui::LabPage labPage;
    ui::GuardianPanel angel;
    ExplainOverlay explainOverlay { *this };

    // bottom bar
    ui::IconButton settings, folder, save, undo, redo;
    juce::TextButton presetCount, category;
    ui::NumberBox voices, glide;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String categoryFilter { "ALL" };
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DivinityEditor)
};

} // namespace divinity
