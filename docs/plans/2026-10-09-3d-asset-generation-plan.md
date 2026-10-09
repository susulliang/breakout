# 3D Asset Generation Plan

**Status:** First procedural asset batch implemented; visual/gameplay acceptance remains
**Project:** Breakout, C++17 / Raylib 5.0
**Related documents:** `docs/port3d/assets.md`, `docs/port3d/progress.md`,
`docs/plans/2026-10-09-2d-to-3d-port.md`

## 1. Goal And Guardrails

Build a coherent first set of 3D characters, weapons, animations, props, and
environment surfaces for the isometric shooter without disrupting deterministic
gameplay or requiring administrator access. The initial low-poly batch is now
implemented with runtime C++ mesh generation; this plan records that checkpoint
and the remaining acceptance work.

- Preserve the existing 2D sprites/HUD and procedural 3D fallback while models
  are introduced incrementally.
- Keep the M3 actor dimensions, part transforms, grip, and muzzle positions in
  the tested `ActorPose` contract. Dimensions remain provisional pending
  in-game visual review and gameplay collision integration.
- Use one shared CPU pose for model transforms, aiming, hit volumes, sockets,
  and death-piece snapshots. Render-only animation must not move gameplay roots.
- Keep model authoring optional. The game should still build and run if Blender
  or imported assets are unavailable.
- Generate static, separately transformable meshes in C++ for the first batch.
  Do not make skeletal animation, third-party exporter add-ons, Blender, or a
  machine-wide install a v0.3 dependency.

## 2. Art Direction

### Visual Style

Use **stylized low-poly industrial science fiction**, with the readable
stick-figure silhouette and bold shapes of the existing sprites translated to
simple 3D forms.

- Favor large, distinctive forms and bevels over fine surface noise.
- Use flat or gently faceted shading, a small number of material regions, and
  restrained contact shadows.
- Keep a clear outline in silhouette through shape and value contrast; do not
  depend on a screen-space outline shader.
- Avoid photorealism, tiny labels, transparent grilles, and fine trim that
  disappears at the isometric gameplay scale.
- Keep props visually quieter than actors, pickups, and the exit beacon.

### Palette

These are starting swatches, not a locked color-management profile:

| Use | Base color | Accent |
|---|---|---|
| Player armor | cobalt `#428DEB` | cyan `#48D7EE`, pale steel |
| Enemy armor | crimson `#C83A43` | amber `#F3A246`, dark red |
| Facility structure | gunmetal `#343B4A`, slate `#5B6678` | cool steel |
| Concrete / worn surfaces | neutral gray `#8E929A` | soot and restrained rust |
| Safety markings | charcoal | yellow `#FFC247` |
| Exit / objective | deep teal | bright cyan `#55F0E0` |
| Medical room | cool steel | blue/teal lights |
| Workshop room | dark steel | warm orange lights |

Reserve the brightest saturated colors for player/enemy identity, weapon
readability, pickup recognition, and objective guidance. Ensure the red enemy
silhouette remains distinguishable from red hit feedback by using different
brightness and shape.

## 3. World Scale And Presentation

### Fixed Engine Conventions

- `+Y` is up; `+X` and `+Z` are the ground plane; local `+Z` is model forward.
- One world unit equals one map cell. Cell positions use centers at
  `(x + 0.5, z + 0.5)`.
- Character root is at the center of the feet on the floor. Do not add root
  translation to walk cycles.
- Apply transforms before export. Export at unit scale with no negative parent
  scale, hidden required modifiers, or external texture paths.
- World is viewed by an orthographic camera calibrated to an isometric view.
  Important details should read at 1280x720 without zooming.

### Provisional Asset Dimensions

The values below are prototype targets. `ActorPose` tests and user visual review
supersede them before final art/collider dimensions are locked.

