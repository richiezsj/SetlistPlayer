#pragma once
#include <JuceHeader.h>
#include "MetronomeEngine.h"

// ===========================================================================
// MetronomeOutputPanel
//   Configuration for how the metronome is emitted: internal click / MIDI
//   output device, MIDI mode, channel and downbeat/beat note numbers.
//
//   This panel is long-lived (owned by MainComponent) because it owns the
//   open MidiOutput device — the metronome must keep sending MIDI even while
//   the Audio Setup window (where the panel is shown) is closed.
// ===========================================================================
class MetronomeOutputPanel : public juce::Component
{
public:
    explicit MetronomeOutputPanel(MetronomeEngine& metro);
    ~MetronomeOutputPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Preferred height for the controls, used by the host window layout.
    static constexpr int preferredHeight = 214;

private:
    void refreshMidiDevices();
    void applyMidiMode();
    static juce::String noteNumberToName(int n);  // e.g. 41 -> "F2"

    MetronomeEngine& metronome;

    juce::Label    sectionLabel;
    juce::ComboBox midiModeBox;
    juce::ComboBox midiDeviceBox;
    juce::Label    midiModeLabel;
    juce::Label    midiDeviceLabel;

    juce::Label    midiChannelLabel, midiNoteDownLabel, midiNoteBeatLabel;
    juce::Slider   midiChannelSlider, midiNoteDownSlider, midiNoteBeatSlider;

    juce::Label    countInLabel;
    juce::ComboBox countInBox;

    // The open MidiOutput is owned by MetronomeEngine, not here.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetronomeOutputPanel)
};
