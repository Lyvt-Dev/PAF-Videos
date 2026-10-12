#define AppVersion GetEnv('PAF_VERSION')
[Setup]
AppId=PAFVideos
AppName=PAF-Videos
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\PAF-Videos
DefaultGroupName=PAF-Videos
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\PAF-Videos.exe
CloseApplications=yes
[Files]
Source: "..\dist\PAF-Videos\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Registry]
Root: HKCU; Subkey: "Software\PAFVideos"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
[Icons]
Name: "{group}\PAF-Videos"; Filename: "{app}\PAF-Videos.exe"
Name: "{autodesktop}\PAF-Videos"; Filename: "{app}\PAF-Videos.exe"
[Run]
Filename: "{app}\PAF-Videos.exe"; Description: "PAF-Videos starten"; Flags: nowait postinstall skipifsilent
