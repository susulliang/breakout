# assets/ - runtime asset layout (Phase 3)

```
assets/
├── shaders/
│   └── lighting.fs      # 2D dynamic lighting pass (GLSL 330, used by LightingSystem)
├── sprites/
│   ├── README.md        # this file
│   ├── player.png       # generated, transparent 4-frame player sheet
│   └── enemy.png        # generated, transparent 4-frame enemy sheet
│   ├── weapons.png      # generated, 4 transparent weapon icons
│   └── enemy_parts.png  # generated, helmet / torso / arm / leg fragments
```

## How the game finds these files

`ResolveAssetPath()` (`src/World.hpp`) probes, in order:

1. `<current working directory>/assets/...` - running from the project root;
2. `<executable directory>/assets/...` - CMake copies `assets/` next to the
   binary after every build (`POST_BUILD` in `CMakeLists.txt`);
3. `BREAKOUT_SOURCE_ASSET_DIR/...` - the source tree path, compiled in, so an
   IDE launch from any directory still resolves.

If a sprite sheet is missing, the game still runs with a procedural fallback.
The lighting pass also degrades to an unlit present if its shader is missing.

## Generated character sheets

Run `node tools/generate_sprites.js` from the project root to regenerate the
transparent RGBA character sheets and gameplay atlases. All are 256 x 64 px:
four 64 x 64 images in one row, with no padding between frames. The generator
renders at 4x resolution and downsamples for clean edges at the game's display
scale.

The player uses blue tactical armor and a compact slingshot matching the default
projectile. Enemies use a related humanoid silhouette with crimson armor and
amber details. The weapon atlas contains the slingshot, shotgun, marksman rifle,
and gatling gun; the enemy-parts atlas provides detached death fragments. All
designs use bold outlines and flat shading to fit the industrial environment.

## Player sprite sheet

`player.png` is loaded by `PlayerSystem::LoadAssets()`. If it is missing,
the procedural player remains available as a fallback. Both paths use the same
contact shadow and Y-sort key.

Sheet format:

| Property  | Value                                                            |
|-----------|------------------------------------------------------------------|
| File      | `assets/sprites/player.png`                                      |
| Layout    | 1 row, 4 frames, left to right                                   |
| Frame     | 64 x 64 px                                                       |
| Frames    | 0 = idle, then 3 walk frames (played at 7 fps while moving)      |
| Facing    | All frames drawn facing **right**; the left direction is mirrored at draw time (negative source width) |
| Pivot     | Frame centre = feet centre; the sprite is anchored on the tile centre |

## Enemy sprite sheet

`enemy.png` uses the same 1-row, 4-frame, 64 x 64 layout. Frame 0 is the idle
pose; frames 1-3 cycle while the enemy chases. Enemies face the player and
mirror the sheet when needed. If the texture is unavailable, the original
procedural red enemy remains as a fallback.

## Gameplay atlases

`weapons.png` is rendered in the four HUD weapon slots and on level pickups.
`enemy_parts.png` supplies the helmet, torso, arm, and leg pieces scattered on
enemy death; pieces settle, remain briefly, fade, and return to a fixed pool.
