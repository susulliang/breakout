# Runtime 3D Models

The current 3D prototype generates and caches its low-poly meshes in
`src/ProceduralScene3D.cpp`; no model files are required in this directory.
Floor and wall textures are generated in memory by `SceneRenderer3D`.

Optional future `.glb` exports may be placed here if a loader is implemented.
Keep the game playable with its procedural geometry fallback. See
`docs/port3d/assets.md` for the coordinate, scale, and runtime contract.
