# Vehicle Explosion Crash Guard

A lightweight RED4ext crash fix for **Cyberpunk 2077 PC 2.31**.

This mod guards two confirmed invalid-reference paths observed during heavy police combat with repeated vehicle explosions. It only intervenes when one of those invalid paths is detected and redirects execution to an existing safe continuation point in the game.

## What This Mod Does

- Guards a confirmed null-reference path.
- Guards a confirmed invalid virtual-dispatch path caused by a corrupted vtable or function target.
- Uses exact game-build and byte-signature checks before installing either guard.
- Installs each guard independently and does nothing at a location if its signature does not match.

## What This Mod Does Not Change

- Projectile Launch System damage
- Vehicle damage
- Explosion radius or effects
- Police AI or spawn rates
- Combat balance
- Save data

This is **not** a universal fix for every crash involving vehicles or explosions. It only covers the two native crash paths confirmed during development. A crash caused by a different code path, another mod, a driver issue, or damaged game files may not be affected.

## Requirements

- Cyberpunk 2077 PC **2.31**
- RED4ext **1.29.0 or newer**
- Windows x64

RED4ext 1.30.0 is recommended.

CET, redscript, ArchiveXL, TweakXL, Codeware, and REDmod are not required.

## Installation

Install with Vortex, or extract the archive into the Cyberpunk 2077 game folder.

The final file location must be:

```text
Cyberpunk 2077\red4ext\plugins\VehicleExplosionCrashGuard\VehicleExplosionCrashGuard.dll
```

## Updating

1. Close the game.
2. Delete the existing folder:

```text
Cyberpunk 2077\red4ext\plugins\VehicleExplosionCrashGuard
```

3. Install the new version.

## Uninstallation

Close the game and delete:

```text
Cyberpunk 2077\red4ext\plugins\VehicleExplosionCrashGuard
```

The mod does not write anything to save files.

## Verifying Installation

Launch the game to the main menu, then check:

```text
Cyberpunk 2077\red4ext\logs\VehicleExplosionCrashGuard.log
```

For Cyberpunk 2077 PC 2.31, a successful installation should include lines showing:

```text
Build guard: passed
Guard A signature: matched
Guard A hook: installed
Guard B signature: matched
Guard B hook: installed
```

If the game executable does not match the supported build or a byte signature has changed, the affected guard is not installed. This fail-closed behavior prevents the mod from hooking an unknown game version.

## Compatibility and Limitations

- Designed specifically for Cyberpunk 2077 PC 2.31.
- A future game update will normally cause the build or signature checks to fail, disabling protection until the mod is updated.
- Mods that patch the same native game-code locations may conflict.
- The mod may not prevent vehicle- or explosion-related crashes that originate from a different code path.

## Version

### v1.0

- Initial Nexus release.
- Includes two build-verified native crash guards.
- Uses a lightweight normal path without per-call `VirtualQuery`, file I/O, logging, or heap allocation.

## Credits

Built with RED4ext.SDK and SafetyHook.
