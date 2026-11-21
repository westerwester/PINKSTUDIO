# Licenze delle dipendenze

> Nota: queste informazioni servono a orientarti, non sono una consulenza legale. Prima di vendere o
> distribuire PINKSTUDIO verifica i termini aggiornati sui siti dei produttori.

| Componente | Uso in PINKSTUDIO | Licenza |
|---|---|---|
| Motore PINKSTUDIO (`engine/`, `app/`, `tests/`, `tools/`) | tutto il codice di questo progetto | tuo: scegli tu la licenza |
| [JUCE 8.0.15](https://juce.com) | GUI, driver audio/MIDI, scaricato da CMake | doppia licenza: **AGPLv3** oppure licenza commerciale JUCE |
| Header ASIO (inclusi in JUCE 8.0.15) | driver ASIO su Windows (`PINKSTUDIO_ENABLE_ASIO=ON`) | Steinberg ASIO SDK: **GPLv3** oppure licenza proprietaria Steinberg |
| Libreria standard C++ / runtime MSVC | collegata staticamente | licenza di Visual Studio (ridistribuibile) |
| Inno Setup | solo per creare l'installer (non viene distribuito dentro PINKSTUDIO) | licenza Inno Setup (verifica i termini sul sito jrsoftware.org) |

## Cosa significa in pratica

- **Uso personale**: puoi compilare e usare PINKSTUDIO liberamente.
- **Distribuire l'eseguibile a terzi** con la licenza gratuita di JUCE: PINKSTUDIO deve essere
  distribuito con licenza compatibile con l'AGPLv3, sorgente incluso. Con ASIO attivo valgono anche
  i termini GPLv3 degli header ASIO (compatibili con l'AGPLv3).
- **Prodotto commerciale a sorgente chiuso**: servono una licenza commerciale JUCE e la licenza
  proprietaria ASIO di Steinberg (oppure compilare con `-DPINKSTUDIO_ENABLE_ASIO=OFF`).

## Algoritmi e riferimenti

Implementati da zero a partire da documentazione pubblica, nessun codice copiato:

- filtri biquad: formule dell'"Audio EQ Cookbook" (R. Bristow-Johnson)
- K-weighting, gating e true peak: ITU-R BS.1770-4 / EBU R128
- compressore: computer di guadagno con soft knee (Giannoulis, Massberg, Reiss, JAES 2012)
- filtro di stato variabile TPT (Zavalishin, "The Art of VA Filter Design")
- oscillatori band-limited con polyBLEP
- frequenze dei piatti in stile 808: valori noti dei circuiti analogici
