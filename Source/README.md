# SetlistPlayer
### A JUCE/C++ Setlist Manager with Metronome & Audio Playback

---

## Features

- **Project management** — New / Open / Save projects (`.setlist` format, serialised ValueTree)
- **Setlist panel** (left) — Ordered list of songs with drag-and-drop reordering, ▲/▼ buttons, add/remove
- **Song editor panel** (right) — Edit song name, BPM (20–300), time signature, load/clear a WAV or AIFF backing track
- **Transport panel** (centre) — Beat indicator lights, large BPM display, Play/Stop/Next buttons, volume slider, position readout
- **Metronome engine** — Internal synthesised click (sine burst, accent on beat 1), fully synchronised with audio playback
- **MIDI output** — Optional MIDI click on channel 10 (note 37) sent alongside the internal click
- **Audio engine** — Stereo WAV/AIFF/MP3/FLAC playback using `AudioTransportSource`, mixed with the metronome in real-time

---

## Requirements

| Tool | Version |
|------|---------|
| JUCE | 7.x |
| Xcode | 14+ |
| macOS | 10.13+ |
| C++ | 17 |

---

## Build Instructions

### 1. Install JUCE
Download from https://juce.com or clone:
```
git clone https://github.com/juce-framework/JUCE.git
```

### 2. Open with Projucer
Open `SetlistPlayer.jucer` in the Projucer application.

Set the **JUCE modules path** to your JUCE installation folder (e.g. `/Users/you/JUCE/modules`).

### 3. Export & Build
- Click **"Save Project and Open in Xcode"**
- In Xcode, choose the **SetlistPlayer - Release** scheme
- Press **⌘B** to build
- The `.app` will be in `Builds/MacOSX/build/Release/`

---

## Project File Format (`.setlist`)

Binary JUCE ValueTree with structure:
```xml
<SetlistProject projectName="My Setlist">
  <Song name="Song 1" bpm="120" beatsPerBar="4" beatUnit="4" audioFilePath="/path/to/file.wav"/>
  <Song name="Song 2" bpm="95"  beatsPerBar="3" beatUnit="4" audioFilePath=""/>
  ...
</SetlistProject>
```

---

## Architecture

```
MainComponent
├── SetlistPanel       (left column)  — ListBox, add/remove/reorder songs
├── TransportPanel     (centre)       — BPM display, beat LEDs, play/stop/next
├── SongEditorPanel    (right column) — Song details editor
├── MetronomeEngine    (AudioSource)  — Click synthesis + MIDI output
├── AudioPlayerEngine  (AudioSource)  — WAV/AIFF file playback
└── MixerSource        (AudioSource)  — Mixes metronome + audio for device output
```

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| ⌘N | New Project |
| ⌘O | Open Project |
| ⌘S | Save Project |
| ⌘⇧S | Save As |
| ⌘T | Add Song |
| ⌘Q | Quit |

---

## Extending the App

- **MIDI Output selection**: Add a `ComboBox` populated with `MidiOutput::getAvailableDevices()` and pass the selected device to `MetronomeEngine::setMidiOutput()`
- **Click volume slider**: Expose `clickAmplitude` in `MetronomeEngine` and connect a slider
- **Pre-roll / count-in**: Add a countdown state in `MetronomeEngine` before starting the audio transport
- **Song notes / key**: Add a `String notes` and `String musicalKey` field to the `Song` struct
