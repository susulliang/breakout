#include "Props.hpp"

#include "IsometricMath.hpp"
#include "World.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

/**
 * @file Props.cpp
 * @brief Placement + rendering of the non collidable decor layer.
 *
 * Art direction (Task 6.1): the props are the only high saturation elements of
 * the scene - dark industrial environment, bright accents that break the gloom.
 * Every prop is drawn as a flat graphic volume: a dark contact shadow, two
 * shaded side faces and a lighter top face (same lighting convention as the
 * walls), so the whole scene stays visually consistent.
 */

namespace
{
// --- placement tuning -----------------------------------------------------
constexpr std::uint32_t kPlacementSalt = 0x9E3779B9u;
constexpr std::uint32_t kRoomStride    = 0x85EBCA6Bu;
constexpr int   kMinRoomSide     = 4;     ///< Rooms smaller than this get no props.
constexpr int   kMaxPropsPerRoom = 4;
constexpr float kMinPropSpacing  = 2.2f;  ///< Grid units between two props.
constexpr float kSpawnKeepOut    = 2.0f;  ///< Keep the spawn / exit tiles readable.

// --- palette (industrial wasteland) --------------------------------------
const Color kMetalDark  = Color{  46,  52,  64, 255 };
const Color kMetalMid   = Color{  96, 104, 118, 255 };
const Color kMetalLight = Color{ 168, 176, 190, 255 };

const Color kCurtainBlue     = Color{  38,  92, 168, 255 };
const Color kCurtainBlueTop  = Color{  74, 138, 220, 255 };
const Color kCurtainBlueDeep = Color{  22,  58, 116, 255 };

const Color kTableTop    = Color{ 126, 118, 104, 255 };
const Color kTableSideX  = Color{  84,  78,  68, 255 };
const Color kTableSideY  = Color{  62,  58,  50, 255 };

const Color kLockerTop   = Color{ 255, 172,  72, 255 };
const Color kLockerFaceX = Color{ 226, 124,  34, 255 };
const Color kLockerFaceY = Color{ 186,  96,  22, 255 };

const Color kMachineTop   = Color{  70,  80,  92, 255 };
const Color kMachineFaceX = Color{  44,  52,  62, 255 };
const Color kMachineFaceY = Color{  30,  36,  44, 255 };
const Color kNeonGreen    = Color{ 104, 255, 140, 255 };

const Color kScreenBody  = Color{  36,  42,  54, 255 };
const Color kScreenBlue  = Color{  96, 196, 255, 255 };

// --- helpers --------------------------------------------------------------
int RandomInt(std::mt19937& rng, int low, int high)
{
    std::uniform_int_distribution<int> distribution(low, high);
    return distribution(rng);
}

float RandomFloat(std::mt19937& rng, float low, float high)
{
    std::uniform_real_distribution<float> distribution(low, high);
    return distribution(rng);
}

Vector2 Lerp2(Vector2 a, Vector2 b, float t)
{
    return Vector2{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

void DrawQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color)
{
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, d, color);
}

/// Slightly lightened / darkened copy, used for the per instance variation.
Color ShadeColor(Color color, float amount)
{
    const auto shift = [amount](unsigned char channel) -> unsigned char
    {
        const float value = static_cast<float>(channel) + amount * 255.0f;
        return static_cast<unsigned char>(value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value));
    };
    return Color{ shift(color.r), shift(color.g), shift(color.b), color.a };
}

/// Inset quad used to paint a panel on a box face.
void DrawInsetFace(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float inset, Color color)
{
    const Vector2 center{ (p0.x + p1.x + p2.x + p3.x) * 0.25f,
                          (p0.y + p1.y + p2.y + p3.y) * 0.25f };
    DrawQuad(Lerp2(p0, center, inset), Lerp2(p1, center, inset),
             Lerp2(p2, center, inset), Lerp2(p3, center, inset), color);
}

