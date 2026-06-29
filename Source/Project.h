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

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree tree("Song");
        tree.setProperty("name",          name,          nullptr);
        tree.setProperty("bpm",           bpm,           nullptr);
        tree.setProperty("beatsPerBar",   beatsPerBar,   nullptr);
        tree.setProperty("beatUnit",      beatUnit,      nullptr);
        tree.setProperty("audioFilePath", audioFilePath, nullptr);
        return tree;
    }

    static Song fromValueTree(const juce::ValueTree& tree)
    {
        Song s;
        s.name          = tree.getProperty("name",          "New Song");
        s.bpm           = tree.getProperty("bpm",           120.0);
        s.beatsPerBar   = tree.getProperty("beatsPerBar",   4);
        s.beatUnit      = tree.getProperty("beatUnit",      4);
        s.audioFilePath = tree.getProperty("audioFilePath", "").toString();
        if (s.audioFilePath.isNotEmpty())
            s.audioFile = juce::File(s.audioFilePath);
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
        for (auto& song : songs)
            root.appendChild(song.toValueTree(), nullptr);

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
        for (int i = 0; i < root.getNumChildren(); ++i)
            songs.add(Song::fromValueTree(root.getChild(i)));

        projectFile = file;
        return true;
    }
};
