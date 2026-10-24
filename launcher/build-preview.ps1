param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\launcher-preview'),
    [string]$Toolchain = 'C:\msys64\clang64\bin'
)
$ErrorActionPreference = 'Stop'
$taskRepo = Split-Path -Parent $PSScriptRoot
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$env:PATH = "$Toolchain;$env:PATH"
Push-Location $taskRepo
try {
    $resource = Join-Path $taskOutput 'launcher.res.o'
    & (Join-Path $Toolchain 'windres.exe') -I launcher launcher/launcher.rc -O coff -o $resource
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
    & (Join-Path $Toolchain 'clang++.exe') -O2 -std=c++20 -municode -mwindows -static -o (Join-Path $taskOutput 'DriveClub.exe') launcher/driveclub_launcher.cpp launcher/prepare.cpp launcher/gpu_check.cpp launcher/datx.cpp launcher/unlock.cpp launcher/graphics.cpp launcher/carswitch.cpp launcher/updater.cpp $resource -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -ldwmapi -lwinhttp -lbcrypt -lcomctl32
    if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed' }
} finally { Pop-Location }
