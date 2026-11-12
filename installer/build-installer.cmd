@echo off
REM Build the DanceThingy installer with NSIS.
REM
REM   build-installer.cmd        -> dist\DanceThingy-0.4.0-Setup.exe
REM   build-installer.cmd test   -> dist\DanceThingy-0.4.0-Setup-test.exe
REM                                 (user-level sandbox build for verification)
REM
REM NSIS_HOME defaults to E:\NSIS; override it if makensis lives elsewhere.

setlocal
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
REM NSIS memory-maps large temp files; keep them off the nearly-full system drive.
set "TMP=%ROOT%\build\nsis-tmp"
set "TEMP=%TMP%"
if not exist "%TMP%" mkdir "%TMP%"
if not defined NSIS_HOME set "NSIS_HOME=E:\NSIS"
set "MAKENSIS=%NSIS_HOME%\makensis.exe"

if not exist "%MAKENSIS%" (
  echo [ERROR] makensis.exe not found: "%MAKENSIS%"
  echo         Set NSIS_HOME to your NSIS installation folder.
  exit /b 1
)

set "APP=%ROOT%\build\DanceOverlay_artefacts\Release\DanceOverlay.exe"
set "VST3=%ROOT%\build\DanceThingy_artefacts\Release\VST3\DanceThingy.vst3\Contents\x86_64-win\DanceThingy.vst3"

if not exist "%APP%" (
  echo [ERROR] Standalone build missing: "%APP%"
  echo         Run: cmake --build --preset default
  exit /b 1
)
if not exist "%VST3%" (
  echo [ERROR] VST3 build missing: "%VST3%"
  echo         Run: cmake --build --preset default
  exit /b 1
)

if not exist "%ROOT%\dist" mkdir "%ROOT%\dist"

set "TESTDEF="
if /I "%~1"=="test" set "TESTDEF=/DTESTBUILD"

REM The real installer bundles the VC++ 2015-2022 runtime when it is missing.
REM The test build skips this; the local machine already has the runtime.
if defined TESTDEF goto :skip_redist

set "PF86=%ProgramFiles(x86)%"
if defined VCREDIST set "REDIST=%VCREDIST%"
if defined REDIST goto :have_redist

set "REDIST="
for /f "delims=" %%F in ('dir /b /s "%PF86%\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\vc_redist.x64.exe" 2^>nul') do if not defined REDIST set "REDIST=%%F"

:have_redist
if not defined REDIST (
  echo [ERROR] vc_redist.x64.exe not found.
  echo         Install VS2022 Build Tools, or set VCREDIST to the file path.
  exit /b 1
)
if not exist "%ROOT%\build\redist" mkdir "%ROOT%\build\redist"
copy /Y "%REDIST%" "%ROOT%\build\redist\vc_redist.x64.exe" >nul
echo [INFO] VC++ redistributable: "%REDIST%"

:skip_redist

"%MAKENSIS%" /V2 %TESTDEF% "%~dp0DanceThingy.nsi"
if errorlevel 1 exit /b %errorlevel%

echo [OK] Installer built in "%ROOT%\dist"
exit /b 0
