#pragma once

#include "Game.hpp"
#include "IsometricMath.hpp"

#include "raylib.h"

#include <cmath>
#include <cstddef>
#include <string>

/**
 * @file World.hpp
 * @brief Shared, header-only helpers for the flat cartesian world.
 *
 * Everything the systems (Player / Combat / Props / Lighting) and main.cpp need
 * in common lives here so that the projection convention, the collision probes
 * and the isometric primitive drawing cannot drift apart between files:
 *
 *   - cartesian <-> screen projection (tileWidth = 64, tileHeight = 32)
 *   - safe grid accessors (bounds checked - never read gridMap out of range)
 *   - small vector math (raylib's raymath unit is intentionally not used)
 *   - isometric primitives (diamond / cube) with the "shadow first" convention
 *   - runtime asset path resolution (build dir vs. source dir)
 */

// ---------------------------------------------------------------------------
// Projection (single source of truth - mirrored by main.cpp for the camera)
// ---------------------------------------------------------------------------

/// Cartesian grid position -> screen offset in pixels (isometric projection).
inline Vector2 CartesianToScreen(Vector2 cartesian)
{
    const Vector2 iso = IsometricMath::CartesianToIsometric(cartesian);
    return Vector2{ iso.x * (kTileWidth * 0.5f), iso.y * kTileHeight };
}

/// Screen offset in pixels -> cartesian grid position (inverse projection).
inline Vector2 ScreenToCartesian(Vector2 screen)
{
    const Vector2 iso{ screen.x / (kTileWidth * 0.5f), screen.y / kTileHeight };
    return IsometricMath::IsometricToCartesian(iso);
}

// ---------------------------------------------------------------------------
// Grid access - every read is bounds checked
// ---------------------------------------------------------------------------

/// Tile under (x, y); returns kTileVoid for coordinates outside the map.
inline int TileAt(const Game& game, int x, int y)
{
    if (x < 0 || x >= game.mapWidth || y < 0 || y >= game.mapHeight)
    {
        return kTileVoid;
    }
    return game.gridMap[static_cast<std::size_t>(y) * static_cast<std::size_t>(game.mapWidth) +
                        static_cast<std::size_t>(x)];
}

/// True when the cell holds a blocking wall (kTileWall).
inline bool IsSolidTile(const Game& game, int x, int y)
{
    return TileAt(game, x, y) == kTileWall;
}

/// True when the cartesian point falls inside a blocking wall cell.
inline bool IsSolidAtPoint(const Game& game, Vector2 cartesian)
{
    return IsSolidTile(game, static_cast<int>(std::floor(cartesian.x)),
                              static_cast<int>(std::floor(cartesian.y)));
}

/// True when the cartesian position is inside the generated map bounds.
inline bool IsInsideMap(const Game& game, Vector2 cartesian)
{
    return cartesian.x >= 0.0f && cartesian.y >= 0.0f &&
           cartesian.x <= static_cast<float>(game.mapWidth - 1) &&
           cartesian.y <= static_cast<float>(game.mapHeight - 1);
}

// ---------------------------------------------------------------------------
// Minimal vector math (no raymath dependency)
// ---------------------------------------------------------------------------

inline Vector2 VectorAdd(Vector2 a, Vector2 b) { return Vector2{ a.x + b.x, a.y + b.y }; }
inline Vector2 VectorSub(Vector2 a, Vector2 b) { return Vector2{ a.x - b.x, a.y - b.y }; }
inline Vector2 VectorScale(Vector2 v, float scale) { return Vector2{ v.x * scale, v.y * scale }; }

inline float VectorLengthSqr(Vector2 v) { return v.x * v.x + v.y * v.y; }
inline float VectorLength(Vector2 v) { return std::sqrt(VectorLengthSqr(v)); }

/// Normalized copy; returns {0, 0} for a zero-length input (never NaN).
inline Vector2 VectorNormalized(Vector2 v)
{
    const float lengthSqr = VectorLengthSqr(v);
    if (lengthSqr <= 1e-8f)
    {
        return Vector2{ 0.0f, 0.0f };
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSqr);
    return Vector2{ v.x * inverseLength, v.y * inverseLength };
}

inline float ClampFloat(float value, float low, float high)
{
    return (value < low) ? low : ((value > high) ? high : value);
}

inline int ClampInt(int value, int low, int high)
{
    return (value < low) ? low : ((value > high) ? high : value);
}

// ---------------------------------------------------------------------------
// Isometric primitives (art direction: dark grout, flat graphic faces)
// ---------------------------------------------------------------------------

/// The four corners of an isometric tile diamond.
struct IsoDiamond
{
    Vector2 top{};
    Vector2 right{};
    Vector2 bottom{};
    Vector2 left{};
};

/// Diamond centred on a screen position (halfWidth = 32, halfHeight = 16 by default).
inline IsoDiamond MakeDiamond(Vector2 center, float halfWidth = kTileWidth * 0.5f,
                              float halfHeight = kTileHeight * 0.5f)
{
    IsoDiamond diamond;
    diamond.top    = Vector2{ center.x, center.y - halfHeight };
    diamond.right  = Vector2{ center.x + halfWidth, center.y };
    diamond.bottom = Vector2{ center.x, center.y + halfHeight };
    diamond.left   = Vector2{ center.x - halfWidth, center.y };
    return diamond;
}

