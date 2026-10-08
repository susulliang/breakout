#include "MapRenderer.hpp"

#include "IsometricMath.hpp"
#include "World.hpp"

#include <cstddef>

// -----------------------------------------------------------------------------
//  Art direction (Phase 3.E)
//
//  Clean, flat, graphic surfaces - no noise textures, no grime:
//    * floors   : a deep grout border around every diamond, so the isometric
//                 grid order stays perfectly readable even in a dark room;
//    * walls    : two tone faceted cubes - light grey / white top face over
//                 dark grey side faces (bottom dark, top light);
//    * shadows  : a contact shadow derived from the geometric base edge of the
//                 cube, drawn before the cube itself (Task 6.3).
// -----------------------------------------------------------------------------
namespace {

// --- Floor: dark slate with a deep grid line --------------------------------
const Color kFloorBase  = { 62,  68,  82, 255 };
const Color kFloorAlt   = { 55,  60,  73, 255 };
const Color kFloorGrout = { 20,  23,  30, 255 };   ///< the visible grid line
const Color kFloorInner = { 92, 100, 118, 255 };   ///< thin highlight inside the tile

// --- Wall: light cap over dark faces ----------------------------------------
const Color kWallTop    = { 206, 213, 224, 255 };
const Color kWallFaceX  = { 112, 120, 138, 255 };
const Color kWallFaceY  = {  74,  81,  97, 255 };
const Color kWallEdge   = {  24,  28,  36, 255 };
const Color kWallSkirt  = {  38,  43,  54, 255 };

/// Grout inset: how much smaller the inner (lit) diamond is than the cell.
constexpr float kGroutInsetX = 2.5f;
constexpr float kGroutInsetY = 1.25f;

/// Fake light direction: everything casts towards the lower right.
constexpr float kShadowShiftX = 5.0f;
constexpr float kShadowShiftY = 2.5f;

/// Queues one flat floor tile: grout diamond first, lit diamond on top.
void SubmitFloorTile(Renderer& renderer, float depth, Vector2 center, Color fill)
{
    renderer.Submit(depth, [center, fill]() {
        const IsoDiamond cell = MakeDiamond(center);

        // The full cell in grout colour: what remains visible after the inner
        // diamond is drawn on top becomes the dark grid line.
        DrawDiamond(cell, kFloorGrout);

        const IsoDiamond tile = ShrinkDiamond(cell, kGroutInsetX, kGroutInsetY);
        DrawDiamond(tile, fill);

        // A single inner highlight keeps each tile from reading as flat paint.
        DrawLineEx(tile.top, tile.left, 1.0f, kFloorInner);
    });
}

/// Queues the contact shadow of a wall, derived from the cube's base diamond.
void SubmitWallShadow(Renderer& renderer, float depth, Vector2 center)
{
    renderer.Submit(depth, [center]() {
        const IsoDiamond base = MakeDiamond(center);

        // Anchored ellipse: the footprint the cube appears to press into.
        DrawContactShadow(Vector2{ center.x + kShadowShiftX, center.y + kShadowShiftY },
                          kTileWidth * 0.5f, kTileHeight * 0.5f, 0.34f);

        // Skirt: the two camera facing base edges extruded along the light
        // direction - "the shadow is derived from the wall's own base".
        const Vector2 shift{ kShadowShiftX * 1.6f, kShadowShiftY * 1.6f };

        DrawTriangle(base.right, base.bottom, VectorAdd(base.bottom, shift), kWallSkirt);
        DrawTriangle(base.right, VectorAdd(base.bottom, shift), VectorAdd(base.right, shift), kWallSkirt);
        DrawTriangle(base.bottom, base.left, VectorAdd(base.left, shift), kWallSkirt);
        DrawTriangle(base.bottom, VectorAdd(base.left, shift), VectorAdd(base.bottom, shift), kWallSkirt);
    });
}

/// Queues one raised wall cube: dark base skirt, side faces, light top cap.
void SubmitWallCube(Renderer& renderer, float depth, Vector2 center, float wallHeight)
{
    renderer.Submit(depth, [center, wallHeight]() {
        const IsoDiamond base = MakeDiamond(center);

        // The two visible side faces (+X front-right, +Y front-left) plus the
        // light top cap. DrawIsoBox keeps the winding consistent for raylib.
        DrawIsoBox(center, kTileWidth * 0.5f, kTileHeight * 0.5f, wallHeight,
                   kWallTop, kWallFaceX, kWallFaceY);

        // Crisp outline on the cap and the silhouette edge for the graphic look.
        const IsoDiamond top = MakeDiamond(Vector2{ center.x, center.y - wallHeight });
        DrawDiamondOutline(top, 1.0f, kWallEdge);
        DrawLineEx(base.right, top.right, 1.0f, kWallEdge);
        DrawLineEx(top.right, top.bottom, 1.0f, kWallEdge);
        DrawLineEx(top.bottom, base.bottom, 1.0f, kWallEdge);
    });
}

}   // namespace

// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------
void MapRenderer::RenderMap(const Game& game)
{
    // RenderMap() only queues commands: BeginFrame() / Flush() stay under the
    // control of main.cpp, so the map, the props, the bullets and the player
    // share one Y-sorted queue and therefore one correct back-to-front order.
    //
    // const_cast rationale: the level data is genuinely read-only here, but the
    // render queue is a per-frame scratch buffer stored inside Game. It is not
    // logical game state, so writing to it through a const Game& cannot corrupt
    // the simulation - it is what keeps the public signature `const Game&`.
    Renderer& renderer = const_cast<Game&>(game).renderer;

    const std::size_t expectedSize =
        static_cast<std::size_t>(game.mapWidth) * static_cast<std::size_t>(game.mapHeight);
    if (game.gridMap.size() != expectedSize || game.mapWidth <= 0 || game.mapHeight <= 0) {
        return;   // no level generated yet - nothing to queue
    }

    for (int y = 0; y < game.mapHeight; ++y)
    {
        for (int x = 0; x < game.mapWidth; ++x)
        {
            const int tile = game.gridMap[static_cast<std::size_t>(y) * game.mapWidth + x];
            if (tile == kTileVoid) {
                continue;   // outside the level, never drawn
            }

            const Vector2 cartesian{ static_cast<float>(x), static_cast<float>(y) };
            const Vector2 isometric = IsometricMath::CartesianToIsometric(cartesian);

            const Vector2 center{
                isometric.x * (kTileWidth * 0.5f),
                isometric.y * kTileHeight
            };

            // Sorting key: the isometric Y grows towards the viewer, so this is
            // exactly the back-to-front order the painter's algorithm needs.
            const float depth = isometric.y;

            if (tile == kTileFloor)
            {
                // Checkerboard shading keeps the large, flat rooms readable
                // without introducing any texture noise.
                const bool checker = ((x + y) % 2 == 0);
                SubmitFloorTile(renderer, depth, center, checker ? kFloorBase : kFloorAlt);
            }
            else
            {
                // Shadow key strictly below the cube's own key: the shadow is
                // painted first, the cube then covers its inner half.
                SubmitWallShadow(renderer, depth - 0.001f, center);
                SubmitWallCube(renderer, depth, center, kWallHeight);
            }
        }
    }
}
