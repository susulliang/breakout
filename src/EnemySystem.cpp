/**
 * @file EnemySystem.cpp
 * @brief Enemy population, AI, combat, and rendering.
 *
 * Enemy movement reuses the same AABB tile-sliding logic as PlayerSystem.
 * Bresenham line-of-sight is the sole LOS checker: if the straight grid-line
 * between enemy and player crosses any kTileWall, the enemy goes IDLE.
 *
 * Bullet-enemy combat is brute-force (200 enemies x 500 bullets) but bounded
 * at 100k iterations per frame and cheap (two Vector2Dist comparisons + two
 * bool writes).
 *
 * Phase 5: impacts now also emit particle bursts (flesh hit / kill), and the
 * enemies are submitted into the shared Y-sort queue instead of being drawn
 * immediately, so they occlude and are occluded exactly like the map, the
 * props, the bullets, the player and the particles.
 */

#include "EnemySystem.hpp"

#include "IsometricMath.hpp"
#include "ParticleSystem.hpp"
#include "World.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace {

// -----------------------------------------------------------------------------
//  Phase 5: particle bursts spawned by bullet impacts.
//  Bursts are small: the wall sparks (CombatSystem) plus up to 70 enemies make
//  the 1000 slot pool a real budget, and SpawnExplosion() refuses to overflow it.
// -----------------------------------------------------------------------------
constexpr int kBurstHitFlesh = 8;
constexpr int kBurstDeath    = 26;
const Color   kBloodColor    = Color{ 196, 48, 52, 255 };

// -----------------------------------------------------------------------------
//  Phase 5 fix: contact-damage budget.
//  The player used to lose 1 HP on EVERY frame a body touched them, which
//  emptied the 100 HP bar in under two seconds ("health drops to 0 with no
//  apparent contact"). Damage is now rationed by a shared immunity window -
//  one hit of kContactDamage every kContactDamageCooldown seconds, no matter
//  how many enemies are overlapping at the same time.
// -----------------------------------------------------------------------------
constexpr int   kContactDamage         = 8;
constexpr float kContactDamageCooldown = 0.8f;

// -----------------------------------------------------------------------------
//  Difficulty matrix: [level] -> [spawnCount, speedMultiplier, hitPoints]
//  Linear interpolation fills the gaps between these anchor points.
// -----------------------------------------------------------------------------
struct DifficultyAnchor
{
    int  level;
    int  spawnCount;
    float speedMul;
    float hitPoints;
};

constexpr DifficultyAnchor kDifficultyTable[] =
{
    { 1,   5,  1.00f,  10.0f  },
    { 3,  12,  1.20f,  15.0f  },
    { 5,  25,  1.40f,  25.0f  },
    { 8,  45,  1.70f,  40.0f  },
    { 10, 70,  2.00f,  60.0f  },
};

static_assert(sizeof(kDifficultyTable) / sizeof(*kDifficultyTable) >= 5);

/**
 * @brief Linear interpolation through the difficulty table for a given level.
 *
 * Level 1-3: interpolate between level 1 and level 3 anchors.
 * Level 4-5: interpolate between level 3 and level 5 anchors.
 * Level 6-8: interpolate between level 5 and level 8 anchors.
 * Level 9-10: interpolate between level 8 and level 10 anchors.
 * Levels outside [1,10] clamp to the nearest anchor.
 */
void GetDifficultyForLevel(int level, int& spawnCount, float& speedMul, float& hitPoints)
{
    level = std::max(1, std::min(10, level));

    if (level <= 1) { spawnCount = 5;   speedMul = 1.0f; hitPoints = 10.0f; return; }
    if (level >= 10) { spawnCount = 70;  speedMul = 2.0f; hitPoints = 60.0f; return; }

    // Find the lower and upper anchors that bracket this level.
    const DifficultyAnchor* lo = nullptr;
    const DifficultyAnchor* hi = nullptr;
    for (int i = 0; i < static_cast<int>(sizeof(kDifficultyTable) / sizeof(*kDifficultyTable)) - 1; ++i)
    {
        if (kDifficultyTable[i].level <= level && level <= kDifficultyTable[i + 1].level)
        {
            lo = &kDifficultyTable[i];
            hi = &kDifficultyTable[i + 1];
            break;
        }
    }
    if (!lo || !hi)
    {
        // Fallback: clamp to nearest anchor.
        lo = &kDifficultyTable[0];
        hi = lo;
    }

    // Clamp to valid anchor range (in case the table has a gap).
    if (lo->level > level) { lo = hi; }
    if (hi->level < level) { hi = lo; }

    const float t = (static_cast<float>(level) - lo->level) / std::max(1, hi->level - lo->level);

    spawnCount = static_cast<int>(static_cast<float>(lo->spawnCount) * (1.0f - t) + static_cast<float>(hi->spawnCount) * t);
    speedMul   = lo->speedMul * (1.0f - t) + hi->speedMul * t;
    hitPoints  = lo->hitPoints * (1.0f - t) + hi->hitPoints * t;
}

