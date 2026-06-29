#pragma once
#include <JuceHeader.h>
#include "Project.h"

class SetlistPanel : public juce::Component,
                     public juce::ListBoxModel
{
public:
    SetlistPanel();
    ~SetlistPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setProject(Project* project);
    void refreshList();
    void selectRow(int index);

    int getSelectedIndex() const;
    Song* getSelectedSong();

    std::function<void(int index)> onSongSelected;
    std::function<void()> onSetlistChanged;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                          int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override {}
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override;

private:
    Project* project = nullptr;

    juce::Label titleLabel;
    juce::ListBox listBox;
    juce::TextButton addButton, removeButton, moveUpButton, moveDownButton;

    void addSong();
    void removeSong();
    void moveSongUp();
    void moveSongDown();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetlistPanel)
};
