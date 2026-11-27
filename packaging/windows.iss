#define AppVersion GetEnv("BVOS_VERSION")

[Setup]
AppId=ai.blueview.ReadAloud
AppName=BvOs Read Aloud
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\BvOs Read Aloud
DefaultGroupName=BvOs Read Aloud
UninstallDisplayIcon={app}\bvos-read-aloud.exe
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\artifacts
OutputBaseFilename=bvos-read-aloud-{#AppVersion}-windows-x64
Compression=lzma2
SolidCompression=yes

[Files]
Source: "..\build\windows-package\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\BvOs Read Aloud"; Filename: "{app}\bvos-read-aloud.exe"
