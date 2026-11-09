param(
 [Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$ToolchainRoot='C:\msys64\clang64',
 [string]$DlssDirectory='',
 [string]$OfflineNugetSource='',
 [switch]$NativeOnly
)
$ErrorActionPreference='Stop'
$taskSource=[IO.Path]::GetFullPath($SourceDirectory)
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
$taskTools=[IO.Path]::GetFullPath($ToolchainRoot)
if(Test-Path -LiteralPath $taskOutput){throw 'Package output must be a new directory'}
$env:PATH=(Join-Path $taskTools 'bin')+';'+$env:PATH
& (Join-Path $taskTools 'bin\cmake.exe') --build (Join-Path $taskSource 'build') --target DriveClubBackend --parallel 2
if($LASTEXITCODE -ne 0){throw 'Build failed'}
& python (Join-Path $PSScriptRoot 'verify.py') --source $taskSource
if($LASTEXITCODE -ne 0){throw 'Identity/source verification failed'}
New-Item -ItemType Directory -Path (Join-Path $taskOutput 'runtime') | Out-Null
Copy-Item -LiteralPath (Join-Path $taskSource 'build\finalized-runtime\DriveClubGame.exe') -Destination (Join-Path $taskOutput 'runtime\DriveClubGame.exe')
Copy-Item -LiteralPath (Join-Path $taskSource 'build\DriveClub.Backend.exe') -Destination $taskOutput
Copy-Item -LiteralPath (Join-Path $taskSource 'build\DriveClub.Native.exe') -Destination $taskOutput
if(!$NativeOnly){
 $taskPublish=@('publish',(Join-Path $taskSource 'host\launcher\wpf\DriveClub.Launcher.csproj'),'-c','Release','-r','win-x64','--self-contained','false','--ignore-failed-sources','-o',$taskOutput)
 if($OfflineNugetSource){$taskPublish+=@('--source',[IO.Path]::GetFullPath($OfflineNugetSource),'-p:NuGetAudit=false')}
 & dotnet @taskPublish
 if($LASTEXITCODE -ne 0){throw 'WPF publish failed; the .NET8 desktop SDK is required'}
 $taskRes=Join-Path $taskSource 'build\working-main-setup.res.o'
 & (Join-Path $taskTools 'bin\windres.exe') -I (Join-Path $taskSource 'host\launcher\setup') (Join-Path $taskSource 'host\launcher\setup\Setup.rc') -O coff -o $taskRes
 if($LASTEXITCODE -ne 0){throw 'Bootstrap resource compilation failed'}
 & (Join-Path $taskTools 'bin\clang++.exe') -Os -std=c++17 -municode -mwindows -static -s -o (Join-Path $taskOutput 'DriveClub.exe') (Join-Path $taskSource 'host\launcher\setup\Bootstrap.cpp') $taskRes -lshell32 -luser32 -ladvapi32
 if($LASTEXITCODE -ne 0){throw 'Bootstrap build failed'}
}
# Copy runtime DLL dependencies without executing any packaged program.
$taskCollect=@((Join-Path $PSScriptRoot 'collect_runtime.py'),'--source',$taskSource,'--package',$taskOutput,'--toolchain',$taskTools)
if($DlssDirectory){$taskCollect+=@('--dlss',[IO.Path]::GetFullPath($DlssDirectory))}
& python @taskCollect
if($LASTEXITCODE -ne 0){throw 'Runtime dependency collection failed'}
Write-Output ('Packaged finalized runtime and matching backend: '+$taskOutput)
