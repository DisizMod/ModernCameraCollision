# Modern Camera Collision

An SKSE plugin for Skyrim SE 1.5.97 and AE (1.6.x, 1.7.x); not VR. Works
under SmoothCam: its sweeps get the same rules and fade; the motion and the
prediction rays are left to it. The third-person camera stops at walls
and floors, sees around beams, posts, pots and campfires, eases its way in
and out, and fades what it is let through. Configured through MCM Helper.

Requires SKSE, Address Library, SkyUI and MCM Helper. Incompatible with
SmoothCam, which does its own camera positioning.

## Building

```powershell
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/Users/Sami/source/vcpkg/scripts/buildsystems/vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release
```

The build deploys the DLL, its PDB and `dist/` into
`%LOCALAPPDATA%/ModOrganizer/Skyrim Special Edition/mods/ModernCameraCollision`.
Override with `-DMCC_DEPLOY_DIR=...`.

`docs/NOTES.md` has the rules, the engine facts, and the two pieces that
need the Creation Kit: the ESP and the compiled MCM script.
