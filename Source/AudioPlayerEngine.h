#pragma once
#include <atomic>
#include <JuceHeader.h>

class AudioPlayerEngine : public juce::AudioSource
{
public:
    AudioPlayerEngine();
    ~AudioPlayerEngine() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override;

    bool loadFile(const juce::File& file);
    void unloadFile();

    void play();
    void stop();
    void pause();
    bool isPlaying() const;
    bool isFileLoaded() const { return fileLoaded; }

    double getLengthInSeconds() const;
    double getCurrentPositionInSeconds() const;
    void setPosition(double seconds);

    void setGain(float g) { gain = g; }
    float getGain() const { return gain; }
    void  setPan(float p)  { pan   = juce::jlimit(-1.0f, 1.0f, p); }
    void  setMuted(bool m) { muted = m; }
    float getPan()   const { return pan; }
    bool  isMuted()  const { return muted; }

    std::function<void()> onPlaybackFinished;

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    std::atomic<bool>  fileLoaded { false };
    std::atomic<float> gain  { 1.0f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  muted  { false };

    // Latches true once onPlaybackFinished has been posted, so the callback
    // fires exactly once per playback (the stream stays "finished" for many
    // blocks). Reset on play()/loadFile().
    std::atomic<bool>  finishedNotified { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPlayerEngine)
};
