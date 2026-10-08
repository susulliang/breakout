#include "CombatSystem.hpp"

#include "IsometricMath.hpp"
#include "ParticleSystem.hpp"
#include "World.hpp"

#include "raylib.h"

#include <cmath>
#include <cstddef>

namespace
{
/// Minimum cursor distance (grid units) before a shot is accepted.
constexpr float kMinAimLength = 0.05f;

/// Bullets are integrated in sub-steps of at most this many cells so that a fast
/// projectile cannot tunnel through a one tile thick wall.
constexpr float kMaxSubStep = 0.2f;
constexpr int   kMaxSubSteps = 64;

/// Rendering: bullets fly at chest height.
constexpr float kBulletLift = 7.0f;
constexpr float kGlowRadius = 9.0f;

const Color kBulletCore  = Color{ 255, 214, 64, 255 };
const Color kBulletRim   = Color{ 255, 246, 200, 220 };

/// Phase 5: sparks thrown out of the wall a bullet breaks on.
constexpr int kWallSparkCount = 6;
const Color   kWallSparkColor = Color{ 255, 214, 64, 255 };

/// First inactive slot of the pool, or nullptr when the budget is exhausted.
Bullet* FindFreeSlot(Game& game)
{
    for (std::size_t i = 0; i < game.bullets.size(); ++i)
    {
        if (!game.bullets[i].active)
        {
            return &game.bullets[i];
        }
    }
    return nullptr;
}

/// Tests the bullet tip and centre against the wall layer.
bool HitsWall(const Game& game, const Bullet& bullet)
{
    const Vector2 direction = VectorNormalized(bullet.vel);
    const Vector2 tip{ bullet.pos.x + direction.x * bullet.radius,
                       bullet.pos.y + direction.y * bullet.radius };
    return IsSolidAtPoint(game, tip) || IsSolidAtPoint(game, bullet.pos);
}
}   // namespace

void CombatSystem::ResetPool(Game& game)
{
    for (Bullet& bullet : game.bullets)
    {
        bullet.active = false;
        bullet.vel    = Vector2{ 0.0f, 0.0f };
    }
}

int CombatSystem::ActiveCount(const Game& game)
{
    int count = 0;
    for (const Bullet& bullet : game.bullets)
    {
        if (bullet.active)
        {
            ++count;
        }
    }
    return count;
}

void CombatSystem::UpdateCombat(Game& game, float deltaTime)
{
    // --- 1. spawning: left click -> unproject cursor -> aim direction ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), game.camera);
        const Vector2 aimPoint   = ScreenToCartesian(mouseWorld);
        const Vector2 toAim{ aimPoint.x - game.player.pos.x, aimPoint.y - game.player.pos.y };

        if (VectorLength(toAim) > kMinAimLength)
        {
            const Vector2 direction = VectorNormalized(toAim);

            if (Bullet* slot = FindFreeSlot(game))
            {
                slot->active = true;
                slot->pos    = game.player.pos;
                slot->vel    = Vector2{ direction.x * slot->speed, direction.y * slot->speed };
            }
            else
            {
                TraceLog(LOG_WARNING, "CombatSystem: bullet pool (%i) exhausted", static_cast<int>(game.bullets.size()));
            }
        }
    }

    // --- 2. integration + collision -------------------------------------
    for (Bullet& bullet : game.bullets)
    {
        if (!bullet.active)
        {
            continue;
        }

        const float travel   = VectorLength(bullet.vel) * deltaTime;
        const int   steps    = ClampInt(static_cast<int>(std::ceil(travel / kMaxSubStep)), 1, kMaxSubSteps);
        const float stepTime = deltaTime / static_cast<float>(steps);

        for (int step = 0; step < steps && bullet.active; ++step)
        {
            bullet.pos.x += bullet.vel.x * stepTime;
            bullet.pos.y += bullet.vel.y * stepTime;

            const bool insideMap = IsInsideMap(game, bullet.pos);
            const bool hitWall   = insideMap && HitsWall(game, bullet);

            if (!insideMap || hitWall)
            {
                if (hitWall)
                {
                    // Phase 5: a wall impact throws a short burst of sparks. The
                    // burst is spawned in cartesian space (tile units) like every
                    // other entity, and the pool protects itself against overflow.
                    ParticleSystem::SpawnExplosion(game, bullet.pos, kWallSparkCount, kWallSparkColor);
                }

                bullet.active = false;   // wall hit or left the map -> back to the pool
            }
        }
    }
}

void CombatSystem::RenderBullets(const Game& game)
{
    // Same rationale as MapRenderer::RenderMap(): the pool is read only, only the
    // shared Y-sort queue (a per-frame scratch buffer) is written through.
    Renderer& renderer = const_cast<Game&>(game).renderer;

    for (const Bullet& bullet : game.bullets)
    {
        if (!bullet.active)
        {
            continue;
        }

        const Vector2 screen = CartesianToScreen(bullet.pos);
        const float   depth  = IsometricMath::CartesianToIsometric(bullet.pos).y + 0.03f;

        renderer.Submit(depth, [screen]()
        {
            const Vector2 core{ screen.x, screen.y - kBulletLift };

            DrawCircleV(core, kGlowRadius, Fade(kBulletCore, 0.16f));

            const IsoDiamond diamond = MakeDiamond(core, 5.0f, 2.5f);
            DrawDiamond(diamond, kBulletCore);
            DrawDiamondOutline(diamond, 1.0f, kBulletRim);
            DrawCircleV(core, 1.8f, WHITE);
        });
    }
}
