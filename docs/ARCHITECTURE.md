# Architettura di PINKSTUDIO

## Livelli

```
┌──────────────────────────────────────────────────────────────────────────┐
│ app/ (JUCE 8)                                                            │
│  MainComponent ── menu, comandi, scorciatoie (ApplicationCommandManager)  │
│  TransportBar · ArrangementView · MixerPanel · MasterSection · Settings   │
│  PinkLookAndFeel (4 temi)                                                 │
│  AudioHost ── juce::AudioDeviceManager (ASIO / WASAPI / DirectSound)      │
│             └─ MidiMessageCollector (tastiere MIDI)                       │
└──────────────┬───────────────────────────────────────────────────────────┘
               │ processBlock(input, output, eventi MIDI live)   ← unico punto di contatto
┌──────────────▼───────────────────────────────────────────────────────────┐
│ engine/ (C++20 puro, nessuna dipendenza da GUI o driver)                  │
│  AudioEngine ── transport, coda comandi SPSC, snapshot di rendering       │
│  Project (modello)  ──commit()──►  EngineSnapshot (immutabile)            │
│  Mixer: canali, insert, send, sidechain, solo, ordine topologico          │
│  DSP: Biquad, PinkEQ, PinkComp, PinkLimiter, PinkDelay, PinkUtility       │
│  Meter: peak/RMS, LUFS (BS.1770-4), true peak 4x, correlazione            │
│  Strumenti: PinkSynth, PinkDrum, Metronome                                │
│  I/O: WAV reader/writer, OfflineRenderer · UndoManager · TempoMap         │
└──────────────────────────────────────────────────────────────────────────┘
```

Il motore si compila e si testa da solo (`pinkstudio_engine` + `pinkstudio_tests`): la GUI può
cambiare senza toccare l'audio, e lo stesso motore gira nel render offline (`pinkrender`).

## Modello dei thread

| Thread | Cosa fa | Regole |
|---|---|---|
| Message thread (GUI) | modifica il `Project`, chiama `commit()`, invia comandi di transport, legge i meter | può allocare e bloccare |
| Audio thread | solo `AudioEngine::processBlock()` | niente allocazioni, lock, I/O o free |
| Thread MIDI di JUCE | riempie il `MidiMessageCollector` | letto dal callback audio |

Comunicazione:

- **Comandi** (play, stop, locate, loop, metronomo, panic): coda `SpscQueue` wait-free.
- **Parametri continui** (volume, pan, mute, parametri dei plugin): `std::atomic<float>`;
  il motore applica rampe per evitare click.
- **Cambi strutturali** (tracce, clip, routing, insert, solo, tempo): il message thread compila
  un nuovo `EngineSnapshot` immutabile e lo pubblica con `SnapshotExchange`. L'audio thread lo
  prende all'inizio del blocco successivo. Il vecchio snapshot finisce in un "cimitero" e viene
  distrutto **sul message thread** solo dopo che l'audio thread ha completato almeno un blocco
  successivo (contatore di epoche). Verificato con ThreadSanitizer e con un test di stress di
  400 modifiche live durante il playback.
- **Meter verso la GUI**: atomici letti da timer a 20–30 Hz.

## Transport e timing

- La posizione musicale è in **PPQ** (quarti, `double`); durante il playback il clock è un
  **contatore intero di campioni**, così i confini dei blocchi sono esatti e un evento cade sempre
  sullo stesso campione con buffer da 64, 512, 1000 o 4096 (test
  `engine_events_are_sample_accurate_for_any_block_size`).
- `TempoMap`: cambi di tempo a gradino e cambi di indicazione di tempo all'inizio delle battute;
  conversioni PPQ ↔ secondi ↔ battuta/movimento/tick con ricerca binaria (real-time safe).
- Loop: il blocco viene diviso nel punto esatto del loop; le note ancora attive ricevono note-off
  al punto di salto; nessun "chase" delle note già iniziate (come nelle DAW principali).
- Stop: note-off a tutte le note attive e ritorno al punto di partenza; un secondo stop torna a 1.1.1.

## Mixer

- Canale 0 = master, canali 1..N = insert (default 128, massimo 512).
- Ogni canale: 10 slot insert, fader, pan, mute, solo, fase, mono, fino a 8 send pre/post fader,
  un'uscita, una sorgente sidechain (uscita post-fader di un altro canale).
- Il routing viene validato a ogni modifica (`wouldCreateCycle`): uscite, send e sidechain non
  possono mai formare un ciclo.
- `commit()` calcola i canali attivi (con segnale), li ordina topologicamente (Kahn) e calcola il
  solo: un canale in solo mantiene udibili tutto ciò che lo alimenta e tutto ciò in cui confluisce.
  Nella demo vengono elaborati 7 canali su 129: i canali vuoti non costano CPU.

## Plugin e strumenti

Tutti gli effetti implementano `pink::Processor` (`prepare / reset / process / getLatencySamples /
acceptsSidechain`) con parametri `pink::Parameter` (atomici, scala lineare/logaritmica/scelta,
mappatura normalizzata 0..1 per automazione e MIDI learn). Gli strumenti implementano
`pink::Instrument` e ricevono una lista di eventi MIDI ordinata e con offset al campione.

L'hosting VST3 (fase 7) si inserirà come adattatore `Processor` attorno a
`juce::AudioPluginInstance`, senza toccare il mixer.

## Decisioni importanti

- **Nessuna allocazione nel callback**: buffer, liste di eventi e code sono pre-allocati in
  `prepare()`. Il test `engine_audio_callback_does_not_allocate` sostituisce `operator new` e conta
  le allocazioni dentro 3000 callback con la demo, il metronomo, MIDI live, locate e panic: 0.
- **Denormali**: FTZ/DAZ attivati in ogni callback (`ScopedNoDenormals`).
- **Blocchi più grandi del previsto**: il motore li spezza in pezzi da `maxBlockSize`.
- **Render offline = stesso codice del live**: l'export suona come il playback.
- **Runtime MSVC statico**: `PINKSTUDIO.exe` non richiede il Visual C++ Redistributable.