// --- prop painters --------------------------------------------------------
void DrawPartitionScreen(Vector2 base, float scale, float variant)
{
    const float halfWidth = 30.0f * scale;
    const float height    = 46.0f * scale;
    const float postWidth = 3.0f * scale;
    const Color curtain   = ShadeColor(kCurtainBlue, variant * 0.08f);

    // Frame: two posts + top rail.
    DrawRectangleRounded(Rectangle{ base.x - halfWidth, base.y - height, postWidth, height },
                         0.4f, 4, kMetalDark);
    DrawRectangleRounded(Rectangle{ base.x + halfWidth - postWidth, base.y - height, postWidth, height },
                         0.4f, 4, kMetalDark);
    DrawRectangleRec(Rectangle{ base.x - halfWidth, base.y - height, halfWidth * 2.0f, 3.0f * scale }, kMetalMid);

    // Curtain body: darker hem at the bottom, lighter fold at the top.
    const float curtainTop = base.y - height + 6.0f * scale;
    const float curtainHeight = height - 12.0f * scale;
    const float curtainWidth  = halfWidth * 2.0f - postWidth * 2.0f;
    DrawRectangleRec(Rectangle{ base.x - halfWidth + postWidth, curtainTop, curtainWidth, curtainHeight },
                     curtain);
    DrawRectangleRec(Rectangle{ base.x - halfWidth + postWidth, curtainTop, curtainWidth, 3.0f * scale },
                     kCurtainBlueTop);
    DrawRectangleRec(Rectangle{ base.x - halfWidth + postWidth, curtainTop + curtainHeight - 4.0f * scale,
                                curtainWidth, 4.0f * scale },
                     kCurtainBlueDeep);

    // Vertical folds.
    for (float offset = 0.25f; offset < 0.9f; offset += 0.25f)
    {
        const float x = base.x - halfWidth + postWidth + curtainWidth * offset;
        DrawLineEx(Vector2{ x, curtainTop + 2.0f * scale },
                   Vector2{ x, curtainTop + curtainHeight - 5.0f * scale }, 1.0f * scale,
                   kCurtainBlueDeep);
    }
}

void DrawWireFence(Vector2 base, float scale, float variant)
{
    const float halfWidth = 28.0f * scale;
    const float height    = 40.0f * scale;
    const float halfDepth = halfWidth * 0.5f;
    const Vector2 bottomLeft{ base.x - halfWidth, base.y - halfDepth };
    const Vector2 bottomRight{ base.x + halfWidth, base.y + halfDepth };
    const Vector2 topLeft{ bottomLeft.x, bottomLeft.y - height };
    const Vector2 topRight{ bottomRight.x, bottomRight.y - height };
    const Color mesh      = ShadeColor(kMetalLight, variant * 0.06f);

    // The fence footprint follows the screen-space projection of one grid axis.
    DrawQuad(topLeft, topRight, bottomRight, bottomLeft,
             Fade(Color{ 18, 22, 30, 255 }, 0.35f));

    const auto pointOnPanel = [&](float across, float up)
    {
        const Vector2 lower = Lerp2(bottomLeft, bottomRight, across);
        const Vector2 upper = Lerp2(topLeft, topRight, across);
        return Lerp2(lower, upper, up);
    };

    constexpr int kMeshColumns = 7;
    constexpr int kMeshRows = 5;
    for (int column = 1; column < kMeshColumns; ++column)
    {
        const float across = static_cast<float>(column) / static_cast<float>(kMeshColumns);
        DrawLineEx(pointOnPanel(across, 0.0f), pointOnPanel(across, 1.0f), 0.8f * scale, mesh);
    }
    for (int row = 1; row < kMeshRows; ++row)
    {
        const float up = static_cast<float>(row) / static_cast<float>(kMeshRows);
        DrawLineEx(pointOnPanel(0.0f, up), pointOnPanel(1.0f, up), 0.8f * scale, mesh);
    }
    for (int row = 0; row < kMeshRows; ++row)
    {
        for (int column = 0; column < kMeshColumns; ++column)
        {
            const float u0 = static_cast<float>(column) / static_cast<float>(kMeshColumns);
            const float u1 = static_cast<float>(column + 1) / static_cast<float>(kMeshColumns);
            const float v0 = static_cast<float>(row) / static_cast<float>(kMeshRows);
            const float v1 = static_cast<float>(row + 1) / static_cast<float>(kMeshRows);
            DrawLineEx(pointOnPanel(u0, v0), pointOnPanel(u1, v1), 0.7f * scale, mesh);
            DrawLineEx(pointOnPanel(u1, v0), pointOnPanel(u0, v1), 0.7f * scale, mesh);
        }
    }

    // Posts and rails retain the same isometric footprint as the mesh.
    DrawLineEx(bottomLeft, topLeft, 3.0f * scale, kMetalMid);
    DrawLineEx(bottomRight, topRight, 3.0f * scale, kMetalMid);
    DrawLineEx(topLeft, topRight, 2.5f * scale, kMetalLight);
    DrawLineEx(bottomLeft, bottomRight, 2.0f * scale, kMetalMid);
}

