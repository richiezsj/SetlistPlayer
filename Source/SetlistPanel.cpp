#include "SetlistPanel.h"

SetlistPanel::SetlistPanel()
{
    titleLabel.setText("SETLIST", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(14.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAABBCC));
    addAndMakeVisible(titleLabel);

    listBox.setModel(this);
    listBox.setMultipleSelectionEnabled(false);
    listBox.onReorder = [this](int from, int to) { reorderSong(from, to); };
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF181828));
    listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xFF334455));
    listBox.setRowHeight(48);
    addAndMakeVisible(listBox);

    addButton.setButtonText("+");
    addButton.onClick = [this] { addSong(); };
    addButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF224433));
    addButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(addButton);

    removeButton.setButtonText("-");
    removeButton.onClick = [this] { removeSong(); };
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF443322));
    removeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(removeButton);

    moveUpButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xb2"));  // ▲
    moveUpButton.onClick = [this] { moveSongUp(); };
    moveUpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A3A));
    moveUpButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(moveUpButton);

    moveDownButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xbc")); // ▼
    moveDownButton.onClick = [this] { moveSongDown(); };
    moveDownButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A3A));
    moveDownButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(moveDownButton);
}

SetlistPanel::~SetlistPanel() {}

void SetlistPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A2A));
    g.setColour(juce::Colour(0xFF334455));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2), 6.0f, 1.0f);
}

void SetlistPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    if (area.getWidth() < 10 || area.getHeight() < 10) return;
    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(4);

    auto buttonRow = area.removeFromBottom(32);
    int bw = juce::jmax(1, buttonRow.getWidth() / 4);
    addButton.setBounds(buttonRow.removeFromLeft(bw).reduced(2, 0));
    removeButton.setBounds(buttonRow.removeFromLeft(bw).reduced(2, 0));
    moveUpButton.setBounds(buttonRow.removeFromLeft(bw).reduced(2, 0));
    moveDownButton.setBounds(buttonRow.reduced(2, 0));

    area.removeFromBottom(4);
    listBox.setBounds(area);
}

void SetlistPanel::setProject(Project* p)
{
    project = p;
    refreshList();
}

void SetlistPanel::refreshList()
{
    listBox.updateContent();
    listBox.repaint();
}

void SetlistPanel::selectRow(int index)
{
    listBox.selectRow(index);
    listBox.scrollToEnsureRowIsOnscreen(index);
}

int SetlistPanel::getSelectedIndex() const
{
    return listBox.getSelectedRow();
}

Song* SetlistPanel::getSelectedSong()
{
    if (project == nullptr) return nullptr;
    int idx = listBox.getSelectedRow();
    if (idx < 0 || idx >= project->songs.size()) return nullptr;
    return &project->songs.getReference(idx);
}

int SetlistPanel::getNumRows()
{
    return project ? project->songs.size() : 0;
}

void SetlistPanel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                    int width, int height, bool rowIsSelected)
{
    if (project == nullptr || rowNumber >= project->songs.size()) return;
    const auto& song = project->songs[rowNumber];

    if (rowIsSelected)
        g.fillAll(juce::Colour(0xFF2A4060));
    else
        g.fillAll(rowNumber % 2 == 0 ? juce::Colour(0xFF1E1E2E) : juce::Colour(0xFF222236));

    // Track number circle
    g.setColour(juce::Colour(0xFF4488FF).withAlpha(0.6f));
    g.fillEllipse(8, (height - 28) / 2.0f, 28, 28);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.0f).boldened());
    g.drawText(juce::String(rowNumber + 1), 8, (height - 28) / 2, 28, 28, juce::Justification::centred);

    // Song name
    g.setFont(juce::Font(14.0f).boldened());
    g.setColour(juce::Colours::white);
    g.drawText(song.name, 46, 4, juce::jmax(1, width - 110), 22, juce::Justification::centredLeft);

    // BPM
    g.setFont(juce::Font(11.0f));
    g.setColour(juce::Colour(0xFF88AACC));
    g.drawText(juce::String(song.bpm, 1) + " BPM  " +
               juce::String(song.beatsPerBar) + "/" + juce::String(song.beatUnit),
               46, 26, juce::jmax(1, width - 110), 16, juce::Justification::centredLeft);

    // Audio icon
    if (song.audioFile.existsAsFile())
    {
        g.setColour(juce::Colour(0xFF44CC88));
        g.setFont(juce::Font(11.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x99\xaa"), width - 26, (height - 20) / 2, 20, 20, juce::Justification::centred); // ♪
    }

    // Separator
    g.setColour(juce::Colour(0xFF334455));
    g.drawLine(0, (float)height - 1, (float)width, (float)height - 1, 0.5f);
}

void SetlistPanel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (onSongSelected) onSongSelected(row);
}

juce::var SetlistPanel::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    // The dragged row index, consumed by SongListBox::itemDropped.
    return rows.size() > 0 ? juce::var(rows[0]) : juce::var();
}

void SetlistPanel::reorderSong(int from, int to)
{
    if (project == nullptr) return;
    if (from < 0 || from >= project->songs.size()) return;

    // getInsertionIndexForPosition returns 0..numRows; once the source row is
    // removed, a target past it shifts down by one.
    to = juce::jlimit(0, project->songs.size(), to);
    if (to > from) --to;
    if (to == from) return;

    project->moveSong(from, to);
    refreshList();
    listBox.selectRow(to);
    if (onSongSelected)   onSongSelected(to);
    if (onSetlistChanged) onSetlistChanged();
}

void SetlistPanel::addSong()
{
    if (project == nullptr) return;
    Song s;
    s.name = "Song " + juce::String(project->songs.size() + 1);
    project->addSong(s);
    refreshList();
    listBox.selectRow(project->songs.size() - 1);
    if (onSongSelected) onSongSelected(project->songs.size() - 1);
    if (onSetlistChanged) onSetlistChanged();
}

void SetlistPanel::removeSong()
{
    if (project == nullptr) return;
    int idx = listBox.getSelectedRow();
    if (idx < 0 || idx >= project->songs.size()) return;
    project->removeSong(idx);
    refreshList();
    int newSel = juce::jmin(idx, project->songs.size() - 1);
    if (newSel >= 0)
    {
        listBox.selectRow(newSel);
        if (onSongSelected) onSongSelected(newSel);
    }
    if (onSetlistChanged) onSetlistChanged();
}

void SetlistPanel::moveSongUp()
{
    if (project == nullptr) return;
    int idx = listBox.getSelectedRow();
    if (idx <= 0) return;
    project->moveSong(idx, idx - 1);
    listBox.selectRow(idx - 1);
    refreshList();
    if (onSetlistChanged) onSetlistChanged();
}

void SetlistPanel::moveSongDown()
{
    if (project == nullptr) return;
    int idx = listBox.getSelectedRow();
    if (idx < 0 || idx >= project->songs.size() - 1) return;
    project->moveSong(idx, idx + 1);
    listBox.selectRow(idx + 1);
    refreshList();
    if (onSetlistChanged) onSetlistChanged();
}
