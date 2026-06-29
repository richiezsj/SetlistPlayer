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
    // Beat callback
    // (Metronome output configuration now lives in the Audio Setup window.)
    // -----------------------------------------------------------------------
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
