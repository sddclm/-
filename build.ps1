# ============================================================
#  Mouse Auto Clicker - one-click build script (MinGW-w64 / w64devkit)
#  Usage:
#    .\build.ps1                          # auto-locate toolchain
#    .\build.ps1 -Toolchain "D:\dir\w64devkit"  # manual path
# ============================================================
param([string]$Toolchain = "")

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path

function Test-TC([string]$p) {
    if ($p -and (Test-Path (Join-Path $p "bin\g++.exe")) -and (Test-Path (Join-Path $p "bin\windres.exe"))) { return $p }
    return $null
}

$tc = $null
if ($Toolchain) { $tc = Test-TC $Toolchain }
if (-not $tc -and $env:W64DEVKIT) { $tc = Test-TC $env:W64DEVKIT }
if (-not $tc) { $tc = Test-TC (Join-Path $Root "tools\w64devkit") }
$pathFile = Join-Path $Root "toolchain.path"
if (-not $tc -and (Test-Path $pathFile)) { $tc = Test-TC ((Get-Content $pathFile -TotalCount 1).Trim()) }
if (-not $tc) {
    foreach ($c in @("C:\w64devkit","D:\w64devkit","C:\dev\w64devkit","C:\Program Files\w64devkit","D:\Program Files\w64devkit")) {
        $tc = Test-TC $c
        if ($tc) { break }
    }
}
if (-not $tc) {
    Write-Host "Toolchain not found (need g++.exe / windres.exe)." -ForegroundColor Red
    Write-Host "Run: .\build.ps1 -Toolchain '<w64devkit dir>'" -ForegroundColor Yellow
    exit 1
}
Write-Host "Toolchain: $tc" -ForegroundColor Cyan

$gpp      = Join-Path $tc "bin\g++.exe"
$wr       = Join-Path $tc "bin\windres.exe"
# Put toolchain bin on PATH so g++ can find as.exe / ld.exe etc.
$env:PATH = (Join-Path $tc "bin") + ";" + $env:PATH

$srcDir   = Join-Path $Root "src"
$distDir  = Join-Path $Root "dist"
$buildDir = Join-Path $Root "build"
New-Item -ItemType Directory -Force -Path $distDir, $buildDir | Out-Null

Push-Location $srcDir
try {
    Write-Host "[1/2] Compiling resources (windres)..." -ForegroundColor Cyan
    & $wr -i "app.rc" -o (Join-Path $buildDir "app.res") -O coff
    if ($LASTEXITCODE -ne 0) { throw "windres failed" }

    Write-Host "[2/2] Compiling & linking (g++)..." -ForegroundColor Cyan
    $out = Join-Path $distDir "AutoClicker.exe"
    & $gpp -std=c++20 -O2 -s -static -mwindows "mc_app.cpp" (Join-Path $buildDir "app.res") -o $out -lgdiplus -lwinmm -lgdi32 -luser32 -lshell32 -ldwmapi -lole32 -luuid -lcomctl32
    if ($LASTEXITCODE -ne 0) { throw "g++ failed" }

    # 中文名副本: 鼠标连点器.exe
    $cnName = (-join [char[]](0x9F20, 0x6807, 0x8FDE, 0x70B9, 0x5668)) + ".exe"
    Copy-Item $out (Join-Path $distDir $cnName) -Force
    Write-Host ""
    Write-Host "Build OK!" -ForegroundColor Green
    Write-Host "  $out"
    Write-Host "  $(Join-Path $distDir $cnName)"
} finally {
    Pop-Location
}
