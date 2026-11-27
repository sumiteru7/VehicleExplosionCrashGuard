# v1.2.0-beta.1 (2026-11-27)

- Added Guard C for an additional Patch 2.5 crash.

# v1.1.0-alpha.1 (2026-11-22)

- Added support for Cyberpunk 2077 Patch 2.5.

## Previous history

# Changelog

## v1.0.0 — 2026-08-04

- Initial public source release for the Nexus v1.0 build.
- Guards two confirmed Cyberpunk 2077 PC 2.31 native crash paths.
- Includes independent full-signature validation for Guard A and Guard B.
- Uses fail-closed hook installation when build or signature checks do not match.
- Preserves the lightweight Guard B design finalized during the internal v0.2.3 development cycle.

### Repository packaging

- Added an English repository README and retained the end-user Nexus README separately.
- Added the project license and complete third-party notices.
- Updated the build packaging step to include license notices automatically.
- Corrected the documented CMake minimum to 3.28, matching SafetyHook v0.7.0.
