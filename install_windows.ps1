$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Package = Join-Path $ScriptDir "build-windows\ofx-package\AnimeSmoother.ofx.bundle"

if (-not (Test-Path $Package)) {
    $Package = Join-Path $ScriptDir "build\ofx-package\AnimeSmoother.ofx.bundle"
}

if (-not (Test-Path $Package)) {
    Write-Host "Plugin bundle not found."
    Write-Host "Build it first, for example:"
    Write-Host '  cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DOFX_SUPPORT_ROOT=C:\path\to\openfx'
    Write-Host '  cmake --build build-windows --config Release'
    exit 1
}

$DestRoot = Join-Path $env:CommonProgramFiles "OFX\Plugins"
$DestDir = Join-Path $DestRoot "AnimeSmoother"
$Dest = Join-Path $DestDir "AnimeSmoother.ofx.bundle"
$OldDest = Join-Path $DestRoot "MLAAEdgeSmoother"
$OldLineDest = Join-Path $DestRoot "AnimeLineSmoother"

New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
if (Test-Path $Dest) {
    Remove-Item -Recurse -Force $Dest
}
if (Test-Path $OldDest) {
    Remove-Item -Recurse -Force $OldDest
}
if (Test-Path $OldLineDest) {
    Remove-Item -Recurse -Force $OldLineDest
}

Copy-Item -Recurse $Package $Dest

Write-Host "Installed:"
Write-Host "  $Dest"
Write-Host ""
Write-Host "Restart Autograph or DaVinci Resolve, then look for Anime Smoother under Filter/Anime."
