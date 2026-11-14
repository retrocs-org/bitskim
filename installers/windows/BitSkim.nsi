Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!define ROOT "../.."
!ifndef PRODUCT
!define PRODUCT "BitSkim"
!endif
!ifndef PREVIOUS_APP_MUTEX
!define PREVIOUS_APP_MUTEX "Local\BitSkim0.1"
!endif
!define KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}"
!ifndef APP_MUTEX
!define APP_MUTEX "Local\BitSkim0.2"
!endif
!ifndef OUTPUT
!define OUTPUT "${ROOT}/outputs/BitSkim-0.2-Windows10-x64-Setup.exe"
!endif
Name "${PRODUCT} 0.2"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\${PRODUCT}"
RequestExecutionLevel admin
SetCompressor /SOLID zlib
VIProductVersion "0.2.0.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "ProductVersion" "0.2"
VIAddVersionKey "FileVersion" "0.2"
VIAddVersionKey "FileDescription" "BitSkim 0.2 all-users installer"
VIAddVersionKey "LegalCopyright" "BitSkim contributors; see included licenses"
!define MUI_ICON "${ROOT}/assets/icons/BitSkim.ico"
!define MUI_UNICON "${ROOT}/assets/icons/BitSkim.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "Install BitSkim 0.2 in Program Files for all users.$\r$\n$\r$\nClose BitSkim, including its tray icon, before updating.$\r$\n$\r$\nYour preferences and received files are preserved. Administrator approval is required for program files; each user keeps their own AppData settings."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${ROOT}/LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\BitSkim.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!macro CheckMutex MUTEX_NAME
 System::Call 'kernel32::OpenMutexW(i 0x100000, i 0, w "${MUTEX_NAME}") p.r0'
 ${If} $0 != 0
  System::Call 'kernel32::CloseHandle(p r0)'
  MessageBox MB_OK|MB_ICONEXCLAMATION "Close BitSkim using Quit in its tray menu, then run this installer again." /SD IDOK
  SetErrorLevel 1
  Abort
 ${EndIf}
!macroend
!macro CheckRunning
 !insertmacro CheckMutex "${APP_MUTEX}"
 !insertmacro CheckMutex "${PREVIOUS_APP_MUTEX}"
!macroend
Function .onInit
 ${IfNot} ${AtLeastWin10}
  MessageBox MB_OK|MB_ICONSTOP "BitSkim requires Windows 10 or later." /SD IDOK
  SetErrorLevel 1
  Abort
 ${EndIf}
 ${IfNot} ${RunningX64}
  MessageBox MB_OK|MB_ICONSTOP "This package requires a 64-bit Windows system with x64 application support." /SD IDOK
  SetErrorLevel 1
  Abort
 ${EndIf}
 SetShellVarContext all
 SetRegView 64
 ReadRegStr $1 HKCU "${KEY}" "InstallLocation"
 ${If} $1 != ""
  MessageBox MB_OK|MB_ICONEXCLAMATION "An older per-user BitSkim installation was found at $1. Uninstall that copy first, then run this installer. Your AppData preferences and received files are preserved." /SD IDOK
  SetErrorLevel 1
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
 SetRegView 64
 !insertmacro CheckRunning
FunctionEnd
Section "BitSkim (required)" SEC_MAIN
 SectionIn RO
 SetOutPath "$INSTDIR"
 SetOverwrite on
 File "${ROOT}/build/windows/BitSkim.exe"
 File /oname=BUILD-PROVENANCE.json "${ROOT}/build/provenance/windows.json"
 File /oname=README.MD "${ROOT}/docs/WINDOWS.md"
  File "../../docs/TRANSFER-TOOLS.md"
 File /oname=TRANSFERS.md "${ROOT}/docs/TRANSFERS.md"
 File "${ROOT}/LICENSE"
 File "${ROOT}/THIRD-PARTY-NOTICES.MD"
 File "${ROOT}/assets/themes/CATPPUCCIN-LICENSE"
 File "${ROOT}/assets/licenses/OpenSSL-LICENSE"
 File "${ROOT}/assets/licenses/NSIS-LICENSE"
 File /oname=INSTALLATION.md "${ROOT}/docs/INSTALLATION.md"
 SetOutPath "$INSTDIR\Tools"
 File "${ROOT}/build/windows/BitSkim-CoreCheck.exe"
 SetOutPath "$INSTDIR"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKLM "${KEY}" "DisplayName" "${PRODUCT}"
 WriteRegStr HKLM "${KEY}" "DisplayVersion" "0.2"
 WriteRegStr HKLM "${KEY}" "Publisher" "BitSkim contributors"
 WriteRegStr HKLM "${KEY}" "InstallLocation" "$INSTDIR"
 WriteRegStr HKLM "${KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegStr HKLM "${KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
 WriteRegStr HKLM "${KEY}" "DisplayIcon" "$INSTDIR\BitSkim.exe,0"
 WriteRegStr HKLM "${KEY}" "URLInfoAbout" "https://github.com/retrocs-org/bitskim"
 WriteRegDWORD HKLM "${KEY}" "NoModify" 1
 WriteRegDWORD HKLM "${KEY}" "NoRepair" 1
 CreateDirectory "$SMPROGRAMS\${PRODUCT}"
 CreateShortCut "$SMPROGRAMS\${PRODUCT}\${PRODUCT}.lnk" "$INSTDIR\BitSkim.exe"
 CreateShortCut "$SMPROGRAMS\${PRODUCT}\Uninstall ${PRODUCT}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd
Section /o "Desktop shortcut" SEC_DESKTOP
 CreateShortCut "$DESKTOP\${PRODUCT}.lnk" "$INSTDIR\BitSkim.exe"
 WriteRegDWORD HKLM "${KEY}" "DesktopShortcut" 1
SectionEnd
Section "Uninstall"
 ClearErrors
 Delete "$INSTDIR\BitSkim.exe"
 ${If} ${Errors}
  MessageBox MB_OK|MB_ICONSTOP "BitSkim could not be removed. Close the app, then try again." /SD IDOK
  SetErrorLevel 1
  Abort
 ${EndIf}
 Delete "$INSTDIR\Tools\BitSkim-CoreCheck.exe"
 RMDir "$INSTDIR\Tools"
 Delete "$INSTDIR\README.MD"
  Delete "$INSTDIR\TRANSFER-TOOLS.md"
 Delete "$INSTDIR\TRANSFERS.md"
 Delete "$INSTDIR\BUILD-PROVENANCE.json"
 Delete "$INSTDIR\LICENSE"
 Delete "$INSTDIR\THIRD-PARTY-NOTICES.MD"
 Delete "$INSTDIR\CATPPUCCIN-LICENSE"
 Delete "$INSTDIR\OpenSSL-LICENSE"
 Delete "$INSTDIR\NSIS-LICENSE"
 Delete "$INSTDIR\INSTALLATION.md"
 Delete "$INSTDIR\Uninstall.exe"
 Delete "$SMPROGRAMS\${PRODUCT}\${PRODUCT}.lnk"
 Delete "$SMPROGRAMS\${PRODUCT}\Uninstall ${PRODUCT}.lnk"
 RMDir "$SMPROGRAMS\${PRODUCT}"
 ReadRegDWORD $0 HKLM "${KEY}" "DesktopShortcut"
 ${If} $0 == 1
  Delete "$DESKTOP\${PRODUCT}.lnk"
 ${EndIf}
 DeleteRegKey HKLM "${KEY}"
 ; Deliberately non-recursive: preserve unknown/user files and AppData settings.
 RMDir "$INSTDIR"
SectionEnd
