#define MyAppName "LA PAUTA TUNE V1"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "La Pauta"
#define MyAppExeName "LaPautaTune_Standalone.exe"

[Setup]
AppId={{A9B8C7D6-E5F4-4321-9876-1234567890AB}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\La Pauta\La Pauta Tune V1
DefaultGroupName={#MyAppName}
OutputDir=..\installer-output
OutputBaseFilename=LaPautaTuneV1_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}

[Tasks]
Name: "desktopicon"; Description: "Crear acceso directo en el escritorio"; GroupDescription: "Accesos directos:"

[Files]
; GitHub Actions copies the compiled JUCE Standalone executable here.
Source: "..\package\Standalone\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; VST3 is installed to the standard 64-bit VST3 location when present.
Source: "..\package\VST3\*"; DestDir: "{commoncf}\VST3\La Pauta Tune V1.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\LA PAUTA TUNE V1"; Filename: "{app}\{#MyAppExeName}"; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))
Name: "{autodesktop}\LA PAUTA TUNE V1"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir LA PAUTA TUNE V1"; Flags: postinstall nowait skipifsilent; Check: FileExists(ExpandConstant('{app}\{#MyAppExeName}'))
