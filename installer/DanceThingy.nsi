; DanceThingy installer (NSIS 3.x, Unicode)
;
; Default build installs:
;   Standalone : %ProgramFiles%\DanceThingy\DanceOverlay.exe
;   VST3       : %CommonProgramFiles%\VST3\DanceThingy.vst3
;
; Test build (makensis /DTESTBUILD) uses %TEMP% and HKCU so it can be
; silently installed and removed without administrator rights.

Unicode True
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!define APP_NAME "DanceThingy"
!define APP_VERSION "0.3.0"
!define APP_VERSION4 "0.3.0.0"
!define APP_PUBLISHER "MusicalThingy"
!define APP_EXE "DanceOverlay.exe"
!define VST3_BUNDLE "DanceThingy.vst3"

!ifdef TESTBUILD
  !define INSTALL_DIR      "$TEMP\DanceThingyTestApp"
  !define VST3_DIR         "$TEMP\DanceThingyTestVST3"
  !define STARTMENU_DIR    "$TEMP\DanceThingyTestMenu"
  !define DESKTOP_LNK      "$TEMP\DanceOverlayTest.lnk"
  !define UNINST_KEY       "Software\DanceThingyTest"
  !define LANG_REG_ROOT    "HKCU"
  !define LANG_REG_KEY     "Software\DanceThingyTest"
  !define UNINST_ROOT      "HKCU"
  !define OUT_FILE         "..\dist\DanceThingy-${APP_VERSION}-Setup-test.exe"
  !define PRODUCT_LABEL    "${APP_NAME} ${APP_VERSION} (Test)"
  RequestExecutionLevel user
!else
  !define INSTALL_DIR      "$PROGRAMFILES64\${APP_NAME}"
  !define VST3_DIR         "$COMMONFILES64\VST3"
  !define STARTMENU_DIR    "$SMPROGRAMS\${APP_NAME}"
  !define DESKTOP_LNK      "$DESKTOP\DanceOverlay.lnk"
  !define UNINST_KEY       "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
  !define LANG_REG_ROOT    "HKLM"
  !define LANG_REG_KEY     "Software\${APP_NAME}"
  !define UNINST_ROOT      "HKLM"
  !define OUT_FILE         "..\dist\DanceThingy-${APP_VERSION}-Setup.exe"
  !define PRODUCT_LABEL    "${APP_NAME} ${APP_VERSION}"
  RequestExecutionLevel admin
!endif

Name "${PRODUCT_LABEL}"
OutFile "${OUT_FILE}"
InstallDir "${INSTALL_DIR}"
InstallDirRegKey ${UNINST_ROOT} "${UNINST_KEY}" "InstallLocation"
BrandingText "${PRODUCT_LABEL}"
SetCompressor /SOLID lzma
SetCompressorDictSize 32
SetDatablockOptimize on
CRCCheck on
XPStyle on

VIProductVersion "${APP_VERSION4}"
VIAddVersionKey /LANG=1033 "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=1033 "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=1033 "FileDescription" "${APP_NAME} Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "GPL-3.0"

!define MUI_ABORTWARNING
!define MUI_LANGDLL_REGISTRY_ROOT "${LANG_REG_ROOT}"
!define MUI_LANGDLL_REGISTRY_KEY "${LANG_REG_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch DanceOverlay"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "Japanese"

; ---- localized strings -----------------------------------------------------
LangString SecAppName    ${LANG_English}    "DanceOverlay (standalone)"
LangString SecVstName    ${LANG_English}    "DanceThingy VST3 plugin"
LangString SecStartName  ${LANG_English}    "Start Menu shortcuts"
LangString SecDesktopName ${LANG_English}   "Desktop shortcut"
LangString DescApp       ${LANG_English}    "Transparent, always-on-top desktop character (DanceOverlay.exe)."
LangString DescVst       ${LANG_English}    "Copy the VST3 bundle into the shared system VST3 folder for your DAW."
LangString DescStart     ${LANG_English}    "Create Start Menu entries for DanceOverlay and the uninstaller."
LangString DescDesktop   ${LANG_English}    "Create a desktop shortcut for DanceOverlay."
LangString AppRunningPrompt ${LANG_English} "DanceOverlay is running. Please close it and run Setup again."
LangString DawRunningPrompt ${LANG_English} "A DAW appears to be using DanceThingy.vst3. Please close the DAW and run Setup again."

LangString SecAppName    ${LANG_SimpChinese} "DanceOverlay（独立悬浮窗）"
LangString SecVstName    ${LANG_SimpChinese} "DanceThingy VST3 插件"
LangString SecStartName  ${LANG_SimpChinese} "开始菜单快捷方式"
LangString SecDesktopName ${LANG_SimpChinese} "桌面快捷方式"
LangString DescApp       ${LANG_SimpChinese} "透明、置顶的桌面小人（DanceOverlay.exe）。"
LangString DescVst       ${LANG_SimpChinese} "把 VST3 拷贝到系统公共 VST3 目录，供 DAW 加载。"
LangString DescStart     ${LANG_SimpChinese} "在开始菜单创建 DanceOverlay 与卸载入口。"
LangString DescDesktop   ${LANG_SimpChinese} "在桌面创建 DanceOverlay 快捷方式。"
LangString AppRunningPrompt ${LANG_SimpChinese} "DanceOverlay 正在运行。请先退出它，再重新运行安装程序。"
LangString DawRunningPrompt ${LANG_SimpChinese} "有 DAW 正在占用 DanceThingy.vst3。请先关闭 DAW，再重新运行安装程序。"

