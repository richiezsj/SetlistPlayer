#include "MetronomeOutputPanel.h"

MetronomeOutputPanel::MetronomeOutputPanel(MetronomeEngine& metro)
    : metronome(metro)
{
    // -----------------------------------------------------------------------
    // Section header
    // -----------------------------------------------------------------------
    sectionLabel.setText("METRONOME OUTPUT", juce::dontSendNotification);
    sectionLabel.setFont(juce::Font(11.0f).boldened());
    sectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF778899));
    sectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sectionLabel);

    // -----------------------------------------------------------------------
    // MIDI mode / device
    // -----------------------------------------------------------------------
    midiModeLabel.setText("Mode:", juce::dontSendNotification);
    midiModeLabel.setFont(juce::Font(12.0f));
    midiModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    addAndMakeVisible(midiModeLabel);

    midiModeBox.addItem("Internal click", 1);
    midiModeBox.addItem("MIDI out only",  2);
    midiModeBox.addItem("Internal + MIDI", 3);
    midiModeBox.setSelectedId(1, juce::dontSendNotification);
    midiModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A2A3A));
    midiModeBox.setColour(juce::ComboBox::textColourId,       juce::Colours::white);
    midiModeBox.onChange = [this] { applyMidiMode(); };
    addAndMakeVisible(midiModeBox);

    midiDeviceLabel.setText("MIDI device:", juce::dontSendNotification);
    midiDeviceLabel.setFont(juce::Font(12.0f));
    midiDeviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    addAndMakeVisible(midiDeviceLabel);

    midiDeviceBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A2A3A));
    midiDeviceBox.setColour(juce::ComboBox::textColourId,       juce::Colours::white);
    midiDeviceBox.onChange = [this] { applyMidiMode(); };
    addAndMakeVisible(midiDeviceBox);

    refreshMidiDevices();

    // -----------------------------------------------------------------------
    // MIDI note & channel controls
    // -----------------------------------------------------------------------
    auto setupNoteSlider = [this](juce::Slider& s, juce::Label& lbl,
                                  const juce::String& text, int defaultVal,
                                  std::function<void(int)> onChange)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(11.0f));
        lbl.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::IncDecButtons);
        s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 52, 22);
        s.setRange(0, 127, 1);
        s.setValue(defaultVal, juce::dontSendNotification);
        s.setColour(juce::Slider::textBoxTextColourId,       juce::Colours::white);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF1A1A2E));
        s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xFF334455));
        s.onValueChange = [this, &s, onChange]
        {
            int val = (int)s.getValue();
            onChange(val);
            s.setTooltip(noteNumberToName(val));
        };
        s.setTooltip(noteNumberToName(defaultVal));
        addAndMakeVisible(s);
    };

    setupNoteSlider(midiNoteDownSlider, midiNoteDownLabel, "Downbeat:",
                    metronome.getMidiNoteDown(),
                    [this](int n){ metronome.setMidiNoteDown(n); });

    setupNoteSlider(midiNoteBeatSlider, midiNoteBeatLabel, "Beat:",
                    metronome.getMidiNoteBeat(),
                    [this](int n){ metronome.setMidiNoteBeat(n); });

    midiChannelLabel.setText("Channel:", juce::dontSendNotification);
    midiChannelLabel.setFont(juce::Font(11.0f));
    midiChannelLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    addAndMakeVisible(midiChannelLabel);

    midiChannelSlider.setSliderStyle(juce::Slider::IncDecButtons);
    midiChannelSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    midiChannelSlider.setRange(1, 16, 1);
    midiChannelSlider.setValue(metronome.getMidiChannel(), juce::dontSendNotification);
    midiChannelSlider.setColour(juce::Slider::textBoxTextColourId,       juce::Colours::white);
    midiChannelSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF1A1A2E));
    midiChannelSlider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xFF334455));
    midiChannelSlider.onValueChange = [this]
    {
        metronome.setMidiChannel((int)midiChannelSlider.getValue());
    };
    addAndMakeVisible(midiChannelSlider);

    // -----------------------------------------------------------------------
    // Count-in: preparation bars before the backing track starts
    // -----------------------------------------------------------------------
    countInLabel.setText("Count-in:", juce::dontSendNotification);
    countInLabel.setFont(juce::Font(12.0f));
    countInLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    addAndMakeVisible(countInLabel);

    countInBox.addItem("Off",    1);   // itemId maps to bar count below
    countInBox.addItem("1 bar",  2);
    countInBox.addItem("2 bars", 3);
    countInBox.addItem("4 bars", 4);
    countInBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A2A3A));
    countInBox.setColour(juce::ComboBox::textColourId,       juce::Colours::white);
    countInBox.onChange = [this]
    {
        static const int barsForId[] = { 0, 0, 1, 2, 4 };  // index by selectedId
        metronome.setCountInBars(barsForId[countInBox.getSelectedId()]);
    };
    // Reflect the engine's current value (Off by default).
    const int bars = metronome.getCountInBars();
    countInBox.setSelectedId(bars >= 4 ? 4 : bars + 1, juce::dontSendNotification);
    addAndMakeVisible(countInBox);
}

