# 3D Port Baseline

**Captured:** 2026-10-09
**Purpose:** Record the source/build baseline and tested gameplay contracts before the renderer migration.

## Working-Tree And Machine Constraints

- Existing prototype modifications and generated sprite atlases were present before this port stage and are preserved.
- No repository reset, checkout, administrator action, global install, or machine-wide configuration was used.
- New build directory: `build/port3d`.
- Toolchain: local WinLibs MinGW GCC 16.1, Ninja, CMake 4.4.1, Raylib 5.0 source already available in `build/_deps/raylib-src`.
- Existing `build/verify` Release build succeeded before M0 changes.
- Repository worktree was already dirty. This baseline does not attribute every existing modification to the port.

## Current 2D Contracts

- Projection: screen X = `32 * (x - y)` pixels; screen Y = `16 * (x + y)` pixels.
- Grid visual: 64x32 pixel diamond, 32 pixel wall extrusion.
- Levels: deterministic generation from level number; 30x30 level 1 through 100x100 level 10; rooms joined by L-shaped corridors.
- Screen-cardinal movement: W/up, A/left, S/down, D/right; mouse aim is independent.
- HP: 100 maximum; medkit key 5 restores up to 40, with three per run and no consumption at full HP.
- Enemy hit: headshot multiplier 4x; one-second stagger at 32% chase speed and directional knockback.
- Weapon slots: slingshot, shotgun (90% placement roll), marksman (50%), gatling (20%); keys 1-4.
- Player UI and crosshair are screen-space. Cursor floor-tile highlight is absent.
- Runtime source assets: player/enemy/weapon/enemy-parts PNG atlases and lighting fragment shader.

## Automated Baseline Tests

Implemented in `tests/PortMathTests.cpp` and `tests/GameplayParityTests.cpp`.

- Projection/inverse round-trip for a grid of negative and positive Cartesian points.
- Deterministic level generation by generating every level twice and comparing grids.
- Correct expected map dimensions and valid floor spawn/exit cells for levels 1-10.
- Four-way connectivity of every generated floor cell from player spawn.
- Horizontal, vertical, and diagonal LOS blocker handling, including excluded endpoints.
- Medkit healing, clamping, full-health non-consumption, and empty inventory.
- Bounded fixed-pool sizes for enemy slots and weapon pickups.

Command run:

```powershell
cmake -S . -B build/port3d -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DFETCHCONTENT_SOURCE_DIR_RAYLIB="C:/Users/Yufeng.Liang/Documents/breakout/build/_deps/raylib-src"
cmake --build build/port3d --parallel
ctest --test-dir build/port3d --output-on-failure
```

Result at baseline capture: 2/2 CTest tests passed. The test checks use an always-active check helper and remain active in Release (`NDEBUG`) builds.

## Runtime Visual QA

The approved prototype was previously built/launched in this workspace and its player, enemy, weapons, and enemy-fragment atlas loads were observed in Raylib logs. This M0 stage did not capture new screenshots or re-test interactive mouse/keyboard behavior. Visual baseline screenshot capture remains a manual QA item; do not report it as complete.

## Known Issues To Address Deliberately

- Map drawing historically centered tile art at integer coordinates, while collision uses floor-based `[cell,cell+1)` intervals.
- LOS previously skipped walls on horizontal/vertical rays; the tested grid algorithm now checks all intermediate cells.
- Screen-space head/body hit tests and pixel-height bullet presentation must be replaced by explicit world-space volumes.
- Wall hits and enemy hits are processed in separate passes; M3 must compare first impact along the shot segment.
- Current point-light selection mixes tile and projected pixel coordinates; world-space lighting will replace it.
- Current map and actor renderer is 2D painter sorting, not 3D geometry.
