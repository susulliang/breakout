#include "PlayerSystem.hpp"

#include "CameraSystem.hpp"
#include "IsometricMath.hpp"
#include "World.hpp"

#include "raylib.h"

#include <cmath>
#include <cstddef>

namespace
{
// --- walk cycle -----------------------------------------------------------
constexpr int   kSpriteFrameCount   = 4;
constexpr float kSpriteFramesPerSecond = 7.0f;
constexpr float kSpriteFrameSize    = 64.0f;   ///< Frame size inside the sprite sheet.

// --- rendering ------------------------------------------------------------
constexpr float kSpriteScale = 0.75f;   ///< 64 px frame -> 48 px on screen (~1.2 tiles).
constexpr float kShadowAlpha = 0.42f;
constexpr float kShadowHalfWidth  = 15.0f;
constexpr float kShadowHalfHeight = 7.0f;

/// Procedural fallback figure: stacked isometric cylinder, cold blue (Task 6.1).
void DrawProceduralPlayer(Vector2 screen)
{
    const Color bodyDark  = Color{ 34, 62, 132, 255 };
    const Color bodyMid   = Color{ 58, 110, 204, 255 };
    const Color bodyLight = Color{ 122, 186, 255, 255 };
    const Color visor     = Color{ 22, 28, 44, 255 };
    const Color rim       = Color{ 176, 224, 255, 255 };

    const float halfWidth  = 13.0f;
    const float halfHeight = 6.5f;
    const float bodyHeight = 30.0f;

    // Body block.
    DrawRectangleRounded(Rectangle{ screen.x - halfWidth, screen.y - bodyHeight,
                                    halfWidth * 2.0f, bodyHeight },
                         0.35f, 6, bodyMid);
    // Shaded right side, rim light on the left.
    DrawRectangleRounded(Rectangle{ screen.x + 3.0f, screen.y - bodyHeight + 1.0f,
                                    halfWidth - 3.0f, bodyHeight - 1.0f },
                         0.35f, 6, bodyDark);
    DrawRectangleRec(Rectangle{ screen.x - halfWidth + 2.0f, screen.y - bodyHeight + 3.0f,
                                2.0f, bodyHeight - 6.0f }, rim);

    // Bottom ring + shoulder cap.
    DrawEllipse(static_cast<int>(screen.x), static_cast<int>(screen.y), halfWidth, halfHeight, bodyDark);
    DrawEllipse(static_cast<int>(screen.x), static_cast<int>(screen.y - bodyHeight),
                halfWidth, halfHeight, bodyMid);
    DrawEllipse(static_cast<int>(screen.x - 1.0f), static_cast<int>(screen.y - bodyHeight - 1.0f),
                halfWidth * 0.78f, halfHeight * 0.78f, bodyLight);

    // Helmet visor.
    DrawRectangleRounded(Rectangle{ screen.x - 9.0f, screen.y - bodyHeight - 5.0f, 18.0f, 6.0f },
                         0.6f, 4, visor);
    DrawLineEx(Vector2{ screen.x - 7.0f, screen.y - bodyHeight - 2.0f },
               Vector2{ screen.x + 7.0f, screen.y - bodyHeight - 2.0f }, 1.4f,
               Color{ 96, 220, 255, 255 });
}
}   // namespace

void PlayerSystem::LoadAssets(Game& game)
{
    const std::string path = ResolveAssetPath("sprites/player.png");
    if (!path.empty())
    {
        game.playerTexture = LoadTexture(path.c_str());
        TraceLog(LOG_INFO, "PlayerSystem: sprite sheet loaded from %s", path.c_str());
    }
    if (game.playerTexture.id == 0)
    {
        TraceLog(LOG_WARNING, "PlayerSystem: assets/sprites/player.png unavailable, "
                              "falling back to procedural player drawing");
    }
}

void PlayerSystem::UnloadAssets(Game& game)
{
    if (game.playerTexture.id != 0)
    {
        UnloadTexture(game.playerTexture);
        game.playerTexture = Texture2D{};
    }
}

int PlayerSystem::CurrentFrame(const Game& game)
{
    const int frame = static_cast<int>(game.playerAnimPhase * kSpriteFramesPerSecond);
    return ((frame % kSpriteFrameCount) + kSpriteFrameCount) % kSpriteFrameCount;
}

