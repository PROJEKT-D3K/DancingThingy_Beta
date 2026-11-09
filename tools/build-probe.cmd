@echo off
rem Builds the standalone WASAPI probe (tools\wasapi-probe.cpp).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo vcvars64.bat not found
    exit /b 1
)
cd /d "%~dp0.."
cl /EHsc /utf-8 /nologo tools\wasapi-probe.cpp ole32.lib /Fe:tools\wasapi-probe.exe
