# 3D Model Asset Contract

The first v0.3 asset batch is generated as cached low-poly Raylib meshes in
`src/ProceduralScene3D.cpp`. It has no Blender, `.glb`, or machine-wide tool
dependency. This document records the runtime contract and separates the
integrated procedural prototype from future authored/exported assets.

## Runtime And Authoring Layout

- The integrated 3D assets are generated once at startup by
  `ProceduralScene3D`; cached meshes are reused for characters, four weapons,
  six props, pickups, effects, and the exit beacon.
- Environment floor/wall textures are generated in memory by
  `SceneRenderer3D` at 128x128 and repeated using map-mesh UVs. They are not
  external image files or a normal/emissive-map pipeline.
- `assets/models/` is reserved for optional future exports. No `.glb` files are
  required by the current executable.
- The legacy 2D sprites and renderer remain available. The sprite generator
  (`node tools/generate_sprites.js`) is separate from 3D geometry generation.
- If optional exported assets are introduced later, keep authoring sources
  outside the runtime package and preserve the procedural fallback.

## Coordinate And Scale Rules

- Use right-handed gameplay coordinates: `+Y` up, `+X/+Z` on the ground, and
  local `+Z` forward.
- One world unit equals one map tile. Character roots sit at the center of the
  feet on the ground plane; weapon and prop pivots are documented per asset.
- Apply object transforms before export. Export unit scale and no negative or
  non-uniform parent scale.
- `ActorPose` now defines a 1.30-unit character height, 0.145-unit head radius,
  0.235-unit body radius, feet-root placement, six part transforms, and weapon
  grip/muzzle positions. These are prototype values; visual review and gameplay
  collision integration remain necessary before they are treated as final.
- Prefer vertex colors or embedded textures. Avoid external texture paths,
  Blender-only modifiers, required add-ons, and runtime skinning for the first
  asset batch.

## Integrated Procedural Batch

- Player and enemy use six separately transformable cached meshes: head, torso,
  left/right arms, and left/right legs.
- Four procedural weapon silhouettes are generated: slingshot, shotgun,
  marksman rifle, and gatling. Their grip/muzzle positions are attached through
  the shared CPU pose; there are no named glTF nodes.
- Six static prop meshes cover partition, fence, parts table, locker, vending
  machine, and screen panel. A cyan exit beacon is generated separately.
- Pickups, bullets, particles, and death fragments also use 3D geometry in the
  3D scene.

Keep detachable character parts as independent meshes with pivots at the
corresponding joint. Walk, idle, stagger, and recoil are procedural CPU-pose
motions. The pose currently drives rendered parts and weapon attachment, but
projectile hit testing and death-piece snapshots still use legacy 2D gameplay
coordinates and have not yet been unified with the 3D pose.

## Generation And Validation

1. Keep actor dimensions, part hierarchy, grip/muzzle offsets, and collider
   dimensions covered by `ActorPose` tests; revise them from visual/gameplay QA.
2. Review the procedural mannequin, weapon silhouettes, feet/root alignment,
   and aim direction in the isometric camera before treating proportions as
   final.
3. Keep Blender optional and user-local. The current game build requires only
   CMake, the existing Raylib dependency, and the repository's Windows toolchain;
   do not require elevation or install software machine-wide.
4. If `.glb` import is added later, add a Raylib model-load smoke test for
   finite bounds, scale, pivots, material availability, and graceful fallback.
5. Verify the packaged executable from outside the source directory and check
   repeated level rebuilds and GPU resource cleanup.

The procedural asset batch is integrated as a first playable prototype, not
final art. Visual scale/material review, gameplay-space hit volumes, 3D
projectile-vs-wall/enemy ordering, and packaged-build verification remain open.
