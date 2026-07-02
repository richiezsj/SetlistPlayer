#include "TransportPanel.h"

TransportPanel::TransportPanel(MetronomeEngine& metro, AudioPlayerEngine& player)
    : metronome(metro), audioPlayer(player)
{
    // -----------------------------------------------------------------------
    // Song name / BPM / status labels
    // -----------------------------------------------------------------------
    songNameLabel.setFont(Theme::fontBold(21.0f));
    songNameLabel.setColour(juce::Label::textColourId, Theme::textPrimary);
    songNameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(songNameLabel);

    bpmDisplayLabel.setFont(Theme::fontBold(40.0f));
    bpmDisplayLabel.setColour(juce::Label::textColourId, Theme::textPrimary);
    bpmDisplayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmDisplayLabel);

    statusLabel.setFont(Theme::font(12.5f));
    statusLabel.setColour(juce::Label::textColourId, Theme::textSecondary);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    positionLabel.setFont(Theme::font(13.0f));
    positionLabel.setColour(juce::Label::textColourId, Theme::textTertiary);
    positionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(positionLabel);

    // -----------------------------------------------------------------------
    // Transport buttons
    // -----------------------------------------------------------------------
    // Play is the primary action (accent-filled); the rest are neutral.
    playButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6") + "  Play");
    playButton.onClick = [this] { playClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(playButton);

    pauseButton.setButtonText(juce::String::fromUTF8("\xe2\x9d\x9a") + "  Pause");
    pauseButton.onClick = [this] { if (onPauseResume) onPauseResume(); };
    addAndMakeVisible(pauseButton);

    stopButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xa0") + "  Stop");
    stopButton.onClick = [this] { stopClicked(); };
    addAndMakeVisible(stopButton);

    nextButton.setButtonText("Next  " + juce::String::fromUTF8("\xe2\x96\xb6\xe2\x96\xb6"));
    nextButton.onClick = [this] { if (onNextSong) onNextSong(); };
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
    // Flat elevated card for the transport area.
    g.setColour(Theme::panelBg);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), Theme::radiusLarge);

    // Beat indicators - use the exact area reserved in resized()
    drawBeatIndicators(g, beatAreaBounds);

    // Hairline divider between the mixer strips and the main area
    const int stripW = 100;
    g.setColour(Theme::separator);
    g.fillRect(getWidth() - stripW * 2 - 12, 12, 1, getHeight() - 24);
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
            // Downbeat uses the warm Click accent, other beats the blue accent.
            g.setColour(isDownBeat ? Theme::clickAccent : Theme::accent);
            g.fillEllipse(dot.toFloat());
        }
        else
        {
            g.setColour(Theme::controlBg);
            g.fillEllipse(dot.toFloat());
            if (isDownBeat)
            {
                g.setColour(Theme::textTertiary);
                g.drawEllipse(dot.toFloat().reduced(1), 1.5f);
            }
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
    int bw = btnRow.getWidth() / 4;
    playButton.setBounds(btnRow.removeFromLeft(bw).reduced(4, 0));
    pauseButton.setBounds(btnRow.removeFromLeft(bw).reduced(4, 0));
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
    if (onPlay) onPlay();
}

void TransportPanel::stopClicked()
{
    playing     = false;
    currentBeat = 0;
    currentBar  = 0;
    repaint();
    if (onStop) onStop();
}

void TransportPanel::setPaused(bool isPaused)
{
    paused = isPaused;
    pauseButton.setButtonText(paused
        ? juce::String::fromUTF8("\xe2\x96\xb6") + "  Resume"
        : juce::String::fromUTF8("\xe2\x9d\x9a") + "  Pause");
    // Resume becomes the accented (primary) action while paused.
    pauseButton.setColour(juce::TextButton::buttonColourId,
                          paused ? Theme::accent : Theme::controlBg);
}
