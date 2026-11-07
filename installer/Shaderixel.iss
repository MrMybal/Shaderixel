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

[InstallDelete]
; Examples of earlier versions, now in Exemples\2D and Exemples\3D (only the shipped files; the folder of the
; UI documents goes when it is left empty).
Type: files; Name: "{app}\Exemples\01_Calques_et_effets.sxl"
Type: files; Name: "{app}\Exemples\02_Groupes_et_ecretage.sxl"
Type: files; Name: "{app}\Exemples\03_Formes_et_styles.sxl"
Type: files; Name: "{app}\Exemples\04_Texte.sxl"
Type: files; Name: "{app}\Exemples\05_Mode_nodal.sxl"
Type: files; Name: "{app}\Exemples\06_Graphe_d_effet.sxl"
Type: files; Name: "{app}\Exemples\07_Courbes.sxl"
Type: files; Name: "{app}\Exemples\08_Blend_If.sxl"
Type: files; Name: "{app}\Exemples\09_Fluidite.sxl"
Type: files; Name: "{app}\Exemples\10_Trace_plume.sxl"
Type: files; Name: "{app}\Exemples\11_Selection_rapide.sxl"
Type: files; Name: "{app}\Exemples\12_LUT_etalonnage.sxl"
Type: files; Name: "{app}\Exemples\13_Materiau_PBR.sxl"
Type: files; Name: "{app}\Exemples\14_Deformation.sxl"
Type: files; Name: "{app}\Exemples\15_Trace_depuis_selection.sxl"
Type: files; Name: "{app}\Exemples\16_Tasse_Paint3D.sx3d"
Type: files; Name: "{app}\Exemples\17_Tonneau_Paint3D.sx3d"
Type: files; Name: "{app}\Exemples\18_Vase_grec_Paint3D.sx3d"
Type: files; Name: "{app}\Exemples\19_Coffre_Paint3D.sx3d"
Type: files; Name: "{app}\Exemples\Calques_Photoshop.psd"
Type: files; Name: "{app}\Exemples\Ciel.hdr"
Type: files; Name: "{app}\Exemples\Coffre.obj"
Type: files; Name: "{app}\Exemples\LUT_Sarcelle_Orange.cube"
Type: files; Name: "{app}\Exemples\Tasse.obj"
Type: files; Name: "{app}\Exemples\Tonneau.obj"
Type: files; Name: "{app}\Exemples\Tore.glb"
Type: files; Name: "{app}\Exemples\Vase.obj"
Type: files; Name: "{app}\examples\ui_toolkit_demo.sxl"
Type: files; Name: "{app}\examples\ui_uv_library_demo.sxl"
Type: dirifempty; Name: "{app}\examples"

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Shaderixel Alpha"; Filename: "{app}\Shaderixel.exe"; WorkingDir: "{app}"
Name: "{group}\Shaderixel Alpha (Vulkan)"; Filename: "{app}\ShaderixelVk.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Shaderixel Alpha"; Filename: "{app}\Shaderixel.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{code:GetLaunchExecutable}"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,Shaderixel Alpha}"; Flags: nowait postinstall skipifsilent

; Only installed files are removed. User-created projects, presets and settings
; are deliberately not matched by recursive uninstall deletion rules.

[Code]
function GetLaunchExecutable(Param: String): String;
begin
  if CompareText(ExpandConstant('{param:Backend|d3d12}'), 'vulkan') = 0 then
    Result := ExpandConstant('{app}\ShaderixelVk.exe')
  else
    Result := ExpandConstant('{app}\Shaderixel.exe');
end;
