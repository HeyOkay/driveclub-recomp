param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\launcher-0.3.1-panel-width'),
    [string]$Toolchain = 'C:\msys64\clang64\bin'
)
$ErrorActionPreference = 'Stop'
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
$taskNative = Join-Path $taskOutput 'native-build'
& (Join-Path $PSScriptRoot 'build-preview.ps1') -OutputDirectory $taskNative -Toolchain $Toolchain
Copy-Item -LiteralPath (Join-Path $taskNative 'DriveClub.exe') -Destination (Join-Path $taskOutput 'DriveClub.Backend.exe')
& dotnet build (Join-Path $PSScriptRoot 'wpf\DriveClub.Launcher.csproj') -c Release --ignore-failed-sources -o $taskOutput
if ($LASTEXITCODE -ne 0) { throw 'WPF build failed' }
