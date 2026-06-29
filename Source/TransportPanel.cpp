#include "TransportPanel.h"

TransportPanel::TransportPanel(MetronomeEngine& metro, AudioPlayerEngine& player)
    : metronome(metro), audioPlayer(player)
{
    // -----------------------------------------------------------------------
    // Song name / BPM / status labels
    // -----------------------------------------------------------------------
    songNameLabel.setFont(juce::Font(20.0f).boldened());
    songNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    songNameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(songNameLabel);

    bpmDisplayLabel.setFont(juce::Font(36.0f).boldened());
    bpmDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF4488FF));
    bpmDisplayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmDisplayLabel);

    statusLabel.setFont(juce::Font(12.0f).italicised());
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    positionLabel.setFont(juce::Font(13.0f));
    positionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAABBCC));
    positionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(positionLabel);

    // -----------------------------------------------------------------------
    // Transport buttons
    // -----------------------------------------------------------------------
    playButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6") + "  PLAY");
    playButton.onClick = [this] { playClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF226644));
    playButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xFF33AA66));
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(playButton);

    stopButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xa0") + "  STOP");
    stopButton.onClick = [this] { stopClicked(); };
    stopButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF662222));
    stopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(stopButton);

    nextButton.setButtonText("NEXT " + juce::String::fromUTF8("\xe2\x96\xb6\xe2\x96\xb6"));
    nextButton.onClick = [this] { if (onNextSong) onNextSong(); };
    nextButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF334466));
    nextButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(nextButton);

    // -----------------------------------------------------------------------
    // Audio channel strip
    // -----------------------------------------------------------------------
    audioStrip.onVolumeChanged = [this](float v) { audioPlayer.setGain(v); };
    audioStrip.onPanChanged    = [this](float p) { audioPlayer.setPan(p); };
    audioStrip.onMuteChanged   = [this](bool m)  { audioPlayer.setMuted(m); };
    addAndMakeVisible(audioStrip);

    // -----------------------------------------------------------------------
    // Metronome channel strip
    // -----------------------------------------------------------------------
    metroStrip.onVolumeChanged = [this](float v) { metronome.setGain(v); };
    metroStrip.onPanChanged    = [this](float p) { metronome.setPan(p); };
    metroStrip.onMuteChanged   = [this](bool m)  { metronome.setMuted(m); };
    addAndMakeVisible(metroStrip);

    // -----------------------------------------------------------------------
    // MIDI metronome controls
    // -----------------------------------------------------------------------
    midiSectionLabel.setText("METRONOME OUTPUT", juce::dontSendNotification);
    midiSectionLabel.setFont(juce::Font(10.0f).boldened());
    midiSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF778899));
    midiSectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(midiSectionLabel);

    midiModeLabel.setText("Mode:", juce::dontSendNotification);
    midiModeLabel.setFont(juce::Font(11.0f));
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
    midiDeviceLabel.setFont(juce::Font(11.0f));
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
        lbl.setFont(juce::Font(10.0f));
        lbl.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::IncDecButtons);
        s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 52, 20);
        s.setRange(0, 127, 1);
        s.setValue(defaultVal, juce::dontSendNotification);
        s.setColour(juce::Slider::textBoxTextColourId,       juce::Colours::white);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF1A1A2E));
        s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xFF334455));
        s.onValueChange = [this, &s, onChange]
        {
            int val = (int)s.getValue();
            onChange(val);
            // Show note name in tooltip
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

    midiChannelLabel.setText("Ch:", juce::dontSendNotification);
    midiChannelLabel.setFont(juce::Font(10.0f));
    midiChannelLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    addAndMakeVisible(midiChannelLabel);

    midiChannelSlider.setSliderStyle(juce::Slider::IncDecButtons);
    midiChannelSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 34, 20);
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
    metronome.onBeat = [this](int beat, int bar)
    {
        currentBeat = beat;
        currentBar  = bar;
        needsRepaint.store(true);
    };

    clearSong();
    startTimerHz(30);
}

