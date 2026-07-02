# SetlistPlayer — Documentazione tecnica e tracciamento caratteristiche

> Documento di riferimento per lo stato delle funzionalità, l'architettura e il backlog dei miglioramenti.
> Ultimo aggiornamento: 2026-07-02 · Versione app: 1.0.0 · JUCE 8 · C++17 · macOS / iOS

---

## 1. Panoramica

SetlistPlayer è un'applicazione live per band e musicisti che unisce in un unico strumento:

- **gestione della scaletta** (lista ordinata di brani),
- **player delle basi** (backing track stereo),
- **metronomo** sincronizzato (click interno e/o uscita MIDI),
- **mixer** a due canali (Base / Click) con fader, pan e mute indipendenti.

L'obiettivo è passare dal brano al brano successivo durante il concerto con un solo tasto, senza dover gestire player e metronomo separati.

---

## 2. Architettura

```
JUCEApplication (Main.cpp)
└── MainWindow (DocumentWindow)
    └── MainComponent  ── MenuBarModel + ApplicationCommandTarget
        │
        ├── Dati
        │   └── Project { projectName, Array<Song>, projectFile }
        │
        ├── Audio (costruiti prima della UI, distrutti dopo)
        │   ├── AudioDeviceManager
        │   ├── MetronomeEngine     (AudioSource)  — sintesi click + MIDI out
        │   ├── AudioPlayerEngine   (AudioSource)  — riproduzione base
        │   ├── MetronomeOutputPanel               — possiede il MidiOutput aperto
        │   ├── MixerSource         (AudioSource)  — somma click + base, calcola RMS
        │   └── AudioSourcePlayer                  — ponte device ↔ MixerSource
        │
        └── UI
            ├── MenuBarComponent
            ├── SetlistPanel        (sinistra) — lista brani, add/remove/riordina
            ├── TransportPanel      (centro)   — BPM, LED movimenti, play/stop/next, 2 channel strip
            └── SongEditorPanel     (destra)   — nome, BPM, metro, base audio
```

### Catena audio

Il `MixerSource` (definito inline in `MainComponent.h`) somma i due `AudioSource` in un buffer temporaneo pre-allocato in `prepareToPlay`, così **il thread audio non alloca mai memoria**. Per ogni canale calcola l'RMS e lo inoltra ai VU meter via `std::function` chiamate dal thread audio.

Gain e pan sono applicati **dentro** ogni engine (equal-power pan). Il mute azzera il buffer ma il playhead della base continua ad avanzare (`transportSource` viene comunque consumato), così Base e Click restano allineati anche a canale mutato.

### Persistenza

I progetti sono serializzati come `juce::ValueTree` binario in file `.setlist`:

```
SetlistProject (projectName)
 ├── Song (name, bpm, beatsPerBar, beatUnit, audioFilePath)
 └── Song (...)
```

I percorsi audio sono salvati come **path assoluti** (vedi §6, limitazione nota).

### Threading

| Risorsa | Thread scrittura | Thread lettura |
|---|---|---|
| `sampleCounter`, `currentBeat/Bar` | audio | audio |
| `playing` (metronomo) | message | audio (`std::atomic`) |
| `bpm`, `numerator`, `gain`, `pan`, `muted`, note/canale MIDI | message | audio (`std::atomic`) |
| Device MIDI (`midiOutput`) | message | audio (protetto da `CriticalSection` + try-lock) |
| Livelli VU (`level`) | audio | message (`std::atomic`) |

---

## 3. Mappa dei file sorgente

