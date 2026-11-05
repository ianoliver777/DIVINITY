#pragma once

#include "PluginProcessor.h"
#include "UI/Pages.h"
#include "UI/GuardianPanel.h"

namespace divinity
{
//==============================================================================
/** Glossy keys with cyan glow on pressed notes (v0.7 look). */
class GlassKeyboard : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override;
};

//==============================================================================
/** Bottom bar on every tab: wheels, bend range, keyboard, PORTA / VEL / KEYTRK / PAN / VOL, scope. */
class KeyboardStrip : public juce::Component
{
public:
    explicit KeyboardStrip (DivinityProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    DivinityProcessor& proc;
    GlassKeyboard keyboard;
    juce::Slider pitch, mod;
    ui::NumberBox bend;
    ui::IconButton bendMenu;
    juce::OwnedArray<ui::Knob> knobs;
    ui::ScopeDisplay scope;
};

//==============================================================================
class DivinityEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::ChangeListener
{
public:
    static constexpr int kBaseWidth = 1448, kAngelWidth = 262, kBaseHeight = 1000;
    /** Width of the unscaled layout: wider while Guardian Angel is shown. */
    int getBaseWidth() const noexcept { return proc.uiAngelOpen ? kBaseWidth + kAngelWidth : kBaseWidth; }
    void setAngelOpen (bool open);

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
        explicit Content (DivinityEditor& e);
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        DivinityEditor& owner;
        DivinityProcessor& proc() const { return owner.proc; }
        ui::CrispImage band;
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
    void showCategoryMenu (juce::Component& target);
    void showMainMenu();
    void savePresetDialog (bool asCopy = false);
    void importPreset();
    void startExplain();
    void setUiScale (float s);
    void applySizeLimits();
    void updateAngelToggleText();
    juce::Array<int> presetsInCategory() const;

    DivinityProcessor& proc;
    ui::DivinityLookAndFeel lnf;
    Content content { *this };
    juce::TooltipWindow tooltips { &content, 600 };

    // header
    ui::IconButton settings, help, menu;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton angelToggle { "ANGEL" };

    // preset strip
    ui::IconButton folder, save, undo, redo, copy, favs, prevPreset, nextPreset, star, savePlus;
    juce::TextButton presetName;
    juce::TextButton arpButton { "ARP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpAttach;
    ui::SegmentedChoice voiceMode;
    ui::NumberBox voices, glideBox;
    ui::FineSlider glideKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttach;

    // pages + angel + keyboard
    ui::CreatePage createPage;
    ui::EffectsPage effectsPage;
    ui::PlayPage playPage;
    ui::SoulView soulPage;
    ui::LabPage labPage;
    ui::BrowserPage browserPage;
    ui::LegacyPageHost effectsHost { effectsPage }, playHost { playPage }, soulHost { soulPage }, labHost { labPage };
    ui::GuardianPanel angel;
    KeyboardStrip keys;
    ExplainOverlay explainOverlay { *this };

    juce::Rectangle<int> presetField;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String categoryFilter { "ALL" };
    juce::String cpuText, ramText, voicesText;
    float voiceFill = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DivinityEditor)
};

} // namespace divinity
