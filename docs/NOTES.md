# Modern Camera Collision — notes

Grew out of a probe inside OpenExpressionReplacer (Sept 18–21, 2026); the
history of how each rule was found is in that repo's commits (`28762ca`
through `d10dbab`). This is what came out, and why.

## The rules

1. The engine's third-person camera does one sphere sweep per frame from the
   pivot to the wanted position on the `L_CAMERA` layer. We front its hit
   collector and judge every hit before the engine sorts them; a dropped hit
   is simply not forwarded, and the engine takes the next-closest.
2. **One verdict** for the camera's own hits and the whiskers' alike:
   - the player's own body: never stops (the engine's to ignore);
   - terrain and ground: always;
   - a layer set to *through*: never, and the occluder is faded whole;
   - a layer set to *stop*: always;
   - otherwise: stops only when **enough of the disc's rays from the player
     land on it** AND **the shape under the hit is not small**.
3. **The disc**: through the hit point, facing the player; the centre plus
   rings of eight samples out to its radius. Each sample is reached by a ray
   from the player (starting 40 units out of the body) through it and a set
   distance past it; a sample is taken when the same reference stops that
   ray. Cover is the taken share. The ray ends past the disc so the same kit
   piece's wall further back does not stand in for a beam. Hysteresis per
   reference: dropped below the threshold, re-kept from +12 points,
   re-dropped at −25, remembered 0.5 s.
4. **Small**: the trishape under the hit (its bound sphere holds the point,
   10 units of slack) has a bound radius under the minimum. Small is faded
   and never collides. A pot ≈ 20, a campfire 60–100, kit walls/floors
   190–220, beams and posts 170–335 (they share their wall's bound, so the
   disc is what lets beams through).
5. **The category is the collision layer** (the COLL records, `L_CAMERA`'s
   CNAM list is what the camera can hit at all), never the base form or the
   model path. A layer gets a rule only after checking what is actually on
   it in Skyrim.esm.
6. **Motion**: the distance eases in toward the nearer of the engine's stop
   and the whiskers' cap (small constant — the engine parks the camera ~5
   units short of a surface, so easing in is time spent inside it), is held
   after the way clears, then eases out. Cinemachine's damping-when-occluded,
   smoothing time, damping.
7. **Whiskers** (Nesky, *50 Camera Mistakes*): rays from the pivot to where
   the camera would be at yaw ±10/20/30 (horizontal), pitch ±10/20/30
   (vertical), yaw ±20 with pitch ±20 (diagonal), and one 80 units straight
   back. Each hit is judged by the one verdict, once per reference an
   update. The ±10 ones cap the distance at what they measure free (#7).
   The swing (#5) — turning the free rotation away from a blocked side — is
   built and **off**: without the player's heading and movement input it
   fights their intent (#6). Positive yaw is counter-clockwise from above;
   the engine's free-rotation yaw and pitch both run the other way.
8. **Fade**: the shapes under a let-through hit get a blend alpha property
   of ours on a **cloned** material (materials are shared engine-wide), the
   alpha eased over 0.2 s, restored when no longer picked or when the camera
   collision stops running (a per-frame tick from Present). Shapes with an
   alpha property of their own are left alone: their shadow pass multiplies
   texture alpha by material alpha, and a window's frame at 0.3 stops
   casting the light shaft. The shadow-pass query is hooked to show faded
   shapes as they were.

## Engine facts

- `ThirdPersonState::UpdateCameraCollision` SE id 49980 (no AE id in
  CommonLibSSE-NG 7.5.1 — AE is refused); `hkpWorld::LinearCast` 60554;
  `hkpWorld::CastRay` 60551. CommonLib declares `hkpCdPointCollector`'s
  dtor/`Reset` without defining them: the collector ABI is laid out by hand.
- Statics are one MOPP shape per reference.
- Shader-property `alpha` alone shows nothing on statics; `materialAlpha`
  alone only on already-blended shapes; a visible fade needs the property.
- The camera's translation is overwritten after the engine's update; the
  engine's own rotation comes from yaw/pitch, not from where the camera is.
- **SmoothCam is incompatible**: it positions the camera itself and does its
  own collision, so nothing here takes effect with it loaded.

## Setup that needs tools outside this repo

- **The ESP** `dist/ModernCameraCollision.esp` (not yet made): a quest with
  the `MCC_MCM` script attached and a `PlayerAlias` alias carrying
  `SKI_PlayerLoadGameAlias`, as MCM Helper's "Creating a Config Script"
  page describes. Make it in the Creation Kit or xEdit; flag it ESL.
- **The script** `dist/Source/Scripts/MCC_MCM.psc` compiles against MCM
  Helper's SDK (`MCM_ConfigBase.psc`) and SkyUI's; the `.pex` goes to
  `dist/Scripts/`.
- MCM Helper and SkyUI are runtime requirements.
