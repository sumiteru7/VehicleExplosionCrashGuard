param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot
)

$ErrorActionPreference = "Stop"
$Source = Join-Path (Split-Path -Parent $PSScriptRoot) "dist\red4ext\plugins\VehicleExplosionCrashGuard"
$Destination = Join-Path $GameRoot "red4ext\plugins\VehicleExplosionCrashGuard"

if (-not (Test-Path (Join-Path $Source "VehicleExplosionCrashGuard.dll"))) {
    throw "Built DLL not found. Run scripts\build_release.ps1 first."
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Copy-Item -Force (Join-Path $Source "*") $Destination
Write-Host "Installed to: $Destination" -ForegroundColor Green
