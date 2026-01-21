[Setup]
AppName=JoyManager
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\JoyManager
DefaultGroupName=JoyManager
UninstallDisplayIcon={app}\joymanager.exe
Compression=lzma2
SolidCompression=yes
OutputDir=.
OutputBaseFilename=JoyManager-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyBuildDir}\joymanager.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\JoyManager"; Filename: "{app}\joymanager.exe"
Name: "{commondesktop}\JoyManager"; Filename: "{app}\joymanager.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\joymanager.exe"; Description: "{cm:LaunchProgram,JoyManager}"; Flags: nowait postinstall skipifsilent
