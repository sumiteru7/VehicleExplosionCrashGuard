# Vehicle Explosion Crash Fix v1.2.0-beta.1

## Thanks to KimuraDev

This update incorporates **Gustavo Kimura (KimuraDev / GustavoKimura)**'s investigation and Guard C implementation from PR #1. He analyzed minidumps from an affected Patch 2.5 environment and identified the separate reference-assignment path behind exe+0x22CA442 / read 0x164. Thank you for sharing the analysis and code.

Contribution: https://github.com/sumiteru7/VehicleExplosionCrashGuard/pull/1

## What changed

- Adds KimuraDev's Guard C for the low-address reference path leading to exe+0x22CA442.
- Retains the existing v1.1.0-alpha.1 Guard A/B implementation.
- Checks the exact game build and each guard's full signature before installing it. Guard C additionally verifies its five-byte hook footprint before enabling.
- Retains lightweight normal-path processing and reports redirects through the existing diagnostic monitor.

No changes to weapon/vehicle damage, explosions, police AI/spawns, gameplay balance or save data.

## Requirements

- Cyberpunk 2077 PC 2.5, executable version 3.0.5813403 (file components 3.0.88.46235)
- RED4ext for Patch 2.5
- Windows x64

CET, redscript, ArchiveXL, TweakXL, Codeware and REDmod are not required.

## Verification status

Game startup, build validation, and installation of Guard A/B/C have been verified on Patch 2.5. Actual activation against the reported crash and crash-prevention effectiveness have not yet been confirmed in our environment. This is a beta release for feedback from affected systems.

Zero redirects is normal when no guarded condition occurs.

## Installation / updating

1. Close the game.
2. Back up the old VehicleExplosionCrashGuard folder outside the game's plugin directory.
3. Extract this ZIP's red4ext folder into the Cyberpunk 2077 game root, replacing this plugin's files, or install with Vortex.

DLL location: red4ext/plugins/VehicleExplosionCrashGuard/VehicleExplosionCrashGuard.dll

## Logs and feedback

Check red4ext/logs/VehicleExplosionCrashGuard.log and the same-session plugin log. Expected: v1.2.0-beta.1, build guard passed, A/B/C signatures matched and A/B/C hooks installed.

If a crash remains, please provide the complete matching %LOCALAPPDATA%/REDEngine/ReportQueue/ folder ZIP, the same-session RED4ext/plugin logs, and a brief description through Nexus private message.

## Removal / rollback

Close the game and remove red4ext/plugins/VehicleExplosionCrashGuard. To roll back on Patch 2.5, restore the saved v1.1.0-alpha.1 folder. The old v1.0 release is for game 2.31 and must not be used on 2.5. No save-data changes need undoing.

## Licenses

VehicleExplosionCrashGuard and third-party licenses

Original project: MIT
RED4ext.SDK 1.0.0: MIT (https://github.com/WopsS/RED4ext.SDK)
SafetyHook v0.7.0: BSL-1.0 (https://github.com/cursey/safetyhook)
Zydis v4.1.0 and bundled Zycore: MIT (https://github.com/zyantific/zydis)


Original project
================

MIT License

Copyright (c) 2026 sumiteru7

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


RED4ext.SDK
===========

MIT License

Copyright (c) 2020 - present Octavian Dima

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


SafetyHook
==========

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.


Zydis
=====

The MIT License (MIT)

Copyright (c) 2014-2024 Florian Bernd
Copyright (c) 2014-2024 Joel Höner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


Zycore
======

The MIT License (MIT)

Copyright (c) 2018-2024 Florian Bernd
Copyright (c) 2018-2024 Joel Höner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
