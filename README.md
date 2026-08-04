# Vehicle Explosion Crash Guard

Source code for the Cyberpunk 2077 Nexus mod **Vehicle Explosion Crash Fix**.

A lightweight RED4ext native plugin for **Cyberpunk 2077 PC 2.31**. It guards two confirmed invalid-reference crash paths observed during heavy combat and repeated vehicle explosions.

## Scope

This plugin guards two exact native code paths:

- **Guard A:** a null shared-reference path
- **Guard B:** an invalid vtable / virtual-dispatch path

It does **not** change:

- vehicle damage
- explosion damage or radius
- Projectile Launch System behavior
- police AI or spawn behavior
- gameplay balance
- save data

This is not intended as a universal fix for every vehicle-related crash.

## Why a native RED4ext plugin?

The confirmed crashes occur inside native executable code. Guarding those exact instruction paths requires a native hook; CET/Lua and gameplay-data mods cannot intercept them directly.

## Safety design

The plugin is fail-closed:

- Cyberpunk 2077 build information is checked at startup.
- Each guard has its own full byte-signature check.
- Guard A and Guard B are enabled independently.
- A guard is not installed when its expected signature does not match.
- The plugin never guesses new hook locations after a game update.

Version 1.0 targets only **Cyberpunk 2077 PC 2.31**.

## Requirements

- Cyberpunk 2077 PC 2.31
- RED4ext 1.29.0 or later
- Windows x64

Not required:

- Cyber Engine Tweaks
- redscript
- ArchiveXL
- TweakXL
- Codeware
- REDmod
- Phantom Liberty

## Build requirements

- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.28 or later
- Git
- PowerShell

Dependencies are fetched and pinned by CMake:

- RED4ext.SDK 1.0.0
- SafetyHook v0.7.0
- Zydis v4.1.0, fetched by SafetyHook

## Build

From the repository root in PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\scripts\build_release.ps1
```

The deployable files are written to:

```text
dist\red4ext\plugins\VehicleExplosionCrashGuard\
├─ VehicleExplosionCrashGuard.dll
├─ README.md
├─ THIRD_PARTY_NOTICES.md
└─ LICENSE.txt
```

## Install

Use the Nexus archive, or copy the built plugin to:

```text
Cyberpunk 2077\red4ext\plugins\VehicleExplosionCrashGuard\
```

The repository also includes an installation helper:

```powershell
.\scripts\install.ps1 -GameRoot "D:\Games\Cyberpunk 2077"
```

## Uninstall

Delete:

```text
Cyberpunk 2077\red4ext\plugins\VehicleExplosionCrashGuard
```

Or use:

```powershell
.\scripts\uninstall.ps1 -GameRoot "D:\Games\Cyberpunk 2077"
```

## Verify

Launch the game and check:

```text
Cyberpunk 2077\red4ext\logs\VehicleExplosionCrashGuard.log
```

Expected startup state on the supported build:

```text
Build guard: passed
Guard A hook: installed
Guard B hook: installed
```

## Repository layout

```text
src/                         Plugin source code
scripts/                     Build, install, and uninstall helpers
docs/TECHNICAL_ANALYSIS_JA.md
                             DMP analysis and guard-design history in Japanese
LICENSES/                    Third-party license texts
CMakeLists.txt               Build definition and pinned dependencies
CMakePresets.json            Visual Studio 2022 x64 presets
README_NEXUS.md              End-user README copied into built packages
RELEASE_SHA256.txt           Hashes of the original Nexus v1.0 archive and DLL
```

## Technical details

The source contains the supported-build metadata, hook RVAs, full signatures, validation rules, and safe-resume logic. The development analysis is preserved in [docs/TECHNICAL_ANALYSIS_JA.md](docs/TECHNICAL_ANALYSIS_JA.md).

## Development note

This project was developed with AI-assisted crash analysis and implementation. Testing, release decisions, packaging, and publication were performed by `sumiteru7`.

## License

The original project source is released under the [MIT License](LICENSE).

Third-party components retain their own licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the [LICENSES](LICENSES) directory.

## Disclaimer

This is an unofficial community mod and is not affiliated with or endorsed by CD PROJEKT RED.