void DrawPartsTable(Vector2 base, float scale, float variant)
{
    const float halfWidth  = 26.0f * scale;
    const float halfHeight = 13.0f * scale;
    const float lift       = 22.0f * scale;

    DrawIsoBox(base, halfWidth, halfHeight, lift, kTableTop, kTableSideX, kTableSideY);

    // Clutter on the table top (scattered parts, no gameplay meaning).
    const Vector2 topCenter{ base.x, base.y - lift };
    const Color bolt   = Color{ 196, 204, 214, 255 };
    const Color can    = Color{ 236, 128, 52, 255 };
    const Color sheet  = Color{ 120, 196, 236, 255 };

    DrawEllipse(static_cast<int>(topCenter.x - 9.0f * scale), static_cast<int>(topCenter.y - 2.0f * scale),
                7.0f * scale, 4.0f * scale, bolt);
    DrawEllipse(static_cast<int>(topCenter.x + 7.0f * scale), static_cast<int>(topCenter.y + 3.0f * scale),
                5.0f * scale, 3.0f * scale, can);
    const IsoDiamond paper = MakeDiamond(Vector2{ topCenter.x + 2.0f * scale, topCenter.y - 4.0f * scale },
                                         9.0f * scale, 4.5f * scale);
    DrawDiamond(paper, sheet);

    if (variant > 0.5f)
    {
        // A small monitor standing on the table.
        DrawRectangleRec(Rectangle{ topCenter.x - 3.0f * scale, topCenter.y - 18.0f * scale,
                                    12.0f * scale, 9.0f * scale }, kScreenBody);
        DrawRectangleRec(Rectangle{ topCenter.x - 1.5f * scale, topCenter.y - 16.5f * scale,
                                    9.0f * scale, 6.0f * scale }, Fade(kScreenBlue, 0.9f));
    }
}

void DrawLocker(Vector2 base, float scale, float variant)
{
    const float halfWidth  = 15.0f * scale;
    const float halfHeight = 7.5f * scale;
    const float lift       = 34.0f * scale;
    const Color top    = ShadeColor(kLockerTop, variant * 0.06f);
    const Color faceX  = ShadeColor(kLockerFaceX, variant * 0.06f);
    const Color faceY  = ShadeColor(kLockerFaceY, variant * 0.06f);

    DrawIsoBox(base, halfWidth, halfHeight, lift, top, faceX, faceY);

    // Front door seam + handle + vent slits on the +X face.
    const IsoDiamond bottom = MakeDiamond(base, halfWidth, halfHeight);
    const IsoDiamond upper  = MakeDiamond(Vector2{ base.x, base.y - lift }, halfWidth, halfHeight);

    const Vector2 seamTop = Lerp2(upper.right, upper.bottom, 0.5f);
    const Vector2 seamBottom = Lerp2(bottom.right, bottom.bottom, 0.5f);
    DrawLineEx(seamTop, seamBottom, 1.6f * scale, Color{ 92, 46, 10, 200 });

    DrawCircleV(Lerp2(bottom.right, upper.right, 0.5f), 1.8f * scale, kMetalDark);
    for (int slit = 1; slit <= 3; ++slit)
    {
        const float t = 0.72f + static_cast<float>(slit) * 0.06f;
        DrawLineEx(Lerp2(bottom.right, upper.right, t), Lerp2(bottom.bottom, upper.bottom, t),
                   1.2f * scale, Color{ 92, 46, 10, 170 });
    }
}

