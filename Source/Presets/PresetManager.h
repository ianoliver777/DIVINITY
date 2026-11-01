#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Parameters.h"

namespace divinity
{
//==============================================================================
struct PresetInfo
{
    juce::String name, category, author, tags, description;
    bool isFactory = true;
    int  factoryIndex = -1;
    juce::File file;
};

/** A factory preset is a list of (paramID, value) overrides on top of defaults. */
struct FactoryPreset
{
    juce::String name, category, tags, description;
    std::vector<std::pair<juce::String, juce::var>> values;   // var: number, or string for a choice name

    FactoryPreset& set (const juce::String& id, double v)            { values.emplace_back (id, v); return *this; }
    FactoryPreset& choice (const juce::String& id, const char* name) { values.emplace_back (id, juce::String (name)); return *this; }
};

std::vector<FactoryPreset> createFactoryPresets();

//==============================================================================
/**
    Owns preset discovery, loading, saving and favourites. All methods must be
    called on the message thread. Preset files are versioned XML that store
    real-unit parameter values (not normalised), so they survive range changes.
*/
class PresetManager : public juce::ChangeBroadcaster,
                      private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr const char* kExtension = ".divpreset";

    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() override;

    void rescan();
    const juce::Array<PresetInfo>& getPresets() const noexcept { return presets; }

    bool loadPreset (int index);
    void loadNext()     { if (! presets.isEmpty()) loadPreset ((juce::jmax (0, currentIndex) + 1) % presets.size()); }
    void loadPrevious() { if (! presets.isEmpty()) loadPreset ((juce::jmax (0, currentIndex) - 1 + presets.size()) % presets.size()); }
    void loadInit();

    /** Saves the current sound as a user preset. Returns an empty string or an error message. */
    juce::String saveUserPreset (const juce::String& name, const juce::String& category);
    juce::String importPreset (const juce::File& file);
    juce::String exportCurrent (const juce::File& file) const;

    int  getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentName() const   { return currentName; }
    bool isModified() const noexcept      { return modified.load(); }

    bool isFavourite (const PresetInfo& p) const;
    void toggleFavourite (const PresetInfo& p);

    /** Called when restoring host state: remembers which preset name was active. */
    void setCurrentFromState (const juce::String& name, bool wasModified);

    static juce::File getUserPresetFolder();

    // ---- serialisation (also used by tests) ----
    std::unique_ptr<juce::XmlElement> createPresetXml (const PresetInfo& info) const;
    /** Applies a preset XML. Parameters missing from the file get their defaults. */
    bool applyPresetXml (const juce::XmlElement& xml);
    void applyFactoryPreset (const FactoryPreset& fp);

private:
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}
    void setAllToDefault();
    void setParamReal (const juce::String& id, float realValue);
    juce::String favouriteKey (const PresetInfo& p) const;
    void loadSettings();
    void saveSettings();

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<FactoryPreset> factory;
    juce::Array<PresetInfo> presets;
    int currentIndex = -1;
    juce::String currentName { "Init" };
    std::atomic<bool> modified { false };
    std::atomic<bool> loading { false };
    juce::StringArray favourites;
};

} // namespace divinity