/// Moves every corner of the diamond towards its centre (uniform grout inset).
inline IsoDiamond ShrinkDiamond(const IsoDiamond& diamond, float insetX, float insetY)
{
    const Vector2 center{ (diamond.left.x + diamond.right.x) * 0.5f,
                          (diamond.top.y + diamond.bottom.y) * 0.5f };

    const auto towards = [&center, insetX, insetY](Vector2 corner) -> Vector2
    {
        float dx = corner.x - center.x;
        float dy = corner.y - center.y;
        dx = (dx > 0.0f) ? (dx - insetX) : (dx + insetX);
        dy = (dy > 0.0f) ? (dy - insetY) : (dy + insetY);
        if ((corner.x - center.x > 0.0f && dx < 0.0f) || (corner.x - center.x < 0.0f && dx > 0.0f)) dx = 0.0f;
        if ((corner.y - center.y > 0.0f && dy < 0.0f) || (corner.y - center.y < 0.0f && dy > 0.0f)) dy = 0.0f;
        return Vector2{ center.x + dx, center.y + dy };
    };

    IsoDiamond result;
    result.top    = towards(diamond.top);
    result.right  = towards(diamond.right);
    result.bottom = towards(diamond.bottom);
    result.left   = towards(diamond.left);
    return result;
}

/// Fills the diamond with two triangles (raylib has no native quad fill).
inline void DrawDiamond(const IsoDiamond& diamond, Color color)
{
    DrawTriangle(diamond.top, diamond.bottom, diamond.right, color);
    DrawTriangle(diamond.top, diamond.left, diamond.bottom, color);
}

/// Outlines the diamond (used for the hover highlight).
inline void DrawDiamondOutline(const IsoDiamond& diamond, float thickness, Color color)
{
    DrawLineEx(diamond.top, diamond.right, thickness, color);
    DrawLineEx(diamond.right, diamond.bottom, thickness, color);
    DrawLineEx(diamond.bottom, diamond.left, thickness, color);
    DrawLineEx(diamond.left, diamond.top, thickness, color);
}

/// Contact shadow: an alpha black ellipse that anchors a prop to its tile.
inline void DrawContactShadow(Vector2 baseCenter, float halfWidth, float halfHeight, float alpha)
{
    DrawEllipse(static_cast<int>(baseCenter.x), static_cast<int>(baseCenter.y),
                halfWidth, halfHeight, Fade(BLACK, alpha));
}

/**
 * @brief Draws an isometric box (top face + two visible side faces).
 * @param baseCenter Screen position of the tile centre the box stands on.
 * @param halfWidth  Half width of the tile footprint in pixels.
 * @param halfHeight Half height of the tile footprint in pixels.
 * @param lift       Vertical extrusion in pixels.
 * @param topColor   Top face colour.
 * @param faceXColor Colour of the +X (right/front) face.
 * @param faceYColor Colour of the +Y (left/front) face.
 */
inline void DrawIsoBox(Vector2 baseCenter, float halfWidth, float halfHeight, float lift,
                       Color topColor, Color faceXColor, Color faceYColor)
{
    const IsoDiamond base = MakeDiamond(baseCenter, halfWidth, halfHeight);
    const IsoDiamond top  = MakeDiamond(Vector2{ baseCenter.x, baseCenter.y - lift }, halfWidth, halfHeight);

    // Right/front face (+X) and left/front face (+Y).
    DrawTriangle(base.right, top.right, base.bottom, faceXColor);
    DrawTriangle(base.right, top.bottom, base.bottom, faceXColor);
    DrawTriangle(base.left, base.bottom, top.bottom, faceYColor);
    DrawTriangle(base.left, top.bottom, top.left, faceYColor);

    DrawDiamond(top, topColor);
}

// ---------------------------------------------------------------------------
// Assets - resolve the same path from the build dir and from the source tree
// ---------------------------------------------------------------------------

/**
 * @brief Resolves a path relative to the project assets/ directory.
 *
 * Lookup order: current working directory, the executable directory and the
 * CMake-provided source directory, so the game runs both straight from the
 * build folder and from an IDE launch without copying anything by hand.
 *
 * @return The first existing path, or an empty string when nothing matched.
 */
inline std::string ResolveAssetPath(const char* relativePath)
{
    std::string candidates[3];
    candidates[0] = std::string("assets/") + relativePath;
    candidates[1] = std::string(GetApplicationDirectory()) + "assets/" + relativePath;
#ifdef BREAKOUT_SOURCE_ASSET_DIR
    candidates[2] = std::string(BREAKOUT_SOURCE_ASSET_DIR) + "/" + relativePath;
#endif

    for (const std::string& candidate : candidates)
    {
        if (candidate.empty())
        {
            continue;
        }
        if (FileExists(candidate.c_str()))
        {
            return candidate;
        }
    }
    return std::string();
}
