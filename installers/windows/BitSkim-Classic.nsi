; ANSI output is required for Windows 95/98/Me. No external installer plug-ins.
Unicode false
!include "LogicLib.nsh"
!define ROOT "../.."
!define KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\BitSkimClassic"
Name "BitSkim Classic 0.1"
OutFile "${ROOT}/outputs/BitSkim-0.1-Windows-Classic-x86-Setup.exe"
InstallDir "$PROGRAMFILES\BitSkim Classic"
RequestExecutionLevel admin
SetCompressor /SOLID zlib
Icon "${ROOT}/assets/icons/BitSkim-Windows-Classic.ico"
UninstallIcon "${ROOT}/assets/icons/BitSkim-Windows-Classic.ico"
VIProductVersion "0.1.0.0"
VIAddVersionKey /LANG=1033 "ProductName" "BitSkim Classic"
VIAddVersionKey /LANG=1033 "FileVersion" "0.1"
VIAddVersionKey /LANG=1033 "ProductVersion" "0.1"
VIAddVersionKey /LANG=1033 "FileDescription" "BitSkim Classic 0.1 experimental x86 installer"
VIAddVersionKey /LANG=1033 "LegalCopyright" "BitSkim contributors; see included licenses"
LicenseText "Experimental Windows 95/98/Me/XP client. Native runtime testing is pending. Close BitSkim before installing or updating."
Page license
LicenseData "${ROOT}/LICENSE"
Page components
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

!macro CheckRunning
 FindWindow $0 "BitSkimLegacy"
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONEXCLAMATION "Close BitSkim Classic before installing, updating or uninstalling." /SD IDOK
  Abort
 ${EndIf}
!macroend
Function .onInit
 SetShellVarContext all
 ReadRegStr $1 HKCU "${KEY}" "InstallLocation"
 ${If} $1 != ""
  MessageBox MB_OK|MB_ICONEXCLAMATION "An older per-user BitSkim Classic installation was found at $1. Uninstall it before installing the Program Files version. Settings are preserved; save any old BitSkim.ini if you used a portable copy." /SD IDOK
  Abort
 ${EndIf}
 ReadRegStr $0 HKLM "${KEY}" "InstallLocation"
 ${If} $0 != ""
  StrCpy $INSTDIR $0
 ${EndIf}
 !insertmacro CheckRunning
FunctionEnd
Function un.onInit
 SetShellVarContext all
 !insertmacro CheckRunning
FunctionEnd
Section "BitSkim Classic (required)"
 SectionIn RO
 ClearErrors
 SetOutPath "$INSTDIR"
 SetOverwrite on
 File "${ROOT}/build/windows-legacy/BitSkim.exe"
 File /oname=README.MD "${ROOT}/docs/WINDOWS-LEGACY.md"
  File "../../docs/TRANSFER-TOOLS.md"
 File /oname=BUILD-PROVENANCE.json "${ROOT}/build/provenance/windows-legacy.json"
 File "${ROOT}/LICENSE"
 File "${ROOT}/THIRD-PARTY-NOTICES.MD"
 File "${ROOT}/assets/licenses/NSIS-LICENSE"
 File "${ROOT}/assets/themes/CATPPUCCIN-LICENSE"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 ${If} ${Errors}
  MessageBox MB_OK|MB_ICONSTOP "Installation could not be completed. Close BitSkim and choose a writable folder, then try again." /SD IDOK
  Abort
 ${EndIf}
 ; Per-user AppData preferences.ini is never installed or removed.
 WriteRegStr HKLM "${KEY}" "DisplayName" "BitSkim Classic"
 WriteRegStr HKLM "${KEY}" "DisplayVersion" "0.1"
 WriteRegStr HKLM "${KEY}" "InstallLocation" "$INSTDIR"
 WriteRegStr HKLM "${KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegStr HKLM "${KEY}" "DisplayIcon" "$INSTDIR\BitSkim.exe,0"
 WriteRegStr HKLM "${KEY}" "URLInfoAbout" "https://github.com/retrocs-org/bitskim"
 WriteRegDWORD HKLM "${KEY}" "NoModify" 1
 WriteRegDWORD HKLM "${KEY}" "NoRepair" 1
 CreateDirectory "$SMPROGRAMS\BitSkim Classic"
 CreateShortCut "$SMPROGRAMS\BitSkim Classic\BitSkim Classic.lnk" "$INSTDIR\BitSkim.exe"
 CreateShortCut "$SMPROGRAMS\BitSkim Classic\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd
Section /o "Desktop shortcut"
 CreateShortCut "$DESKTOP\BitSkim Classic.lnk" "$INSTDIR\BitSkim.exe"
 WriteRegDWORD HKLM "${KEY}" "DesktopShortcut" 1
SectionEnd
Section "Uninstall"
 ClearErrors
 Delete "$INSTDIR\BitSkim.exe"
 ${If} ${Errors}
  MessageBox MB_OK|MB_ICONSTOP "Could not remove BitSkim. Close it and try again." /SD IDOK
  Abort
 ${EndIf}
 Delete "$INSTDIR\README.MD"
  Delete "$INSTDIR\TRANSFER-TOOLS.md"
 Delete "$INSTDIR\BUILD-PROVENANCE.json"
 Delete "$INSTDIR\LICENSE"
 Delete "$INSTDIR\THIRD-PARTY-NOTICES.MD"
 Delete "$INSTDIR\NSIS-LICENSE"
 Delete "$INSTDIR\CATPPUCCIN-LICENSE"
 Delete "$INSTDIR\Uninstall.exe"
 Delete "$SMPROGRAMS\BitSkim Classic\BitSkim Classic.lnk"
 Delete "$SMPROGRAMS\BitSkim Classic\Uninstall.lnk"
 RMDir "$SMPROGRAMS\BitSkim Classic"
 ReadRegDWORD $0 HKLM "${KEY}" "DesktopShortcut"
 ${If} $0 == 1
  Delete "$DESKTOP\BitSkim Classic.lnk"
 ${EndIf}
 DeleteRegKey HKLM "${KEY}"
 ; Nonrecursive; leave settings and all received/unknown files intact.
 RMDir "$INSTDIR"
SectionEnd
