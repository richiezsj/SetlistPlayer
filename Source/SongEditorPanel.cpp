#include "SongEditorPanel.h"

SongEditorPanel::SongEditorPanel()
{
    sectionLabel.setText("SONG EDITOR", juce::dontSendNotification);
    sectionLabel.setFont(juce::Font(14.0f).boldened());
    sectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAABBCC));
    addAndMakeVisible(sectionLabel);

    nameLabel.setText("Song Name:", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(nameLabel);

    nameEditor.setMultiLine(false);
    nameEditor.setReturnKeyStartsNewLine(false);
    nameEditor.onTextChange = [this] { nameChanged(); };
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A3A));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF445566));
    addAndMakeVisible(nameEditor);

    bpmLabel.setText("BPM:", juce::dontSendNotification);
    bpmLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(bpmLabel);

    bpmSlider.setRange(20.0, 300.0, 0.1);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 24);
    bpmSlider.onValueChange = [this] { bpmChanged(); };
    bpmSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFF4488FF));
    bpmSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF334455));
    addAndMakeVisible(bpmSlider);

    tsLabel.setText("Time Signature:", juce::dontSendNotification);
    tsLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(tsLabel);

    for (int i = 1; i <= 12; ++i) tsNumerator.addItem(juce::String(i), i);
    tsNumerator.setSelectedId(4, juce::dontSendNotification);
    tsNumerator.onChange = [this] { tsChanged(); };
    tsNumerator.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A2A3A));
    addAndMakeVisible(tsNumerator);

    for (int d : { 2, 4, 8, 16 }) tsDenominator.addItem(juce::String(d), d);
    tsDenominator.setSelectedId(4, juce::dontSendNotification);
    tsDenominator.onChange = [this] { tsChanged(); };
    tsDenominator.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2A2A3A));
    addAndMakeVisible(tsDenominator);

    keyLabel.setText("Key:", juce::dontSendNotification);
    keyLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(keyLabel);

    keyEditor.setMultiLine(false);
    keyEditor.setReturnKeyStartsNewLine(false);
    keyEditor.onTextChange = [this] { keyChanged(); };
    keyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A3A));
    keyEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    keyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF445566));
    addAndMakeVisible(keyEditor);

    notesLabel.setText("Notes:", juce::dontSendNotification);
    notesLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(notesLabel);

    notesEditor.setMultiLine(true);
    notesEditor.setReturnKeyStartsNewLine(true);
    notesEditor.onTextChange = [this] { notesChanged(); };
    notesEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A3A));
    notesEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    notesEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF445566));
    addAndMakeVisible(notesEditor);

    audioLabel.setText("Audio Track:", juce::dontSendNotification);
    audioLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(audioLabel);

    audioFileLabel.setText("No file loaded", juce::dontSendNotification);
    audioFileLabel.setFont(juce::Font(11.0f).italicised());
    audioFileLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8899AA));
    audioFileLabel.setJustificationType(juce::Justification::centredLeft);
    audioFileLabel.setMinimumHorizontalScale(0.5f);
    addAndMakeVisible(audioFileLabel);

    loadAudioButton.setButtonText("Load Audio");
    loadAudioButton.onClick = [this] { loadAudioClicked(); };
    loadAudioButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF334466));
    loadAudioButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(loadAudioButton);

    clearAudioButton.setButtonText("Clear");
    clearAudioButton.onClick = [this] { clearAudioClicked(); };
    clearAudioButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF553333));
    clearAudioButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(clearAudioButton);

    clearSong();
}

SongEditorPanel::~SongEditorPanel() {}

void SongEditorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));
    g.setColour(juce::Colour(0xFF334455));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2), 6.0f, 1.0f);
}

void SongEditorPanel::resized()
{
    auto area = getLocalBounds().reduced(12);
    if (area.getWidth() < 10 || area.getHeight() < 10) return;
    int row = 28, gap = 8;

    sectionLabel.setBounds(area.removeFromTop(row));
    area.removeFromTop(gap);

    nameLabel.setBounds(area.removeFromTop(16));
    nameEditor.setBounds(area.removeFromTop(row));
    area.removeFromTop(gap);

    bpmLabel.setBounds(area.removeFromTop(16));
    bpmSlider.setBounds(area.removeFromTop(row));
    area.removeFromTop(gap);

    tsLabel.setBounds(area.removeFromTop(16));
    auto tsRow = area.removeFromTop(row);
    tsNumerator.setBounds(tsRow.removeFromLeft(80));
    tsRow.removeFromLeft(8);
    tsRow.removeFromLeft(20);
    tsDenominator.setBounds(tsRow.removeFromLeft(80));
    area.removeFromTop(gap);

    keyLabel.setBounds(area.removeFromTop(16));
    keyEditor.setBounds(area.removeFromTop(row).removeFromLeft(120));
    area.removeFromTop(gap);

    // Audio controls sit at the bottom; Notes fills the space in between.
    auto audioFileArea = area.removeFromBottom(24);
    auto audioRowArea  = area.removeFromBottom(row);
    auto audioLabelArea = area.removeFromBottom(16);
    area.removeFromBottom(gap);

    notesLabel.setBounds(area.removeFromTop(16));
    notesEditor.setBounds(area);   // remaining space

    audioLabel.setBounds(audioLabelArea);
    loadAudioButton.setBounds(audioRowArea.removeFromLeft(100));
    audioRowArea.removeFromLeft(gap);
    clearAudioButton.setBounds(audioRowArea.removeFromLeft(60));
    audioFileLabel.setBounds(audioFileArea);
}

