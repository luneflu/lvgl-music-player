<#
.SYNOPSIS
    Build and run script for Windows (PowerShell).
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64", "x86", "ARM64")]
    [string]$Platform = "x64",

    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Check OS
$isWindows = [System.Environment]::OSVersion.Platform -match "Win" -or $PSVersionTable.PSVersion.Major -le 5
if (-not $isWindows) {
    throw "This script is intended for Windows. On macOS/Linux, please run ./run.sh"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "LVGL Windows Launcher (PowerShell)" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan

# Locate vswhere
$vswherePaths = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
)
$vswhere = $vswherePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $vswhere) {
    throw "vswhere.exe not found. Visual Studio is required."
}

# Locate MSBuild
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1

if (-not $msbuild -or -not (Test-Path $msbuild)) {
    throw "MSBuild.exe not found."
}

Write-Host "Using MSBuild: $msbuild" -ForegroundColor Gray

# Locate project file
$projectFile = Join-Path $ScriptDir "visual_studio\LvglWindowsSimulator\LvglWindowsSimulator.vcxproj"
if (-not (Test-Path $projectFile)) {
    throw "Project file not found: $projectFile"
}

Write-Host "Building LvglWindowsSimulator ($Configuration | $Platform)..." -ForegroundColor Yellow
& $msbuild $projectFile /p:Configuration=$Configuration /p:Platform=$Platform /nologo /m /v:m

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild build failed with exit code $LASTEXITCODE"
}

Write-Host "Build Succeeded!" -ForegroundColor Green

if (-not $BuildOnly) {
    $binPath = Join-Path $ScriptDir "visual_studio\Output\Binaries\$Configuration\$Platform\LvglWindowsSimulator.exe"
    if (-not (Test-Path $binPath)) {
        throw "Executable not found at: $binPath"
    }
    Write-Host "Running $binPath..." -ForegroundColor Cyan
    & $binPath
}
