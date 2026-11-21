# Guida all'utilizzo — PINKSTUDIO 0.1.0

## Primo avvio

1. Apri PINKSTUDIO: si carica la sessione demo **Pink Sugar Rush** (150 BPM, 8 battute, re minore).
2. Premi **F10** (*Options → Audio settings*):
   - **Audio device type**: *ASIO* (scheda audio esterna, latenza più bassa), *Windows Audio*
     (WASAPI condiviso), *Windows Audio (Exclusive Mode)* (WASAPI esclusivo, bassa latenza),
     *DirectSound* (compatibilità).
   - **Output/Input**: dispositivo e canali attivi (PINKSTUDIO suona sulle uscite 1-2).
   - **Sample rate** (44.1–192 kHz, se supportato dalla scheda) e **Buffer size**.
   - **Active MIDI inputs**: le tastiere MIDI collegate.
   - **Test**: suona un tono di prova sulle uscite.
   - In basso: **meter di test degli ingressi** 1–8 e latenze reali di ingresso/uscita.
   Le impostazioni vengono salvate e ripristinate al prossimo avvio. Con ASIO, il pannello di
   controllo della scheda permette anche il monitoraggio diretto hardware.
3. Premi **Spazio**: la demo parte in loop.

## Schermata principale

### Barra del transport
| Controllo | Funzione |
|---|---|
| ▶ / ❚❚ | Play / pausa |
| ■ | Stop: torna al punto da cui è partito il playback; un secondo stop torna all'inizio |
| ⟲ | Loop on/off |
| Metronomo | Click con accento sul primo movimento |
| TEMPO | Trascina, usa la rotella o fai doppio clic per scrivere i BPM (annullabile) |
| Posizione | Battuta . movimento . tick, tempo trascorso e indicazione di tempo |
| CPU / XRUN | Carico del motore audio e numero di interruzioni del driver |

### Arrangement (panoramica)
- **Clic** nell'area delle tracce: sposta il playhead (agganciato al movimento).
- **Trascina sul righello** in alto: imposta la regione di loop (a battute intere) e la attiva.
- **M** accanto al nome: mute della traccia (annullabile con Ctrl+Z).
- Le clip mostrano un'anteprima delle note. La modifica delle note arriva col Piano Roll (fase 4).

### Mixer
Per ogni canale usato: pan (doppio clic = centro), **M** mute, **S** solo, fader (doppio clic = 0 dB),
meter stereo con peak hold, catena di insert, send e uscita. Il solo mantiene udibili i canali che
alimentano o ricevono il canale in solo (per esempio il bus del delay).

Routing della demo:

| Canale | Contenuto | Insert / routing |
|---|---|---|
| Kick | PinkDrum | → Master |
| Drums | PinkDrum (rullante, clap, hi-hat) | → Master |
| Bass | PinkSynth "Sub Bass" | PinkComp in sidechain dal Kick |
| Chords | PinkSynth "Supersaw Pad" | PinkEQ, PinkComp in sidechain dal Kick, send al Delay Bus |
| Lead | PinkSynth "Kawaii Pluck" | send al Delay Bus |
| Delay Bus | — | PinkDelay 1/8 puntato ping-pong |
| Master | — | PinkEQ, PinkLimiter (ceiling −1 dBFS) |

### Sezione master
Momentary / Short-term / Integrated in LUFS (EBU R128), true peak attuale e massimo (dBTP),
peak L/R, riduzione del limiter, CPU, correlazione di fase (−1 … +1). **Reset** azzera la misura
integrata e il true peak massimo.

## Suonare con una tastiera MIDI
Le note in arrivo dalle tastiere attive suonano il **Lead** (PinkSynth "Kawaii Pluck") anche a
transport fermo, con velocity, pitch bend, mod wheel (vibrato) e pedale sustain.
In caso di note bloccate: **Shift+Esc** (*MIDI panic*).

## Export
**Ctrl+R** (*File → Export mixdown*): render offline in WAV 24 bit di tutta la canzone più 2,5 s di
coda, alla frequenza di campionamento del dispositivo. La finestra si apre nella cartella progetti
scelta durante l'installazione. Al termine la barra di stato mostra durata, LUFS integrati, true
peak e velocità di render.

## Temi
*View*: **Pink Default**, **Dark Pink**, **Cyber Pink**, **Pastel**. La scelta viene ricordata.

## Scorciatoie (personalizzabili in *Options → Keyboard shortcuts*)
| Azione | Tasto |
|---|---|
| Play / pausa | Spazio |
| Stop | Invio |
| Vai all'inizio | Home |
| Loop | L |
| Metronomo | Ctrl+M |
| Impostazioni audio | F10 |
| Export WAV | Ctrl+R |
| Annulla / Ripeti | Ctrl+Z / Ctrl+Y (o Ctrl+Shift+Z) |
| MIDI panic | Shift+Esc |
| Esci | Ctrl+Q |

Annullabili oggi: tempo, mute delle tracce, solo dei canali.

## Limiti di questa versione
Non ci sono ancora: registrazione, editing di clip e note, piano roll, step sequencer, browser,
salvataggio dei progetti, automazione, hosting VST3, finestre dei plugin stock, PinkTune, PinkWave.
Sono pianificati nelle fasi successive: vedi `ROADMAP.md`.
