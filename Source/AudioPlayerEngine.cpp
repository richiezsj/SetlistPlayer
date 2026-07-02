#include "AudioPlayerEngine.h"

AudioPlayerEngine::AudioPlayerEngine()
{
    formatManager.registerBasicFormats();
}

AudioPlayerEngine::~AudioPlayerEngine()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
}

void AudioPlayerEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioPlayerEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (!fileLoaded)
    {
        info.clearActiveBufferRegion();
        return;
    }

    // Always consume samples to keep the playhead in sync
    transportSource.getNextAudioBlock(info);

    if (muted)
    {
        info.clearActiveBufferRegion();
    }
    else
    {
        // Apply gain and pan (equal-power)
        const int numCh = info.buffer->getNumChannels();
        if (numCh >= 2)
        {
            float panL = gain * std::cos((pan + 1.0f) * juce::MathConstants<float>::halfPi * 0.5f);
            float panR = gain * std::sin((pan + 1.0f) * juce::MathConstants<float>::halfPi * 0.5f);
            info.buffer->applyGain(0, info.startSample, info.numSamples, panL);
            info.buffer->applyGain(1, info.startSample, info.numSamples, panR);
        }
        else
        {
            info.buffer->applyGain(info.startSample, info.numSamples, gain);
        }
    }

    // Notify finish exactly once: the stream reports "finished" for many
    // blocks, so latch to avoid flooding the message thread with callbacks.
    if (!transportSource.isPlaying() && transportSource.hasStreamFinished())
    {
        if (!finishedNotified.exchange(true) && onPlaybackFinished)
            juce::MessageManager::callAsync([this] { onPlaybackFinished(); });
    }
}

void AudioPlayerEngine::releaseResources()
{
    transportSource.releaseResources();
}

bool AudioPlayerEngine::loadFile(const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr) return false;

    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();

    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate, 2);
    fileLoaded = true;
    finishedNotified = false;
    return true;
}

void AudioPlayerEngine::unloadFile()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
    fileLoaded = false;
}

void AudioPlayerEngine::play()
{
    if (fileLoaded)
    {
        finishedNotified = false;
        transportSource.setPosition(0.0);
        transportSource.start();
    }
}

void AudioPlayerEngine::stop()
{
    transportSource.stop();
    if (fileLoaded)
        transportSource.setPosition(0.0);
}

void AudioPlayerEngine::pause()
{
    transportSource.stop();
}

void AudioPlayerEngine::resume()
{
    if (fileLoaded)
    {
        finishedNotified = false;
        transportSource.start();   // keep the current position
    }
}

bool AudioPlayerEngine::isPlaying() const
{
    return transportSource.isPlaying();
}

double AudioPlayerEngine::getLengthInSeconds() const
{
    return transportSource.getLengthInSeconds();
}

double AudioPlayerEngine::getCurrentPositionInSeconds() const
{
    return transportSource.getCurrentPosition();
}

void AudioPlayerEngine::setPosition(double seconds)
{
    transportSource.setPosition(seconds);
}
