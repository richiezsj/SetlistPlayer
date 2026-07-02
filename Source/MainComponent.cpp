#include "MainComponent.h"
#include "SetlistPdf.h"

MainComponent::MainComponent()
{
    
    // ---- Build UI first, audio starts at the end ----

    // Command manager
    commandManager.registerAllCommandsForTarget(this);
    addKeyListener(commandManager.getKeyMappings());

    // Menu bar
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);

    // Setlist panel (left)
    setlistPanel = std::make_unique<SetlistPanel>();
    setlistPanel->setProject(&project);
    setlistPanel->onSongSelected  = [this](int idx) { onSongSelected(idx); };
    setlistPanel->onSetlistChanged = [this]
    {
        projectModified = true;
        updateTitle();
        setlistPanel->refreshList();
    };
    addAndMakeVisible(*setlistPanel);

    // Transport panel (centre)
    transportPanel = std::make_unique<TransportPanel>(metronome, audioPlayer);
    transportPanel->onPlay     = [this] { onPlayTriggered(); };
    transportPanel->onStop     = [this] { onStopTriggered(); };
    transportPanel->onPauseResume = [this] { onPauseToggled(); };
    transportPanel->onNextSong = [this] { onNextSong(); };
    addAndMakeVisible(*transportPanel);

    // Auto-advance: when the backing track reaches its end, move the selection
    // to the next song (does not auto-play — the musician presses Play/Space).
    // Posted on the message thread by the engine, so UI access here is safe.
    audioPlayer.onPlaybackFinished = [this] { onNextSong(); };

    // Count-in: the engine calls this on the first real downbeat, after the
    // preparation bars. Start the base then (guard against a stop during the
    // count-in, when the metronome is no longer playing).
    metronome.onCountInFinished = [this]
    {
        if (metronome.isPlaying() && audioPlayer.isFileLoaded())
            audioPlayer.play();
    };

    // Song editor panel (right)
    editorPanel = std::make_unique<SongEditorPanel>();
    editorPanel->onSongChanged = [this]
    {
        projectModified = true;
        updateTitle();
        setlistPanel->refreshList();
        if (auto* s = setlistPanel->getSelectedSong())
        {
            transportPanel->loadSong(s);
            metronome.setBpm(s->bpm);
            metronome.setTimeSignature(s->beatsPerBar, s->beatUnit);
        }
    };
    addAndMakeVisible(*editorPanel);

    updateTitle();
    setWantsKeyboardFocus(true);

    // setSize AFTER all panels exist — triggers resized() safely
    setSize(1100, 700);

    // ---- Start audio AFTER all UI objects exist ----
    deviceManager.initialiseWithDefaultDevices(0, 2);
    sourcePlayer.setSource(&mixerSource);
    deviceManager.addAudioCallback(&sourcePlayer);

    // ---- Wire VU meter callbacks (called on audio thread – lock-free) ----
    mixerSource.onLevelAudio = [this](float rms)
    {
        transportPanel->audioStrip.pushLevel(rms);
    };
    mixerSource.onLevelMetro = [this](float rms)
    {
        transportPanel->metroStrip.pushLevel(rms);
    };

    // Restore persisted mix / count-in (after all UI exists).
    loadSettings();
}

MainComponent::~MainComponent()
{
    saveSettings();

    // Clear level callbacks before audio thread is stopped
    mixerSource.onLevelAudio = nullptr;
    mixerSource.onLevelMetro = nullptr;

    // Clear auto-advance / count-in callbacks so a late notification can't
    // reach a half-destroyed component.
    audioPlayer.onPlaybackFinished = nullptr;
    metronome.onCountInFinished    = nullptr;

    // Stop audio thread before any member is destroyed
    deviceManager.removeAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(nullptr);
    mixerSource.releaseResources();

    // Clear metronome callback before TransportPanel is destroyed
    metronome.onBeat = nullptr;
}


void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0F0F1A));
}

void MainComponent::resized()
{
    if (menuBar == nullptr || setlistPanel == nullptr ||
        editorPanel == nullptr || transportPanel == nullptr)
        return;

    auto area = getLocalBounds();
    menuBar->setBounds(area.removeFromTop(24));

    int leftW  = 280;
    int rightW = 320;

    setlistPanel->setBounds(area.removeFromLeft(leftW).reduced(4));
    editorPanel->setBounds(area.removeFromRight(rightW).reduced(4));
    transportPanel->setBounds(area.reduced(4));
}