| File | Righe | Responsabilità |
|---|---|---|
| `Main.cpp` | 53 | Bootstrap `JUCEApplication`, `MainWindow` |
| `MainComponent.{h,cpp}` | 180 / 580 | Orchestrazione, menu, comandi, ciclo di vita audio, finestre About / Audio Setup, `MixerSource` |
| `Project.h` | 86 | Modello dati `Song` + `Project`, load/save `ValueTree` |
| `MetronomeEngine.{h,cpp}` | 84 / 129 | Sintesi click, timing beat/bar, uscita MIDI |
| `AudioPlayerEngine.{h,cpp}` | 47 / 128 | Caricamento e riproduzione base, gain/pan/mute |
| `TransportPanel.{h,cpp}` | 316 / 240 | Transport, LED movimenti, `VuMeter`, `PanKnob`, `ChannelStrip` |
| `SetlistPanel.{h,cpp}` | 46 / 206 | Lista brani (`ListBoxModel`), add/remove/move |
| `SongEditorPanel.{h,cpp}` | 49 / 234 | Editor brano (nome, BPM, metro, base audio) |
| `MetronomeOutputPanel.{h,cpp}` | 45 / 208 | Modalità click/MIDI, device, canale, note |

---

## 4. Caratteristiche — stato attuale

Legenda: ✅ implementato · 🟡 parziale/limitato · ⬜ non presente

### Scaletta
- ✅ Lista brani con numero traccia, nome, BPM, metro e icona base
- ✅ Aggiungi / rimuovi brano
- ✅ Riordino con pulsanti ▲ / ▼
- 🟡 Riordino drag-and-drop — `getDragSourceDescription` è presente ma **manca il drop target**: il trascinamento non riordina davvero (il README lo cita come funzionante)
- ✅ Selezione brano → carica editor, transport e base audio

### Player basi
- ✅ Riproduzione stereo WAV / AIFF / MP3 / FLAC
- ✅ Gain, pan equal-power, mute indipendenti
- ✅ Indicatore posizione `m:ss / m:ss`
- 🟡 Pausa/ripresa — `pause()` esiste ma `play()` riparte sempre da 0 (nessun resume)
- ⬜ Seek / barra di scorrimento / waveform
- ⬜ Auto-avanzamento a fine traccia (`onPlaybackFinished` presente ma non collegato)

### Metronomo
- ✅ Click sintetizzato con accento sul downbeat (1800 Hz / 1200 Hz)
- ✅ Range BPM 20–300, metro con numeratore 1–12
- ✅ Gain / pan / mute dedicati
- ✅ Callback `onBeat` per i LED dei movimenti
- 🟡 Il **denominatore del metro è ignorato** nel timing: la durata del beat è sempre 1/4 (`60/bpm`), quindi 6/8, 3/8 ecc. non hanno il feel corretto
- ⬜ Count-in / pre-roll

### Uscita MIDI
- ✅ Modalità: click interno / solo MIDI / interno + MIDI
- ✅ Selezione device, canale (1–16), note downbeat e beat configurabili
- ✅ Il device MIDI resta aperto anche a finestra Audio Setup chiusa
- 🟡 `noteOn` inviato senza `noteOff` (ok per percussioni GM one-shot, note appese con suoni tenuti)
- 🟡 `sendMessageNow` chiamato dal thread audio (non RT-safe, possibile jitter/blocco)

### Transport & UI
- ✅ BPM grande, LED movimenti con accento, Play / Stop / Next
- ✅ Due channel strip (Base / Click) con fader, VU meter (peak-hold + clip), pan knob custom
- ✅ Barra spaziatrice = Play/Stop globale (anche con focus su un controllo figlio)
- ✅ Tema scuro coerente, finestre About e Audio Setup

### Progetto / file
- ✅ Nuovo / Apri / Salva / Salva con nome (`.setlist`)
- ✅ Prompt "salva prima?" su **Nuovo progetto**
- ✅ Indicatore modifiche non salvate nel titolo (`*`)
- ✅ Prompt di salvataggio su **Nuovo**, **Apri** e **Uscita/chiusura finestra**
- ⬜ Progetti recenti / autosave

### Menu & scorciatoie
- ✅ ⌘N / ⌘O / ⌘S / ⌘⇧S / ⌘T / ⌘Q / ⌘, (Audio Setup)
- ✅ Menu File / Edit / Help, About con versione

---

## 5. Impostazioni persistite vs. volatili

