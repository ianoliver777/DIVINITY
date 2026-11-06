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
#include "Midi/MidiPreview.h"

namespace divinity
{
//==============================================================================
/** Lock-free broadcast ring of stereo samples for the UI meters (scope, spectrum, stereo imager).
    One writer (the audio thread); any number of readers, each with its own cursor, so a visible
    display never starves another one. Readers that fall behind skip ahead to recent audio. */
class ScopeFifo
{
public:
    static constexpr int kSize = 16384;   // power of two
    void push (const float* l, const float* r, int n) noexcept
    {
        auto w = written.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((w + i) & (kSize - 1));
            left[idx] = l[i];
            right[idx] = r[i];
        }
        written.store (w + n, std::memory_order_release);
    }
    /** Copies the samples written since `cursor` (at most maxN) and advances the cursor.
        mono gets (L+R)/2; left / right (optional) get the channels. Returns the count. */
    int pull (juce::int64& cursor, float* mono, int maxN, float* l = nullptr, float* r = nullptr) const noexcept
    {
        const auto total = written.load (std::memory_order_acquire);
        if (cursor > total || total - cursor > kSize / 2) cursor = total - std::min<juce::int64> ((juce::int64) maxN, (juce::int64) (kSize / 2));
        if (cursor < 0) cursor = 0;
        const int n = (int) std::min<juce::int64> ((juce::int64) maxN, total - cursor);
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((cursor + i) & (kSize - 1));
            if (mono != nullptr) mono[i] = 0.5f * (left[idx] + right[idx]);
            if (l != nullptr) l[i] = left[idx];
            if (r != nullptr) r[i] = right[idx];
        }
        cursor += n;
        return n;
    }
private:
    std::atomic<juce::int64> written { 0 };
    std::array<float, kSize> left {}, right {};
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

    // MIDI generator (PLAY tab): audition player + the generator's state (message thread)
    MidiPreview midiPreview;
    std::atomic<double> hostBpm { 120.0 };
    struct MidiGenState
    {
        midigen::Settings settings;
        midigen::Progression progression;
        midigen::Clip clip;
        bool hasClip = false;
    } midiGen;

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
