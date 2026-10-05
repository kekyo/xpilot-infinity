; XPilot Infinity installer for packages assembled by package-windows.sh.

!ifndef XPILOT_PACKAGE_DIR
  !error "XPILOT_PACKAGE_DIR is required"
!endif
!ifndef XPILOT_OUTPUT
  !error "XPILOT_OUTPUT is required"
!endif
!ifndef XPILOT_VERSION
  !error "XPILOT_VERSION is required"
!endif
!ifndef XPILOT_FILE_VERSION
  !error "XPILOT_FILE_VERSION is required"
!endif
!ifndef XPILOT_ARCH
  !error "XPILOT_ARCH is required"
!endif
!ifndef XPILOT_ICON
  !error "XPILOT_ICON is required"
!endif
!ifndef XPILOT_SERVER_CONFIG
  !error "XPILOT_SERVER_CONFIG is required"
!endif

!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "Sections.nsh"
!include "x64.nsh"

!define PRODUCT_NAME "XPilot Infinity"
!define PRODUCT_KEY "Software\XPilot Infinity"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\XPilot Infinity"
!define SERVICE_NAME "XPilotInfinityServer"
!define SERVICE_DISPLAY_NAME "XPilot Infinity Server"
!define SERVICE_REGISTRY_KEY "SYSTEM\CurrentControlSet\Services\${SERVICE_NAME}"

Unicode true
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Name "${PRODUCT_NAME} ${XPILOT_VERSION}"
OutFile "${XPILOT_OUTPUT}"
BrandingText "${PRODUCT_NAME}"
ShowInstDetails show
ShowUnInstDetails show
VIProductVersion "${XPILOT_FILE_VERSION}"
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${XPILOT_VERSION}"
VIAddVersionKey /LANG=1033 "FileDescription" "${PRODUCT_NAME} installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${XPILOT_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" \
  "Copyright XPilot Infinity contributors"

!if "${XPILOT_ARCH}" == "x86_64"
  InstallDir "$PROGRAMFILES64\XPilot Infinity"
!else
  InstallDir "$PROGRAMFILES32\XPilot Infinity"
!endif
InstallDirRegKey HKLM "${PRODUCT_KEY}" "InstallDir"

!define MUI_ABORTWARNING
!define MUI_ICON "${XPILOT_ICON}"
!define MUI_UNICON "${XPILOT_ICON}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\xpilot-infinity-sdl.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${XPILOT_PACKAGE_DIR}\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Var ServiceCommand

Section "!XPilot Infinity client and server" SEC_CORE
  SectionIn RO
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  File /r "${XPILOT_PACKAGE_DIR}\*"
  File /oname=icon.ico "${XPILOT_ICON}"

  CreateDirectory "$SMPROGRAMS\XPilot Infinity"
  CreateShortcut \
    "$SMPROGRAMS\XPilot Infinity\XPilot Infinity.lnk" \
    "$INSTDIR\xpilot-infinity-sdl.exe" "" "$INSTDIR\icon.ico" 0 \
    SW_SHOWNORMAL "" "Open the XPilot Infinity server browser"
  CreateShortcut \
    "$SMPROGRAMS\XPilot Infinity\XPilot Infinity Server Tray.lnk" \
    "$INSTDIR\xpilot-infinity-tray.exe" "" "$INSTDIR\icon.ico" 0 \
    SW_SHOWNORMAL "" "Manage the installed XPilot Infinity server service"

  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${PRODUCT_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${XPILOT_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\icon.ico"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "XPilot Infinity contributors"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" \
    '$\"$INSTDIR\uninstall.exe$\"'
  WriteRegStr HKLM "${UNINSTALL_KEY}" "QuietUninstallString" \
    '$\"$INSTDIR\uninstall.exe$\" /S'
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section /o "Dedicated server service (manual start)" SEC_SERVER_SERVICE
  SetShellVarContext all
  CreateDirectory "$APPDATA\XPilot Infinity\server"
  Call ConfigureServerDataPermissions
  IfFileExists \
    "$APPDATA\XPilot Infinity\server\xpilot-infinity-server.conf" \
    server_config_exists
  SetOutPath "$APPDATA\XPilot Infinity\server"
  File /oname=xpilot-infinity-server.conf "${XPILOT_SERVER_CONFIG}"
server_config_exists:
  Call ConfigureServerService
SectionEnd

LangString DESC_CORE ${LANG_ENGLISH} \
  "Install the SDL client, dedicated server, service tray, game data, and Start menu shortcuts."
LangString DESC_SERVER_SERVICE ${LANG_ENGLISH} \
  "Register the dedicated server with Windows. The service uses manual startup and is not started by this installer."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE} $(DESC_CORE)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SERVER_SERVICE} \
    $(DESC_SERVER_SERVICE)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function ConfigureServerDataPermissions
  StrCpy $0 "$APPDATA\XPilot Infinity\server"
  ; LocalService needs write access only to its application data directory.
  ; ProgramData also inherits Users create/write rights. Remove those inherited
  ; grants so the settings helper can safely replace the shared configuration.
  ; Keep ordinary users' read access for the tray and editing copies.
  nsExec::ExecToStack \
    '"$SYSDIR\icacls.exe" "$0" /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" "*S-1-5-19:(OI)(CI)M" "*S-1-5-32-545:(OI)(CI)RX"'
  Pop $1
  Pop $2
  ${If} $1 != 0
    StrCpy $4 $1
    StrCpy $2 "Could not update the server data ACL (icacls exit $1): $2"
    Call FailServiceRegistration
  ${EndIf}
