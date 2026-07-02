#pragma once
#include <atomic>
#include <JuceHeader.h>

class MetronomeEngine : public juce::AudioSource
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

    // MIDI note configuration
    void setMidiChannel(int ch)      { midiChannel  = juce::jlimit(1, 16, ch); }
    void setMidiNoteDown(int note)   { midiNoteDown = juce::jlimit(0, 127, note); }
    void setMidiNoteBeat(int note)   { midiNoteBeat = juce::jlimit(0, 127, note); }
    int  getMidiChannel()  const { return midiChannel; }
    int  getMidiNoteDown() const { return midiNoteDown; }
    int  getMidiNoteBeat() const { return midiNoteBeat; }

    // Callbacks
    std::function<void(int beat, int bar)> onBeat;

private:
    void generateClick(juce::AudioBuffer<float>& buffer, int sample, bool isDownBeat);
    void generateMidiBeat(juce::MidiBuffer& midi, int sampleOffset, bool isDownBeat);

    double sampleRate     = 44100.0;          // audio-thread owned (set in prepareToPlay)
    std::atomic<double> bpm         { 120.0 };
    std::atomic<int>    numerator   { 4 };
    std::atomic<int>    denominator { 4 };

    // Audio-thread only
    double samplesPerBeat = 0.0;
    double sampleCounter  = 0.0;
    int currentBeat       = 0;
    int currentBar        = 0;

    std::atomic<bool>  playing     { false };
    std::atomic<bool>  useMidi     { false };
    std::atomic<bool>  useInternal { true };
    std::atomic<int>   midiChannel  { 10 };   // GM percussion channel
    std::atomic<int>   midiNoteDown { 41 };   // F2 — downbeat
    std::atomic<int>   midiNoteBeat { 41 };   // F2 — other beats (same default)
    std::atomic<float> gain  { 1.0f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  muted { false };

    // MIDI output device is owned here so it outlives any UI panel that
    // configures it. Swapped under midiLock; the audio thread touches it only
    // via a try-lock, so it never blocks and never dereferences a freed ptr.
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::CriticalSection midiLock;

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