| Impostazione | Persistita nel `.setlist` |
|---|---|
| Nome progetto, brani, BPM, metro, path base | ✅ |
| Volume / pan / mute di Base e Click | ❌ (reset a ogni avvio) |
| Modalità e device MIDI, canale, note | ❌ |
| Device audio / sample rate / buffer | ❌ (default di sistema a ogni avvio) |

---

## 6. Miglioramenti proposti (backlog)

Ordinati per priorità.

### ✅ Priorità alta — correttezza / robustezza (RISOLTI 2026-07-02)
1. ~~**Data race su `midiOut`**~~ → **Fatto.** Il device MIDI è ora di proprietà di `MetronomeEngine` (`std::unique_ptr`), scambiato sotto `CriticalSection`; l'audio thread lo usa solo via `ScopedTryLock` (salta il click se il device è in fase di swap, senza mai bloccare né dereferenziare un puntatore liberato).
2. ~~**Parametri condivisi non atomici**~~ → **Fatto.** `bpm`, `numerator`, `denominator`, `gain`, `pan`, `muted`, `useMidi`, `useInternal`, note/canale MIDI (metronomo) e `fileLoaded`, `gain`, `pan`, `muted` (player) sono ora `std::atomic`.
3. ~~**Prompt salvataggio mancante su Apri e su Uscita**~~ → **Fatto.** Helper `MainComponent::canDiscardCurrentProject()` usato da Nuovo, Apri e Uscita (anche chiusura finestra e ⌘Q). Corretta anche l'inversione Sì/No/Annulla del prompt originale.

### Priorità media — comportamento musicale
4. **Denominatore del metro nel timing** — usare `samplesPerBeat = (60/bpm) * sr * (4.0/beatUnit)` (o gestione esplicita dei tempi composti) così 6/8, 3/8 ecc. suonano corretti.
5. **MIDI RT-safe** — accodare i messaggi in un `MidiBuffer` e inviarli fuori dal thread audio (o via `MidiOutput::sendBlockOfMessages`); aggiungere `noteOff`.
6. **VU meter — doppio gain** — l'RMS calcolato dal `MixerSource` è già post-gain (gli engine applicano il gain nel blocco), ma `ChannelStrip::pushLevel` lo rimoltiplica per il valore del fader → il meter sovrastima. Scegliere un solo punto di applicazione.
7. **Count-in / pre-roll** — battute di preparazione prima dell'avvio della base (già in roadmap README).
8. **Auto-avanzamento** — collegare `AudioPlayerEngine::onPlaybackFinished` per passare al brano successivo a fine base.

### Priorità bassa — funzionalità / pulizia
9. **Persistere il mix** (volume/pan/mute) e le impostazioni MIDI nel progetto o in `PropertiesFile`.
10. **Path audio relativi** al file `.setlist` + segnalazione file mancante all'apertura.
11. **Drag-and-drop reale** nella scaletta (o rimuovere `getDragSourceDescription` e aggiornare il README).
12. **Resume della base** — `play()` non dovrebbe forzare `setPosition(0)` se si vuole vera pausa.
13. **Pulizia dead code** — `MetronomeEngine::generateClick` / `generateMidiBeat` sono dichiarati ma non usati (la sintesi è inline in `getNextAudioBlock`).
14. Campi **note** e **tonalità** per brano; **export PDF** della scaletta (roadmap README).

---

## 7. Note di build

- Il progetto è pilotato da `SetlistPlayer.jucer` (Projucer).
- `Builds/` e `JuceLibraryCode/` sono rigenerati da Projucer (non versionare a mano).
- Target: macOS 10.13+, iOS; C++17; moduli JUCE 8.

---

## 8. Changelog documentazione

| Data | Nota |
|---|---|
| 2026-07-02 | Prima stesura: stato caratteristiche + backlog miglioramenti |
| 2026-07-02 | Risolti i 3 fix ad alta priorità: thread-safety device MIDI, parametri condivisi atomici, prompt di salvataggio su Apri/Uscita |
</content>
</invoke>
