#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace divinity
{
class DivinityProcessor;

/**
    Guardian Angel's built-in, offline brain.

    Everything is deterministic and local: a knowledge base of DIVINITY's
    features, the parameter help texts, recipe-based sound creation, rule-based
    patch ideas and a musically-safe randomiser. No network, no AI service.
    All patch changes go through the parameter system (undoable, host-visible).
    Must be used on the message thread.
*/
class GuardianBrain
{
public:
    struct Reply
    {
        juce::String text;
        juce::String actionLabel;            // optional primary button
        std::function<void()> action;
        juce::String secondaryLabel;         // optional secondary button
        std::function<void()> secondary;
    };

    explicit GuardianBrain (DivinityProcessor& p);

    Reply greet() const;
    Reply ask (const juce::String& question);
    Reply createSound (const juce::String& description);
    Reply suggestIdea();
    Reply randomize();
    Reply explain (const juce::String& tooltipOrHelp) const;
    juce::String tipForTab (const juce::String& tab) const;

private:
    // parameter helpers (each call is a complete, undoable gesture)
    float get (const juce::String& id) const;
    void set (const juce::String& id, float real);
    void setChoice (const juce::String& id, const juce::String& name);
    void clearModSlots();
    int  firstFreeModSlot() const;
    bool hasRouting (const juce::String& src, const juce::String& dst) const;
    void addRouting (const juce::String& src, const juce::String& dst, float amount);
    void beginEdit (const juce::String& what);

    bool tryTweak (const juce::String& q, Reply& out);
    bool searchKnowledge (const juce::String& q, Reply& out) const;
    bool searchParameters (const juce::String& q, Reply& out) const;

    DivinityProcessor& proc;
    juce::Random rng;
    int ideaCursor = 0;
};

} // namespace divinity