void SongEditorPanel::loadSong(Project* proj, int index)
{
    currentProject = proj;
    currentIndex   = index;

    auto* s = song();
    if (s == nullptr) { clearSong(); return; }

    // Silence callbacks while populating to avoid spurious writes
    nameEditor.onTextChange  = nullptr;
    bpmSlider.onValueChange  = nullptr;
    tsNumerator.onChange     = nullptr;
    tsDenominator.onChange   = nullptr;
    keyEditor.onTextChange   = nullptr;
    notesEditor.onTextChange = nullptr;

    nameEditor.setText(s->name, juce::dontSendNotification);
    bpmSlider.setValue(s->bpm,  juce::dontSendNotification);
    tsNumerator.setSelectedId(s->beatsPerBar, juce::dontSendNotification);
    tsDenominator.setSelectedId(s->beatUnit,  juce::dontSendNotification);
    keyEditor.setText(s->key,     juce::dontSendNotification);
    notesEditor.setText(s->notes, juce::dontSendNotification);
    audioFileLabel.setText(s->audioFile.existsAsFile()
                               ? s->audioFile.getFileName()
                               : "No file loaded",
                           juce::dontSendNotification);

    // Re-wire callbacks
    nameEditor.onTextChange  = [this] { nameChanged(); };
    bpmSlider.onValueChange  = [this] { bpmChanged(); };
    tsNumerator.onChange     = [this] { tsChanged(); };
    tsDenominator.onChange   = [this] { tsChanged(); };
    keyEditor.onTextChange   = [this] { keyChanged(); };
    notesEditor.onTextChange = [this] { notesChanged(); };

    setEnabled(true);
}

void SongEditorPanel::clearSong()
{
    currentProject = nullptr;
    currentIndex   = -1;

    nameEditor.onTextChange  = nullptr;
    bpmSlider.onValueChange  = nullptr;
    tsNumerator.onChange     = nullptr;
    tsDenominator.onChange   = nullptr;
    keyEditor.onTextChange   = nullptr;
    notesEditor.onTextChange = nullptr;

    nameEditor.setText("",    juce::dontSendNotification);
    bpmSlider.setValue(120.0, juce::dontSendNotification);
    tsNumerator.setSelectedId(4,   juce::dontSendNotification);
    tsDenominator.setSelectedId(4, juce::dontSendNotification);
    keyEditor.setText("",   juce::dontSendNotification);
    notesEditor.setText("", juce::dontSendNotification);
    audioFileLabel.setText("Select a song to edit", juce::dontSendNotification);

    nameEditor.onTextChange  = [this] { nameChanged(); };
    bpmSlider.onValueChange  = [this] { bpmChanged(); };
    tsNumerator.onChange     = [this] { tsChanged(); };
    tsDenominator.onChange   = [this] { tsChanged(); };
    keyEditor.onTextChange   = [this] { keyChanged(); };
    notesEditor.onTextChange = [this] { notesChanged(); };

    setEnabled(false);
}

void SongEditorPanel::nameChanged()
{
    auto* s = song();
    if (s == nullptr) return;
    s->name = nameEditor.getText();
    if (onSongChanged) onSongChanged();
}

void SongEditorPanel::bpmChanged()
{
    auto* s = song();
    if (s == nullptr) return;
    s->bpm = bpmSlider.getValue();
    if (onSongChanged) onSongChanged();
}

void SongEditorPanel::loadAudioClicked()
{
    if (song() == nullptr) return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Audio File",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.mp3;*.flac");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.isEmpty()) return;

            auto* s = song(); // re-fetch after async: array may have reallocated
            if (s == nullptr) return;

            auto file = results[0];
            s->audioFile     = file;
            s->audioFilePath = file.getFullPathName();
            audioFileLabel.setText(file.getFileName(), juce::dontSendNotification);
            if (onSongChanged) onSongChanged();
        });
}

void SongEditorPanel::clearAudioClicked()
{
    auto* s = song();
    if (s == nullptr) return;
    s->audioFile     = juce::File();
    s->audioFilePath = "";
    audioFileLabel.setText("No file loaded", juce::dontSendNotification);
    if (onSongChanged) onSongChanged();
}

void SongEditorPanel::tsChanged()
{
    auto* s = song();
    if (s == nullptr) return;
    s->beatsPerBar = tsNumerator.getSelectedId();
    s->beatUnit    = tsDenominator.getSelectedId();
    if (onSongChanged) onSongChanged();
}

void SongEditorPanel::keyChanged()
{
    auto* s = song();
    if (s == nullptr) return;
    s->key = keyEditor.getText();
    if (onSongChanged) onSongChanged();
}

void SongEditorPanel::notesChanged()
{
    auto* s = song();
    if (s == nullptr) return;
    s->notes = notesEditor.getText();
    if (onSongChanged) onSongChanged();
}
