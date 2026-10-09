# 2D-to-3D Port Progress

Implementation follows `docs/plans/2026-10-09-2d-to-3d-port.md`. The legacy
backend remains the default until the M6 parity gate.

## Completed

- M0: baseline notes, local asset synchronization, math/gameplay tests, and
  horizontal/vertical LOS correction.
- M1: canonical half-open cell coordinates and cell centers; shared
  footprint-aware X-first movement; captured frame input; calibrated
  orthographic camera; `--renderer=2d|3d` and F8 selection; ground-plane
  ray picking; temporary 3D grid/actor diagnostic scene.
- M2: CPU map chunk construction at 16x16 cells; floor grout/checker surfaces;
  wall top/exposed-face generation with internal-face culling; uploaded GPU
  mesh cache, rebuilt on level start/R and released at shutdown; depth-tested
  3D map pass with the existing 2D HUD.
- M3 asset prototype: tested CPU `ActorPose` and `Collision3D` helpers;
  generated low-poly character parts, four weapons, six prop meshes, pickup,
  effect, and beacon geometry; procedural floor/wall UV textures; shared pose
  drives character articulation and weapon attachment in the 3D renderer.

## Verification

- Configured and built `build/port3d` with the repository-local Raylib source
  override; no administrator privileges or machine-wide installation used.
- `cmake --build build/port3d --parallel` succeeds.
- `ctest --test-dir build/port3d --output-on-failure` passes all five tests:
  `breakout_math`, `breakout_gameplay`, `breakout_map_mesh`,
  `breakout_actor_pose`, and `breakout_collision_3d`.
- Launched `Breakout.exe --renderer=3d` on Intel UHD 770 / OpenGL 3.3; Raylib
  initialized and uploaded the level chunk and procedural asset meshes
  successfully. The 3D window is left open for interactive user review.
- `git diff --check` succeeds; Git only reports its existing LF-to-CRLF
  working-copy notices.

## Outstanding

- Camera projection, cursor alignment at viewport corners, resize behavior,
  actor/weapon scale and aim readability, foreground-wall occlusion, and
  repeated GPU rebuild/resource balance still need interactive visual QA.
- `Collision3D` sweep helpers are unit-tested but are not connected to gameplay
  bullet resolution. Bullet hits still use legacy 2D screen-space silhouette
  checks, and enemy hit volumes/death snapshots are not yet sourced from the
  shared 3D pose.
- Procedural world-space debris/effects, lighting/material polish, packaged
  execution outside the source tree, and full 2D/3D gameplay parity remain.
- Keep the 2D renderer as default and retain the F8/`--renderer` switch until
  the M6 parity and acceptance gates are met.
