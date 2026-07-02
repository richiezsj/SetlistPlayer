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

    // Re-sync the count-in selector from the engine (after restoring settings).
    void refreshCountIn();

    // --- Persistence support (used by MainComponent's settings file) ---
    int getMidiMode() const;                         // combo id: 1/2/3
    juce::String getSelectedDeviceIdentifier() const;
    juce::String getSelectedDeviceName() const;
    // Restore mode/device/channel/notes. Falls back to the internal click if
    // the saved device is no longer available. Opens the device as needed.
    void restoreState(int mode, const juce::String& deviceId,
                      const juce::String& deviceName,
                      int channel, int noteDown, int noteBeat);

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
