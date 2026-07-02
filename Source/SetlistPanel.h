#pragma once
#include <JuceHeader.h>
#include "Project.h"

class SetlistPanel : public juce::Component,
                     public juce::ListBoxModel,
                     public juce::DragAndDropContainer
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
    // ListBox that accepts a dropped row to reorder the setlist. The drag is
    // started by the ListBoxModel's getDragSourceDescription (the row index).
    struct SongListBox : public juce::ListBox,
                         public juce::DragAndDropTarget
    {
        std::function<void(int from, int to)> onReorder;

        bool isInterestedInDragSource(const SourceDetails& d) override
        {
            return d.description.isInt();
        }
        void itemDragMove(const SourceDetails&) override {}
        void itemDropped(const SourceDetails& d) override
        {
            const int from = (int) d.description;
            const int to   = getInsertionIndexForPosition(d.localPosition.x,
                                                          d.localPosition.y);
            if (onReorder) onReorder(from, to);
        }
    };

    Project* project = nullptr;

    juce::Label titleLabel;
    SongListBox listBox;
    juce::TextButton addButton, removeButton, moveUpButton, moveDownButton;

    void addSong();
    void removeSong();
    void moveSongUp();
    void moveSongDown();
    void reorderSong(int from, int to);   // drag-and-drop reorder

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetlistPanel)
};