FunctionEnd

Function ConfigureServerService
  StrCpy $0 "$APPDATA\XPilot Infinity\server"
  StrCpy $ServiceCommand \
    '\"$INSTDIR\xpilot-infinity-server.exe\" --windows-service --windows-service-log \"$0\xpilot-infinity-server.log\" -defaultsFileName \"$0\xpilot-infinity-server.conf\"'

  nsExec::ExecToStack \
    '"$SYSDIR\sc.exe" query "${SERVICE_NAME}"'
  Pop $1
  Pop $2
  ${If} $1 = 0
    ; Preserve an existing service's startup mode during an upgrade.
    nsExec::ExecToStack \
      '"$SYSDIR\sc.exe" config "${SERVICE_NAME}" binPath= "$ServiceCommand" obj= "NT AUTHORITY\LocalService" DisplayName= "${SERVICE_DISPLAY_NAME}"'
  ${Else}
    nsExec::ExecToStack \
      '"$SYSDIR\sc.exe" create "${SERVICE_NAME}" binPath= "$ServiceCommand" start= demand obj= "NT AUTHORITY\LocalService" DisplayName= "${SERVICE_DISPLAY_NAME}"'
  ${EndIf}
  Pop $1
  Pop $2
  ${If} $1 != 0
    StrCpy $4 $1
    Call FailServiceRegistration
  ${EndIf}
  DetailPrint \
    "Registered ${SERVICE_DISPLAY_NAME} for manual startup without starting it."
FunctionEnd

Function FailServiceRegistration
  FileOpen $5 "$TEMP\XPilotInfinityInstaller.log" a
  FileWrite $5 \
    "Could not register the XPilot Infinity server service (error $4).$\r$\n"
  FileWrite $5 "Installer command output: $2$\r$\n"
  FileClose $5
  IfSilent service_failure_silent
  MessageBox MB_ICONSTOP|MB_OK \
    "Could not register the XPilot Infinity server service (error $4)."
  Abort
service_failure_silent:
  SetErrorLevel 1
  Quit
FunctionEnd

Function SelectServerService
  SectionGetFlags ${SEC_SERVER_SERVICE} $2
  IntOp $2 $2 | ${SF_SELECTED}
  SectionSetFlags ${SEC_SERVER_SERVICE} $2
FunctionEnd

Function .onInit
  SetShellVarContext all
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}
!if "${XPILOT_ARCH}" == "x86_64"
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP|MB_OK \
      "The x86_64 installer requires 64-bit Windows."
    Abort
  ${EndIf}
!endif

  ReadRegStr $0 HKLM "${SERVICE_REGISTRY_KEY}" "ImagePath"
  ${If} $0 != ""
    Call SelectServerService
  ${EndIf}
  ${GetParameters} $0
  ${GetOptions} $0 "/SERVER_SERVICE=" $1
  ${If} $1 == "1"
    Call SelectServerService
  ${EndIf}
FunctionEnd

Section "Uninstall"
  SetShellVarContext all
  nsExec::ExecToLog '"$SYSDIR\sc.exe" stop "${SERVICE_NAME}"'
  Pop $0
  Sleep 1500
  nsExec::ExecToLog '"$SYSDIR\sc.exe" delete "${SERVICE_NAME}"'
  Pop $0

  Delete "$SMPROGRAMS\XPilot Infinity\XPilot Infinity.lnk"
  Delete "$SMPROGRAMS\XPilot Infinity\XPilot Infinity Server Tray.lnk"
  RMDir "$SMPROGRAMS\XPilot Infinity"
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
  DeleteRegKey HKLM "${PRODUCT_KEY}"
  RMDir /r "$INSTDIR"
  ; Server configuration and logs under ProgramData are intentionally kept.
SectionEnd

Function un.onInit
  SetShellVarContext all
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}
FunctionEnd