| Asset | Provisional dimensions in world units | Notes |
|---|---:|---|
| Player | height `1.30`, width `0.46`, depth `0.34` | Feet-root; head about `0.24` high |
| Standard enemy | height `1.24`, width `0.48`, depth `0.36` | Slightly broader silhouette |
| Small pickup | maximum extent `0.34` | Floats above a one-cell footprint |
| Slingshot | length `0.38` | Compact silhouette, aligned to hand grip |
| Shotgun | length `0.56` | Wide barrel and stock read clearly |
| Marksman rifle | length `0.78` | Long barrel/sight silhouette |
| Gatling | length `0.68` | Distinct drum/barrel cluster |
| Exit beacon | height `0.90`, footprint `0.16` | Vertical cyan landmark |
| Locker | `0.46 x 0.50 x 0.95` | Width x depth x height |
| Vending machine | `0.62 x 0.50 x 1.10` | Bright face panel, non-emissive fallback |
| Parts table | `0.95 x 0.68 x 0.68` | Scattered detail must stay sparse |
| Partition screen | width `1.80`, height `1.20`, depth `0.12` | Grid-aligned, opaque panels |
| Wire fence | width `1.80`, height `1.00`, depth `0.04` | Opaque low-detail wire geometry |
| Screen panel | `0.48 x 0.32 x 0.82` | Readable cyan screen region |

At the current camera scale, one world unit is roughly 45 projected pixels.
Check the player and weapon silhouettes against the existing 64-pixel sprite
footprint and the one-cell debug grid before locking these dimensions.

## 4. Asset Inventory

### Player

Generate one blue tactical player model, with:

- Six independently transformable meshes: `part_head`, `part_torso`,
  `part_arm_l`, `part_arm_r`, `part_leg_l`, and `part_leg_r`.
- A clear head/body separation suitable for the existing 4x headshot rule.
- A readable hand/grip pose and a neutral pose that works for all four weapons.
- Simple joint pivots compatible with the CPU pose hierarchy.
- Optional material variants only after the base mesh reads at the target view.

### Enemies

Generate a crimson base humanoid sharing the six-part hierarchy, but with a
broader head/shoulder silhouette and amber details. First produce one standard
enemy. Add alternate armor/material variants only after the standard enemy's
head hit region, stagger, and death separation are validated.

Do not make variants by changing collision dimensions in art. Gameplay collider
dimensions stay in `ActorPose`.

### Weapons

Generate four separate low-poly meshes:

1. **Slingshot:** fork, elastic, and pouch; clearly the lightest/shortest weapon.
2. **Shotgun:** broad stock and barrel; visibly distinct from the slingshot.
3. **Marksman rifle:** long barrel and simple sight; longest silhouette.
4. **Gatling:** multi-barrel front and drum/body; rotating detail may be a later
   procedural animation.

Each model must have named `socket_grip` and `socket_muzzle` nodes that match the
M3 socket contract. Include a muzzle direction marker in the authoring preview.
World pickups use the model; existing 2D weapon icons can remain in the HUD.

### Environment Props

Port the six existing non-solid prop types as separate models:

- Partition screen with opaque blue panels and a steel frame.
- Wire fence aligned to X/Z map axes, using thin opaque geometry initially.
- Parts table with only a few large, high-contrast pieces.
- Orange storage locker.
- Green-accented vending machine.
- Blue screen panel / terminal.

Add the cyan exit beacon as a separate objective model. Props stay out of
collision, pathfinding, line-of-sight, and projectile queries unless a later
gameplay change explicitly adds that behavior.

### Environment Surface Set

Produce a compact modular surface set rather than a unique texture per room:

- **Floor:** clean metal plate, worn metal plate, concrete, and dark service
  grate. Use geometry or a non-alpha material for grates.
- **Wall:** painted steel panel, concrete, and a darker industrial wall.
- **Trim:** hazard stripe, edge band, bolts, vents, and panel seams.
- **Decals:** a small set of floor arrows, warning marks, and room identifiers;
  keep them optional and do not bake lighting into them.

The map mesh now carries UVs and the 3D renderer generates repeating 128x128
floor/wall textures at runtime. Vertex colors remain the fallback. This is an
albedo/base-color path only: normal maps, emissive maps, bloom, and advanced
lighting are not implemented.

## 5. Animation Plan

### Runtime Strategy

For the first production pass, generate **procedural pose clips** in C++ from
the shared M3 `ActorPose`, rather than exporting skeletal clips. This avoids
different rendered and collision poses, keeps weapon sockets stable, and works
with the current fixed-size gameplay state. Imported mesh parts remain static;
the CPU animation applies their local transforms.

All clips are root-motion-free, deterministic, and parameterized by normalized
phase. Movement phase follows actual ground speed, including the enemy hit-slow
factor. Animation never changes HP, collision, weapon timing, or projectile
results.

### Required Clips

