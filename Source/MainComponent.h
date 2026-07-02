#pragma once
#include <atomic>
#include <JuceHeader.h>
#include "AppLookAndFeel.h"
#include "Project.h"
#include "MetronomeEngine.h"
#include "AudioPlayerEngine.h"
#include "SetlistPanel.h"
#include "SongEditorPanel.h"
#include "TransportPanel.h"
#include "MetronomeOutputPanel.h"

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::ApplicationCommandTarget
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void parentHierarchyChanged() override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform(const juce::ApplicationCommandTarget::InvocationInfo& info) override;

    // Prompts to save when there are unsaved changes. Returns true if the
    // caller may proceed (nothing to save, saved, or user chose to discard),
    // false if the user cancelled. Used by New / Open / Quit.
    bool canDiscardCurrentProject();

private:
    enum CommandIDs
    {
        newProject    = 0x2000,
        openProject   = 0x2001,
        saveProject   = 0x2002,
        saveProjectAs = 0x2003,
        addSong       = 0x2004,
        quitApp       = 0x2005,
        aboutApp      = 0x2006,
        audioSetup    = 0x2007,
        exportPdf     = 0x2008,
    };

    void cmdNewProject();
    void cmdOpenProject();
    void cmdSaveProject();
    void cmdSaveProjectAs();
    void cmdAbout();
    void cmdAudioSetup();
    void cmdExportPdf();
    void onSongSelected(int index);
    void onPlayTriggered();
    void onStopTriggered();
    void onPauseToggled();
    void onNextSong();
    void updateTitle();

    // Persist the mix (per-channel gain/pan/mute) and count-in across runs, in
    // a global settings file (not per-project). MIDI device/mode is not stored
    // because device availability varies between launches.
    void loadSettings();
    void saveSettings();
    juce::ApplicationProperties appProperties;

    // Forwards space key events from the top-level window back here,
    // so space works even when a child (slider, button...) has focus
    struct SpaceKeyListener : public juce::KeyListener
    {
        MainComponent& owner;
        explicit SpaceKeyListener(MainComponent& o) : owner(o) {}
        bool keyPressed(const juce::KeyPress& k, juce::Component*) override
        {
            return owner.keyPressed(k);
        }
    };
    SpaceKeyListener spaceListener { *this };

    // Flat macOS-style look, applied app-wide. Declared first so it is
    // constructed before, and destroyed after, every component that uses it.
    AppLookAndFeel lookAndFeel;

    // -------------------------------------------------------
    // Data
    // -------------------------------------------------------
    Project project;
    int  selectedSongIndex = -1;
    bool projectModified   = false;

    // Pause/resume state. baseWasPlayingAtPause remembers whether the backing
    // track was running, so resume only restarts it if it was (e.g. not during
    // a count-in, where the base has not started yet).
    bool paused = false;
    bool baseWasPlayingAtPause = false;

    // -------------------------------------------------------
    // Audio — declared before MixerSource so they are
    // constructed first and destroyed last
    // -------------------------------------------------------
    juce::AudioDeviceManager deviceManager;
    MetronomeEngine           metronome;
    AudioPlayerEngine         audioPlayer;

    // Long-lived metronome output controls (mode / MIDI device / notes).
    // Owns the open MidiOutput, so it must outlive the Audio Setup window
    // where it is shown. Declared after `metronome` so it is destroyed
    // before the engine it references.
    MetronomeOutputPanel      metronomeOutput { metronome };

    // Simple mixer: sums metronome + audio player.
    // Pre-allocates its temp buffer in prepareToPlay so the
    // audio thread never allocates memory.
    struct MixerSource : public juce::AudioSource
    {
        MixerSource(MetronomeEngine& m, AudioPlayerEngine& a)
            : metro(m), audio(a) {}

        void prepareToPlay(int blockSize, double sr) override
        {
            metro.prepareToPlay(blockSize, sr);
            audio.prepareToPlay(blockSize, sr);
            tempBuffer.setSize(2, blockSize, false, true, true);
        }

        void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
        {
            if (info.buffer == nullptr) return;
            info.clearActiveBufferRegion();

            const int numCh      = info.buffer->getNumChannels();
            const int numSamples = info.numSamples;

            if (numCh <= 0 || numSamples <= 0) return;

            if (tempBuffer.getNumSamples() < numSamples || tempBuffer.getNumChannels() < numCh)
                tempBuffer.setSize(numCh, numSamples, false, true, true);

            // --- audio player ---
            tempBuffer.clear(0, numSamples);
            juce::AudioSourceChannelInfo tmp(&tempBuffer, 0, numSamples);
            audio.getNextAudioBlock(tmp);
            if (onLevelAudio)
            {
                float rms = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                    rms += tempBuffer.getRMSLevel(ch, 0, numSamples);
                onLevelAudio(rms / (float)juce::jmax(1, numCh));
            }
            for (int ch = 0; ch < numCh; ++ch)
                info.buffer->addFrom(ch, info.startSample, tempBuffer, ch, 0, numSamples);

            // --- metronome ---
            tempBuffer.clear(0, numSamples);
            metro.getNextAudioBlock(tmp);
            if (onLevelMetro)
            {
                float rms = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                    rms += tempBuffer.getRMSLevel(ch, 0, numSamples);
                onLevelMetro(rms / (float)juce::jmax(1, numCh));
            }
            for (int ch = 0; ch < numCh; ++ch)
                info.buffer->addFrom(ch, info.startSample, tempBuffer, ch, 0, numSamples);
        }

        void releaseResources() override
        {
            metro.releaseResources();
            audio.releaseResources();
            tempBuffer.setSize(0, 0);
        }

        MetronomeEngine&   metro;
        AudioPlayerEngine& audio;
        juce::AudioBuffer<float> tempBuffer;

        // Set these before audio starts – called on audio thread, must be lock-free
        std::function<void(float)> onLevelAudio;
        std::function<void(float)> onLevelMetro;
    };

    MixerSource              mixerSource  { metronome, audioPlayer };
    juce::AudioSourcePlayer  sourcePlayer;

    // -------------------------------------------------------
    // UI — declared after audio so they are destroyed first
    // -------------------------------------------------------
    juce::ApplicationCommandManager          commandManager;
    std::unique_ptr<juce::MenuBarComponent>  menuBar;
    std::unique_ptr<SetlistPanel>            setlistPanel;
    std::unique_ptr<SongEditorPanel>         editorPanel;
    std::unique_ptr<TransportPanel>          transportPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
