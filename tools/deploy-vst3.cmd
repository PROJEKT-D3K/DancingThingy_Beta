@echo off
REM Deploy built VST3 bundle to the system VST3 folder (needs elevation)
setlocal
set LOG=%~dp0deploy-vst3.log
echo ===== deploy started %date% %time% ===== > "%LOG%"

set "SRC=%~dp0..\build\DanceThingy_artefacts\Release\VST3"
robocopy "%SRC%" "C:\Program Files\Common Files\VST3" DanceThingy.vst3 /E /NFL /NDL /NJH /NJS >> "%LOG%" 2>&1

if errorlevel 8 (
  echo [ERROR] robocopy failed with code %errorlevel% >> "%LOG%"
  exit /b 1
)
echo ===== deploy finished ok ===== >> "%LOG%"
dir "C:\Program Files\Common Files\VST3" >> "%LOG%" 2>&1
exit /b 0