// ============================================================
// MenuBarModel
// ============================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    if (menuIndex == 0)
    {
        menu.addCommandItem(&commandManager, newProject);
        menu.addCommandItem(&commandManager, openProject);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, saveProject);
        menu.addCommandItem(&commandManager, saveProjectAs);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, exportPdf);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, audioSetup);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, quitApp);
    }
    else if (menuIndex == 1)
    {
        menu.addCommandItem(&commandManager, addSong);
    }
    else if (menuIndex == 2)
    {
        menu.addCommandItem(&commandManager, aboutApp);
    }
    return menu;
}

void MainComponent::menuItemSelected(int, int) {}

// ============================================================
// Commands
// ============================================================
void MainComponent::getAllCommands(juce::Array<juce::CommandID>& commands)
{
    commands.addArray({ newProject, openProject, saveProject, saveProjectAs, addSong, quitApp, aboutApp, audioSetup, exportPdf });
}

void MainComponent::getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    switch (commandID)
    {
        case newProject:    result.setInfo("New Project",    "Create new project",         "File", 0);
                            result.addDefaultKeypress('N', juce::ModifierKeys::commandModifier); break;
        case openProject:   result.setInfo("Open Project...", "Open project file",         "File", 0);
                            result.addDefaultKeypress('O', juce::ModifierKeys::commandModifier); break;
        case saveProject:   result.setInfo("Save Project",   "Save current project",       "File", 0);
                            result.addDefaultKeypress('S', juce::ModifierKeys::commandModifier); break;
        case saveProjectAs: result.setInfo("Save As...",     "Save project with new name", "File", 0);
                            result.addDefaultKeypress('S', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier); break;
        case addSong:       result.setInfo("Add Song",       "Add a new song to setlist",  "Edit", 0);
                            result.addDefaultKeypress('T', juce::ModifierKeys::commandModifier); break;
        case quitApp:       result.setInfo("Quit",           "Quit application",           "File", 0);
                            result.addDefaultKeypress('Q', juce::ModifierKeys::commandModifier); break;
        case aboutApp:      result.setInfo("About SetlistPlayer...", "Show application info", "Help", 0); break;
        case audioSetup:    result.setInfo("Audio Setup...", "Configure audio device and sample rate", "File", 0);
                            result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier); break;
        case exportPdf:     result.setInfo("Export PDF...", "Export the setlist as a PDF", "File", 0);
                            result.addDefaultKeypress('E', juce::ModifierKeys::commandModifier); break;
        default: break;
    }
}

bool MainComponent::perform(const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    switch (info.commandID)
    {
        case newProject:    cmdNewProject();    return true;
        case openProject:   cmdOpenProject();   return true;
        case saveProject:   cmdSaveProject();   return true;
        case saveProjectAs: cmdSaveProjectAs(); return true;
        case addSong:
        {
            setlistPanel->onSongSelected = nullptr;
            Song s; s.name = "Song " + juce::String(project.songs.size() + 1);
            project.addSong(s);
            setlistPanel->refreshList();
            projectModified = true;
            updateTitle();
            setlistPanel->onSongSelected = [this](int idx) { onSongSelected(idx); };
            return true;
        }
        case quitApp:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;
        case aboutApp:
            cmdAbout();
            return true;
        case audioSetup:
            cmdAudioSetup();
            return true;
        case exportPdf:
            cmdExportPdf();
            return true;
        default: return false;
    }
}

// ============================================================
// Persistent settings (mix + count-in), global across projects
// ============================================================
void MainComponent::loadSettings()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "SetlistPlayer";
    opts.filenameSuffix      = "settings";
    opts.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters(opts);

    auto* p = appProperties.getUserSettings();
    if (p == nullptr) return;

    // Apply through the (public) channel controls with notification so both the
    // UI and the engines pick up the restored values.
    auto& a = transportPanel->audioStrip;
    a.volSlider.setValue (p->getDoubleValue("baseGain", 1.0), juce::sendNotificationSync);
    a.panKnob.setValue   (p->getDoubleValue("basePan",  0.0), juce::sendNotificationSync);
    a.muteButton.setToggleState(p->getBoolValue("baseMute", false), juce::sendNotificationSync);

    auto& m = transportPanel->metroStrip;
    m.volSlider.setValue (p->getDoubleValue("clickGain", 1.0), juce::sendNotificationSync);
    m.panKnob.setValue   (p->getDoubleValue("clickPan",  0.0), juce::sendNotificationSync);
    m.muteButton.setToggleState(p->getBoolValue("clickMute", false), juce::sendNotificationSync);

    metronome.setCountInBars(p->getIntValue("countInBars", 0));
    metronomeOutput.refreshCountIn();

    // MIDI output: mode / device / channel / notes. restoreState falls back to
    // the internal click if the saved device is no longer present.
    metronomeOutput.restoreState(
        p->getIntValue("midiMode", 1),
        p->getValue   ("midiDeviceId",   {}),
        p->getValue   ("midiDeviceName", {}),
        p->getIntValue("midiChannel",  metronome.getMidiChannel()),
        p->getIntValue("midiNoteDown", metronome.getMidiNoteDown()),
        p->getIntValue("midiNoteBeat", metronome.getMidiNoteBeat()));
}

