#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "DSP/SynthEngine.h"
#include "DSP/FXChain.h"
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
class DivinityProcessor : public juce::AudioProcessor
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
    ScopeFifo scope;

    std::atomic<float> meterL { 0 }, meterR { 0 };
    std::atomic<float> cpuLoad { 0 };
    std::atomic<bool> panicRequested { false };

    const WavetableBank& getWavetables() const noexcept { return *bank; }
    int getActiveVoices() const noexcept { return engine.getActiveVoiceCount(); }
    float getLfoValue (int i) const noexcept { return engine.getLfoValue (i); }
    const FXChain::Display& getFxDisplay() const noexcept { return fx.display; }

    // UI preference (not part of the sound): workspace & scale are stored with the session.
    juce::String uiWorkspace { "DESIGN" };
    float uiScale = 1.0f;

private:
    juce::SharedResourcePointer<WavetableBank> bank;
    SynthEngine engine;
    FXChain fx;
    juce::AudioProcessLoadMeasurer loadMeasurer;
    juce::MidiBuffer uiMidiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DivinityProcessor)
};

} // namespace divinity