// -----------------------------------------------------------------------------
//  Bresenham line of sight: walk from (x0, y0) to (x1, y1) on the grid.
//  Returns true if the line stays entirely within non-wall cells (kTileFloor)
//  from enemy position (exclusive) to player position (exclusive).
//  If the start position itself is a wall tile, we skip the check.
// -----------------------------------------------------------------------------
bool LineOfSight(const Game& game, int x0, int y0, int x1, int y1)
{
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);

    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;

    int x = x0;
    int y = y0;

    int err = dx - dy;

    // Walk in steps. Each step: check the cell BEFORE entering it (don't check the target itself).
    while (x != x1 || y != y1)
    {
        // The current cell: if we're not at the start and not at the end, must be walkable.
        if (x != x0 && y != y0 && x != x1 && y != y1)
        {
            const int tile = TileAt(game, x, y);
            if (tile != kTileFloor)
            {
                return false;
            }
        }

        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }

    return true;
}

// -----------------------------------------------------------------------------
//  AABB tile-sliding movement along one axis. Same pattern as PlayerSystem.
// -----------------------------------------------------------------------------
static bool BlockedAlongX(const Game& game, float targetX, float centerY, float radius)
{
    const int col = static_cast<int>(std::floor(targetX + radius));
    if (col < 0 || col >= game.mapWidth) { return true; }

    for (float yOff = -radius; yOff <= radius; yOff += 2.0f * radius)
    {
        const int row = static_cast<int>(std::floor(centerY + yOff));
        if (row < 0 || row >= game.mapHeight) { return true; }

        const int tile = TileAt(game, col, row);
        if (tile == kTileWall) return true;
    }
    return false;
}

