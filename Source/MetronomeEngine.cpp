#include "MetronomeEngine.h"

MetronomeEngine::MetronomeEngine() {}

MetronomeEngine::~MetronomeEngine()
{
    stop();
    stopTimer();   // must stop the drain thread before members are destroyed
}

void MetronomeEngine::prepareToPlay(int /*samplesPerBlockExpected*/, double sr)
{
    sampleRate = sr;
    // BPM refers to the quarter note; scale by 4/denominator so the beat unit
    // (e.g. the eighth in 6/8) has the correct duration.
    samplesPerBeat = (60.0 / bpm) * sampleRate * (4.0 / denominator);
    clickSamplesHi = (int)(0.025 * sampleRate);
    clickSamplesLo = (int)(0.018 * sampleRate);
}

void MetronomeEngine::releaseResources() {}

void MetronomeEngine::setBpm(double newBpm)
{
    bpm.store(juce::jlimit(20.0, 300.0, newBpm));
    // samplesPerBeat is recomputed on the audio thread each block from bpm.
}

void MetronomeEngine::setMidiOutputDevice(std::unique_ptr<juce::MidiOutput> device)
{
    const bool hasDevice = (device != nullptr);
    {
        // Blocks briefly if the timer thread is mid-send, guaranteeing the old
        // device is never freed while in use.
        const juce::ScopedLock sl(midiLock);
        midiOutput = std::move(device);
    }

    // Run the drain thread only while a device is present. Every 1 ms is well
    // below one beat and keeps click jitter inaudible.
    if (hasDevice) startTimer(1);
    else           stopTimer();
}

void MetronomeEngine::setTimeSignature(int num, int den)
{
    numerator   = juce::jmax(1, num);
    denominator = juce::jmax(1, den);   // guard the 4/denominator division on the audio thread
}

void MetronomeEngine::start()
{
    sampleCounter = 0.0;
    currentBeat   = 0;
    currentBar    = 0;
    clickCountdown = 0;
    lastMidiNote   = -1;   // no note is sounding yet

    // Arm the count-in: countInBars whole bars of clicks before the song.
    countInBeatsLeft = countInBars.load() * numerator.load();
    countInFinishing = false;

    playing = true;
}

void MetronomeEngine::stop()
{
    playing = false;
    clickCountdown = 0;

    // Silence any hanging MIDI note. Called on the message thread, so it is
    // safe to touch the device directly under the lock.
    const juce::ScopedLock sl(midiLock);
    if (midiOutput != nullptr)
        midiOutput->sendMessageNow(juce::MidiMessage::allNotesOff(midiChannel.load()));
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

    // Recompute each block: BPM is the quarter-note rate, scaled by
    // 4/denominator so the beat unit matches the time signature (6/8, 3/8, ...).
    samplesPerBeat = (60.0 / bpm.load()) * sampleRate * (4.0 / denominator.load());

    for (int i = 0; i < numSamples; ++i)
    {
        if (sampleCounter <= 0.0)
        {
            // The count-in has just elapsed: this boundary is the first real
            // downbeat. Realign the bar/beat and start the backing track.
            if (countInFinishing)
            {
                countInFinishing = false;
                currentBeat = 0;
                currentBar  = 0;
                if (onCountInFinished)
                    juce::MessageManager::callAsync([this]
                        { if (onCountInFinished) onCountInFinished(); });
            }

            bool isDown = (currentBeat == 0);
            clickCountdown  = isDown ? clickSamplesHi : clickSamplesLo;
            isDownBeatClick = isDown;
            clickEnvelope   = 1.0f;

            if (useMidi.load())
            {
                // Enqueue only — the timer thread performs the actual device I/O,
                // so the audio thread never blocks or touches the device.
                const int ch = midiChannel.load();
                const juce::uint8 note = isDown ? (juce::uint8)midiNoteDown.load()
                                                : (juce::uint8)midiNoteBeat.load();
                const juce::uint8 velocity = isDown ? (juce::uint8)100 : (juce::uint8)70;

                // Turn off the previous note first so held sounds don't stack.
                if (lastMidiNote >= 0)
                    pushMidiEvent((juce::uint8)(0x80 | (lastMidiChannel - 1)),
                                  (juce::uint8)lastMidiNote, 0);

                pushMidiEvent((juce::uint8)(0x90 | (ch - 1)), note, velocity);
                lastMidiNote    = note;
                lastMidiChannel = ch;
            }

            if (onBeat) onBeat(currentBeat, currentBar);

            currentBeat++;
            if (currentBeat >= numerator)
            {
                currentBeat = 0;
                currentBar++;
            }

            // Count down the preparation clicks; when the last one has played,
            // arm the transition so the next boundary starts the song.
            if (countInBeatsLeft > 0 && --countInBeatsLeft == 0)
                countInFinishing = true;

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

void MetronomeEngine::pushMidiEvent(juce::uint8 status, juce::uint8 data1, juce::uint8 data2)
{
    // Audio thread (producer). Never blocks: if the FIFO is full the event is
    // dropped rather than stalling the audio callback.
    int start1, size1, start2, size2;
    midiFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        midiFifoBuffer[(size_t) start1] = { status, data1, data2 };
        midiFifo.finishedWrite(1);
    }
}

void MetronomeEngine::hiResTimerCallback()
{
    // Timer thread (consumer). Holds midiLock so it can't race the device swap.
    const juce::ScopedLock sl(midiLock);

    int start1, size1, start2, size2;
    midiFifo.prepareToRead(midiFifo.getNumReady(), start1, size1, start2, size2);

    auto sendRange = [this](int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto& e = midiFifoBuffer[(size_t) (start + i)];
            if (midiOutput != nullptr)
                midiOutput->sendMessageNow(juce::MidiMessage(e.status, e.data1, e.data2));
        }
    };
    sendRange(start1, size1);
    sendRange(start2, size2);

    midiFifo.finishedRead(size1 + size2);
}
