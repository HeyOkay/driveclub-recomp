# Lets other players reach this PC for DriveClub online (Windows Firewall, inbound).
# Double-click Firewall-Freigabe.bat; Windows asks once for admin rights.
# The game (UDP, peer-to-peer races) always; the server (TCP 8080) when driveclubsrv.exe is found.
param([string]$Root = $PSScriptRoot)

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Root `"$Root`""
    exit
}

$game = Join-Path $Root 'runtime\DriveClubGame.exe'
Get-NetFirewallRule -DisplayName 'DriveClub Online*' -ErrorAction SilentlyContinue | Remove-NetFirewallRule
New-NetFirewallRule -DisplayName 'DriveClub Online (game UDP)' -Direction Inbound -Action Allow `
    -Program $game -Protocol UDP -Profile Any | Out-Null
Write-Host "Spiel freigegeben / game allowed: $game"

$serverDir = Join-Path $Root 'server'
$cfg = Join-Path $Root 'online.ini'
if (Test-Path $cfg) {
    $line = Get-Content $cfg | Where-Object { $_ -match '^server=(.+)$' } | Select-Object -First 1
    if ($line -match '^server=(.+)$') { $serverDir = $Matches[1].Trim() }
}
if (Test-Path (Join-Path $serverDir 'driveclubsrv.exe')) {
    New-NetFirewallRule -DisplayName 'DriveClub Online (server TCP 8080)' -Direction Inbound -Action Allow `
        -Protocol TCP -LocalPort 8080 -Profile Any | Out-Null
    Write-Host 'Server freigegeben / server allowed: TCP 8080'
}
Start-Sleep 4
