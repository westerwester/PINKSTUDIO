@echo off
rem PINKSTUDIO - build locale per Windows 10/11 x64
rem Requisiti: Visual Studio 2022 o piu' recente con "Sviluppo di applicazioni desktop con C++", Git.
rem Facoltativo: Inno Setup 6 per creare PINKSTUDIO_SETUP.exe
setlocal
cd /d "%~dp0\.."

where git >nul 2>nul
if errorlevel 1 goto :no_git

where cmake >nul 2>nul
if not errorlevel 1 goto :have_cmake
rem CMake non e' nel PATH: provo quello incluso in Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_cmake
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "VS_CMAKE=%%i"
if not defined VS_CMAKE goto :no_cmake
for %%d in ("%VS_CMAKE%") do set "PATH=%%~dpd;%PATH%"

:have_cmake
echo [PINKSTUDIO] 1/5 Configurazione (la prima volta scarica JUCE, serve internet)...
cmake -S . -B build -A x64 -DPINKSTUDIO_ENABLE_ASIO=ON
if errorlevel 1 goto :failed

echo [PINKSTUDIO] 2/5 Compilazione Release...
cmake --build build --config Release --parallel
if errorlevel 1 goto :failed

echo [PINKSTUDIO] 3/5 Test del motore audio...
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 goto :failed
build\tests\Release\pinkstudio_tests.exe --report build\test-report.md >nul

echo [PINKSTUDIO] 4/5 Render di verifica della demo...
build\tools\pinkrender\Release\pinkrender.exe --demo build\pinkstudio_demo.wav
if errorlevel 1 goto :failed

echo [PINKSTUDIO] 5/5 Installer...
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" goto :no_inno
"%ISCC%" /DAppVersion=0.1.0 /DBuildDir=..\build installer\PINKSTUDIO.iss
if errorlevel 1 goto :failed
goto :done

:no_inno
echo [PINKSTUDIO] Inno Setup 6 non trovato: salto l'installer. Scaricalo da jrsoftware.org se ti serve.

:done
echo.
echo [PINKSTUDIO] Completato.
echo   Programma : build\app\PinkStudio_artefacts\Release\PINKSTUDIO.exe
echo   Installer : dist\PINKSTUDIO_SETUP.exe  - solo se Inno Setup e' installato
echo   Test      : build\test-report.md
exit /b 0

:no_git
echo [PINKSTUDIO] Git non trovato. Installalo da https://git-scm.com e riprova.
exit /b 1

:no_cmake
echo [PINKSTUDIO] CMake non trovato. Installa Visual Studio con il carico di lavoro C++ oppure CMake da cmake.org.
exit /b 1

:failed
echo.
echo [PINKSTUDIO] ERRORE: il passaggio precedente non e' riuscito. Leggi i messaggi qui sopra.
exit /b 1