MetronomeOutputPanel::~MetronomeOutputPanel()
{
    metronome.setUseMidi(false);
    metronome.setMidiOutputDevice(nullptr);
}

void MetronomeOutputPanel::refreshCountIn()
{
    const int bars = metronome.getCountInBars();   // 0/1/2/4 -> id 1/2/3/4
    countInBox.setSelectedId(bars >= 4 ? 4 : bars + 1, juce::dontSendNotification);
}

void MetronomeOutputPanel::refreshMidiDevices()
{
    auto devices = juce::MidiOutput::getAvailableDevices();
    midiDeviceBox.clear(juce::dontSendNotification);

    if (devices.isEmpty())
    {
        midiDeviceBox.addItem("(no MIDI outputs)", 1);
        midiDeviceBox.setSelectedId(1, juce::dontSendNotification);
        midiDeviceBox.setEnabled(false);
    }
    else
    {
        for (int i = 0; i < devices.size(); ++i)
            midiDeviceBox.addItem(devices[i].name, i + 1);
        midiDeviceBox.setSelectedId(1, juce::dontSendNotification);
        midiDeviceBox.setEnabled(midiModeBox.getSelectedId() != 1);
    }
}

void MetronomeOutputPanel::applyMidiMode()
{
    int mode = midiModeBox.getSelectedId();  // 1=internal, 2=MIDI only, 3=both

    bool needsMidi = (mode == 2 || mode == 3);
    midiDeviceBox.setEnabled(needsMidi && midiDeviceBox.getNumItems() > 0
                             && midiDeviceBox.getItemText(0) != "(no MIDI outputs)");

    metronome.setUseInternal(mode != 2);   // internal off only in MIDI-only mode
    metronome.setUseMidi(needsMidi);

    if (needsMidi)
    {
        auto devices = juce::MidiOutput::getAvailableDevices();
        int idx = midiDeviceBox.getSelectedId() - 1;
        if (idx >= 0 && idx < devices.size())
            metronome.setMidiOutputDevice(juce::MidiOutput::openDevice(devices[idx].identifier));
        else
            metronome.setMidiOutputDevice(nullptr);
    }
    else
    {
        metronome.setMidiOutputDevice(nullptr);
    }
}

juce::String MetronomeOutputPanel::noteNumberToName(int n)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    int octave = (n / 12) - 1;
    return juce::String(names[n % 12]) + juce::String(octave)
         + "  (" + juce::String(n) + ")";
}

void MetronomeOutputPanel::paint(juce::Graphics& g)
{
    // Separator line under the section header
    g.setColour(juce::Colour(0xFF2A3A4A));
    g.fillRect(0, sectionLabel.getBottom() + 2, getWidth(), 1);
}

void MetronomeOutputPanel::resized()
{
    auto area = getLocalBounds();
    if (area.getWidth() < 10 || area.getHeight() < 10) return;

    sectionLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);

    const int labelW = 84;

    auto modeRow = area.removeFromTop(24);
    midiModeLabel.setBounds(modeRow.removeFromLeft(labelW));
    midiModeBox.setBounds(modeRow);

    area.removeFromTop(6);
    auto devRow = area.removeFromTop(24);
    midiDeviceLabel.setBounds(devRow.removeFromLeft(labelW));
    midiDeviceBox.setBounds(devRow);

    area.removeFromTop(6);
    auto chRow = area.removeFromTop(24);
    midiChannelLabel.setBounds(chRow.removeFromLeft(labelW));
    midiChannelSlider.setBounds(chRow.removeFromLeft(110));

    area.removeFromTop(6);
    auto downRow = area.removeFromTop(24);
    midiNoteDownLabel.setBounds(downRow.removeFromLeft(labelW));
    midiNoteDownSlider.setBounds(downRow.removeFromLeft(140));

    area.removeFromTop(6);
    auto beatRow = area.removeFromTop(24);
    midiNoteBeatLabel.setBounds(beatRow.removeFromLeft(labelW));
    midiNoteBeatSlider.setBounds(beatRow.removeFromLeft(140));

    area.removeFromTop(6);
    auto countInRow = area.removeFromTop(24);
    countInLabel.setBounds(countInRow.removeFromLeft(labelW));
    countInBox.setBounds(countInRow.removeFromLeft(120));
}
