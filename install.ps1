# Installs the Run as Administrator plugin for the current user. Run from the unzipped release folder.
$ErrorActionPreference = 'Stop'
$cfg = Join-Path $env:APPDATA 'OpenRGB'
if (Get-Process OpenRGB -ErrorAction SilentlyContinue | Where-Object SessionId -ne 0) {
    Write-Warning 'Close OpenRGB first (tray icon > Exit), then run this again.'; exit 1
}
New-Item -ItemType Directory -Force (Join-Path $cfg 'plugins') | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'plugins\*.dll') (Join-Path $cfg 'plugins') -Force
Write-Host "Installed to $cfg\plugins. Start OpenRGB; it asks once whether to set up running as administrator."
