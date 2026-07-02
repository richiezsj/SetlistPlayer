#pragma once
#include <atomic>
#include <JuceHeader.h>
#include "MetronomeEngine.h"
#include "AudioPlayerEngine.h"
#include "Project.h"

// ===========================================================================
// VuMeter  –  vertical bar with peak hold, driven from audio thread via atomic
// ===========================================================================
class VuMeter : public juce::Component, private juce::Timer
{
public:
    VuMeter()  { startTimerHz(30); }
    ~VuMeter() { stopTimer(); }

    // Called from audio thread – lock-free
    void pushLevel(float linearRms)
    {
        float cur = level.load();
        // fast attack, slow decay handled in timer
        if (linearRms > cur)
            level.store(linearRms);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        float h = b.getHeight();
        float w = b.getWidth();

        // Background
        g.setColour(juce::Colour(0xFF111120));
        g.fillRoundedRectangle(b, 2.0f);

        // Compute fill height from current level (0..1.5 mapped to full bar)
        float displayLevel = juce::jlimit(0.0f, 1.0f, displayVal / 1.5f);
        float fillH = displayLevel * h;

        if (fillH > 0.5f)
        {
            // Gradient: green → yellow → red
            juce::ColourGradient grad(
                juce::Colour(0xFF44FF88), b.getX(), b.getBottom(),
                juce::Colour(0xFFFF3322), b.getX(), b.getY(), false);
            grad.addColour(0.75, juce::Colour(0xFFFFCC00));

            juce::Rectangle<float> fill(b.getX(), b.getBottom() - fillH, w, fillH);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fill, 2.0f);
        }

        // Peak hold line
        if (peakVal > 0.005f)
        {
            float peakY = b.getBottom() - juce::jlimit(0.0f, 1.0f, peakVal / 1.5f) * h;
            g.setColour(peakVal > 1.0f ? juce::Colour(0xFFFF3322) : juce::Colour(0xFFFFFFAA));
            g.fillRect(b.getX(), peakY - 1.0f, w, 2.0f);
        }

        // Clip indicator at top
        if (peakVal >= 1.42f)
        {
            g.setColour(juce::Colour(0xFFFF2200));
            g.fillRoundedRectangle(b.getX(), b.getY(), w, 5.0f, 2.0f);
        }
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        // Click to reset clip indicator
        peakVal  = 0.0f;
        peakHold = 0;
        repaint();
    }

private:
    void timerCallback() override
    {
        float incoming = level.exchange(0.0f);

        // Smooth display: fast attack, slow decay
        if (incoming > displayVal)
            displayVal = incoming;
        else
            displayVal *= 0.88f;   // ~30ms decay at 30Hz

        // Peak hold
        if (incoming > peakVal)
        {
            peakVal  = incoming;
            peakHold = 90;         // hold for ~3s at 30Hz
        }
        else if (peakHold > 0)
        {
            --peakHold;
        }
        else
        {
            peakVal *= 0.92f;
        }

        repaint();
    }

    std::atomic<float> level { 0.0f };
    float displayVal = 0.0f;
    float peakVal    = 0.0f;
    int   peakHold   = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeter)
};

// ===========================================================================
// Rotary pan knob - custom paint, no LookAndFeel dependency
// ===========================================================================
class PanKnob : public juce::Slider
{
public:
    PanKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setRange(-1.0, 1.0, 0.01);
        setValue(0.0, juce::dontSendNotification);
        setDoubleClickReturnValue(true, 0.0);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float r  = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        g.setColour(juce::Colour(0xFF2A2A3A));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        float midAngle         = (startAngle + endAngle) * 0.5f;
        float val              = (float)getValue();
        float curAngle         = midAngle + val * (endAngle - midAngle);

        juce::Path arc;
        arc.addArc(cx - r + 5.0f, cy - r + 5.0f,
                   (r - 5.0f) * 2.0f, (r - 5.0f) * 2.0f,
                   (val < 0.0f ? curAngle : midAngle),
                   (val < 0.0f ? midAngle : curAngle),
                   true);
        g.setColour(val == 0.0f ? juce::Colour(0xFF445566) : juce::Colour(0xFF4488FF));
        g.strokePath(arc, juce::PathStrokeType(2.5f));

        float px = cx + (r - 7.0f) * std::cos(curAngle - juce::MathConstants<float>::halfPi);
        float py = cy + (r - 7.0f) * std::sin(curAngle - juce::MathConstants<float>::halfPi);
        g.setColour(juce::Colours::white);
        g.drawLine(cx, cy, px, py, 2.0f);

