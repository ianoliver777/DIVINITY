#pragma once

#include "Panels.h"
#include "../Midi/MidiGenerator.h"

namespace divinity::ui
{
//==============================================================================
/**
    MIDI GENERATOR (PLAY tab): chord progressions, leads, basslines and drum loops.
    Pick a part, key, scale, style and length; audition it through DIVINITY; then drag
    the DRAG MIDI tile straight onto a track in the DAW (or save it as a .mid file).
    Chords, lead and bass share one progression, so the parts fit together.
*/
class MidiGenPanel : public Panel, private juce::Timer
{
public:
    explicit MidiGenPanel (DivinityProcessor& p);
    ~MidiGenPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Writes the current clip to a temporary .mid file (also used by the drag tile). */
    juce::File writeTempFile();

private:
    class PianoRoll;
    class DragTile;
    void timerCallback() override;
    void regenerate (bool newProgression);
    void syncButtons();
    void startPreview();
    void saveAs();

    DivinityProcessor& proc;
    juce::OwnedArray<juce::TextButton> partButtons, barButtons;
    juce::ComboBox key, scale, style;
    juce::TextButton newProg { "NEW PROGRESSION" }, vary { "NEW VARIATION" }, play { "PLAY" }, save { "SAVE .MID" }, sevenths { "7THS" };
    std::unique_ptr<PianoRoll> roll;
    std::unique_ptr<DragTile> tile;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace divinity::ui
