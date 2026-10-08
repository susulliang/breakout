# assets/ - runtime asset layout (Phase 3)

```
assets/
├── shaders/
│   └── lighting.fs      # 2D dynamic lighting pass (GLSL 330, used by LightingSystem)
├── sprites/
│   ├── README.md        # this file
│   └── player.png       # OPTIONAL - see "Player sprite sheet" below
└── README.md
```

## How the game finds these files

`ResolveAssetPath()` (`src/World.hpp`) probes, in order:

1. `<current working directory>/assets/...` - running from the project root;
2. `<executable directory>/assets/...` - CMake copies `assets/` next to the
   binary after every build (`POST_BUILD` in `CMakeLists.txt`);
3. `BREAKOUT_SOURCE_ASSET_DIR/...` - the source tree path, compiled in, so an
   IDE launch from any directory still resolves.

If none matches, the game still runs: the lighting pass degrades to an unlit
present (see `LightingSystem::IsAvailable()`) and the player falls back to the
procedural polygon rendering described below.

## Player sprite sheet (optional)

`player.png` is **optional**. `PlayerSystem::LoadAssets()` tries to load
`assets/sprites/player.png`; when it is missing (`Texture2D::id == 0`) the
player is drawn as the procedural low-poly figure instead. Both paths use the
same contact shadow and the same Y-sort key, so nothing else changes.

Expected sheet format:

| Property  | Value                                                            |
|-----------|------------------------------------------------------------------|
| File      | `assets/sprites/player.png`                                      |
| Layout    | 1 row, 4 frames, left to right                                   |
| Frame     | 64 x 64 px                                                       |
| Frames    | 0 = idle, then 3 walk frames (played at 7 fps while moving)      |
| Facing    | All frames drawn facing **right**; the left direction is mirrored at draw time (negative source width) |
| Pivot     | Frame centre = feet centre; the sprite is anchored on the tile centre |

Art direction brief (Task 6.1):

* Q-version miniature proportions - short body, oversized helmet/head;
* tactical gear: helmet, vest, shoulder pads, backpack outline;
* smooth, flat-shaded surfaces with a single top-left key light; no noise,
  no grime textures;
* dark base palette with one saturated accent so the character reads against
  the dark industrial floors (the placeholder colour is a bright blue torso
  with an orange visor stripe).

Until a bitmap is provided, this file documents the contract and the procedural
fallback in `src/PlayerSystem.cpp` renders the equivalent silhouette.