void DrawVendingMachine(Vector2 base, float scale, float variant)
{
    const float halfWidth  = 13.0f * scale;
    const float halfHeight = 6.5f * scale;
    const float lift       = 40.0f * scale;

    // Neon halo bleeding onto the floor.
    DrawCircleV(Vector2{ base.x, base.y - lift * 0.4f }, 24.0f * scale, Fade(kNeonGreen, 0.10f));

    DrawIsoBox(base, halfWidth, halfHeight, lift, kMachineTop, kMachineFaceX, kMachineFaceY);

    const IsoDiamond bottom = MakeDiamond(base, halfWidth, halfHeight);
    const IsoDiamond upper  = MakeDiamond(Vector2{ base.x, base.y - lift }, halfWidth, halfHeight);

    // Lit display on the front (+X) face.
    DrawInsetFace(bottom.right, bottom.bottom, upper.bottom, upper.right, 0.18f,
                  ShadeColor(kNeonGreen, variant * 0.05f));
    // Product rows.
    for (int row = 1; row <= 3; ++row)
    {
        const float t = static_cast<float>(row) * 0.22f;
        DrawLineEx(Lerp2(bottom.right, upper.right, t), Lerp2(bottom.bottom, upper.bottom, t),
                   1.0f * scale, Fade(BLACK, 0.35f));
    }

    // Front edge highlight.
    DrawLineEx(upper.right, upper.bottom, 1.4f * scale, Fade(WHITE, 0.75f));
}

void DrawScreenPanel(Vector2 base, float scale, float variant)
{
    const float halfWidth = 14.0f * scale;
    const float height    = 34.0f * scale;

    // Slim stand.
    DrawRectangleRec(Rectangle{ base.x - 2.0f * scale, base.y - height * 0.45f, 4.0f * scale,
                                height * 0.45f }, kMetalDark);
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(base.y), 9.0f * scale, 4.5f * scale,
                Color{ 28, 32, 40, 255 });

    // Panel body + glowing screen.
    DrawRectangleRounded(Rectangle{ base.x - halfWidth, base.y - height, halfWidth * 2.0f,
                                    height * 0.6f }, 0.12f, 4, kScreenBody);
    DrawRectangleRec(Rectangle{ base.x - halfWidth + 2.0f * scale, base.y - height + 2.5f * scale,
                                halfWidth * 2.0f - 4.0f * scale, height * 0.6f - 5.0f * scale },
                     Fade(kScreenBlue, 0.92f));

    // Scan lines.
    for (float y = base.y - height + 5.0f * scale; y < base.y - height * 0.45f; y += 4.0f * scale)
    {
        DrawLineEx(Vector2{ base.x - halfWidth + 2.0f * scale, y },
                   Vector2{ base.x + halfWidth - 2.0f * scale, y }, 0.8f, Fade(BLACK, 0.22f));
    }

    // Bloom.
    DrawCircleV(Vector2{ base.x, base.y - height * 0.7f }, 20.0f * scale, Fade(kScreenBlue, 0.10f));
    if (variant > 0.5f)
    {
        DrawRectangleRec(Rectangle{ base.x - 5.0f * scale, base.y - height * 0.72f, 10.0f * scale,
                                    2.0f * scale }, Fade(WHITE, 0.85f));
    }
}

/// Picks the decor of a room slot: slot 0 is a structural divider, the rest are accents.
PropKind ChooseKind(std::mt19937& rng, int slot)
{
    const int pick = RandomInt(rng, 0, 2);
    if (slot == 0)
    {
        switch (pick)
        {
            case 0:  return PropKind::PartitionScreen;
            case 1:  return PropKind::WireFence;
            default: return PropKind::PartsTable;
        }
    }
    switch (pick)
    {
        case 0:  return PropKind::Locker;
        case 1:  return PropKind::VendingMachine;
        default: return PropKind::ScreenPanel;
    }
}
}   // namespace

