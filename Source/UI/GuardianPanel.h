#pragma once

#include "Components.h"
#include "../Assistant/GuardianBrain.h"

namespace divinity::ui
{
//==============================================================================
/** The Guardian Angel character: halo, barbed-wire wreath, X eyes, plush body,
    wings. Pure vector, gently animated (float, halo pulse, glow when speaking). */
class AngelArt : public juce::Component, private juce::Timer
{
public:
    AngelArt();
    void paint (juce::Graphics&) override;
    void speak() { talk = 1.0f; }
    void setAwake (bool a) { awake = a; repaint(); }
private:
    void timerCallback() override;
    float phase = 0.0f, talk = 0.0f;
    bool awake = true;
};

//==============================================================================
/** Right-hand Guardian Angel panel: character, speech bubble, actions, ask box. */
class GuardianPanel : public juce::Component
{
public:
    GuardianPanel (DivinityProcessor& p);
    ~GuardianPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void showReply (const GuardianBrain::Reply&);
    void showTabTip (const juce::String& tab);
    void explainText (const juce::String& tooltip);   // from the editor's explain mode
    std::function<void()> onExplainRequested;
    GuardianBrain& getBrain() { return brain; }

private:
    class ActionButton;
    void submitAsk();
    void hideBubble();

    DivinityProcessor& proc;
    GuardianBrain brain;
    AngelArt art;

    struct Bubble : public juce::Component
    {
        void paint (juce::Graphics&) override;
        juce::TextEditor text;
        juce::TextButton primary, secondary, close { "x" };
    } bubble;
    GuardianBrain::Reply current;

    std::unique_ptr<ActionButton> toggle, create, ideas, explain, randomize;
    juce::TextEditor askBox;
    IconButton send;
    bool awaitingDescription = false;
};

} // namespace divinity::ui