static bool BlockedAlongY(const Game& game, float targetY, float centerX, float radius)
{
    const int row = static_cast<int>(std::floor(targetY + radius));
    if (row < 0 || row >= game.mapHeight) { return true; }

    for (float xOff = -radius; xOff <= radius; xOff += 2.0f * radius)
    {
        const int col = static_cast<int>(std::floor(centerX + xOff));
        if (col < 0 || col >= game.mapWidth) { return true; }

        const int tile = TileAt(game, col, row);
        if (tile == kTileWall) return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
//  Random helpers for population
// -----------------------------------------------------------------------------
int RandomInt(std::mt19937& rng, int lo, int hi)
{
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

}   // namespace

void EnemySystem::PopulateEnemies(Game& game)
{
    // Clear all slots.
    for (auto& e : game.enemies) { e.active = false; }
    game.enemyCount = 0;

    if (game.rooms.empty()) { return; }

    // Get difficulty for current level.
    int spawnCount = 0;
    float speedMul = 1.0f;
    float hitPts   = 10.0f;
    GetDifficultyForLevel(game.currentLevel, spawnCount, speedMul, hitPts);

    // Seed per level.
    constexpr uint32_t kSalt = 0x9E3779B9u;
    const uint32_t levelSeed = static_cast<uint32_t>(game.currentLevel) * kSalt;
    std::mt19937 rng(levelSeed);

    std::vector<Vector2> placed;
    const float minSpacing = 1.5f;    // grid units between enemies
    const float keepOutSpawn = 3.0f;  // don't spawn within 3 tiles of player spawn

    for (int i = 0; i < spawnCount; ++i)
    {
        // Pick a random room.
        const int roomIdx = RandomInt(rng, 0, static_cast<int>(game.rooms.size()) - 1);
        const RoomRect& room = game.rooms[static_cast<std::size_t>(roomIdx)];

        // Try to place an enemy on a floor tile inside the room.
        bool placedEnemy = false;
        for (int attempt = 0; attempt < 50 && !placedEnemy; ++attempt)
        {
            const int cellX = RandomInt(rng, room.x, room.x + room.width - 1);
            const int cellY = RandomInt(rng, room.y, room.y + room.height - 1);

            const int tile = TileAt(game, cellX, cellY);
            if (tile != kTileFloor) { continue; }

            const Vector2 candidate{ static_cast<float>(cellX), static_cast<float>(cellY) };

            // Keepout: distance from player spawn.
            const Vector2 spawnDelta{ candidate.x - game.playerSpawn.x, candidate.y - game.playerSpawn.y };
            if (std::sqrt(spawnDelta.x * spawnDelta.x + spawnDelta.y * spawnDelta.y) < keepOutSpawn)
            {
                continue;
            }

            // Keepout: distance from existing enemies.
            bool tooClose = false;
            for (const Vector2& p : placed)
            {
                const Vector2 d{ candidate.x - p.x, candidate.y - p.y };
                const float dist = std::sqrt(d.x * d.x + d.y * d.y);
                if (dist < minSpacing)
                {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) { continue; }

            // Find the next free slot.
            for (auto& e : game.enemies)
            {
                if (e.active) continue;
                e.active = true;
                e.pos = candidate;
                e.hp = hitPts;
                e.speed = speedMul;
                e.state = EnemyState::IDLE;
                e.radius = 0.3f;
                placed.push_back(candidate);
                ++game.enemyCount;
                placedEnemy = true;
                break;
            }
        }
    }
}

void EnemySystem::UpdateEnemies(Game& game, float deltaTime)
{
    // Phase 5 fix: tick the contact-damage immunity window down once per frame,
    // before any collision test below can refill it.
    if (game.player.hurtCooldown > 0.0f)
    {
        game.player.hurtCooldown -= deltaTime;
        if (game.player.hurtCooldown < 0.0f)
        {
            game.player.hurtCooldown = 0.0f;
        }
    }

    // --- Phase 1: Update active enemies --------------------------------------
    for (auto& e : game.enemies)
    {
        if (!e.active) continue;

        // Bresenham line-of-sight check.
        const int startCellX = static_cast<int>(std::floor(e.pos.x));
        const int startCellY = static_cast<int>(std::floor(e.pos.y));
        const int endCellX   = static_cast<int>(std::floor(game.player.pos.x));
        const int endCellY   = static_cast<int>(std::floor(game.player.pos.y));

        const bool hasLOS = LineOfSight(game, startCellX, startCellY, endCellX, endCellY);

        if (hasLOS)
        {
            e.state = EnemyState::CHASE;
        }
        else
        {
            e.state = EnemyState::IDLE;
        }

        if (e.state == EnemyState::CHASE)
        {
            // Direction toward player.
            const Vector2 dir{ game.player.pos.x - e.pos.x, game.player.pos.y - e.pos.y };
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len < 1e-4f) continue; // avoid divide-by-zero; enemy is on top of player

            const Vector2 norm{ dir.x / len, dir.y / len };

            // Move along X first, then Y (wall-sliding).
            const float speed = e.speed * deltaTime;

            const float targetX = e.pos.x + norm.x * speed;
            if (!BlockedAlongX(game, targetX, e.pos.y, e.radius))
            {
                e.pos.x = targetX;
            }

            const float targetY = e.pos.y + norm.y * speed;
            if (!BlockedAlongY(game, targetY, e.pos.x, e.radius))
            {
                e.pos.y = targetY;
            }
        }
    }

    // --- Phase 2: Enemy-enemy overlap resolution ------------------------------
    for (int i = 0; i < static_cast<int>(game.enemies.size()); ++i)
    {
        if (!game.enemies[i].active) continue;

        for (int j = i + 1; j < static_cast<int>(game.enemies.size()); ++j)
        {
            if (!game.enemies[j].active) continue;

            const float dx = game.enemies[j].pos.x - game.enemies[i].pos.x;
            const float dy = game.enemies[j].pos.y - game.enemies[i].pos.y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            const float minDist = game.enemies[i].radius + game.enemies[j].radius;
            if (dist < minDist && dist > 1e-6f)
            {
                const float push = (minDist - dist) * 0.5f;
                const float invDist = 1.0f / dist;
                const float px = dx * invDist * push;
                const float py = dy * invDist * push;

                // Push away; clamp to avoid pushing into walls.
                game.enemies[i].pos.x -= px;
                game.enemies[i].pos.y -= py;
                game.enemies[j].pos.x += px;
                game.enemies[j].pos.y += py;

                // Clamp to bounds.
                game.enemies[i].pos.x = std::max(0.0f, std::min(game.enemies[i].pos.x, static_cast<float>(game.mapWidth - 1)));
                game.enemies[i].pos.y = std::max(0.0f, std::min(game.enemies[i].pos.y, static_cast<float>(game.mapHeight - 1)));
                game.enemies[j].pos.x = std::max(0.0f, std::min(game.enemies[j].pos.x, static_cast<float>(game.mapWidth - 1)));
                game.enemies[j].pos.y = std::max(0.0f, std::min(game.enemies[j].pos.y, static_cast<float>(game.mapHeight - 1)));
            }
        }
    }

    // --- Phase 4: Enemy-player collision (damage) ------------------------------
    for (auto& e : game.enemies)
    {
        if (!e.active) continue;

        const float dx = e.pos.x - game.player.pos.x;
        const float dy = e.pos.y - game.player.pos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        const float minDist = e.radius + game.player.radius;
        if (dist < minDist && dist > 1e-6f)
        {
            // Push enemy away to avoid continuous collision.
            const float push = (minDist - dist) * 0.5f;
            const float invDist = 1.0f / dist;
            const float px = dx * invDist * push;
            const float py = dy * invDist * push;

            e.pos.x += px;
            e.pos.y += py;

            // Clamp to map bounds.
            e.pos.x = std::max(0.0f, std::min(e.pos.x, static_cast<float>(game.mapWidth - 1)));
            e.pos.y = std::max(0.0f, std::min(e.pos.y, static_cast<float>(game.mapHeight - 1)));

            // Phase 5 fix: the guard above already proved a real radius overlap
            // (dist < e.radius + player.radius), so this is genuine contact - but
            // it only hurts once the immunity window has elapsed, otherwise a
            // body parked on the player would drain 100 HP in ~1.7 s.
            if (game.player.hurtCooldown <= 0.0f)
            {
                game.player.hp -= kContactDamage;
                if (game.player.hp < 0)
                {
                    game.player.hp = 0;
                }
                game.player.hurtCooldown = kContactDamageCooldown;
            }
        }
    }

    // --- Phase 3: Bullet-enemy combat -----------------------------------------
    for (auto& b : game.bullets)
    {
        if (!b.active) continue;
        for (auto& e : game.enemies)
        {
            if (!e.active) continue;

            const float dx = e.pos.x - b.pos.x;
            const float dy = e.pos.y - b.pos.y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < b.radius + e.radius)
            {
                const Vector2 impact = b.pos;   // capture before the bullet dies
                b.active = false;

                e.hp -= 1.0f;
                if (e.hp <= 0.0f)
                {
                    e.active = false;
                    --game.enemyCount;

                    // Phase 5: a kill throws a bigger burst than a plain hit.
                    ParticleSystem::SpawnExplosion(game, e.pos, kBurstDeath, kBloodColor);
                }
                else
                {
                    ParticleSystem::SpawnExplosion(game, impact, kBurstHitFlesh, kBloodColor);
                }

                break; // bullet hits only one enemy
            }
        }

        // Early-out: if all enemies dead, stop iterating.
        if (game.enemyCount <= 0) break;
    }
}

void EnemySystem::RenderEnemies(const Game& game)
{
    // Same rationale as MapRenderer::RenderMap(): the pool is read only, only the
    // shared Y-sort queue (a per-frame scratch buffer) is written through. Every
    // lambda captures BY VALUE so it can outlive this scope.
    Renderer& renderer = const_cast<Game&>(game).renderer;

    for (const auto& e : game.enemies)
    {
        if (!e.active) continue;

        const Vector2 screen = CartesianToScreen(e.pos);
        const float depth = IsometricMath::CartesianToIsometric(e.pos).y + 0.02f;

        renderer.Submit(depth, [screen]()
        {
            const Color red     = Color{ 220, 60, 60, 255 };
            const Color darkRed = Color{ 160, 40, 40, 255 };

            // Contact shadow first (same convention as the props and the player).
            DrawContactShadow(screen, 20.0f, 10.0f, 0.35f);

            // Simple red box centred on the diamond.
            const float halfW = kTileWidth * 0.35f;
            const float halfH = kTileHeight * 0.35f;
            DrawRectangleRounded(
                Rectangle{ screen.x - halfW, screen.y - halfH - 20.0f, halfW * 2.0f, halfH * 2.0f },
                0.2f, 3, red
            );
            DrawRectangleRounded(
                Rectangle{ screen.x - halfW + 3.0f, screen.y - halfH - 17.0f, halfW * 2.0f - 6.0f, halfH * 2.0f - 6.0f },
                0.2f, 2, darkRed
            );
        });
    }
}