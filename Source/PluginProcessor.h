#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "DSP/SynthEngine.h"
#include "DSP/FXChain.h"
#include "DSP/Arpeggiator.h"
#include "DSP/StepSequencer.h"
#include "DSP/Sampler.h"
#include "Presets/PresetManager.h"

namespace divinity
{
//==============================================================================
/** Lock-free single-producer/single-consumer ring of mono samples for the UI scope. */
class ScopeFifo
{
public:
    static constexpr int kSize = 8192;
    void push (const float* l, const float* r, int n) noexcept
    {
        int s1, z1, s2, z2;
        fifo.prepareToWrite (n, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) data[(size_t) (s1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < z2; ++i) data[(size_t) (s2 + i)] = 0.5f * (l[z1 + i] + r[z1 + i]);
        fifo.finishedWrite (z1 + z2);
    }
    int pull (float* dest, int maxN) noexcept
    {
        int s1, z1, s2, z2;
        fifo.prepareToRead (juce::jmin (maxN, fifo.getNumReady()), s1, z1, s2, z2);
        std::copy (data.begin() + s1, data.begin() + s1 + z1, dest);
        std::copy (data.begin() + s2, data.begin() + s2 + z2, dest + z1);
        fifo.finishedRead (z1 + z2);
        return z1 + z2;
    }
private:
    juce::AbstractFifo fifo { kSize };
    std::array<float, kSize> data {};
};

//==============================================================================
class DivinityProcessor : public juce::AudioProcessor,
                          private juce::ValueTree::Listener,
                          private juce::Timer
{
public:
    DivinityProcessor();
    ~DivinityProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DIVINITY"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presets.getCurrentName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //--------------------------------------------------------------------------
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard feeds MIDI in here
    juce::MidiMessageCollector uiMidi;       // pitch/mod wheels from the UI
    PresetManager presets;
    ScopeFifo scope;      // post-FX (what you hear)
    ScopeFifo scopePre;   // pre-FX (the raw voices) for the spectrum's PRE view

    std::atomic<float> meterL { 0 }, meterR { 0 };
    std::atomic<float> cpuLoad { 0 };
    std::atomic<bool> panicRequested { false };

    const WavetableBank& getWavetables() const noexcept { return *bank; }
    int getActiveVoices() const noexcept { return engine.getActiveVoiceCount(); }
    float getLfoValue (int i) const noexcept { return engine.getLfoValue (i); }
    float getSoulX() const noexcept { return engine.getSoul().getEffectiveX(); }
    float getSoulY() const noexcept { return engine.getSoul().getEffectiveY(); }
    float getDivineAmount() const noexcept { return engine.getSoul().getEffectiveDivine(); }
    const FXChain::Display& getFxDisplay() const noexcept { return fx.display; }
    float getRandomValue() const noexcept { return engine.getRandomValue(); }
    float getStepValue() const noexcept { return engine.getStepValue(); }
    int getStepIndex() const noexcept { return stepSeq.isTriggering() ? stepSeq.displayStep.load() : engine.getStepIndex(); }
    int getMaxVoices() const noexcept { return engine.getSnapshot().maxVoices; }

    //--------------------------------------------------------------------------
    // Sampler: factory content + the user's own sample (path saved with the session / preset)
    const FactorySamples& getFactorySamples() const noexcept { return *factorySamples; }
    UserSampleSlot& getUserSample() noexcept { return userSample; }
    /** Message thread: loads a WAV/AIFF/FLAC/OGG as the user sample and selects it. Returns an error or empty. */
    juce::String loadUserSample (const juce::File& file, bool selectIt = true);
    static inline const juce::Identifier userSampleProperty { "userSample" };

    /** FX rack order (module keys). Setting it is undoable and saved with sessions and presets. */
    juce::StringArray getFxOrder() const;
    void setFxOrder (const juce::StringArray& keys, bool undoable = true);

    // UI preference (not part of the sound): workspace & scale are stored with the session.
    juce::String uiWorkspace { "CREATE" };   // CREATE / EFFECTS / PLAY / SOUL / LAB
    float uiScale = 1.0f;
    bool uiAngelOpen = false;   // 0.7: Guardian Angel starts tucked away (SHOW ANGEL)
    bool uiAngelTips = true;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void timerCallback() override { userSample.collectGarbage(); }
    juce::SharedResourcePointer<WavetableBank> bank;
    juce::SharedResourcePointer<FactorySamples> factorySamples;
    UserSampleSlot userSample;
    SynthEngine engine;
    FXChain fx;
public:
    Arpeggiator arp;   // public: the UI reads its display atomics
    StepSequencer stepSeq;
private:
    bool wasStepTriggering = false;
public:
private:
    juce::AudioProcessLoadMeasurer loadMeasurer;
    juce::MidiBuffer uiMidiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DivinityProcessor)
};

} // namespace divinity
