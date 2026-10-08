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

## Verification

- Configured and built `build/port3d` with the repository-local Raylib source
  override; no administrator privileges or machine-wide installation used.
- `cmake --build build/port3d --parallel` succeeds.
- `ctest --test-dir build/port3d --output-on-failure` passes all three tests:
  `breakout_math`, `breakout_gameplay`, and `breakout_map_mesh`.
- Launched `Breakout.exe --renderer=3d` on Intel UHD 770 / OpenGL 3.3; Raylib
  initialized and uploaded the level chunk meshes successfully.
- `git diff --check` succeeds; Git only reports its existing LF-to-CRLF
  working-copy notices.

## Outstanding

- Camera axis projection, cursor alignment at viewport corners, resize behavior,
  foreground-wall occlusion, and repeated GPU rebuild/resource balance still
  need interactive visual QA.
- The 3D backend is still diagnostic for actors and props. Physical projectile
  sweeps, unified hit volumes, 3D character/weapon/prop geometry, world-space
  debris/effects/lighting, and final parity remain future milestones.
- M3 is the next implementation stage. Do not make 3D the default or remove the
  legacy backend until M6 acceptance is complete.
