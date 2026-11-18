; Windows installer for Raz Burg AudioCrafts plugins (VST3 + AAX).
; Add /DWithAax=1 to include the AAX (only once it is PACE-signed: retail Pro Tools refuses an unsigned AAX).
; Build: ISCC /DPluginName=Lumen /DPluginVersion=0.3.1 /DSourceDir=<dir with X.vst3 and X.aaxplugin> /DOutputDir=<dir> installer.iss

#ifndef PluginName
  #error PluginName is required
#endif
#ifndef PluginVersion
  #define PluginVersion "0.0.0"
#endif

[Setup]
AppId={{RBA-{#PluginName}-Windows}
AppName={#PluginName}
AppVersion={#PluginVersion}
AppPublisher=Raz Burg AudioCrafts
AppPublisherURL=https://audiocrafts.razburg.com
DefaultDirName={commoncf64}\Raz Burg AudioCrafts\{#PluginName}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#PluginName}-{#PluginVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#PluginName} (Raz Burg AudioCrafts)

#ifdef WithAax
  #define FullDesc "VST3 and AAX"
#else
  #define FullDesc "VST3"
#endif

[Types]
Name: "full"; Description: "{#FullDesc}"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 (Cubase, Studio One, Live, ...)"; Types: full custom
#ifdef WithAax
Name: "aax"; Description: "AAX (Pro Tools)"; Types: full custom
#endif

[Files]
Source: "{#SourceDir}\{#PluginName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#PluginName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#ifdef WithAax
Source: "{#SourceDir}\{#PluginName}.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\{#PluginName}.aaxplugin"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#PluginName}.vst3"
Type: filesandordirs; Name: "{commoncf64}\Avid\Audio\Plug-Ins\{#PluginName}.aaxplugin"
