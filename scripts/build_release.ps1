$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$PluginDist = Join-Path $Root "dist\red4ext\plugins\VehicleExplosionCrashGuard"
$DllPath = Join-Path $PluginDist "VehicleExplosionCrashGuard.dll"

function Assert-NativeSuccess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Step
    )

    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

Push-Location $Root
try {
    # Prevent a stale DLL from making a failed build look successful.
    if (Test-Path $PluginDist) {
        Remove-Item -Recurse -Force $PluginDist
    }

    cmake --preset vs2022-x64
    Assert-NativeSuccess -Step "CMake configure"

    cmake --build --preset release
    Assert-NativeSuccess -Step "Release build"

    if (-not (Test-Path $DllPath)) {
        throw "Build command completed, but the expected DLL was not created: $DllPath"
    }

    Write-Host ""
    Write-Host "Build succeeded. Deployable files:" -ForegroundColor Green
    Write-Host $PluginDist
    Write-Host "DLL verified:" -ForegroundColor Green
    Write-Host $DllPath
}
finally {
    Pop-Location
}
