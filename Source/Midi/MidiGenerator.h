#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace divinity::midigen
{
//==============================================================================
/** What the generator makes. */
enum class Part { Chords, Lead, Bass, Drums };

/** One generated note. Times in beats (quarter notes) from the start of the clip. */
struct Note
{
    double start = 0.0, length = 0.25;
    int pitch = 60;
    float velocity = 0.8f;
};

/** A generated clip: notes + its length + a human-readable summary. */
struct Clip
{
    Part part = Part::Chords;
    std::vector<Note> notes;
    double lengthBeats = 16.0;
    juce::String summary;   // e.g. "Am - F - C - G"
    juce::String fileStem;  // e.g. "DIVINITY Trap Chords A minor"
};

/** The chord progression the parts share, so chords, lead and bass fit together. */
struct Progression
{
    std::vector<int> degrees { 0, 5, 2, 6 };   // scale degrees (0 = tonic), one per chord
    bool sevenths = false;
};

struct Settings
{
    Part part = Part::Chords;
    int key = 9;          // 0 = C ... 11 = B
    int scale = 0;        // index into scaleNames()
    int style = 0;        // index into styleNames()
    int bars = 4;         // 2, 4 or 8
    juce::uint32 seed = 1;
};

const juce::StringArray& partNames();    // CHORDS LEAD BASS DRUMS
const juce::StringArray& keyNames();     // C C# D ...
const juce::StringArray& scaleNames();   // Minor, Major, Harmonic Minor, Dorian, Phrygian, Minor Pentatonic
const juce::StringArray& styleNames();   // Trap, Wave, Hip-Hop, Drill, Lo-Fi, Emotional

/** A fresh progression for a style (seeded). */
Progression makeProgression (int style, juce::uint32 seed);

/** Generates a part over the given progression (drums ignore it). Deterministic for a given seed. */
Clip generate (const Settings&, const Progression&);

/** Writes the clip as a standard MIDI file (type 1, 960 PPQ, one named track). */
bool writeMidiFile (const Clip&, double bpm, const juce::File& dest);

/** The note names of a progression, e.g. "Am - F - C - G". */
juce::String describeProgression (const Settings&, const Progression&);

/** GM drum notes used by the DRUMS part. */
namespace drums
{
    constexpr int kick = 36, rim = 37, snare = 38, clap = 39, hatClosed = 42, hatPedal = 44, hatOpen = 46, perc = 75;
}

} // namespace divinity::midigen