void MainComponent::saveSettings()
{
    auto* p = appProperties.getUserSettings();
    if (p == nullptr) return;

    p->setValue("baseGain",  audioPlayer.getGain());
    p->setValue("basePan",   audioPlayer.getPan());
    p->setValue("baseMute",  audioPlayer.isMuted());
    p->setValue("clickGain", metronome.getGain());
    p->setValue("clickPan",  metronome.getPan());
    p->setValue("clickMute", metronome.isMuted());
    p->setValue("countInBars", metronome.getCountInBars());

    p->setValue("midiMode",       metronomeOutput.getMidiMode());
    p->setValue("midiDeviceId",   metronomeOutput.getSelectedDeviceIdentifier());
    p->setValue("midiDeviceName", metronomeOutput.getSelectedDeviceName());
    p->setValue("midiChannel",    metronome.getMidiChannel());
    p->setValue("midiNoteDown",   metronome.getMidiNoteDown());
    p->setValue("midiNoteBeat",   metronome.getMidiNoteBeat());

    p->saveIfNeeded();
}

// ============================================================
// Project management
// ============================================================
bool MainComponent::canDiscardCurrentProject()
{
    if (!projectModified) return true;

    // showYesNoCancelBox: 1 = Yes (first button), 2 = No (middle), 0 = Cancel.
    auto result = juce::NativeMessageBox::showYesNoCancelBox(
        juce::MessageBoxIconType::QuestionIcon, "Unsaved Changes",
        "Save changes to the current project before continuing?",
        nullptr, nullptr);

    if (result == 1) { cmdSaveProject(); return true; }  // Yes  → save, proceed
    if (result == 2) return true;                        // No   → discard, proceed
    return false;                                        // Cancel → abort
}

void MainComponent::cmdNewProject()
{
    if (!canDiscardCurrentProject()) return;

    onStopTriggered();
    project = Project();
    selectedSongIndex = -1;
    projectModified = false;
    setlistPanel->setProject(&project);
    editorPanel->clearSong();
    transportPanel->clearSong();
    updateTitle();
}

void MainComponent::cmdOpenProject()
{
    if (!canDiscardCurrentProject()) return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Open Setlist Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.setlist");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.isEmpty()) return;

            onStopTriggered();
            if (project.load(results[0]))
            {
                selectedSongIndex = -1;
                projectModified   = false;
                setlistPanel->setProject(&project);
                editorPanel->clearSong();
                transportPanel->clearSong();
                updateTitle();

                // Warn if any referenced audio file is missing on disk.
                auto missing = project.missingAudioFiles();
                if (! missing.isEmpty())
                    juce::NativeMessageBox::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Missing audio files",
                        "The backing track could not be found for:\n\n  "
                            + missing.joinIntoString("\n  ")
                            + "\n\nThose songs will play without audio until relinked.",
                        this);
            }
        });
}

void MainComponent::cmdSaveProject()
{
    if (!project.projectFile.existsAsFile())
    {
        cmdSaveProjectAs();
        return;
    }
    project.save(project.projectFile);
    projectModified = false;
    updateTitle();
}

void MainComponent::cmdSaveProjectAs()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Setlist Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(project.projectName + ".setlist"),
        "*.setlist");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.isEmpty()) return;

            auto file = results[0].withFileExtension("setlist");
            project.projectName = file.getFileNameWithoutExtension();
            project.save(file);
            projectModified = false;
            updateTitle();
        });
}

void MainComponent::updateTitle()
{
    juce::String title = "SetlistPlayer - " + project.projectName;
    if (projectModified) title += " *";
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName(title);
}

// ============================================================
// Song selection & playback
// ============================================================
void MainComponent::onSongSelected(int index)
{
    selectedSongIndex = index;
    Song* song = setlistPanel->getSelectedSong();
    if (song == nullptr)
    {
        editorPanel->clearSong();
        transportPanel->clearSong();
        return;
    }

    editorPanel->loadSong(&project, index);   // pass project + index, not raw pointer
    transportPanel->loadSong(song);

    metronome.setBpm(song->bpm);
    metronome.setTimeSignature(song->beatsPerBar, song->beatUnit);

    onStopTriggered();

    if (song->audioFile.existsAsFile())
        audioPlayer.loadFile(song->audioFile);
    else
        audioPlayer.unloadFile();
}

