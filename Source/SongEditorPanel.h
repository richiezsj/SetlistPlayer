#pragma once
#include <JuceHeader.h>
#include "Project.h"

class SongEditorPanel : public juce::Component
{
public:
    SongEditorPanel();
    ~SongEditorPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Pass the project and index instead of a raw pointer
    void loadSong(Project* proj, int index);
    void clearSong();

    std::function<void()> onSongChanged;

private:
    Project* currentProject = nullptr;
    int      currentIndex   = -1;

    // Helper — always fetches fresh from array
    Song* song() const
    {
        if (currentProject == nullptr || currentIndex < 0
            || currentIndex >= currentProject->songs.size())
            return nullptr;
        return &currentProject->songs.getReference(currentIndex);
    }

    juce::Label titleLabel;
    juce::Label nameLabel, bpmLabel, tsLabel, keyLabel, notesLabel, audioLabel;
    juce::TextEditor nameEditor, keyEditor, notesEditor;
    juce::Slider bpmSlider;
    juce::TextButton loadAudioButton, clearAudioButton;
    juce::Label audioFileLabel;
    juce::ComboBox tsNumerator, tsDenominator;
    juce::Label sectionLabel;

    void nameChanged();
    void bpmChanged();
    void loadAudioClicked();
    void clearAudioClicked();
    void tsChanged();
    void keyChanged();
    void notesChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongEditorPanel)
};
