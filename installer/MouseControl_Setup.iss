; Inno Setup Script for MouseControl
#define MyAppName "MouseControl"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "MouseControl Project"
#define MyAppExeName "MouseControl.exe"

[Setup]
AppId={{9F7B2F6E-2E7A-4D3B-9BC1-A1E4B5C6D7E8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=MouseControl-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "Launch MouseControl with Windows"; GroupDescription: "Windows Integration:"

[Files]
Source: "..\build\Release\MouseControl.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\device_inspector.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\fmt.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\spdlog.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\sqlite3.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Device Inspector"; Filename: "{app}\device_inspector.exe"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"" --minimized"; Flags: uninsdeletevalue; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
