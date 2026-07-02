#pragma once
#include <atomic>
#include <JuceHeader.h>

class MetronomeEngine : public juce::AudioSource,
                        private juce::HighResolutionTimer
{
public:
    MetronomeEngine();
    ~MetronomeEngine() override;

    // AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override;

    // Control
    void setBpm(double bpm);
    void setTimeSignature(int numerator, int denominator);
    void start();
    void stop();
    void reset();
    void pause();    // stop clicking but keep beat/bar position
    void resume();   // continue from the paused position
    bool isPlaying() const { return playing; }

    // Mix
    void  setGain(float g) { gain  = juce::jlimit(0.0f, 1.5f, g); }
    void  setPan (float p) { pan   = juce::jlimit(-1.0f, 1.0f, p); }
    void  setMuted(bool m) { muted = m; }
    float getGain()  const { return gain; }
    float getPan()   const { return pan; }
    bool  isMuted()  const { return muted; }

    // MIDI out — the engine takes ownership of the device so it can be
    // swapped safely with respect to the audio thread (see midiLock).
    void setUseMidi(bool b)     { useMidi     = b; }
    void setUseInternal(bool b) { useInternal = b; }
    void setMidiOutputDevice(std::unique_ptr<juce::MidiOutput> device);

    // Count-in: play this many bars before the song proper begins. 0 = off.
    void setCountInBars(int bars) { countInBars = juce::jlimit(0, 8, bars); }
    int  getCountInBars() const   { return countInBars; }

    // MIDI note configuration
    void setMidiChannel(int ch)      { midiChannel  = juce::jlimit(1, 16, ch); }
    void setMidiNoteDown(int note)   { midiNoteDown = juce::jlimit(0, 127, note); }
    void setMidiNoteBeat(int note)   { midiNoteBeat = juce::jlimit(0, 127, note); }
    int  getMidiChannel()  const { return midiChannel; }
    int  getMidiNoteDown() const { return midiNoteDown; }
    int  getMidiNoteBeat() const { return midiNoteBeat; }

    // Callbacks
    std::function<void(int beat, int bar)> onBeat;
    // Fired (on the message thread) at the first real downbeat, once the
    // count-in bars have elapsed. Used to start the backing track in time.
    std::function<void()> onCountInFinished;

private:
    // Lock-free hand-off of MIDI events from the audio thread to the timer
    // thread, which owns the actual (blocking) device I/O.
    struct MidiFifoEvent { juce::uint8 status, data1, data2; };
    void pushMidiEvent(juce::uint8 status, juce::uint8 data1, juce::uint8 data2);
    void hiResTimerCallback() override;   // drains the FIFO on a dedicated thread

    double sampleRate     = 44100.0;          // audio-thread owned (set in prepareToPlay)
    std::atomic<double> bpm         { 120.0 };
    std::atomic<int>    numerator   { 4 };
    std::atomic<int>    denominator { 4 };

    // Audio-thread only
    double samplesPerBeat = 0.0;
    double sampleCounter  = 0.0;
    int currentBeat       = 0;
    int currentBar        = 0;

    // Count-in state (audio-thread only). countInBeatsLeft counts down the
    // preparation clicks; countInFinishing marks the next beat boundary as the
    // first real downbeat, where onCountInFinished fires.
    int  countInBeatsLeft = 0;
    bool countInFinishing = false;

    std::atomic<bool>  playing     { false };
    std::atomic<bool>  useMidi     { false };
    std::atomic<bool>  useInternal { true };
    std::atomic<int>   countInBars { 0 };
    std::atomic<int>   midiChannel  { 10 };   // GM percussion channel
    std::atomic<int>   midiNoteDown { 41 };   // F2 — downbeat
    std::atomic<int>   midiNoteBeat { 41 };   // F2 — other beats (same default)
    std::atomic<float> gain  { 1.0f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  muted { false };

    // MIDI output device is owned here so it outlives any UI panel that
    // configures it. It is touched only by the timer thread (drain) and the
    // message thread (device swap), both under midiLock — never the audio
    // thread, which only enqueues into the lock-free FIFO below.
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::CriticalSection midiLock;

    // Single-producer (audio thread) / single-consumer (timer thread) FIFO.
    static constexpr int midiFifoCapacity = 256;
    juce::AbstractFifo midiFifo { midiFifoCapacity };
    std::array<MidiFifoEvent, midiFifoCapacity> midiFifoBuffer;

    // Audio-thread only: the note currently sounding, so the next beat can turn
    // it off (-1 = none). Channel is captured so the note-off matches.
    int lastMidiNote    = -1;
    int lastMidiChannel = 10;

    // Click synthesis
    float clickDuration = 0.02f; // seconds
    int clickSamplesHi  = 0;
    int clickSamplesLo  = 0;
    int clickCountdown  = 0;
    bool isDownBeatClick = false;
    float clickAmplitude = 0.8f;
    float clickEnvelope  = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetronomeEngine)
};