void PlayerSystem::UpdatePlayer(Game& game, float deltaTime)
{
    game.player.fireAnimationTime = std::min(
        1.0f, game.player.fireAnimationTime + deltaTime);

    const Vector2 aimPoint{ game.aim.groundPoint.x, game.aim.groundPoint.z };
    const Vector2 aimDelta{ aimPoint.x - game.player.pos.x, aimPoint.y - game.player.pos.y };

    Vector2 forward{ 1.0f, 0.0f };
    if (VectorLength(aimDelta) > 1e-4f)
    {
        forward = VectorNormalized(aimDelta);
    }
    game.player.aimDirection = forward;

    Vector2 screenDirection = game.input.screenMove;
    const float facingBias = forward.x - forward.y;
    if (std::fabs(facingBias) > 1e-3f)
    {
        game.playerFacing = (facingBias < 0.0f) ? -1 : 1;
    }

    const bool moving = (screenDirection.x != 0.0f) || (screenDirection.y != 0.0f);
    if (moving)
    {
        screenDirection = VectorNormalized(screenDirection);
        const Vector2 velocity = CameraSystem::ScreenMovement(screenDirection, game.player.speed);
        const Vector2 delta{ velocity.x * deltaTime, velocity.y * deltaTime };
        MoveGroundWithWallSlide(game, game.player.pos, delta, game.player.radius);

        // --- 4. animation bookkeeping ----------------------------------
        game.playerAnimPhase += deltaTime;
    }
    else
    {
        game.playerAnimPhase = 0.0f;   // back to the idle frame
    }
}

void PlayerSystem::RenderPlayer(const Game& game)
{
    // Same rationale as MapRenderer::RenderMap(): the player data is read only,
    // while the render queue is a per-frame scratch buffer that merely happens to
    // live inside Game. Writing to it cannot corrupt the simulation, which is
    // what allows this entry point to keep its `const Game&` contract.
    Renderer& renderer = const_cast<Game&>(game).renderer;

    const Vector2 screen = CartesianToScreen(game.player.pos);
    const float   depth  = IsometricMath::CartesianToIsometric(game.player.pos).y + 0.02f;

    const bool      hasTexture = (game.playerTexture.id != 0);
    const Texture2D texture    = game.playerTexture;
    const int       frame      = CurrentFrame(game);
    const int       facing     = game.playerFacing;
    const bool      hasWeaponAtlas = game.weaponTexture.id != 0;
    const Texture2D weaponAtlas = game.weaponTexture;
    const int       selectedWeapon = static_cast<int>(game.weapons.selected);

    renderer.Submit(depth, [screen, hasTexture, texture, frame, facing,
                            hasWeaponAtlas, weaponAtlas, selectedWeapon]()
    {
        // Task 6.3: alpha black contact shadow, keeps the figure "standing" on the tile.
        DrawContactShadow(screen, kShadowHalfWidth, kShadowHalfHeight, kShadowAlpha);

        if (hasTexture)
        {
            // Frame is bottom-centre anchored on the tile centre. A negative source
            // width mirrors the frame horizontally when walking towards screen-left.
            const Rectangle source{ static_cast<float>(frame) * kSpriteFrameSize, 0.0f,
                                    (facing < 0) ? -kSpriteFrameSize : kSpriteFrameSize,
                                    kSpriteFrameSize };
            const float side   = kSpriteFrameSize * kSpriteScale;
            const Rectangle dest{ screen.x, screen.y, side, side };
            const Vector2 origin{ side * 0.5f, side };
            DrawTexturePro(texture, source, dest, origin, 0.0f, WHITE);
        }
        else
        {
            DrawProceduralPlayer(screen);
        }

        if (hasWeaponAtlas && selectedWeapon > 0)
        {
            const float sourceWidth = (facing < 0) ? -64.0f : 64.0f;
            const Rectangle source{ static_cast<float>(selectedWeapon) * 64.0f,
                                    0.0f, sourceWidth, 64.0f };
            const Vector2 heldAt{ screen.x + static_cast<float>(facing) * 12.0f,
                                  screen.y - 27.0f };
            const Rectangle dest{ heldAt.x, heldAt.y, 25.0f, 25.0f };
            DrawTexturePro(weaponAtlas, source, dest, Vector2{ 12.5f, 12.5f },
                           0.0f, WHITE);
        }
    });
}
