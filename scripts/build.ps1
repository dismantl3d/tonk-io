# Usage: .\scripts\build.ps1 [native|web|all]   (default: all)
# Needs CMake + Ninja on PATH, a C++17 compiler for the native build, and the Emscripten SDK (emsdk) for the web build.
# Set $env:EMSDK to your emsdk folder (or activate it in your shell first so `emcmake` is on PATH).
param([string]$Target = 'all')
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

if ($Target -in 'native', 'all') {
  cmake -S . -B build -G Ninja
  cmake --build build
  ctest --test-dir build --output-on-failure
}
if ($Target -in 'web', 'all') {
  if ($env:EMSDK -and -not (Get-Command emcmake -ErrorAction SilentlyContinue)) {
    cmd /c "`"$env:EMSDK\emsdk_env.bat`" >nul 2>&1 && emcmake cmake -S . -B build-web -G Ninja && cmake --build build-web"
  } else {
    emcmake cmake -S . -B build-web -G Ninja
    cmake --build build-web
  }
}
