; Instalador de La Pauta Tune (VST3, Windows 64 bits) - Inno Setup 6

#define MyAppName "La Pauta Tune"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "La Pauta"
#define VstFolder "La Pauta Tune.vst3"

[Setup]
AppId={{6B1F6C2E-3D54-4B7A-9E0A-4C7A2F9D1A11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DirExistsWarning=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=La-Pauta-Tune-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
CreateUninstallRegKey=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
Source: "..\dist\{#VstFolder}\*"; DestDir: "{commoncf64}\VST3\{#VstFolder}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
FinishedLabel=La Pauta Tune se instaló en la carpeta VST3. Abre tu DAW y vuelve a escanear los plugins para verlo.

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#VstFolder}"
