# Vehicle Explosion Crash Fix

A lightweight RED4ext native plugin for Cyberpunk 2077. The current beta targets PC Patch 2.5 and guards three specific invalid-reference crash paths without changing gameplay or save data.

## What's new in v1.2.0-beta.1

Adds Guard C for an additional Patch 2.5 crash, identified and addressed by [Gustavo Kimura (@GustavoKimura)](https://github.com/GustavoKimura) in [PR #1](https://github.com/sumiteru7/VehicleExplosionCrashGuard/pull/1). Thank you for the investigation and contribution!

The existing Guard A/B implementation from v1.1.0-alpha.1 is retained. Build and byte-signature checks prevent installation on unsupported executable paths.

Game startup and installation of all three guards have been verified. Crash-prevention effectiveness has not yet been confirmed in our environment.

## Requirements

- Cyberpunk 2077 PC Patch 2.5 (Windows x64)
- RED4ext compatible with Patch 2.5

For Patch 2.31, use v1.0.

## Download and installation

Download the mod from [Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/32230). Install with Vortex or extract the ZIP into the game directory.

Plugin location: red4ext/plugins/VehicleExplosionCrashGuard/VehicleExplosionCrashGuard.dll

To uninstall, exit the game and remove the VehicleExplosionCrashGuard plugin folder or uninstall via Vortex.

## Build from source

Requires Visual Studio 2022 with C++ desktop development, Git and CMake 3.28 or newer.

Pinned dependencies: RED4ext.SDK 1.0.0, SafetyHook v0.7.0 and Zydis v4.1.0.

    cmake --preset vs2022-x64 -DVECG_BUILD_ALPHA_TESTS=ON
    cmake --build --preset release --parallel 4

The deployable files are under dist/red4ext/plugins/VehicleExplosionCrashGuard/. The repository includes native regression tests and executable verification scripts.

## Credits and license

See [CONTRIBUTORS.md](CONTRIBUTORS.md) for attribution and [CHANGELOG.md](CHANGELOG.md) for version history.

This project was developed with AI-assisted crash analysis and implementation. Testing, release decisions, packaging and publication are managed by sumiteru7.

The project is licensed under MIT. Third-party components retain their own licenses; see THIRD_PARTY_NOTICES.md, LICENSES/ and DISTRIBUTION_LICENSES.txt.

This is an unofficial community mod and is not affiliated with or endorsed by CD PROJEKT RED.
