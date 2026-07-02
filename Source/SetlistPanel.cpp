#include "SetlistPanel.h"

SetlistPanel::SetlistPanel()
{
    titleLabel.setText("SETLIST", juce::dontSendNotification);
    titleLabel.setFont(Theme::sectionFont());
    titleLabel.setColour(juce::Label::textColourId, Theme::textTertiary);
    addAndMakeVisible(titleLabel);

    listBox.setModel(this);
    listBox.setMultipleSelectionEnabled(false);
    listBox.onReorder = [this](int from, int to) { reorderSong(from, to); };
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    listBox.setColour(juce::ListBox::outlineColourId,    juce::Colours::transparentBlack);
    listBox.setRowHeight(52);
    addAndMakeVisible(listBox);

    addButton.setButtonText("+");
    addButton.onClick = [this] { addSong(); };
    addAndMakeVisible(addButton);

    removeButton.setButtonText(juce::String::fromUTF8("\xe2\x88\x92"));  // −
    removeButton.onClick = [this] { removeSong(); };
    addAndMakeVisible(removeButton);

    moveUpButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xb2"));  // ▲
    moveUpButton.onClick = [this] { moveSongUp(); };
    addAndMakeVisible(moveUpButton);

    moveDownButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xbc")); // ▼
    moveDownButton.onClick = [this] { moveSongDown(); };
    addAndMakeVisible(moveDownButton);
}

SetlistPanel::~SetlistPanel() {}

void SetlistPanel::paint(juce::Graphics& g)
{
    g.setColour(Theme::panelBg);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), Theme::radiusLarge);
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

    auto bounds = juce::Rectangle<int>(0, 0, width, height);

    // Selection: a filled accent capsule inset from the edges (macOS sidebar).
    if (rowIsSelected)
    {
        g.setColour(Theme::accent);
        g.fillRoundedRectangle(bounds.reduced(6, 4).toFloat(), Theme::radius);
    }

    const auto nameColour = rowIsSelected ? juce::Colours::white : Theme::textPrimary;
    const auto subColour  = rowIsSelected ? juce::Colours::white.withAlpha(0.85f)
                                          : Theme::textSecondary;

    // Track-number index — quiet, not a saturated badge.
    g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.85f) : Theme::textTertiary);
    g.setFont(Theme::font(12.0f));
    g.drawText(juce::String(rowNumber + 1), 14, 0, 24, height, juce::Justification::centredLeft);

    const int textX = 44;
    const int textW = juce::jmax(1, width - textX - 28);

    // Song name
    g.setColour(nameColour);
    g.setFont(Theme::fontBold(14.0f));
    g.drawText(song.name, textX, 8, textW, 20, juce::Justification::centredLeft);

    // Meta line: BPM · time signature · key
    juce::String meta = juce::String(song.bpm, 1) + " BPM   \xc2\xb7   "
                      + juce::String(song.beatsPerBar) + "/" + juce::String(song.beatUnit);
    if (song.key.isNotEmpty()) meta += "   \xc2\xb7   " + song.key;
    g.setColour(subColour);
    g.setFont(Theme::font(11.5f));
    g.drawText(meta, textX, height - 26, textW, 18, juce::Justification::centredLeft);

    // Audio-present indicator
    if (song.audioFile.existsAsFile())
    {
        g.setColour(rowIsSelected ? juce::Colours::white : Theme::textSecondary);
        g.setFont(Theme::font(12.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x99\xaa"), width - 26, 0, 18, height,
                   juce::Justification::centred); // ♪
    }

    // Hairline separator between rows (not under the selected capsule)
    if (! rowIsSelected)
    {
        g.setColour(Theme::separator);
        g.fillRect(textX, height - 1, width - textX - 8, 1);
    }
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