| Character | Clip | Target timing | Motion |
|---|---|---:|---|
| Player | idle loop | `2.4 s` | Subtle torso breathing and weapon sway |
| Player | walk loop | `0.60-0.75 s` at base speed | Alternating legs/arms; modest body bob |
| Player | fire recoil | `0.10-0.14 s` | Weapon/forearm kick and quick return |
| Player | hit reaction | `0.18-0.28 s` | Short upper-body flinch; no root displacement |
| Player | defeat | `0.35-0.55 s` | Optional readable collapse after GAME_OVER |
| Enemy | idle loop | `1.8-2.6 s` | Small asymmetric breathing/head motion |
| Enemy | chase loop | `0.55-0.70 s` | Stronger alternating gait, speed follows AI |
| Enemy | hit stagger | `0.20-0.35 s` | Punch lean/recoil, consistent with one-second slow |
| Enemy | death break | `0.25-0.45 s` | Snapshot six live part transforms into debris |

The timing is a starting brief. Tune against actual gameplay and ensure the
enemy's one-second slow does not hold a frozen hit pose for the entire second.
Keep torso/head hit-volume transforms derived from the same animated pose.

### Weapon Motion

- Slingshot pouch/string pull: optional short draw pose when firing.
- Shotgun/rifle: recoil followed by smooth recovery.
- Gatling: barrel spin only while firing, with no impact on aim or muzzle socket.
- Pickups: slow vertical bob (`0.03-0.06` units) and yaw rotation; keep the
  collision pickup radius independent of this visual offset.

## 6. Texture And Material Budgets

### Resolution

- Target approximately **128 texels per world unit** for floor/wall tiles.
  A one-unit tile therefore starts at `128 x 128` texels.
- Use `256 texels per world unit` only for close hero surfaces or weapon
  markings that remain visible at baseline zoom.
- Tileable floor/wall source images: `128 x 128` or `256 x 256` pixels.
- Packaged environment atlases: start at `1024 x 1024`; use at most
  `2048 x 2048` when the catalog genuinely needs the space.
- Leave at least 4 pixels of atlas padding at 1024 resolution, with edge
  dilation, to avoid mip bleed.

### Maps And Color

- First pass: vertex colors and a small number of albedo/base-color textures.
- Base color is sRGB; normal/roughness data, if added later, is linear.
- Use mostly high-roughness metal/concrete (`roughness 0.65-0.95`) and restrained
  metallic response. Do not paint highlights or shadows into base color.
- Add normal maps only after the world shader supports and validates the
  tangent-space convention; they are not required for the first asset batch.
- Use opaque materials. Avoid alpha-tested fences and transparent screens until
  the transparent depth/write/sort path is implemented and tested.
- Emissive color is a later optional map/flag. Until M5 supports it, use
  restrained bright base colors instead of promising bloom.

### Suggested Runtime Paths

```text
assets/models/player.glb
assets/models/enemy.glb
assets/models/weapon_slingshot.glb
assets/models/weapon_shotgun.glb
assets/models/weapon_marksman.glb
assets/models/weapon_gatling.glb
assets/models/prop_partition_screen.glb
assets/models/prop_wire_fence.glb
assets/models/prop_parts_table.glb
assets/models/prop_locker.glb
assets/models/prop_vending_machine.glb
assets/models/prop_screen_panel.glb
assets/models/exit_beacon.glb
assets/textures/environment/floors.png
assets/textures/environment/walls.png
assets/textures/environment/trim.png
```

Keep Blender `.blend` files, reference images, source PSD/Krita files, and
unpacked texture sources outside `assets/`, for example under `art/source/`.
Only runtime exports belong under `assets/`, which CMake copies beside the
executable.

## 7. Geometry Budgets

These limits are for the first low-poly pass, not engine hard limits:

| Asset group | Target triangle budget |
|---|---:|
| One six-part character | `700-1,400` total |
| One weapon | `120-450` |
| One small prop | `100-500` |
| One large prop / partition | `250-900` |
| Exit beacon | `80-300` |

Spend triangles on the silhouette, hands, headshot-readable head, and weapon
profile. Omit unseen undersides and hidden internal faces where safe. Keep
repeated floor/wall materials on the chunk mesh path rather than creating a
draw call per tile.

## 8. Authoring And Generation Workflow

### Stage A: Style Sheet And Blockout

- Create a one-page palette/material/silhouette sheet using the table above.
- Put primitives for player, one enemy, all four weapon lengths, a one-unit
  floor tile, a wall, and each prop against a one-unit grid.
- Review the pilot in the current orthographic camera at 1280x720.
- No detailed textures or animation polish until head, weapon, and pickup
  silhouettes read correctly.

