#ifndef PayloadDir
  #error PayloadDir must name the prepared installer payload
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef OutputPath
  #error OutputPath is required
#endif
#ifndef OutputName
  #error OutputName is required
#endif

[Setup]
AppId={{D68E8140-342F-4A36-9767-9FAE9BD4F085}
AppName=Shaderixel Alpha
AppVersion={#AppVersion}
AppVerName=Shaderixel {#AppVersion}
AppPublisher=MrMybal
AppPublisherURL=https://github.com/MrMybal/Shaderixel
AppSupportURL=https://github.com/MrMybal/Shaderixel/issues
AppUpdatesURL=https://github.com/MrMybal/Shaderixel/releases
DefaultDirName={localappdata}\Programs\Shaderixel Alpha
DefaultGroupName=Shaderixel Alpha
DisableProgramGroupPage=auto
DisableDirPage=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.19041
WizardStyle=modern
LanguageDetectionMethod=none
LicenseFile={#PayloadDir}\LICENSE
SetupIconFile=..\assets\shaderixel.ico
UninstallDisplayIcon={app}\Shaderixel.exe
UninstallDisplayName=Shaderixel Alpha
OutputDir={#OutputPath}
OutputBaseFilename={#OutputName}
VersionInfoVersion=0.1.0.0
VersionInfoDescription=Shaderixel Alpha Installer
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
CloseApplicationsFilter=Shaderixel.exe,ShaderixelVk.exe
RestartApplications=no
SetupLogging=yes

[Languages]
Name: english; MessagesFile: compiler:Default.isl
Name: french; MessagesFile: compiler:Languages\French.isl

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Shaderixel Alpha"; Filename: "{app}\Shaderixel.exe"; WorkingDir: "{app}"
Name: "{group}\Shaderixel Alpha (Vulkan)"; Filename: "{app}\ShaderixelVk.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Shaderixel Alpha"; Filename: "{app}\Shaderixel.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Shaderixel.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,Shaderixel Alpha}"; Flags: nowait postinstall skipifsilent

; Only installed files are removed. User-created projects, presets and settings
; are deliberately not matched by recursive uninstall deletion rules.
