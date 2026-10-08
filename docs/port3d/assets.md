# 3D Model Asset Contract

This document prepares the v0.3 prototype for model authoring. Final gameplay
models should not be generated until M3 lands `ActorPose` and its socket tests;
the model origin and part names below are stable, but exact actor dimensions
remain provisional until that contract is measured in-game.

## Runtime And Authoring Layout

- Runtime exports go in `assets/models/`; use self-contained binary glTF
  (`.glb`) so textures and materials travel with each model.
- Blender source files and working textures belong in an authoring-only
  directory such as `art/blender/`, never in `assets/`. CMake copies everything
  beneath `assets/` into the runtime package.
- Keep the project playable without imported models. Procedural geometry is the
  required fallback and the default during M3.
- The sprite generator remains separate: `node tools/generate_sprites.js`
  produces 2D atlases and is not a 3D exporter.

## Coordinate And Scale Rules

- Use right-handed gameplay coordinates: `+Y` up, `+X/+Z` on the ground, and
  local `+Z` forward.
- One world unit equals one map tile. Character roots sit at the center of the
  feet on the ground plane; weapon and prop pivots are documented per asset.
- Apply object transforms before export. Export unit scale and no negative or
  non-uniform parent scale.
- Keep the actor dimensions provisional until M3's `ActorPose` measurements are
  checked against the one-tile grid and collision volumes.
- Prefer vertex colors or embedded textures. Avoid external texture paths,
  Blender-only modifiers, required add-ons, and runtime skinning for the first
  asset batch.

## First Model Batch

- `player.glb` and `enemy.glb`: six separately named parts: `part_head`,
  `part_torso`, `part_arm_l`, `part_arm_r`, `part_leg_l`, `part_leg_r`.
- `weapon_slingshot.glb`, `weapon_shotgun.glb`, `weapon_marksman.glb`, and
  `weapon_gatling.glb`: include `socket_grip` and `socket_muzzle` nodes aligned
  to the M3 weapon contract.
- Props are separate static models, named by gameplay type, with a ground
  pivot. Keep wire fences as thin opaque geometry for the first pass.
- `exit_beacon.glb`: a readable vertical cyan marker rooted at the tile center.

Keep detachable character parts as independent meshes with pivots at the
corresponding joint. Do not bake walk cycles, recoil, or death motion into
assets; the shared CPU pose drives rendering, hit volumes, sockets, and death
snapshots.

## Generation And Validation

1. Freeze actor dimensions, part hierarchy, grip/muzzle offsets, and collider
   dimensions in `ActorPose` tests before producing final geometry.
2. Author one player mannequin and one slingshot pilot against a one-unit grid.
   Review silhouette, feet/root alignment, socket transforms, and camera
   readability before generating the rest of the catalog.
3. Keep Blender optional and user-local. The current game build requires only
   CMake, the existing Raylib dependency, and the repository's Windows toolchain;
   do not require elevation or install software machine-wide.
4. Add a Raylib model-load smoke test before importing game content. Check
   successful load, expected named meshes/nodes, finite bounds, scale, pivot,
   material availability, and graceful fallback when a model is missing.
5. Test the packaged executable from outside the source directory so the
   source-tree asset fallback cannot conceal missing runtime exports.

Final assets are not part of this checkpoint. M3's shared pose/socket contract
is the gate for generating the first real 3D model batch.
