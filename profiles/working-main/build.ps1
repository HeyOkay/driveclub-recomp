param(
 [Parameter(Mandatory=$true)][string]$Checkout,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$ToolchainRoot='C:\msys64\clang64',
 [ValidateSet('use','off','generate')][string]$Pgo='use',
 [string]$PgoDirectory='',
 [int]$Jobs=2,
 [switch]$KeepRuntimeDebugInfo,
 [switch]$ConfigureOnly
)
$ErrorActionPreference='Stop'
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
$taskCheckout=[IO.Path]::GetFullPath($Checkout)
$taskTools=[IO.Path]::GetFullPath($ToolchainRoot)
$env:PATH=(Join-Path $taskTools 'bin')+';'+$env:PATH
& python (Join-Path $PSScriptRoot 'assemble.py') --checkout $taskCheckout --output $taskOutput
if($LASTEXITCODE -ne 0){throw 'Source assembly failed'}
$taskBuild=Join-Path $taskOutput 'build'
$taskCmake=Join-Path $taskTools 'bin\cmake.exe'
$taskArgs=@('-S',$taskOutput,'-B',$taskBuild,'-G','Ninja',
 '-DCMAKE_BUILD_TYPE=RelWithDebInfo',('-DCMAKE_C_COMPILER='+ (Join-Path $taskTools 'bin\clang.exe')),
 ('-DCMAKE_CXX_COMPILER='+ (Join-Path $taskTools 'bin\clang++.exe')),
 ('-DCMAKE_RC_COMPILER='+ (Join-Path $taskTools 'bin\windres.exe')),
 ('-DCMAKE_MAKE_PROGRAM='+ (Join-Path $taskTools 'bin\ninja.exe')),
 ('-DCMAKE_PREFIX_PATH='+$taskTools),('-DMAIN_CLANG64_ROOT='+$taskTools),('-DBB_PGO='+$Pgo),'-DBB_BUILD_DIAGNOSTIC_TESTS=OFF')
if($PgoDirectory){$taskArgs+=('-DBB_PGO_DIR='+[IO.Path]::GetFullPath($PgoDirectory))}
if($KeepRuntimeDebugInfo){$taskArgs+='-DMAIN_STRIP_RUNTIME=OFF'}
$taskArgs=@($taskArgs | ForEach-Object { $_.Replace('\','/') })
& $taskCmake @taskArgs
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
if(!$ConfigureOnly){
 & $taskCmake --build $taskBuild --target DriveClubBackend --parallel $Jobs
 if($LASTEXITCODE -ne 0){throw 'Source build failed'}
 & python (Join-Path $PSScriptRoot 'verify.py') --source $taskOutput
 if($LASTEXITCODE -ne 0){throw 'Build identity verification failed'}
}
