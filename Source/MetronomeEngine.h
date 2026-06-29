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

    // MIDI out
    void setMidiOutput(juce::MidiOutput* out) { midiOut = out; }
    void setUseMidi(bool b)     { useMidi     = b; }
    void setUseInternal(bool b) { useInternal = b; }

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

    double sampleRate     = 44100.0;
    double bpm            = 120.0;
    int numerator         = 4;
    int denominator       = 4;

    double samplesPerBeat = 0.0;
    double sampleCounter  = 0.0;
    int currentBeat       = 0;
    int currentBar        = 0;

    std::atomic<bool> playing { false };
    bool useMidi     = false;
    bool useInternal = true;
    int  midiChannel  = 10;   // GM percussion channel
    int  midiNoteDown = 41;   // F2  — downbeat
    int  midiNoteBeat = 41;   // F2  — other beats (same default)
    float gain  = 1.0f;
    float pan   = 0.0f;
    bool  muted = false;

    juce::MidiOutput* midiOut = nullptr;

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
