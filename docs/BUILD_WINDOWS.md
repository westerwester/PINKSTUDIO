# Compilazione e installazione su Windows 10/11 x64

## Cosa serve

| Strumento | Versione | Note |
|---|---|---|
| Visual Studio | 2022 o più recente | carico di lavoro **"Sviluppo di applicazioni desktop con C++"** (include MSVC, Windows SDK e CMake) |
| Git | qualsiasi recente | serve a CMake per scaricare JUCE 8.0.15 alla prima configurazione |
| Inno Setup | 6.3 o più recente | solo per creare `PINKSTUDIO_SETUP.exe` |
| Connessione internet | — | solo alla prima configurazione (download di JUCE, ~80 MB) |

Non servono SDK a parte: JUCE 8.0.15 include già gli header ASIO, WASAPI e DirectSound fanno parte
di Windows.

## Strada 1 — GitHub Actions (nessuna installazione sul PC)

1. Crea un repository su GitHub e carica tutto il contenuto di questa cartella (anche la cartella
   nascosta `.github`).
2. Apri la scheda **Actions**: il workflow **Windows build** parte da solo a ogni push (oppure
   avvialo con *Run workflow*).
3. A fine esecuzione (circa 15–25 minuti la prima volta) scarica l'artifact
   **PINKSTUDIO-windows-x64**. Contiene:
   - `PINKSTUDIO_SETUP.exe` — l'installer
   - `PINKSTUDIO/PINKSTUDIO.exe` — versione portatile, senza installazione
   - `test-report.md` — i 39 test del motore eseguiti con il compilatore Microsoft
   - `pinkstudio_demo.wav` — render della demo prodotto sulla macchina Windows
   - `pinkstudio_ui.png` — screenshot di controllo dell'interfaccia

Se un passaggio fallisce, il log della Action indica il file e la riga.

## Strada 2 — Script sul tuo PC

1. Installa Visual Studio (con il carico di lavoro C++) e Git.
2. Doppio clic su `scripts\build_windows.bat` (oppure eseguilo dal *Developer PowerShell for VS*).
3. Lo script configura, compila, esegue i test, fa un render di verifica e, se trova Inno Setup,
   crea l'installer.

Risultati:

```
build\app\PinkStudio_artefacts\Release\PINKSTUDIO.exe
dist\PINKSTUDIO_SETUP.exe
build\test-report.md
```

## Strada 3 — Comandi manuali

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DAppVersion=0.1.0 /DBuildDir=..\build installer\PINKSTUDIO.iss
```

Opzioni CMake utili:

| Opzione | Default | Effetto |
|---|---|---|
| `PINKSTUDIO_ENABLE_ASIO` | ON | driver ASIO |
| `PINKSTUDIO_JUCE_DIR` | vuoto | usa una copia locale di JUCE invece di scaricarla |
| `PINKSTUDIO_BUILD_TESTS` | ON | test del motore |
| `PINKSTUDIO_BUILD_APP` | ON | applicazione (OFF = solo motore e test, senza JUCE) |

Per lavorare in Visual Studio: *File → Apri → Cartella* sulla cartella del progetto (CMake viene
riconosciuto in automatico), poi seleziona la destinazione `PINKSTUDIO.exe`.

## Installazione

Avvia `PINKSTUDIO_SETUP.exe`. L'installer ti chiede:

1. **Cartella di installazione** (default `C:\Program Files\PINKSTUDIO`)
2. **Cartella dei plugin** (default `Documenti\PINKSTUDIO\Plugins`)
3. **Cartella dei progetti** (default `Documenti\PINKSTUDIO\Projects`): è dove si apre l'export WAV
4. **Cartelle VST3 aggiuntive** (facoltative; verranno usate dallo scanner VST3 della fase 7)

Le scelte vengono salvate in `<installazione>\config\paths.xml` e ripresentate agli aggiornamenti.

**Disinstallazione** (Impostazioni → App): rimuove il programma; chiede se eliminare anche le
impostazioni utente (`%APPDATA%\PINKSTUDIO`: driver audio, scorciatoie, tema). Progetti e cartella
plugin non vengono mai cancellati.

## Problemi comuni

| Problema | Soluzione |
|---|---|
| Windows SmartScreen blocca l'installer | L'eseguibile non ha firma digitale: *Ulteriori informazioni → Esegui comunque*. Per distribuirlo a terzi serve un certificato di firma del codice. |
| L'antivirus mette in quarantena l'exe appena compilato | Falso positivo frequente con exe non firmati: aggiungi un'eccezione per la cartella `build`. |
| `CMake non trovato` | Usa il *Developer PowerShell for VS* oppure installa CMake da cmake.org. |
| Errore durante il download di JUCE | Controlla la connessione/proxy; in alternativa clona JUCE (tag 8.0.15) e passa `-DPINKSTUDIO_JUCE_DIR=C:\percorso\JUCE`. |
| Percorsi troppo lunghi | Metti il progetto in una cartella corta, per esempio `C:\dev\PINKSTUDIO`. |
| Nessun suono | Premi F10, scegli il driver (ASIO della tua scheda se disponibile, altrimenti *Windows Audio*), verifica le uscite 1/2 e premi *Test*. |
| Crepitii | Aumenta il buffer (256 o 512 campioni) oppure usa ASIO/WASAPI esclusivo. |
