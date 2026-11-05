#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>

namespace divinity
{
//==============================================================================
/** One playable sample: mono or stereo float data at its own sample rate. */
struct SampleData
{
    std::vector<float> left, right;   // right empty = mono
    double sampleRate = 48000.0;
    int rootNote = 60;                 // MIDI note that plays at the original pitch
    juce::String name;
    int length() const noexcept { return (int) left.size(); }
};

//==============================================================================
/**
    Built-in sample content, synthesised once at startup and shared by every
    instance (juce::SharedResourcePointer). Drums, textures, vocal and tonal
    one-shots - all generated in code, so there are no files to install.
*/
class FactorySamples
{
public:
    FactorySamples();
    int size() const noexcept { return (int) samples.size(); }
    const SampleData* get (int i) const noexcept { return juce::isPositiveAndBelow (i, size()) ? samples[(size_t) i].get() : nullptr; }
private:
    std::vector<std::unique_ptr<SampleData>> samples;
};

//==============================================================================
/**
    The user's own sample. Loaded on the message thread, handed to the audio
    thread with an atomic pointer swap; replaced samples are kept alive until
    the message thread frees them later, so the audio thread never deletes or
    waits.
*/
class UserSampleSlot
{
public:
    ~UserSampleSlot();

    /** Message thread. Returns an empty string on success, else an error message. */
    juce::String load (const juce::File& file);
    void clear();
    /** Message thread: frees samples the audio thread can no longer be using. */
    void collectGarbage();

    /** Audio thread. */
    const SampleData* get() const noexcept { return current.load (std::memory_order_acquire); }

    juce::File getFile() const { return file; }
    juce::String getStatus() const { return status; }

private:
    std::atomic<SampleData*> current { nullptr };
    std::vector<std::pair<std::unique_ptr<SampleData>, juce::uint32>> retired;   // sample + time it was retired
    std::unique_ptr<SampleData> owned;
    juce::File file;
    juce::String status;
    juce::AudioFormatManager formats;
    bool formatsReady = false;
};

} // namespace divinity
