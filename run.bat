@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo LVGL Windows Launcher
echo ===================================================

rem 1. Check OS
if not "%OS%"=="Windows_NT" (
    echo [ERROR] Windows NT required.
    exit /b 1
)

rem 2. Find MSBuild via vswhere
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    set VSWHERE="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if not exist %VSWHERE% (
    echo [ERROR] Visual Studio installer / vswhere not found.
    echo Please install Visual Studio with C++ desktop development workload.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set MSBUILD_EXE=%%i
)

if "%MSBUILD_EXE%"=="" (
    echo [ERROR] MSBuild.exe not found.
    exit /b 1
)

echo Found MSBuild: "%MSBUILD_EXE%"

rem 3. Build LvglWindowsSimulator
set PROJ_PATH=%~dp0visual_studio\LvglWindowsSimulator\LvglWindowsSimulator.vcxproj
if not exist "%PROJ_PATH%" (
    echo [ERROR] Project file not found at %PROJ_PATH%
    exit /b 1
)

echo Building LvglWindowsSimulator (Debug, x64)...
"%MSBUILD_EXE%" "%PROJ_PATH%" /p:Configuration=Debug /p:Platform=x64 /nologo /m /v:m
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo Build succeeded.

rem 4. Run the executable
set BIN_PATH=%~dp0visual_studio\Output\Binaries\Debug\x64\LvglWindowsSimulator.exe
if not exist "%BIN_PATH%" (
    echo [ERROR] Binary not found at %BIN_PATH%
    exit /b 1
)

echo Launching %BIN_PATH%...
"%BIN_PATH%"

endlocal
