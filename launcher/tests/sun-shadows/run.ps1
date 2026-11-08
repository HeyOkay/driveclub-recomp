param(
  [Parameter(Mandatory=$true)][string]$OutputDirectory,
  [string]$Toolchain = 'C:\msys64\clang64\bin'
)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$launcher=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
& python (Join-Path $launcher 'tools\generate_rear_payload.py') --clang (Join-Path $Toolchain 'clang.exe') --output (Join-Path $out 'rear_native_payload.h') --binary-output (Join-Path $out 'rear.bin')
if($LASTEXITCODE -ne 0){throw 'Shim generation failed'}
& (Join-Path $Toolchain 'clang.exe') -O0 -fno-omit-frame-pointer (Join-Path $PSScriptRoot 'rear_stub_test.c') (Join-Path $PSScriptRoot 'rear_harness.s') -o (Join-Path $out 'rear-test.exe')
if($LASTEXITCODE -ne 0){throw 'Shim test build failed'}
& (Join-Path $out 'rear-test.exe') (Join-Path $out 'rear.bin')
if($LASTEXITCODE -ne 0){throw 'Shim execution tests failed'}
& (Join-Path $Toolchain 'clang++.exe') -std=c++20 -static (Join-Path $PSScriptRoot 'preparation_guard_test.cpp') -lbcrypt -o (Join-Path $out 'guard-test.exe')
if($LASTEXITCODE -ne 0){throw 'Guard test build failed'}
& (Join-Path $out 'guard-test.exe') (Join-Path $out 'guard-fixtures')
if($LASTEXITCODE -ne 0){throw 'Guard tests failed'}
