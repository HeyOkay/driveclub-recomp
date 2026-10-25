param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\launcher-wpf-win-x64'),
    [string]$Toolchain = 'C:\msys64\clang64\bin',
    [switch]$SelfContained
)
$ErrorActionPreference = 'Stop'
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
$taskNative = Join-Path $taskOutput 'native-build'
& (Join-Path $PSScriptRoot 'build-preview.ps1') -OutputDirectory $taskNative -Toolchain $Toolchain
Copy-Item -LiteralPath (Join-Path $taskNative 'DriveClub.exe') -Destination (Join-Path $taskOutput 'DriveClub.Backend.exe')
$taskRuntimeMode = if ($SelfContained) { 'true' } else { 'false' }
& dotnet publish (Join-Path $PSScriptRoot 'wpf\DriveClub.Launcher.csproj') -c Release -r win-x64 --self-contained $taskRuntimeMode --ignore-failed-sources -o $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'WPF publish failed' }
$taskSetupResource = Join-Path $taskNative 'setup.res.o'
& (Join-Path $Toolchain 'windres.exe') -I (Join-Path $PSScriptRoot 'setup') (Join-Path $PSScriptRoot 'setup\Setup.rc') -O coff -o $taskSetupResource
if ($LASTEXITCODE -ne 0) { throw 'Setup resource compilation failed' }
& (Join-Path $Toolchain 'clang++.exe') -O2 -std=c++17 -municode -mwindows -static -o (Join-Path $taskOutput 'Setup.exe') (Join-Path $PSScriptRoot 'setup\Setup.cpp') $taskSetupResource -lshell32 -luser32
if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed' }