void MainComponent::onPlayTriggered()
{
    Song* song = setlistPanel->getSelectedSong();
    if (song == nullptr) return;

    // A fresh Play always starts from the top, clearing any paused state.
    paused = false;
    if (transportPanel) transportPanel->setPaused(false);

    metronome.setBpm(song->bpm);
    metronome.setTimeSignature(song->beatsPerBar, song->beatUnit);
    metronome.start();

    // With a count-in the base is started by onCountInFinished; otherwise now.
    if (metronome.getCountInBars() == 0 && audioPlayer.isFileLoaded())
        audioPlayer.play();
}

void MainComponent::onStopTriggered()
{
    metronome.stop();
    metronome.reset();
    audioPlayer.stop();

    if (paused)
    {
        paused = false;
        if (transportPanel) transportPanel->setPaused(false);
    }
}

void MainComponent::onPauseToggled()
{
    if (!paused)
    {
        // Nothing to pause unless the metronome (and thus playback) is running.
        if (!metronome.isPlaying()) return;

        baseWasPlayingAtPause = audioPlayer.isPlaying();
        metronome.pause();
        audioPlayer.pause();
        paused = true;
    }
    else
    {
        metronome.resume();
        if (baseWasPlayingAtPause) audioPlayer.resume();
        paused = false;
    }

    if (transportPanel) transportPanel->setPaused(paused);
}

void MainComponent::parentHierarchyChanged()
{
    if (auto* tlw = getTopLevelComponent())
        tlw->addKeyListener(&spaceListener);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        if (metronome.isPlaying())
            onStopTriggered();
        else
            onPlayTriggered();
        return true;
    }
    return false;
}

void MainComponent::onNextSong()
{
    onStopTriggered();
    int next = selectedSongIndex + 1;
    if (next < project.songs.size())
    {
        setlistPanel->selectRow(next);   // visually highlight in the list
        onSongSelected(next);
    }
}

// ============================================================
// About window
// ============================================================
class AboutWindow : public juce::DocumentWindow
{
public:
    AboutWindow()
        : DocumentWindow("About SetlistPlayer",
                         juce::Colour(0xFF0F0F1A),
                         DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(false);
        setTitleBarButtonsRequired(DocumentWindow::closeButton, false);
        setResizable(false, false);
        setContentOwned(new AboutContent(), true);
        centreWithSize(420, 340);
        setVisible(true);
    }

    void closeButtonPressed() override { delete this; }

private:
    // ---------------------------------------------------------
    struct AboutContent : public juce::Component
    {
        AboutContent()
        {
            setSize(420, 340);
        }