        g.setColour(juce::Colour(0xFF334455));
        g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanKnob)
};

// ===========================================================================
// ChannelStrip  –  label / mute / vertical fader + VU meter / pan knob
// ===========================================================================
class ChannelStrip : public juce::Component
{
public:
    juce::Label      nameLabel;
    juce::TextButton muteButton { "M" };
    juce::Slider     volSlider;
    PanKnob          panKnob;
    VuMeter          vuMeter;

    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;
    std::function<void(bool)>  onMuteChanged;

    // Called from audio thread (lock-free via VuMeter atomic)
    void pushLevel(float rmsLinear)
    {
        // rmsLinear is already post-gain: the engine applies the fader gain
        // inside its own block, and the RMS is measured on that output. Do NOT
        // multiply by the fader again here (double gain), and it also avoids
        // reading the Slider from the audio thread.
        vuMeter.pushLevel(rmsLinear);
    }

    ChannelStrip(juce::Colour accent, const juce::String& label)
        : accentColour(accent)
    {
        nameLabel.setText(label, juce::dontSendNotification);
        nameLabel.setFont(juce::Font(11.0f).boldened());
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setColour(juce::Label::textColourId, accent);
        addAndMakeVisible(nameLabel);

        muteButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFF2A2A3A));
        muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFAA3322));
        muteButton.setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
        muteButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        muteButton.setClickingTogglesState(true);
        muteButton.onClick = [this]
        {
            bool m = muteButton.getToggleState();
            muteButton.setColour(juce::TextButton::buttonColourId,
                                 m ? juce::Colour(0xFFAA3322) : juce::Colour(0xFF2A2A3A));
            if (onMuteChanged) onMuteChanged(m);
        };
        addAndMakeVisible(muteButton);

        volSlider.setSliderStyle(juce::Slider::LinearVertical);
        volSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        volSlider.setRange(0.0, 1.5, 0.01);
        volSlider.setValue(1.0, juce::dontSendNotification);
        volSlider.setDoubleClickReturnValue(true, 1.0);
        volSlider.setColour(juce::Slider::thumbColourId, accent);
        volSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF334455));
        volSlider.onValueChange = [this]
        {
            if (onVolumeChanged) onVolumeChanged((float)volSlider.getValue());
        };
        addAndMakeVisible(volSlider);

        addAndMakeVisible(vuMeter);

        panKnob.onValueChange = [this]
        {
            if (onPanChanged) onPanChanged((float)panKnob.getValue());
        };
        addAndMakeVisible(panKnob);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xFF1A1A2E));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        g.setColour(accentColour.withAlpha(0.35f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.2f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        if (area.getHeight() < 30) return;

        nameLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);
        muteButton.setBounds(area.removeFromTop(24).reduced(10, 0));
        area.removeFromTop(4);

        auto panArea = area.removeFromBottom(46);
        panKnob.setBounds(panArea);

        // Fader and VU side by side — VU takes 14px on the right
        auto faderArea = area;
        auto vuArea    = faderArea.removeFromRight(14);
        vuArea.reduce(0, 2);
        vuMeter.setBounds(vuArea);
        volSlider.setBounds(faderArea);
    }

private:
    juce::Colour accentColour;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStrip)
};


// ===========================================================================
// TransportPanel
// ===========================================================================
class TransportPanel : public juce::Component, private juce::Timer
{
public:
    TransportPanel(MetronomeEngine& metro, AudioPlayerEngine& player);
    ~TransportPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void loadSong(const Song* song);
    void clearSong();

    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onPauseResume;
    std::function<void()> onNextSong;

    // Reflect paused state in the Pause/Resume button.
    void setPaused(bool isPaused);

    ChannelStrip audioStrip { juce::Colour(0xFF4488FF), "AUDIO" };
    ChannelStrip metroStrip { juce::Colour(0xFFFF9944), "CLICK" };

private:
    void timerCallback() override;
    void updateTransportState();
    void playClicked();
    void stopClicked();
    void drawBeatIndicators(juce::Graphics& g, juce::Rectangle<int> area);

    MetronomeEngine&   metronome;
    AudioPlayerEngine& audioPlayer;

    const Song* currentSong = nullptr;
    bool playing = false;

    juce::TextButton playButton, pauseButton, stopButton, nextButton;
    bool paused = false;
    juce::Label      songNameLabel, bpmDisplayLabel, positionLabel, statusLabel;

    int currentBeat = 0;
    int currentBar  = 0;

    juce::Rectangle<int> beatAreaBounds; // set in resized(), used in paint()

    std::atomic<bool> needsRepaint { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportPanel)
};
