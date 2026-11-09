@echo off
REM DanceThingy toolchain installer - run elevated
REM Installs VS2022 Build Tools (C++ workload) + CMake via Chocolatey
REM All output is written to toolchain-install.log next to this script.

setlocal
set LOG=%~dp0toolchain-install.log
echo ===== DanceThingy toolchain install started %date% %time% ===== > "%LOG%"

where choco >> "%LOG%" 2>&1
if errorlevel 1 (
  echo [ERROR] choco not found in PATH >> "%LOG%"
  exit /b 1
)

echo ----- Installing VS2022 Build Tools with VCTools workload (this is the long one) ----- >> "%LOG%"
choco install visualstudio2022buildtools --packageParameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" -y --no-progress >> "%LOG%" 2>&1
if errorlevel 1 (
  echo [ERROR] visualstudio2022buildtools install failed >> "%LOG%"
  exit /b 2
)

echo ----- Installing CMake ----- >> "%LOG%"
choco install cmake -y --no-progress >> "%LOG%" 2>&1
if errorlevel 1 (
  echo [ERROR] cmake install failed >> "%LOG%"
  exit /b 3
)

echo ----- Verifying ----- >> "%LOG%"
cmake --version >> "%LOG%" 2>&1

echo ===== DanceThingy toolchain install finished %date% %time% ===== >> "%LOG%"
exit /b 0
