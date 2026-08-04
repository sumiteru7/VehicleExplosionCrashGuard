param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot
)

$ErrorActionPreference = "Stop"
$Destination = Join-Path $GameRoot "red4ext\plugins\VehicleExplosionCrashGuard"

if (Test-Path $Destination) {
    Remove-Item -Recurse -Force $Destination
    Write-Host "Removed: $Destination" -ForegroundColor Green
} else {
    Write-Host "Nothing to remove: $Destination"
}