LangString SecAppName    ${LANG_Japanese} "DanceOverlay（スタンドアロン）"
LangString SecVstName    ${LANG_Japanese} "DanceThingy VST3 プラグイン"
LangString SecStartName  ${LANG_Japanese} "スタートメニューのショートカット"
LangString SecDesktopName ${LANG_Japanese} "デスクトップのショートカット"
LangString DescApp       ${LANG_Japanese} "透過・常時最前面のデスクトップキャラクター（DanceOverlay.exe）。"
LangString DescVst       ${LANG_Japanese} "VST3 をシステムの共通 VST3 フォルダへコピーします。"
LangString DescStart     ${LANG_Japanese} "スタートメニューに DanceOverlay とアンインストールへの入口を作成します。"
LangString DescDesktop   ${LANG_Japanese} "デスクトップに DanceOverlay のショートカットを作成します。"
LangString AppRunningPrompt ${LANG_Japanese} "DanceOverlay が実行中です。終了してからセットアップを再実行してください。"
LangString DawRunningPrompt ${LANG_Japanese} "DAW が DanceThingy.vst3 を読み込んでいる可能性があります。DAW を終了してからセットアップを再実行してください。"

; ---- install sections ------------------------------------------------------
!ifndef TESTBUILD
Section -Runtime
  InitPluginsDir
  ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
  ${If} $0 != 1
    SetOutPath "$PLUGINSDIR"
    File "..\build\redist\vc_redist.x64.exe"
    DetailPrint "Installing Microsoft Visual C++ 2015-2022 Redistributable..."
    ExecWait '"$PLUGINSDIR\vc_redist.x64.exe" /install /quiet /norestart' $1
    ${If} $1 != 0
    ${AndIf} $1 != 3010
    ${AndIf} $1 != 1638
      MessageBox MB_ICONSTOP "Microsoft Visual C++ Runtime installation failed (code $1). DanceThingy may not start."
    ${EndIf}
  ${EndIf}
SectionEnd
!endif

Section "$(SecAppName)" SecApp
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "..\build\DanceOverlay_artefacts\Release\${APP_EXE}"
  File /oname=LICENSE.txt "..\LICENSE"
  SetOutPath "$INSTDIR\Docs"
  File "..\README.md"
  File "..\README.zh-CN.md"
  File "..\README.ja.md"
  SetOutPath "$INSTDIR"
SectionEnd

Section "$(SecVstName)" SecVst
  SectionIn RO
  SetOutPath "${VST3_DIR}\${VST3_BUNDLE}"
  File /r "..\build\DanceThingy_artefacts\Release\VST3\${VST3_BUNDLE}\*.*"
SectionEnd

Section "$(SecStartName)" SecStart
  CreateDirectory "${STARTMENU_DIR}"
  CreateShortCut "${STARTMENU_DIR}\DanceOverlay.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortCut "${STARTMENU_DIR}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section /o "$(SecDesktopName)" SecDesktop
  CreateShortCut "${DESKTOP_LNK}" "$INSTDIR\${APP_EXE}"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "DisplayName" "${PRODUCT_LABEL}"
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr ${UNINST_ROOT} "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD ${UNINST_ROOT} "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD ${UNINST_ROOT} "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD ${UNINST_ROOT} "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} "$(DescApp)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVst} "$(DescVst)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStart} "$(DescStart)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "$(DescDesktop)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---- install / uninstall hooks --------------------------------------------
Function .onInit
  !ifdef TESTBUILD
    SetShellVarContext current
  !else
    SetShellVarContext all
  !endif
  SetRegView 64
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "DanceThingy requires 64-bit Windows."
    Abort
  ${EndIf}
  !insertmacro MUI_LANGDLL_DISPLAY
  Call CheckLocks
FunctionEnd

Function un.onInit
  !ifdef TESTBUILD
    SetShellVarContext current
  !else
    SetShellVarContext all
  !endif
  SetRegView 64
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Function CheckLocks
  ${If} ${Silent}
    nsExec::Exec 'taskkill /IM ${APP_EXE} /F'
  ${Else}
    ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
      FileOpen $0 "$INSTDIR\${APP_EXE}" a
      ${If} $0 == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION "$(AppRunningPrompt)"
        Abort
      ${EndIf}
      FileClose $0
    ${EndIf}
    ${If} ${FileExists} "${VST3_DIR}\${VST3_BUNDLE}\Contents\x86_64-win\${VST3_BUNDLE}"
      FileOpen $0 "${VST3_DIR}\${VST3_BUNDLE}\Contents\x86_64-win\${VST3_BUNDLE}" a
      ${If} $0 == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION "$(DawRunningPrompt)"
        Abort
      ${EndIf}
      FileClose $0
    ${EndIf}
  ${EndIf}
FunctionEnd

; ---- uninstall -------------------------------------------------------------
Section "Uninstall"
  nsExec::Exec 'taskkill /IM ${APP_EXE} /F'
  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Docs\README.md"
  Delete "$INSTDIR\Docs\README.zh-CN.md"
  Delete "$INSTDIR\Docs\README.ja.md"
  RMDir "$INSTDIR\Docs"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  RMDir /r "${VST3_DIR}\${VST3_BUNDLE}"
  Delete "${STARTMENU_DIR}\DanceOverlay.lnk"
  Delete "${STARTMENU_DIR}\Uninstall.lnk"
  RMDir "${STARTMENU_DIR}"
  Delete "${DESKTOP_LNK}"
  DeleteRegKey ${UNINST_ROOT} "${UNINST_KEY}"
  DeleteRegKey ${LANG_REG_ROOT} "${LANG_REG_KEY}"
SectionEnd