        void paint(juce::Graphics& g) override
        {
            // Background
            g.fillAll(juce::Colour(0xFF0F0F1A));

            // Top accent bar
            g.setColour(juce::Colour(0xFF4488FF));
            g.fillRect(0, 0, getWidth(), 4);

            // App name
            g.setFont(juce::Font(28.0f).boldened());
            g.setColour(juce::Colours::white);
            g.drawText("SetlistPlayer", 0, 24, getWidth(), 36,
                       juce::Justification::centred);

            // Version
            g.setFont(juce::Font(13.0f).italicised());
            g.setColour(juce::Colour(0xFF4488FF));
            g.drawText("Version 1.0.0", 0, 64, getWidth(), 20,
                       juce::Justification::centred);

            // Divider
            g.setColour(juce::Colour(0xFF334455));
            g.drawLine(40.0f, 94.0f, (float)(getWidth() - 40), 94.0f, 1.0f);

            // Author block
            g.setFont(juce::Font(12.0f).boldened());
            g.setColour(juce::Colour(0xFFAABBCC));
            g.drawText("Sviluppato da", 0, 106, getWidth(), 18,
                       juce::Justification::centred);

            g.setFont(juce::Font(16.0f).boldened());
            g.setColour(juce::Colours::white);
            g.drawText("Il tuo nome / Studio", 0, 128, getWidth(), 22,
                       juce::Justification::centred);

            g.setFont(juce::Font(11.0f));
            g.setColour(juce::Colour(0xFF7788AA));
            g.drawText("tua@email.com", 0, 154, getWidth(), 18,
                       juce::Justification::centred);

            // Divider
            g.setColour(juce::Colour(0xFF334455));
            g.drawLine(40.0f, 182.0f, (float)(getWidth() - 40), 182.0f, 1.0f);

            // Description
            g.setFont(juce::Font(11.5f));
            g.setColour(juce::Colour(0xFF8899AA));
            g.drawText("Live setlist manager con metronomo e backing track",
                       0, 194, getWidth(), 18, juce::Justification::centred);
            g.drawText("Costruito con JUCE 8   |   C++17   |   macOS",
                       0, 214, getWidth(), 18, juce::Justification::centred);

            // Divider
            g.setColour(juce::Colour(0xFF334455));
            g.drawLine(40.0f, 242.0f, (float)(getWidth() - 40), 242.0f, 1.0f);

            // Copyright
            g.setFont(juce::Font(10.5f));
            g.setColour(juce::Colour(0xFF556677));
            g.drawText("Copyright " + juce::String::fromUTF8("\xc2\xa9")
                       + " 2025 Il tuo nome. Tutti i diritti riservati.",
                       0, 254, getWidth(), 16, juce::Justification::centred);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (auto* w = findParentComponentOfClass<AboutWindow>())
                delete w;
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutWindow)
};

void MainComponent::cmdAbout()
{
    new AboutWindow();   // self-deleting on close
}
// ============================================================
// Audio Setup window
// ============================================================
//
// Holds the audio device selector with the metronome output controls
// docked in a section right below it. The metronome panel is owned by
// MainComponent (it keeps the MIDI device open while this window is
// closed), so it is added as a non-owned child and detached on teardown.
class AudioSetupContent : public juce::Component
{
public:
    AudioSetupContent(juce::AudioDeviceManager& dm, MetronomeOutputPanel& metroOut)
        : metronomeOutput(metroOut)
    {
        // AudioDeviceSelectorComponent(deviceManager,
        //   minInputChannels, maxInputChannels,
        //   minOutputChannels, maxOutputChannels,
        //   showMidiInputOptions, showMidiOutputSelector,
        //   showChannelsAsStereoPairs, hideAdvancedOptionsWithButton)
        selector.reset(new juce::AudioDeviceSelectorComponent(
            dm,
            0, 0,       // no inputs needed
            2, 2,       // stereo output
            false,      // no MIDI input list (handled separately)
            false,      // no MIDI output list
            true,       // show stereo pairs
            false));    // show all options immediately
        addAndMakeVisible(selector.get());

        addAndMakeVisible(metronomeOutput);  // not owned — lives in MainComponent

        setSize(500, selectorHeight + gap + MetronomeOutputPanel::preferredHeight);
    }

    ~AudioSetupContent() override
    {
        // Detach the shared panel so it survives this window being deleted.
        removeChildComponent(&metronomeOutput);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        // Audio device selector docked at the top at its natural height,
        // metronome controls immediately below it (no stretched gap).
        selector->setBounds(area.removeFromTop(selectorHeight));
        area.removeFromTop(gap);
        metronomeOutput.setBounds(area.reduced(12, 0));
    }

private:
    // Output-only selector needs ~3 rows (output / sample rate / buffer).
    static constexpr int selectorHeight = 150;
    static constexpr int gap            = 8;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    MetronomeOutputPanel& metronomeOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSetupContent)
};

class AudioSetupWindow : public juce::DocumentWindow
{
public:
    AudioSetupWindow(juce::AudioDeviceManager& dm, MetronomeOutputPanel& metroOut)
        : DocumentWindow("Audio Setup",
                         juce::Colour(0xFF0F0F1A),
                         DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);

        auto* content = new AudioSetupContent(dm, metroOut);
        setContentOwned(content, true);
        centreWithSize(content->getWidth()  + 8,
                       content->getHeight() + getTitleBarHeight() + 8);
        setVisible(true);
    }

    void closeButtonPressed() override { delete this; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSetupWindow)
};

void MainComponent::cmdAudioSetup()
{
    new AudioSetupWindow(deviceManager, metronomeOutput);  // self-deleting on close
}

void MainComponent::cmdExportPdf()
{
    auto suggestedName = (project.projectName.isNotEmpty() ? project.projectName : "Setlist")
                             + ".pdf";
    auto startDir = project.projectFile.existsAsFile()
                        ? project.projectFile.getParentDirectory()
                        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Setlist as PDF", startDir.getChildFile(suggestedName), "*.pdf");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                         juce::FileBrowserComponent::canSelectFiles |
                         juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.hasFileExtension("pdf")) file = file.withFileExtension("pdf");

            if (! setlistpdf::write(project, file))
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon, "Export failed",
                    "Could not write the PDF to:\n" + file.getFullPathName(), this);
        });
}
