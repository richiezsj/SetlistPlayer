#include "MetronomeEngine.h"

MetronomeEngine::MetronomeEngine() {}
MetronomeEngine::~MetronomeEngine() { stop(); }

void MetronomeEngine::prepareToPlay(int /*samplesPerBlockExpected*/, double sr)
{
    sampleRate = sr;
    samplesPerBeat = (60.0 / bpm) * sampleRate;
    clickSamplesHi = (int)(0.025 * sampleRate);
    clickSamplesLo = (int)(0.018 * sampleRate);
}

void MetronomeEngine::releaseResources() {}

void MetronomeEngine::setBpm(double newBpm)
{
    bpm = juce::jlimit(20.0, 300.0, newBpm);
    samplesPerBeat = (60.0 / bpm) * sampleRate;
}

void MetronomeEngine::setTimeSignature(int num, int den)
{
    numerator   = num;
    denominator = den;
}

void MetronomeEngine::start()
{
    sampleCounter = 0.0;
    currentBeat   = 0;
    currentBar    = 0;
    clickCountdown = 0;
    playing = true;
}

void MetronomeEngine::stop()
{
    playing = false;
    clickCountdown = 0;
}

void MetronomeEngine::reset()
{
    stop();
    sampleCounter = 0.0;
    currentBeat = 0;
    currentBar  = 0;
}

void MetronomeEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (!playing)
    {
        info.clearActiveBufferRegion();
        return;
    }

    auto* buffer = info.buffer;
    int numSamples  = info.numSamples;
    int startSample = info.startSample;

    for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
        buffer->clear(ch, startSample, numSamples);

    samplesPerBeat = (60.0 / bpm) * sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        if (sampleCounter <= 0.0)
        {
            bool isDown = (currentBeat == 0);
            clickCountdown  = isDown ? clickSamplesHi : clickSamplesLo;
            isDownBeatClick = isDown;
            clickEnvelope   = 1.0f;

            if (useMidi && midiOut)
            {
                juce::uint8 note     = isDown ? (juce::uint8)midiNoteDown : (juce::uint8)midiNoteBeat;
                juce::uint8 velocity = isDown ? (juce::uint8)100          : (juce::uint8)70;
                auto msg = juce::MidiMessage::noteOn(midiChannel, note, velocity);
                midiOut->sendMessageNow(msg);
            }

            if (onBeat) onBeat(currentBeat, currentBar);

            currentBeat++;
            if (currentBeat >= numerator)
            {
                currentBeat = 0;
                currentBar++;
            }
            sampleCounter += samplesPerBeat;
        }

        // Synthesise click only when not muted AND internal click is enabled
        if (!muted && useInternal && clickCountdown > 0)
        {
            float freq = isDownBeatClick ? 1800.0f : 1200.0f;
            float t    = (float)(clickSamplesHi - clickCountdown) / (float)sampleRate;
            float sample = clickAmplitude * clickEnvelope
                           * std::sin(juce::MathConstants<float>::twoPi * freq * t);
            clickEnvelope *= 0.9995f;

            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
                buffer->addSample(ch, startSample + i, sample);
        }

        if (clickCountdown > 0) clickCountdown--;
        sampleCounter -= 1.0;
    }

    // Apply gain and pan (skipped when muted - buffer is already silent)
    if (!muted)
    {
        const int numCh = buffer->getNumChannels();
        if (numCh >= 2)
        {
            float panL = gain * std::cos((pan + 1.0f) * juce::MathConstants<float>::halfPi * 0.5f);
            float panR = gain * std::sin((pan + 1.0f) * juce::MathConstants<float>::halfPi * 0.5f);
            buffer->applyGain(0, startSample, numSamples, panL);
            buffer->applyGain(1, startSample, numSamples, panR);
        }
        else
        {
            buffer->applyGain(startSample, numSamples, gain);
        }
    }
}
