#pragma once
// 1.1: per-user settings shared by every DIVINITY instance (skin, window size, MIDI CC map).
#include <juce_data_structures/juce_data_structures.h>

namespace divinity::settings
{
    inline juce::PropertiesFile& file()
    {
        static juce::PropertiesFile f ([] {
            juce::PropertiesFile::Options o;
            o.applicationName = "DIVINITY"; o.folderName = "TRIPLE SEVEN AUDIO"; o.filenameSuffix = ".settings";
            o.osxLibrarySubFolder = "Application Support";
            return o; }());
        return f;
    }
    constexpr double kDefaultUiScale = 1.25;   // opens bigger than 1.0 by default
    inline float uiScale() { return (float) juce::jlimit (0.5, 2.0, file().getDoubleValue ("uiScale", kDefaultUiScale)); }
    inline void setUiScale (float s) { file().setValue ("uiScale", (double) s); file().saveIfNeeded(); }
    inline juce::String midiMap() { return file().getValue ("midiMap"); }
    inline void setMidiMap (const juce::String& m) { file().setValue ("midiMap", m); file().saveIfNeeded(); }
}
