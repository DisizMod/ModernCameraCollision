# Nexus page draft

Title: **Modern Camera Collision**
Category: Camera (or User Interface / Gameplay). Tags: SKSE, MCM, Camera.

---

The third-person camera stops at walls and floors, sees around beams, posts,
pots and campfires, eases its way in and out instead of snapping, and fades
what it is let through.

Vanilla treats every solid thing the same: a fence post pulls the camera in as
hard as a wall does, it snaps, and it snaps back the moment you pass. This
plugin replaces the decision, not the camera: the engine still sweeps and
places it, this only judges each thing the sweep hits.

**What it does**

- **Small things don't stop the camera.** A bumper the size of your character
  is stood at the pivot and sampled by rays from where the camera would be; an
  object stops the camera only when it covers enough of those samples and the
  shape you'd hit is not small. A pole covers a sliver and is ignored; a wall
  covers all of it and stops. What is ignored is faded so it doesn't block your
  view.
- **Eased motion.** The pull-in is eased, the pulled-in distance is held for a
  moment after the way clears, and the way back out is eased — a row of posts
  no longer bounces the camera in and out.
- **Sees walls coming.** Prediction rays to either side, above and below tell
  the camera a wall is approaching before it meets it, so it's already on its
  way in.
- **Fade.** Anything the camera is let through fades to a set alpha and comes
  back; faded shapes keep casting their shadows.
- **A rule per collision layer**, from the MCM: Measure (the bumper decides),
  Stop (always), Through (never), Fade (never, and faded whole — trees).
- **Overrides for modders**: any object, by form id or model path, can be given
  a rule in `SKSE/Plugins/ModernCameraCollision/Overrides/*.ini`. An example
  file is included.
- Everything above is a slider or a toggle in the MCM; a debug page draws the
  rays and the bumper over the game.

**Requirements**

- SKSE64
- Address Library for SKSE Plugins
- SkyUI
- MCM Helper

**Compatibility**

- Skyrim SE 1.5.97: tested. AE (1.6.x, 1.7.x): built for it and verified
  against the 1.7.104 executable, but not yet played — reports welcome. VR: no.
- ESL-flagged ESP (a quest for the MCM only; no scripts run in play). Safe to
  add mid-game; to remove, disable and clean the save's orphan script instance
  or don't bother — it holds no state.
- **Not compatible with SmoothCam**, which positions the camera itself.
- Anything that only changes camera offsets, FOV or the crosshair is fine.

**Known limits**

- Skyrim tags some trees as plain statics (e.g. `pine_young_big01`); those are
  measured like any static rather than faded whole. Give them an override or a
  layer rule if it bothers you.
- Shapes that carry their own alpha (window frames, leaves, grates) are not
  faded by default: fading them removes their shadow. There's a toggle.
- The camera-swing (turning away from a blocked side) is built but off; it
  needs the player's heading to feel right.

**Settings** are in `MCM/Config/ModernCameraCollision/settings.ini` (defaults)
and whatever you change in the MCM is saved by MCM Helper.

**Source and licence.** Copyright (C) 2026 DisizMod. Free software under the
GNU General Public License, version 3 or later — it links CommonLibSSE-NG,
which is GPL. The full licence text is in `LICENSE.txt` in the download and at
https://www.gnu.org/licenses/gpl-3.0.html. The complete source is at
https://github.com/DisizMod/ModernCameraCollision. Built on CommonLibSSE-NG
and MinHook. The prediction rays and the hold-then-ease-out follow John
Nesky's "50 Camera Mistakes" (GDC 2014).

Permissions: open — modify and redistribute under the same licence (GPL-3.0).

---

Files: `ModernCameraCollision-1.0.0.zip` (from `docs/package.ps1`).
