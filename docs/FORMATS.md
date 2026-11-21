# Formati supportati — PINKSTUDIO 0.1.0

Vengono elencati solo i formati realmente implementati e testati. I formati della fase successiva
sono indicati separatamente.

## Audio

| Formato | Lettura | Scrittura | Dettagli |
|---|---|---|---|
| WAV PCM 8/16/24/32 bit | ✅ motore | ✅ 16 e 24 bit | 16 bit con dither TPDF |
| WAV float 32 bit | ✅ | ✅ | |
| WAV float 64 bit | ✅ | — | |
| WAV `WAVE_FORMAT_EXTENSIBLE` | ✅ | — | |
| AIFF, FLAC, OGG Vorbis | ⏳ fase 11 | ⏳ fase 11 | tramite i decoder/encoder di JUCE |
| MP3 | ⏳ fase 11 (solo lettura) | — | |

Le clip audio vengono riprodotte a qualsiasi frequenza di campionamento con ricampionamento
cubico (testato 44,1 kHz → 48 kHz).

## Driver e dispositivi (Windows)

| Driver | Stato |
|---|---|
| ASIO | ✅ (header ASIO inclusi in JUCE 8.0.15) |
| WASAPI condiviso / esclusivo / condiviso a bassa latenza | ✅ |
| DirectSound | ✅ |
| Sample rate | quelli offerti dalla scheda, 44,1–192 kHz |
| Ingressi / uscite | fino a 16 + 16 selezionabili; il mix esce su 1-2 |

## MIDI

| Funzione | Stato |
|---|---|
| Ingresso MIDI 1.0 da tastiere/controller (note, velocity, pitch bend, CC1, CC64, CC120, CC123) | ✅ |
| MIDI 2.0 | ⏳ (pianificato insieme al MIDI learn) |
| Import/export file `.mid` | ⏳ fase 4 |

## Plugin

| Formato | Stato |
|---|---|
| Plugin stock interni (PinkEQ, PinkComp, PinkLimiter, PinkDelay, PinkUtility, PinkSynth, PinkDrum) | ✅ |
| Hosting VST3 | ⏳ fase 7 |
| VST2 | non previsto, salvo licenza Steinberg valida |

## Progetti e preset

| Formato | Stato |
|---|---|
| Formato progetto PINKSTUDIO, autosave, backup, recovery | ⏳ fase 11 |
| Preset dei plugin stock | ⏳ fase 8 (i parametri hanno già id stabili e valori normalizzati) |
| `config\paths.xml` (cartelle scelte nell'installer) | ✅ |
