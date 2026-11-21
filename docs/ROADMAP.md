# Roadmap e stato delle funzioni

Legenda: ✅ implementato e testato · 🟡 parziale (cosa manca è scritto) · ⏳ non ancora iniziato.
Le funzioni ⏳ non compaiono nell'interfaccia: niente pulsanti finti.

## Fasi

| Fase | Contenuto | Stato |
|---|---|---|
| 1 | Architettura e struttura del progetto | ✅ |
| 2 | Motore audio funzionante | ✅ |
| 3 | Interfaccia grafica completa | 🟡 finestra principale, temi, mixer, master, transport, impostazioni audio, scorciatoie. Mancano pannelli agganciabili, workspace, editor dei colori, finestre dei plugin |
| 4 | Playlist, Piano Roll, Step Sequencer | 🟡 solo panoramica (playhead, loop, mute). Editing ⏳ |
| 5 | Mixer e routing | 🟡 motore completo; nell'interfaccia mancano la modifica di routing, send e insert |
| 6 | Registrazione audio e MIDI | ⏳ (già presenti: meter di ingresso e MIDI live) |
| 7 | Hosting VST3 | ⏳ (pronti: interfaccia `Processor`, cartelle VST3 dall'installer) |
| 8 | Plugin stock | 🟡 5 effetti + 2 strumenti con DSP reale, senza interfacce dedicate e preset |
| 9 | PinkTune e PinkWave | ⏳ |
| 10 | Strumenti virtuali | 🟡 PinkSynth e PinkDrum versione base |
| 11 | Salvataggio, esportazione, recovery | 🟡 export WAV ✅; progetto, autosave, recovery ⏳ |
| 12 | Ottimizzazione, debugging, test | 🟡 39 test + sanitizer; PDC e mixer multithread ⏳ |
| 13 | Release Windows x64 | 🟡 CMake + CI + installer pronti; prima build MSVC alla prima esecuzione del workflow |

Prossimo passo consigliato: **fase 11 (formato di progetto) + fase 4 (piano roll e editing in
playlist)**, perché senza salvataggio e senza editing la DAW non è ancora usabile per comporre.

## Master prompt, sezione per sezione

| # | Sezione | Stato | Dettagli |
|---|---|---|---|
| 1 | Tecnologia e architettura | ✅ | C++20, JUCE 8.0.15, motore indipendente dalla GUI, ASIO/WASAPI/DirectSound; rendering Direct2D di JUCE 8 su Windows. MIDI 2.0 e VST3 ⏳ |
| 2 | Interfaccia | 🟡 | 4 temi ✅, finestra ridimensionabile ✅, DPI elevati ✅ (JUCE per-monitor). Personalizzazione completa dei colori, pannelli ridimensionabili/agganciabili, workspace ⏳ |
| 3 | Playlist / arrangement | 🟡 | Motore: tracce MIDI e audio, clip, loop, cambi di tempo e di indicazione di tempo, undo/redo. Interfaccia: solo panoramica. Taglio/copia/incolla, stretching, marker, comping, crossfade, bounce ⏳ |
| 4 | Piano roll | ⏳ | |
| 5 | Step sequencer | ⏳ | |
| 6 | Mixer | 🟡 | 128+ canali, bus, send pre/post, sidechain, stereo/mono, peak/RMS/LUFS/correlazione, solo/mute/pan, insert ✅. Spectrum analyzer, multicanale, automazione, gruppi ⏳ |
| 7 | Sistema di plugin | ⏳ | |
| 8 | Driver audio | ✅ | ASIO, WASAPI esclusivo/condiviso, DirectSound, scelta dispositivi e canali, buffer, sample rate, test uscite e ingressi, latenze. Input monitoring software ⏳ (fase 6) |
| 9 | Plugin stock | 🟡 | PinkEQ (parametrico 8 bande, copre 3/7 bande), PinkComp (studio, sidechain, mix parallelo), PinkLimiter, PinkDelay (stereo, ping-pong), PinkUtility (gain, fase, mono, width). Gli altri ⏳ |
| 10 | PinkTune | ⏳ | |
| 11 | PinkWave | ⏳ | |
| 12 | Strumenti | 🟡 | PinkSynth: oscillatori band-limited, unison/detune, sub, noise, filtro, 2 inviluppi, LFO (wavetable e matrice di modulazione ⏳). PinkDrum: kick, snare, clap, hat con choke, 808 (sample import ⏳). PinkKeys, PinkSampler ⏳; PinkBass ⏳ (oggi: preset "Sub Bass" e 808 di PinkDrum) |
| 13 | Registrazione | ⏳ | |
| 14 | Automazione | ⏳ | I parametri hanno già valori normalizzati 0..1 pronti per le automation clip |
| 15 | Mixing e mastering | 🟡 | Canale master, limiter, loudness/true peak/fase ✅. Multiband, imager, dynamic EQ, reference track, A/B, preset di genere ⏳ |
| 16 | Browser | ⏳ | |
| 17 | Progetti e file | 🟡 | WAV ✅, export mixdown ✅; formato progetto, autosave, backup, versioning, collect-and-save, template ⏳ |
| 18 | Funzioni avanzate | 🟡 | Metronomo, tastiere MIDI, monitor CPU, render offline ✅. MIDI learn, macro, tap tempo, PDC, freeze, stem ⏳ |
| 19 | Intelligenza artificiale | ⏳ | Modulo futuro, separato dal motore. Nessun risultato simulato |
| 20 | Performance e stabilità | 🟡 | Callback real-time senza allocazioni né lock (testato), denormali, snapshot sicuri (TSan). Elaborazione multithread, sandboxing dei plugin, crash recovery ⏳ |
| 21 | Schermata principale | 🟡 | Menu, transport (senza Record), playlist, mixer, impostazioni audio, sezione master ✅. Piano roll, step sequencer, browser, plugin manager ⏳ |
| 22 | Scorciatoie | ✅ | Personalizzabili e salvate per tutti i comandi esistenti |
| 23 | Installer | 🟡 | Script Inno Setup con scelta delle 4 cartelle, disinstallazione pulita con scelta sulle impostazioni. Da compilare su Windows (CI) |

## Limitazioni note della versione attuale

- Il limiter controlla il sample peak: possibili overs inter-campione fino a circa +1 dB (vedi report).
- Il furto di voci di PinkSynth oltre 16 note può produrre un piccolo click.
- La latenza del limiter (1,5 ms) non è ancora compensata (PDC); oggi il limiter sta solo sul master.
- L'export avviene sul thread dell'interfaccia (circa 0,5 s per la demo).
