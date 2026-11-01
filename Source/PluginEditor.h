#pragma once

#include "PluginProcessor.h"
#include "UI/Panels.h"
#include "UI/CreateView.h"

namespace divinity
{
//==============================================================================
/** Left navigation: PLAY / SYNTH / MOTION / FX / SOUL / LAB. */
class SideNav : public juce::Component, public juce::SettableTooltipClient
{
public:
    SideNav();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void setSelected (int i) { selected = i; repaint(); }
    std::function<void (int)> onSelect;
    static constexpr int kItemHeight = 65, kTop = 34;
private:
    int itemAt (int y) const { const int i = (y - kTop) / kItemHeight; return juce::isPositiveAndBelow (i, 6) ? i : -1; }
    int selected = 1, hover = -1;
};

//==============================================================================
class DivinityEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::ChangeListener
{
public:
    static constexpr int kBaseWidth = 1672, kBaseHeight = 940;

    explicit DivinityEditor (DivinityProcessor&);
    ~DivinityEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (DivinityEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        DivinityEditor& owner;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { updatePresetDisplay(); }

    void setWorkspace (const juce::String& ws);
    void navSelected (int index);
    void updatePresetDisplay();
    void showPresetMenu();
    void showFileMenu();
    void showSettingsMenu();
    void savePresetDialog();
    void setUiScale (float s);

    DivinityProcessor& proc;
    ui::DivinityLookAndFeel lnf;
    Content content { *this };
    juce::TooltipWindow tooltips { &content, 600 };

    // top bar
    juce::OwnedArray<juce::TextButton> workspaceButtons;
    ui::IconButton prevPreset, nextPreset, heart, save, menu, panic, settings;
    juce::TextButton presetName;
    ui::LevelMeter topMeter;

    SideNav nav;
    ui::OscillatorSection oscSection;
    ui::FilterSection filterSection;
    ui::EnvelopeSection envSection;
    ui::LfoSection lfoSection;
    ui::ModMatrixSection matrixSection;
    ui::MacroSection macroSection;
    ui::FxSection fxSection;
    ui::GlobalSection globalSection;
    ui::CreateView createView;
    ui::KeyboardBar keyboardBar;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String cpuText, voiceText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DivinityEditor)
};

} // namespace divinity
