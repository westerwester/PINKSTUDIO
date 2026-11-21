; PINKSTUDIO - installer Windows x64 (Inno Setup 6.3 o piu' recente)
; Compilazione:  ISCC /DAppVersion=0.1.0 /DBuildDir=..\build installer\PINKSTUDIO.iss
; Output:        dist\PINKSTUDIO_SETUP.exe

#define AppName "PINKSTUDIO"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif
#ifndef Config
  #define Config "Release"
#endif

[Setup]
AppId={{B3F1C6A2-5D4E-4C8B-9A7F-2E6D1C0B9A41}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=PINKSTUDIO
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=PINKSTUDIO_SETUP
SetupIconFile=..\app\Resources\icon.ico
UninstallDisplayIcon={app}\PINKSTUDIO.exe
UninstallDisplayName={#AppName} {#AppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes

[Languages]
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
it.PluginDirTitle=Cartella dei plugin
it.PluginDirDesc=Dove PINKSTUDIO salva preset e plugin stock
it.PluginDirHint=Scegli la cartella dei plugin di PINKSTUDIO, poi premi Avanti.
it.ProjectDirTitle=Cartella dei progetti
it.ProjectDirDesc=Dove salvare progetti ed export audio
it.ProjectDirHint=Scegli la cartella predefinita per i tuoi progetti, poi premi Avanti.
it.Vst3Title=Cartelle VST3 aggiuntive
it.Vst3Desc=Cartelle extra da scansionare per i plugin VST3 (facoltativo)
it.Vst3Hint=La cartella standard C:\Program Files\Common Files\VST3 viene sempre inclusa. Aggiungi qui eventuali altre cartelle, oppure lascia vuoto.
it.Vst3Folder=Cartella
it.Vst3Missing=La cartella "%1" non esiste. Correggila o lascia il campo vuoto.
it.RemoveSettings=Vuoi eliminare anche le impostazioni utente di PINKSTUDIO (driver audio, scorciatoie, tema)?%n%nI progetti NON vengono toccati.
it.Guide=Guida di PINKSTUDIO
en.PluginDirTitle=Plug-in folder
en.PluginDirDesc=Where PINKSTUDIO stores presets and stock plug-ins
en.PluginDirHint=Choose the PINKSTUDIO plug-in folder, then click Next.
en.ProjectDirTitle=Projects folder
en.ProjectDirDesc=Where projects and audio exports are saved
en.ProjectDirHint=Choose the default folder for your projects, then click Next.
en.Vst3Title=Additional VST3 folders
en.Vst3Desc=Extra folders to scan for VST3 plug-ins (optional)
en.Vst3Hint=The standard folder C:\Program Files\Common Files\VST3 is always included. Add other folders here or leave empty.
en.Vst3Folder=Folder
en.Vst3Missing=The folder "%1" does not exist. Fix it or leave the field empty.
en.RemoveSettings=Also delete the PINKSTUDIO user settings (audio driver, shortcuts, theme)?%n%nYour projects are NOT touched.
en.Guide=PINKSTUDIO guide

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Dirs]
Name: "{app}\config"
Name: "{code:GetPluginDir}"; Flags: uninsneveruninstall
Name: "{code:GetProjectDir}"; Flags: uninsneveruninstall

[Files]
Source: "{#BuildDir}\app\PinkStudio_artefacts\{#Config}\PINKSTUDIO.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\tools\pinkrender\{#Config}\pinkrender.exe"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\installer\PLUGINS_README.txt"; DestDir: "{code:GetPluginDir}"; Flags: ignoreversion onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\PINKSTUDIO.exe"
Name: "{autoprograms}\{cm:Guide}"; Filename: "{app}\docs"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\PINKSTUDIO.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PINKSTUDIO.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\config\paths.xml"
Type: dirifempty; Name: "{app}\config"
Type: dirifempty; Name: "{app}"

[Code]
var
  PluginDirPage: TInputDirWizardPage;
  ProjectDirPage: TInputDirWizardPage;
  Vst3Page: TInputQueryWizardPage;

procedure InitializeWizard;
var
  I: Integer;
begin
  PluginDirPage := CreateInputDirPage(wpSelectDir, CustomMessage('PluginDirTitle'),
    CustomMessage('PluginDirDesc'), CustomMessage('PluginDirHint'), False, '');
  PluginDirPage.Add('');
  PluginDirPage.Values[0] := GetPreviousData('PluginDir', ExpandConstant('{userdocs}\PINKSTUDIO\Plugins'));

  ProjectDirPage := CreateInputDirPage(PluginDirPage.ID, CustomMessage('ProjectDirTitle'),
    CustomMessage('ProjectDirDesc'), CustomMessage('ProjectDirHint'), False, '');
  ProjectDirPage.Add('');
  ProjectDirPage.Values[0] := GetPreviousData('ProjectDir', ExpandConstant('{userdocs}\PINKSTUDIO\Projects'));

  Vst3Page := CreateInputQueryPage(ProjectDirPage.ID, CustomMessage('Vst3Title'),
    CustomMessage('Vst3Desc'), CustomMessage('Vst3Hint'));
  for I := 1 to 3 do
  begin
    Vst3Page.Add(CustomMessage('Vst3Folder') + ' ' + IntToStr(I) + ':', False);
    Vst3Page.Values[I - 1] := GetPreviousData('Vst3Dir' + IntToStr(I), '');
  end;
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
var
  I: Integer;
begin
  SetPreviousData(PreviousDataKey, 'PluginDir', PluginDirPage.Values[0]);
  SetPreviousData(PreviousDataKey, 'ProjectDir', ProjectDirPage.Values[0]);
  for I := 1 to 3 do
    SetPreviousData(PreviousDataKey, 'Vst3Dir' + IntToStr(I), Trim(Vst3Page.Values[I - 1]));
end;

function GetPluginDir(Param: String): String;
begin
  Result := PluginDirPage.Values[0];
end;

function GetProjectDir(Param: String): String;
begin
  Result := ProjectDirPage.Values[0];
end;

function XmlEscape(S: String): String;
begin
  Result := S;
  StringChangeEx(Result, '&', '&amp;', True);
  StringChangeEx(Result, '"', '&quot;', True);
  StringChangeEx(Result, '<', '&lt;', True);
  StringChangeEx(Result, '>', '&gt;', True);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  I: Integer;
  Dir: String;
begin
  Result := True;
  if CurPageID = Vst3Page.ID then
    for I := 0 to 2 do
    begin
      Dir := Trim(Vst3Page.Values[I]);
      if (Dir <> '') and (not DirExists(Dir)) then
      begin
        MsgBox(FmtMessage(CustomMessage('Vst3Missing'), [Dir]), mbError, MB_OK);
        Result := False;
        Exit;
      end;
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Lines: TArrayOfString;
  I, N: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    SetArrayLength(Lines, 6);
    Lines[0] := '<?xml version="1.0" encoding="UTF-8"?>';
    Lines[1] := '<PINKSTUDIO_PATHS projects="' + XmlEscape(ProjectDirPage.Values[0]) +
                '" plugins="' + XmlEscape(PluginDirPage.Values[0]) + '">';
    N := 2;
    for I := 0 to 2 do
      if Trim(Vst3Page.Values[I]) <> '' then
      begin
        Lines[N] := '  <VST3_FOLDER path="' + XmlEscape(Trim(Vst3Page.Values[I])) + '"/>';
        N := N + 1;
      end;
    Lines[N] := '</PINKSTUDIO_PATHS>';
    SetArrayLength(Lines, N + 1);
    SaveStringsToUTF8File(ExpandConstant('{app}\config\paths.xml'), Lines, False);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and (not UninstallSilent) then
    if DirExists(ExpandConstant('{userappdata}\PINKSTUDIO')) then
      if MsgBox(CustomMessage('RemoveSettings'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(ExpandConstant('{userappdata}\PINKSTUDIO'), True, True, True);
end;