void PropSystem::Populate(Game& game)
{
    game.props.clear();
    if (game.rooms.empty() || game.mapWidth <= 0 || game.mapHeight <= 0)
    {
        return;
    }

    std::vector<Vector2> placed;
    const std::uint32_t levelSalt = static_cast<std::uint32_t>(game.currentLevel) * kPlacementSalt;

    for (std::size_t roomIndex = 0; roomIndex < game.rooms.size(); ++roomIndex)
    {
        const RoomRect& room = game.rooms[roomIndex];
        if (room.width < kMinRoomSide || room.height < kMinRoomSide)
        {
            continue;
        }

        std::mt19937 rng(levelSalt ^ (static_cast<std::uint32_t>(roomIndex + 1) * kRoomStride));

        const int wanted = ClampInt((room.width * room.height) / 45, 1, kMaxPropsPerRoom);
        int accepted = 0;

        for (int attempt = 0; attempt < wanted * 10 && accepted < wanted; ++attempt)
        {
            const int cellX = RandomInt(rng, room.x, room.x + room.width - 1);
            const int cellY = RandomInt(rng, room.y, room.y + room.height - 1);

            if (TileAt(game, cellX, cellY) != kTileFloor)   // never bury decor inside a wall
            {
                continue;
            }

            const Vector2 candidate{ static_cast<float>(cellX) + 0.5f,
                                     static_cast<float>(cellY) + 0.5f };

            // Keep the spawn / exit tiles and the room centre readable.
            const Vector2 roomCentre{ room.centerX, room.centerY };
            const Vector2 spawnDelta{ candidate.x - game.playerSpawn.x, candidate.y - game.playerSpawn.y };
            const Vector2 exitDelta{ candidate.x - game.levelExit.x, candidate.y - game.levelExit.y };
            const Vector2 centreDelta{ candidate.x - roomCentre.x, candidate.y - roomCentre.y };
            if (VectorLength(spawnDelta) < kSpawnKeepOut || VectorLength(exitDelta) < kSpawnKeepOut ||
                VectorLength(centreDelta) < 1.0f)
            {
                continue;
            }

            // No two props on top of each other.
            bool tooClose = false;
            for (const Vector2& other : placed)
            {
                const Vector2 delta{ candidate.x - other.x, candidate.y - other.y };
                if (VectorLength(delta) < kMinPropSpacing)
                {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose)
            {
                continue;
            }

            Prop prop;
            prop.kind    = ChooseKind(rng, accepted);
            prop.pos     = candidate;
            prop.scale   = RandomFloat(rng, 0.92f, 1.08f);
            prop.variant = RandomFloat(rng, 0.0f, 1.0f);

            game.props.push_back(prop);
            placed.push_back(candidate);
            ++accepted;
        }
    }
}

void PropSystem::Render(const Game& game)
{
    // The decor data is read only; only the shared Y-sort queue is written (see
    // MapRenderer::RenderMap() for the full rationale).
    Renderer& renderer = const_cast<Game&>(game).renderer;

    for (const Prop& prop : game.props)
    {
        const Vector2 screen = CartesianToScreen(prop.pos);
        const float   depth  = IsometricMath::CartesianToIsometric(prop.pos).y;

        // Captured by value: the queue outlives this loop.
        const PropKind kind    = prop.kind;
        const float    scale   = prop.scale;
        const float    variant = prop.variant;

        renderer.Submit(depth, [screen, kind, scale, variant]()
        {
            // Task 6.3: every prop casts a soft contact shadow on its tile.
            DrawContactShadow(screen, 20.0f * scale, 10.0f * scale, 0.30f);

            switch (kind)
            {
                case PropKind::PartitionScreen: DrawPartitionScreen(screen, scale, variant); break;
                case PropKind::WireFence:       DrawWireFence(screen, scale, variant);       break;
                case PropKind::PartsTable:      DrawPartsTable(screen, scale, variant);      break;
                case PropKind::Locker:          DrawLocker(screen, scale, variant);          break;
                case PropKind::VendingMachine:  DrawVendingMachine(screen, scale, variant);  break;
                case PropKind::ScreenPanel:     DrawScreenPanel(screen, scale, variant);     break;
            }
        });
    }
}
