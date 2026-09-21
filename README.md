# Modern Camera Collision

An SKSE plugin for Skyrim Special Edition. The third-person camera stops at
walls and floors, sees around beams, posts, pots and campfires, eases its way
in and out, and fades what it is let through. Configured through MCM Helper.

Runtimes: SE 1.5.97 (tested) and AE 1.6.x / 1.7.x (built and verified against
the 1.7.104 executable, not yet played). Not VR.

Requires SKSE, Address Library for SKSE Plugins, SkyUI and MCM Helper.
Incompatible with SmoothCam, which positions the camera itself.

## How it decides

Every object the camera's sweep hits is judged by its collision layer's rule:

- **Measure** (default): a bumper the size of the player is stood at the pivot
  and sampled by rays from where the camera would be; the object stops the
  camera when it covers enough of those samples *and* the shape under the hit
  is not small (bounding radius under a minimum). A pole covers little and is
  ignored; a wall covers all and stops. What is small is faded, never collided.
- **Stop**: always stops the camera (terrain, ground, animated statics).
- **Through**: never stops it.
- **Fade**: never stops it, and the object is faded whole (trees).

Prediction rays to either side, above and below see a wall coming so the
camera is already easing in; the pulled-in distance is held briefly after the
way clears so a row of posts does not bounce it.

Modders can give single objects a rule in
`Data/SKSE/Plugins/ModernCameraCollision/Overrides/*.ini`; see
`_Example.ini.txt` there.

## Building

Needs Visual Studio 2022 (C++), CMake ≥ 3.21 and vcpkg (`VCPKG_ROOT` set).

```powershell
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release
```

The build deploys the DLL, its PDB and `dist/` into
`%LOCALAPPDATA%/ModOrganizer/Skyrim Special Edition/mods/ModernCameraCollision`;
override with `-DMCC_DEPLOY_DIR=...` or set it empty to skip.

`powershell -File docs/package.ps1` builds the release archive.

`docs/NOTES.md` has the rules, the engine facts, and the two pieces that need
the Creation Kit: the ESP and the compiled MCM script. `docs/translate-config.py`
moves MCM text into `Interface/Translations`; `docs/translations.py` holds the
other languages.

## Licence

Copyright (C) 2026 DisizMod.

Modern Camera Collision is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at your
option) any later version. It is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License (`LICENSE`, or <https://www.gnu.org/licenses/gpl-3.0.html>)
for more details.

It is GPL because it links CommonLibSSE-NG, which is GPL-3.0-or-later.
Third-party notices in `THIRD-PARTY.md`. Source:
<https://github.com/DisizMod/ModernCameraCollision>.