### Stage B: Gameplay Contract Gate

- Complete M3 `ActorPose` dimensions, part pivots, colliders, grip sockets, and
  muzzle sockets with tests.
- Capture a screenshot of debug volumes over the primitive mannequins.
- Confirm units and aim orientation in both 2D and 3D backends.
- Publish the contract before final model production begins.

### Stage C: Procedural Pilot Batch (Implemented)

- Generate player/enemy parts, weapon meshes, six props, pickups, effects, and
  beacon geometry in `ProceduralScene3D`.
- Use `ActorPose` to position articulated parts and attach each weapon.
- Cache GPU meshes/materials for reuse and release them before window shutdown.
- Keep optional `.glb` loading deferred; the current executable has no model
  file dependency.

### Stage D: Characters, Weapons, And Procedural Animation

- Finish the six-part player/enemy assets.
- Implement idle, walk, recoil, stagger, defeat, and death-break pose functions
  against the shared CPU pose.
- Generate weapon geometry one at a time: slingshot, shotgun, marksman, gatling.
- Compare render transforms to debug hit volumes and all muzzle/grip markers.

### Stage E: Props And Environment Textures (First Pass Implemented)

- Generate the six props and exit beacon from the common palette.
- Add UVs and bind runtime-generated repeating floor/wall textures; retain
  vertex-color fallback.
- Defer authored trim/decal atlases, normal maps, and emissive textures until
  their material/shader paths exist.
- Review repeated seams, projected readability, and light response in-game.

### Stage F: Art Polish And Package Validation

- Tune proportions, material contrast, subtle wear, and animation timing from
  screenshots at baseline resolution.
- Check each gameplay room palette under both medical and workshop lighting.
- Run the game from the packaged executable outside the repository.
- Remove source-only files from the runtime package and verify asset copy on
  asset-only changes.

Blender is optional and should be installed only as a portable/user-local tool
if chosen. Do not add a machine-wide installer, admin requirement, or Blender
runtime dependency. The procedural C++ path remains the guaranteed fallback.

## 9. Acceptance Checklist

- [x] Player, enemy, weapons, props, and beacon use the documented world axes,
      pivots, naming, and provisional dimensions.
- [x] M3 tests cover actor part transforms, grip, and muzzle positions; gameplay
      collision still needs integration with the 3D pose.
- [ ] Refine and lock actor, collider, grip, and muzzle dimensions after visual
      dimensions.
- [ ] Player/enemy silhouettes and all four weapons are distinguishable at
      1280x720 without zooming.
- [x] CPU pose drives rendered parts and weapon attachment.
- [ ] CPU pose also drives gameplay hit volumes and death snapshots.
- [ ] Walk cycles follow actual movement speed; hit-slow and recoil do not
      desynchronize the hit volume.
- [ ] Environment textures tile cleanly and remain readable without baked
      lighting or unsupported shader features.
- [ ] Missing/corrupt models or textures fall back to procedural geometry and
      do not prevent the game from starting.
- [ ] Model/texture resources are loaded once, reused, and released by explicit
      non-copyable owners.
- [ ] Runtime package loads from outside the source tree with no admin-installed
      tools or source-file dependency.
- [ ] Art review screenshots include the one-unit grid, debug colliders, each
      weapon socket, medical/workshop palettes, and gameplay-scale views.

## 10. Immediate Next Actions

1. Review the launched 3D build at baseline resolution; tune actor/weapon scale,
   facing, and material contrast from screenshots.
2. Connect swept 3D projectile tests to wall/enemy resolution with nearest-hit
   ordering and pose-derived head/body hit volumes.
3. Use shared pose snapshots for 3D death debris and test repeated level rebuild
   plus GPU resource cleanup.
4. Verify packaged execution outside the source tree; keep the 2D backend as
   default until the M6 parity gate.

## 11. Implemented Checkpoint

- Cached low-poly player/enemy parts, four weapon meshes, six prop meshes,
  pickups, particles/debris geometry, and exit beacon are generated in C++.
- The shared CPU pose drives character articulation and weapon attachment;
  unit tests cover pose orientation, gait, stagger, recoil, and sockets.
- Map UVs and runtime-generated repeating floor/wall textures are integrated.
- Collision sweep primitives are unit-tested but not yet wired into gameplay.
- Release build and all five CTest targets pass. The 3D executable has been
  launched for hands-on visual review; appearance/parity is not yet signed off.
