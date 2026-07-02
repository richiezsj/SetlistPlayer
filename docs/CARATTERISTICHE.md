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

I percorsi audio sono salvati come **path relativi** alla cartella del `.setlist` (`audioFileRelative`), con il path assoluto conservato come fallback; all'apertura i brani con base mancante vengono segnalati.

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
- ✅ Riordino drag-and-drop — `SetlistPanel` è un `DragAndDropContainer` e la `ListBox` (`SongListBox`) è un `DragAndDropTarget` che calcola l'indice di inserimento e riordina davvero
- ✅ Selezione brano → carica editor, transport e base audio

### Player basi
- ✅ Riproduzione stereo WAV / AIFF / MP3 / FLAC
- ✅ Gain, pan equal-power, mute indipendenti
- ✅ Indicatore posizione `m:ss / m:ss`
- 🟡 Pausa/ripresa — `pause()` esiste ma `play()` riparte sempre da 0 (nessun resume)
- ⬜ Seek / barra di scorrimento / waveform
- ✅ Auto-avanzamento a fine traccia — a fine base la selezione passa al brano successivo (senza auto-play; il musicista preme Play/Spazio). Notifica sparata una sola volta (latch) e postata sul message thread

### Metronomo
- ✅ Click sintetizzato con accento sul downbeat (1800 Hz / 1200 Hz)
- ✅ Range BPM 20–300, metro con numeratore 1–12
- ✅ Gain / pan / mute dedicati
- ✅ Callback `onBeat` per i LED dei movimenti
- ✅ Il **denominatore del metro è rispettato** nel timing: `samplesPerBeat = (60/bpm)·sr·(4/beatUnit)`, quindi 6/8, 3/8, 6/4 hanno il feel corretto
- ✅ Count-in / pre-roll — 0/1/2/4 battute di preparazione (selettore in Audio Setup); la base parte sul primo downbeat "vero" via callback. I LED mostrano il conteggio

### Uscita MIDI
- ✅ Modalità: click interno / solo MIDI / interno + MIDI
- ✅ Selezione device, canale (1–16), note downbeat e beat configurabili
- ✅ Il device MIDI resta aperto anche a finestra Audio Setup chiusa
- ✅ `noteOff` inviato: a ogni beat si spegne la nota precedente e allo stop si manda `allNotesOff` (niente note appese con suoni tenuti)
- ✅ Invio MIDI RT-safe: l'audio thread accoda in una FIFO lock-free (`AbstractFifo`), un `HighResolutionTimer` (1 ms) drena e invia fuori dal thread audio

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

### ✅ Priorità media — comportamento musicale (parziale)
4. ~~**Denominatore del metro nel timing**~~ → **Fatto.** `samplesPerBeat = (60/bpm)·sr·(4/denominator)`; il BPM resta riferito alla semiminima e i tempi come 6/8, 3/8, 6/4 hanno la durata corretta. Il denominatore è protetto (`jmax(1, den)`) contro la divisione per zero sull'audio thread.
5. ~~**MIDI RT-safe**~~ → **Fatto.** L'audio thread accoda gli eventi in una FIFO lock-free single-producer/single-consumer (`juce::AbstractFifo`); un `HighResolutionTimer` (1 ms) la drena e invia via `sendMessageNow` sotto `midiLock`, fuori dal thread audio (che ora non tocca più né il lock né il device). Aggiunto `noteOff` della nota precedente a ogni beat e `allNotesOff` allo stop.
6. ~~**VU meter — doppio gain**~~ → **Fatto.** `ChannelStrip::pushLevel` non rimoltiplica più per il fader: l'RMS del `MixerSource` è già post-gain, quindi il meter riflette l'uscita reale. Rimosso anche l'accesso non thread-safe allo `Slider` dal thread audio.
7. ~~**Count-in / pre-roll**~~ → **Fatto.** `MetronomeEngine` suona 0/1/2/4 battute di preparazione (selettore "Count-in" in Audio Setup); al primo downbeat successivo fa partire la base tramite `onCountInFinished` (postato sul message thread, con guardia se si preme Stop durante il count-in). I LED dei movimenti mostrano il conteggio.
8. ~~**Auto-avanzamento**~~ → **Fatto.** `AudioPlayerEngine::onPlaybackFinished` collegato: a fine base la selezione avanza al brano successivo (riusa `onNextSong`, nessun auto-play). La notifica è protetta da un latch (`finishedNotified`) per sparare una sola volta e viene azzerata alla distruzione della UI.

### Priorità bassa — funzionalità / pulizia
9. **Persistere il mix** (volume/pan/mute) e le impostazioni MIDI nel progetto o in `PropertiesFile`.
10. ~~**Path audio relativi**~~ → **Fatto.** Salvato `audioFileRelative` (relativo alla cartella del `.setlist`) accanto al path assoluto di fallback; in apertura si risolve il relativo e, se manca, si ricade sull'assoluto. `Project::missingAudioFiles()` + avviso all'apertura per i brani con base non trovata.
11. ~~**Drag-and-drop reale**~~ → **Fatto.** Trascinando una riga la scaletta si riordina davvero (`SongListBox` come `DragAndDropTarget`, indice via `getInsertionIndexForPosition` con correzione dello shift). I pulsanti ▲/▼ restano disponibili.
12. **Resume della base** — `play()` non dovrebbe forzare `setPosition(0)` se si vuole vera pausa.
13. ~~**Pulizia dead code**~~ → **Fatto.** Rimosse le dichiarazioni inutilizzate `MetronomeEngine::generateClick` / `generateMidiBeat` (la sintesi è inline in `getNextAudioBlock`).
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
| 2026-07-02 | Risolto fix #4 (priorità media): il denominatore del metro è ora rispettato nel timing del beat |
| 2026-07-02 | Risolto fix #6 (priorità media): rimosso il doppio gain sul VU meter (e la lettura dello Slider dal thread audio) |
| 2026-07-02 | Risolto fix #8 (priorità media): auto-avanzamento al brano successivo a fine base |
| 2026-07-02 | Risolto fix #5 (priorità media): invio MIDI RT-safe via FIFO lock-free + HighResolutionTimer, con noteOff/allNotesOff |
| 2026-07-02 | Risolto fix #7 (priorità media): count-in di 0/1/2/4 battute prima dell'avvio della base |
</content>
</invoke>
