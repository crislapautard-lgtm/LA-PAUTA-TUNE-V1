#define MyAppName "La Pauta Tune"
#define MyAppVersion "1.0.0"
[Setup]
AppId={{6B1F6C2E-3D54-4B7A-9E0A-4C7A2F9D1A11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher="La Pauta"
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=La-Pauta-Tune-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
[Files]
Source: "..\dist\La Pauta Tune.vst3\*"; DestDir: "{commoncf64}\VST3\La Pauta Tune.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\La Pauta Tune.vst3"
