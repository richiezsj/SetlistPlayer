#pragma once
#include <JuceHeader.h>

struct Song
{
    juce::String name        = "New Song";
    double bpm               = 120.0;
    int beatsPerBar          = 4;
    int beatUnit             = 4;
    juce::File audioFile;
    juce::String audioFilePath;

    // projectDir is the folder of the .setlist file, used to store the audio
    // path relative to it so the project stays portable when moved together
    // with its audio. The absolute path is kept as a fallback.
    juce::ValueTree toValueTree(const juce::File& projectDir) const
    {
        juce::ValueTree tree("Song");
        tree.setProperty("name",          name,          nullptr);
        tree.setProperty("bpm",           bpm,           nullptr);
        tree.setProperty("beatsPerBar",   beatsPerBar,   nullptr);
        tree.setProperty("beatUnit",      beatUnit,      nullptr);
        tree.setProperty("audioFilePath", audioFilePath, nullptr);

        if (audioFile != juce::File() && projectDir != juce::File())
            tree.setProperty("audioFileRelative",
                             audioFile.getRelativePathFrom(projectDir), nullptr);
        return tree;
    }

    static Song fromValueTree(const juce::ValueTree& tree, const juce::File& projectDir)
    {
        Song s;
        s.name          = tree.getProperty("name",          "New Song");
        s.bpm           = tree.getProperty("bpm",           120.0);
        s.beatsPerBar   = tree.getProperty("beatsPerBar",   4);
        s.beatUnit      = tree.getProperty("beatUnit",      4);
        s.audioFilePath = tree.getProperty("audioFilePath", "").toString();

        // Prefer the path relative to the project; fall back to the stored
        // absolute path (older projects, or audio on another volume).
        auto relative = tree.getProperty("audioFileRelative", "").toString();
        if (relative.isNotEmpty() && projectDir != juce::File())
        {
            auto resolved = projectDir.getChildFile(relative);
            if (resolved.existsAsFile() || s.audioFilePath.isEmpty())
                s.audioFile = resolved;
        }
        if (s.audioFile == juce::File() && s.audioFilePath.isNotEmpty())
            s.audioFile = juce::File(s.audioFilePath);

        // Keep audioFilePath in sync with the resolved location.
        if (s.audioFile != juce::File())
            s.audioFilePath = s.audioFile.getFullPathName();
        return s;
    }
};

class Project
{
public:
    juce::String projectName = "My Setlist";
    juce::Array<Song> songs;
    juce::File projectFile;

    void addSong(const Song& s)   { songs.add(s); }
    void removeSong(int index)    { songs.remove(index); }
    void moveSong(int from, int to)
    {
        if (from == to) return;
        auto s = songs[from];
        songs.remove(from);
        songs.insert(to, s);
    }

    bool save(const juce::File& file)
    {
        juce::ValueTree root("SetlistProject");
        root.setProperty("projectName", projectName, nullptr);
        const auto projectDir = file.getParentDirectory();
        for (auto& song : songs)
            root.appendChild(song.toValueTree(projectDir), nullptr);

        juce::FileOutputStream out(file);
        if (!out.openedOk()) return false;
        out.setPosition(0);   // rewind to start
        out.truncate();       // clear old content
        root.writeToStream(out);
        projectFile = file;
        return true;
    }

    bool load(const juce::File& file)
    {
        juce::FileInputStream in(file);
        if (!in.openedOk()) return false;
        auto root = juce::ValueTree::readFromStream(in);
        if (!root.isValid()) return false;

        projectName = root.getProperty("projectName", "My Setlist").toString();
        songs.clear();
        const auto projectDir = file.getParentDirectory();
        for (int i = 0; i < root.getNumChildren(); ++i)
            songs.add(Song::fromValueTree(root.getChild(i), projectDir));

        projectFile = file;
        return true;
    }

    // Songs that reference an audio file which no longer exists on disk.
    // Used to warn the user after opening a project.
    juce::StringArray missingAudioFiles() const
    {
        juce::StringArray missing;
        for (auto& s : songs)
            if (s.audioFilePath.isNotEmpty() && ! s.audioFile.existsAsFile())
                missing.add(s.name);
        return missing;
    }
};