TransportPanel::~TransportPanel()
{
    metronome.onBeat = nullptr;
    stopTimer();
    metronome.setMidiOutput(nullptr);
    metronome.setUseMidi(false);
    midiOutput.reset();
}

void TransportPanel::refreshMidiDevices()
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

void TransportPanel::applyMidiMode()
{
    int mode = midiModeBox.getSelectedId();  // 1=internal, 2=MIDI only, 3=both

    // Enable/disable device selector
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
        {
            midiOutput = juce::MidiOutput::openDevice(devices[idx].identifier);
            metronome.setMidiOutput(midiOutput.get());
        }
        else
        {
            midiOutput.reset();
            metronome.setMidiOutput(nullptr);
        }
    }
    else
    {
        metronome.setMidiOutput(nullptr);
        midiOutput.reset();
    }
}

juce::String TransportPanel::noteNumberToName(int n)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    int octave = (n / 12) - 1;
    return juce::String(names[n % 12]) + juce::String(octave)
         + "  (" + juce::String(n) + ")";
}

void TransportPanel::timerCallback()
{
    if (needsRepaint.exchange(false))
        repaint();
    updateTransportState();
}

void TransportPanel::updateTransportState()
{
    if (audioPlayer.isFileLoaded())
    {
        double pos = audioPlayer.getCurrentPositionInSeconds();
        double len = audioPlayer.getLengthInSeconds();
        int pm = (int)(pos / 60.0);
        int ps = (int)(pos) % 60;
        int lm = (int)(len / 60.0);
        int ls = (int)(len) % 60;
        positionLabel.setText(juce::String::formatted("%d:%02d / %d:%02d", pm, ps, lm, ls),
                              juce::dontSendNotification);
    }
    else
    {
        positionLabel.setText("Bar " + juce::String(currentBar + 1) +
                              "  Beat " + juce::String(currentBeat + 1),
                              juce::dontSendNotification);
    }
}

void TransportPanel::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(juce::Colour(0xFF1A1A30), 0, 0,
                              juce::Colour(0xFF0E0E1C), 0, (float)getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    // Beat indicators - use the exact area reserved in resized()
    drawBeatIndicators(g, beatAreaBounds);

    // Divider between strips and main area
    const int stripW = 100;
    g.setColour(juce::Colour(0xFF334455));
    g.drawLine((float)(getWidth() - stripW * 2 - 12), 8.0f,
               (float)(getWidth() - stripW * 2 - 12), (float)(getHeight() - 8), 1.0f);

    // MIDI section label separator
    if (midiSectionLabel.getY() > 0)
    {
        g.setColour(juce::Colour(0xFF2A3A4A));
        g.fillRect(8, midiSectionLabel.getY() - 6, getWidth() - stripW * 2 - 24, 1);
    }
}

void TransportPanel::drawBeatIndicators(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (currentSong == nullptr) return;

    int beats   = juce::jmax(1, currentSong->beatsPerBar);
    int dotSize = juce::jmax(4, juce::jmin(36, area.getWidth() / beats - 4));
    int totalW  = beats * (dotSize + 4);
    int startX  = area.getCentreX() - totalW / 2;
    int y       = area.getCentreY() - dotSize / 2;

    for (int i = 0; i < beats; ++i)
    {
        int x = startX + i * (dotSize + 4);
        juce::Rectangle<int> dot(x, y, dotSize, dotSize);

        bool isActive    = playing && (i == currentBeat);
        bool isDownBeat  = (i == 0);

        if (isActive)
        {
            g.setColour(isDownBeat ? juce::Colour(0xFFFF6644) : juce::Colour(0xFF4488FF));
            g.fillEllipse(dot.toFloat());
        }
        else
        {
            g.setColour(juce::Colour(0xFF334455));
            g.fillEllipse(dot.toFloat());
            g.setColour(isDownBeat ? juce::Colour(0xFF884422) : juce::Colour(0xFF334477));
            g.drawEllipse(dot.toFloat().reduced(1), 1.5f);
        }
    }
}

void TransportPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    if (area.getWidth() < 10 || area.getHeight() < 10) return;

    // Channel strips on the right
    const int stripW = 100;
    auto stripsArea = area.removeFromRight(stripW * 2 + 8);
    metroStrip.setBounds(stripsArea.removeFromRight(stripW).reduced(2));
    audioStrip.setBounds(stripsArea.reduced(2));

    area.removeFromRight(4); // gap

    // Left: labels + transport buttons
    songNameLabel.setBounds(area.removeFromTop(36));
    area.removeFromTop(2);
    bpmDisplayLabel.setBounds(area.removeFromTop(48));
    statusLabel.setBounds(area.removeFromTop(18));
    beatAreaBounds = area.removeFromTop(60); // beat dots drawn here in paint()
    positionLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    auto btnRow = area.removeFromTop(44);
    int bw = btnRow.getWidth() / 3;
    playButton.setBounds(btnRow.removeFromLeft(bw).reduced(4, 0));
    stopButton.setBounds(btnRow.removeFromLeft(bw).reduced(4, 0));
    nextButton.setBounds(btnRow.reduced(4, 0));

    // MIDI section below transport buttons
    area.removeFromTop(10);
    midiSectionLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    auto midiModeRow = area.removeFromTop(22);
    midiModeLabel.setBounds(midiModeRow.removeFromLeft(44));
    midiModeBox.setBounds(midiModeRow);

    area.removeFromTop(4);
    auto midiDevRow = area.removeFromTop(22);
    midiDeviceLabel.setBounds(midiDevRow.removeFromLeft(44));
    midiDeviceBox.setBounds(midiDevRow);

    // Note & channel controls (visible only when MIDI is involved)
    bool midiActive = (midiModeBox.getSelectedId() != 1);
    area.removeFromTop(6);
    auto chRow = area.removeFromTop(20);
    midiChannelLabel.setBounds(chRow.removeFromLeft(22));
    midiChannelSlider.setBounds(chRow);

    area.removeFromTop(4);
    auto downRow = area.removeFromTop(20);
    midiNoteDownLabel.setBounds(downRow.removeFromLeft(56));
    midiNoteDownSlider.setBounds(downRow);

    area.removeFromTop(4);
    auto beatRow = area.removeFromTop(20);
    midiNoteBeatLabel.setBounds(beatRow.removeFromLeft(56));
    midiNoteBeatSlider.setBounds(beatRow);
}

void TransportPanel::loadSong(const Song* song)
{
    currentSong = song;
    if (song == nullptr) { clearSong(); return; }

    songNameLabel.setText(song->name, juce::dontSendNotification);
    bpmDisplayLabel.setText(juce::String(song->bpm, 1) + " BPM", juce::dontSendNotification);

    juce::String ts = juce::String(song->beatsPerBar) + "/" + juce::String(song->beatUnit);
    if (song->audioFile.existsAsFile())
        ts += "  " + juce::String::fromUTF8("\xe2\x99\xaa") + " " + song->audioFile.getFileName();
    statusLabel.setText(ts, juce::dontSendNotification);

    setEnabled(true);
}

void TransportPanel::clearSong()
{
    currentSong = nullptr;
    songNameLabel.setText("No song selected", juce::dontSendNotification);
    bpmDisplayLabel.setText("---", juce::dontSendNotification);
    statusLabel.setText("Select a song from the setlist", juce::dontSendNotification);
    positionLabel.setText("", juce::dontSendNotification);
    setEnabled(false);

    // Keep strips enabled so user can adjust mix at any time
    audioStrip.setEnabled(true);
    metroStrip.setEnabled(true);
}

void TransportPanel::playClicked()
{
    playing = true;
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF33AA66));
    if (onPlay) onPlay();
}

void TransportPanel::stopClicked()
{
    playing     = false;
    currentBeat = 0;
    currentBar  = 0;
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF226644));
    repaint();
    if (onStop) onStop();
}
