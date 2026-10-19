; -*- coding: utf-8 -*-
Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "WordFunc.nsh"

!define APP_NAME "街头骑手（试玩版）"
!define APP_KEY "Software\CityCourierPlaytest"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\CityCourierPlaytest"
!define INSTALL_ID "67317578-4bc5-4d28-b5da-8b3023f474d4-preview"
Name "${APP_NAME}"
OutFile "${SETUP_FILE}"
InstallDir "$LOCALAPPDATA\Programs\CityCourierPlaytest"
RequestExecutionLevel user
; Deflate keeps offline playtest rebuilds fast, including already-compressed
; Unreal pak/IoStore data. The payload remains a single self-contained EXE.
SetCompressor /SOLID zlib
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "0.0.1.0"
VIAddVersionKey /LANG=2052 "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=2052 "FileDescription" "街头骑手 Windows 离线试玩安装包"
VIAddVersionKey /LANG=2052 "FileVersion" "0.0.1.0"
VIAddVersionKey /LANG=2052 "LegalCopyright" "CityCourier contributors"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "安装街头骑手试玩版"
!define MUI_WELCOMEPAGE_TEXT "安装后即可试玩，无需安装虚幻引擎。$\r$\n$\r$\n支持 Windows 10/11 64 位，需要支持 DirectX 12 的显卡。安装包包含所需的 Visual C++ 运行库，可离线安装。$\r$\n$\r$\n请保留足够磁盘空间，并在安装前关闭游戏。"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\CityCourier.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_RUN_TEXT "启动街头骑手"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "本试玩版需要 64 位 Windows 10 或 Windows 11。" /SD IDOK
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "本试玩版需要 Windows 10 或 Windows 11。" /SD IDOK
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext current
FunctionEnd

Section "游戏文件与运行库（必需）" Main
  SectionIn RO
  SetOutPath "$INSTDIR"
  !include "${INSTALL_INCLUDE}"
  WriteINIStr "$INSTDIR\.citycourier-install.ini" "Install" "Id" "${INSTALL_ID}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${APP_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${BUILD_TAG}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" ${INSTALLED_KB}
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1

  ; Skip a compatible/newer runtime. A fresh machine installs the bundled
  ; redistributable without needing a network connection or an engine SDK.
  ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
  StrCmp $0 1 0 runtime_install
  ReadRegStr $1 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Version"
  StrCpy $2 $1 1
  StrCmp $2 "v" 0 +2
    StrCpy $1 $1 "" 1
  ${VersionCompare} $1 "${VC_MIN_VERSION}" $2
  StrCmp $2 2 runtime_install runtime_done
runtime_install:
  DetailPrint "正在安装 Visual C++ x64 运行库……"
  ClearErrors
  ExecShellWait "runas" "$INSTDIR\Engine\Extras\Redist\en-us\vc_redist.x64.exe" "/install /quiet /norestart" SW_SHOWNORMAL $0
  IfErrors runtime_failed
  StrCmp $0 0 runtime_done
  StrCmp $0 3010 runtime_reboot
  StrCmp $0 1638 runtime_done
runtime_failed:
  MessageBox MB_OK|MB_ICONEXCLAMATION "运行库安装未完成。请在安装目录 Engine\Extras\Redist\en-us 中手动运行 vc_redist.x64.exe，再启动游戏。" /SD IDOK
  SetErrorLevel 1
  Goto runtime_done
runtime_reboot:
  SetRebootFlag true
runtime_done:
  CreateDirectory "$SMPROGRAMS\街头骑手试玩"
  SetOutPath "$INSTDIR"
  CreateShortCut "$SMPROGRAMS\街头骑手试玩\街头骑手.lnk" "$INSTDIR\CityCourier.exe"
  CreateShortCut "$SMPROGRAMS\街头骑手试玩\卸载.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "桌面快捷方式" Desktop
  SetOutPath "$INSTDIR"
  CreateShortCut "$DESKTOP\街头骑手试玩.lnk" "$INSTDIR\CityCourier.exe"
SectionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext current
  ReadINIStr $0 "$INSTDIR\.citycourier-install.ini" "Install" "Id"
  StrCmp $0 "${INSTALL_ID}" marker_ok
    MessageBox MB_OK|MB_ICONSTOP "找不到本游戏的安装标记，已取消卸载。请从原安装目录运行卸载程序。" /SD IDOK
    Abort
marker_ok:
FunctionEnd

Section "Uninstall"
  ; Only remove the exact files shipped in this build. Never recursively
  ; remove a user-selected directory or the player's AppData save games.
  !include "${UNINSTALL_INCLUDE}"
  Delete "$INSTDIR\.citycourier-install.ini"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$DESKTOP\街头骑手试玩.lnk"
  Delete "$SMPROGRAMS\街头骑手试玩\街头骑手.lnk"
  Delete "$SMPROGRAMS\街头骑手试玩\卸载.lnk"
  RMDir "$SMPROGRAMS\街头骑手试玩"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  DeleteRegKey HKCU "${APP_KEY}"
SectionEnd
