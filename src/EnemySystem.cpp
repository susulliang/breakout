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
#include "GridAlgorithms.hpp"
#include "ParticleSystem.hpp"
#include "World.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
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

constexpr int   kEnemyFrameCount = 4;
constexpr float kEnemyFrameSize  = 64.0f;
constexpr float kEnemySpriteScale = 0.75f;
constexpr float kDeathPieceLifetime = 4.8f;
constexpr float kDeathPieceFadeStart = 3.0f;
constexpr float kDeathGravity = 46.0f;

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
//  Random helpers for population
// -----------------------------------------------------------------------------
int RandomInt(std::mt19937& rng, int lo, int hi)
{
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

DeathPiece* FindFreeDeathPiece(Game& game)
{
    for (DeathPiece& piece : game.deathPieces)
        if (!piece.active) return &piece;
    return nullptr;
}

void SpawnDeathPieces(Game& game, const Enemy& enemy, int facing)
{
    static std::mt19937 rng{ 0xD1E5EEDu };
    std::uniform_real_distribution<float> scatter(-1.7f, 1.7f);
    std::uniform_real_distribution<float> lift(9.0f, 19.0f);
    std::uniform_real_distribution<float> spin(-260.0f, 260.0f);
    const int parts[] = { 0, 1, 2, 2, 3, 3 };

    for (int part : parts)
    {
        DeathPiece* piece = FindFreeDeathPiece(game);
        if (!piece) break;
        piece->active = true;
        piece->settled = false;
        piece->spritePart = part;
        piece->facing = facing;
        piece->pos = enemy.pos;
        piece->vel = Vector2{ scatter(rng), scatter(rng) };
        piece->screenLift = lift(rng);
        piece->verticalVelocity = lift(rng) * 1.2f;
        piece->rotation = spin(rng) * 0.15f;
        piece->rotationVelocity = spin(rng);
        piece->scale = part == 1 ? 0.50f : (part == 0 ? 0.45f : 0.38f);
        piece->age = 0.0f;
    }
}

float DistanceToSegment(Vector2 point, Vector2 start, Vector2 end)
{
    const Vector2 segment{ end.x - start.x, end.y - start.y };
    const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
    if (lengthSquared <= 1e-8f)
    {
        const Vector2 delta{ point.x - start.x, point.y - start.y };
        return std::sqrt(delta.x * delta.x + delta.y * delta.y);
    }
    const float projection = std::max(0.0f, std::min(1.0f,
        ((point.x - start.x) * segment.x + (point.y - start.y) * segment.y) /
        lengthSquared));
    const Vector2 closest{ start.x + segment.x * projection,
                           start.y + segment.y * projection };
    const Vector2 delta{ point.x - closest.x, point.y - closest.y };
    return std::sqrt(delta.x * delta.x + delta.y * delta.y);
}

bool SegmentHitsEllipse(Vector2 start, Vector2 end, Vector2 center,
                        float radiusX, float radiusY)
{
    const Vector2 normalizedStart{ (start.x - center.x) / radiusX,
                                  (start.y - center.y) / radiusY };
    const Vector2 normalizedEnd{ (end.x - center.x) / radiusX,
                                (end.y - center.y) / radiusY };
    return DistanceToSegment(Vector2{ 0.0f, 0.0f }, normalizedStart, normalizedEnd) <= 1.0f;
}

}   // namespace

void EnemySystem::LoadAssets(Game& game)
{
    const std::string path = ResolveAssetPath("sprites/enemy.png");
    if (!path.empty())
    {
        game.enemyTexture = LoadTexture(path.c_str());
        TraceLog(LOG_INFO, "EnemySystem: sprite sheet loaded from %s", path.c_str());
    }
    if (game.enemyTexture.id == 0)
    {
        TraceLog(LOG_WARNING, "EnemySystem: assets/sprites/enemy.png unavailable, "
                              "falling back to procedural enemy drawing");
    }

    const std::string partsPath = ResolveAssetPath("sprites/enemy_parts.png");
    if (!partsPath.empty())
    {
        game.enemyPartsTexture = LoadTexture(partsPath.c_str());
        TraceLog(LOG_INFO, "EnemySystem: fragment atlas loaded from %s", partsPath.c_str());
    }
}

void EnemySystem::RenderDeathPieces(const Game& game)
{
    Renderer& renderer = const_cast<Game&>(game).renderer;
    const bool hasAtlas = game.enemyPartsTexture.id != 0;
    const Texture2D atlas = game.enemyPartsTexture;
    for (const DeathPiece& piece : game.deathPieces)
    {
        if (!piece.active) continue;
        const Vector2 screen = CartesianToScreen(piece.pos);
        const float depth = IsometricMath::CartesianToIsometric(piece.pos).y + 0.035f;
        const float lift = piece.screenLift;
        const float alpha = piece.age <= kDeathPieceFadeStart ? 1.0f :
            ClampFloat((kDeathPieceLifetime - piece.age) /
                       (kDeathPieceLifetime - kDeathPieceFadeStart), 0.0f, 1.0f);
        const int part = piece.spritePart;
        const float scale = piece.scale;
        const float rotation = piece.rotation;
        const int facing = piece.facing;
        renderer.Submit(depth, [screen, hasAtlas, atlas, part, scale, rotation,
                                lift, alpha, facing]()
        {
            const Vector2 center{ screen.x, screen.y - lift };
            if (hasAtlas)
            {
                const Rectangle source{ static_cast<float>(part) * 64.0f, 0.0f,
                                        (facing < 0 ? -64.0f : 64.0f), 64.0f };
                const float side = 64.0f * scale;
                const Rectangle dest{ center.x, center.y - side * 0.5f, side, side };
                DrawTexturePro(atlas, source, dest, Vector2{ side * 0.5f, side * 0.5f },
                               rotation, Fade(WHITE, alpha));
            }
            else
            {
                const Color red = Fade(Color{ 190, 55, 63, 255 }, alpha);
                if (part == 0)
                    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y),
                                7.0f * scale, 5.0f * scale, red);
                else if (part == 1)
                    DrawRectanglePro(Rectangle{ center.x, center.y, 9.0f * scale,
                                                15.0f * scale },
                                     Vector2{ 4.5f * scale, 7.5f * scale }, rotation, red);
                else
                    DrawLineEx(Vector2{ center.x - 5.0f * scale, center.y },
                               Vector2{ center.x + 5.0f * scale, center.y },
                               3.0f * scale, red);
            }
        });
    }
}

void EnemySystem::ResetDeathPieces(Game& game)
{
    for (DeathPiece& piece : game.deathPieces) piece.active = false;
}

void EnemySystem::UnloadAssets(Game& game)
{
    if (game.enemyTexture.id != 0)
    {
        UnloadTexture(game.enemyTexture);
        game.enemyTexture = Texture2D{};
    }
    if (game.enemyPartsTexture.id != 0)
    {
        UnloadTexture(game.enemyPartsTexture);
        game.enemyPartsTexture = Texture2D{};
    }
}

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

            const Vector2 candidate{ static_cast<float>(cellX) + 0.5f,
                                     static_cast<float>(cellY) + 0.5f };

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
                e.hitRadius = 0.68f;
                e.hitSlowTime = 0.0f;
                e.knockbackVelocity = Vector2{ 0.0f, 0.0f };
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
    game.hitFeedbackTime = std::max(0.0f, game.hitFeedbackTime - deltaTime);
    for (DeathPiece& piece : game.deathPieces)
    {
        if (!piece.active) continue;
        piece.age += deltaTime;
        piece.rotation += piece.rotationVelocity * deltaTime;
        piece.pos.x += piece.vel.x * deltaTime;
        piece.pos.y += piece.vel.y * deltaTime;
        piece.vel.x *= std::max(0.0f, 1.0f - deltaTime * 2.5f);
        piece.vel.y *= std::max(0.0f, 1.0f - deltaTime * 2.5f);

        if (!piece.settled)
        {
            piece.screenLift += piece.verticalVelocity * deltaTime;
            piece.verticalVelocity -= kDeathGravity * deltaTime;
            if (piece.screenLift <= 0.0f)
            {
                piece.screenLift = 0.0f;
                piece.verticalVelocity = 0.0f;
                piece.settled = true;
                piece.rotationVelocity *= 0.18f;
            }
        }
        if (piece.age >= kDeathPieceLifetime) piece.active = false;
    }

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

        e.hitSlowTime = std::max(0.0f, e.hitSlowTime - deltaTime);
        const float knockbackStepX = e.knockbackVelocity.x * deltaTime;
        const float knockbackStepY = e.knockbackVelocity.y * deltaTime;
        MoveGroundWithWallSlide(game, e.pos,
                                Vector2{ knockbackStepX, knockbackStepY }, e.radius);
        const float knockbackDamping = std::max(0.0f, 1.0f - deltaTime * 8.0f);
        e.knockbackVelocity.x *= knockbackDamping;
        e.knockbackVelocity.y *= knockbackDamping;

        // Bresenham line-of-sight check.
        const int startCellX = static_cast<int>(std::floor(e.pos.x));
        const int startCellY = static_cast<int>(std::floor(e.pos.y));
        const int endCellX   = static_cast<int>(std::floor(game.player.pos.x));
        const int endCellY   = static_cast<int>(std::floor(game.player.pos.y));

        const bool hasLOS = GridAlgorithms::HasFloorLineOfSight(
            game.gridMap, game.mapWidth, game.mapHeight,
            startCellX, startCellY, endCellX, endCellY);

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
            const float staggerScale = e.hitSlowTime > 0.0f ? 0.32f : 1.0f;
            const float speed = e.speed * staggerScale * deltaTime;

            MoveGroundWithWallSlide(game, e.pos,
                                    Vector2{ norm.x * speed, norm.y * speed }, e.radius);
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

                // Separation is movement too; resolve each axis against walls.
                Enemy& first = game.enemies[i];
                Enemy& second = game.enemies[j];
                MoveGroundWithWallSlide(game, first.pos, Vector2{ -px, -py }, first.radius);
                MoveGroundWithWallSlide(game, second.pos, Vector2{ px, py }, second.radius);
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

            MoveGroundWithWallSlide(game, e.pos, Vector2{ px, py }, e.radius);

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

            const float dist = DistanceToSegment(e.pos, b.previousPos, b.pos);
            const Vector2 enemyScreen = CartesianToScreen(e.pos);
            const Vector2 bulletStartScreen = CartesianToScreen(b.previousPos);
            const Vector2 bulletEndScreen = CartesianToScreen(b.pos);
            const bool silhouetteHit = SegmentHitsEllipse(
                bulletStartScreen, bulletEndScreen,
                Vector2{ enemyScreen.x, enemyScreen.y - 23.0f }, 19.0f, 26.0f);

            if (dist < b.radius + e.hitRadius || silhouetteHit)
            {
                const Vector2 impact = b.pos;   // capture before the bullet dies
                b.active = false;

                const bool headshot = SegmentHitsEllipse(
                    bulletStartScreen, bulletEndScreen,
                    Vector2{ enemyScreen.x, enemyScreen.y - 37.0f }, 10.0f, 9.0f);
                const float damage = b.damage * (headshot ? 4.0f : 1.0f);
                e.hp -= damage;
                e.hitSlowTime = 1.0f;
                e.knockbackVelocity.x += (b.vel.x / std::max(1e-4f, b.speed)) * 3.2f;
                e.knockbackVelocity.y += (b.vel.y / std::max(1e-4f, b.speed)) * 3.2f;
                const float knockbackSpeed = VectorLength(e.knockbackVelocity);
                if (knockbackSpeed > 4.5f)
                {
                    const float scale = 4.5f / knockbackSpeed;
                    e.knockbackVelocity.x *= scale;
                    e.knockbackVelocity.y *= scale;
                }
                game.hitFeedbackTime = headshot ? 0.34f : 0.19f;
                game.hitFeedbackHeadshot = headshot;
                if (e.hp <= 0.0f)
                {
                    e.active = false;
                    --game.enemyCount;

                    // Phase 5: a kill throws a bigger burst than a plain hit.
                    ParticleSystem::SpawnExplosion(game, e.pos, kBurstDeath, kBloodColor);
                    const float facingBias = (game.player.pos.x - e.pos.x) -
                                             (game.player.pos.y - e.pos.y);
                    SpawnDeathPieces(game, e, facingBias < 0.0f ? -1 : 1);
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
        const bool hasTexture = game.enemyTexture.id != 0;
        const Texture2D texture = game.enemyTexture;
        const float phase = game.stateTime * 7.0f + e.pos.x * 2.3f + e.pos.y * 4.1f;
        const int frame = e.state == EnemyState::CHASE
            ? 1 + static_cast<int>(phase) % (kEnemyFrameCount - 1)
            : 0;
        const float facingBias = (game.player.pos.x - e.pos.x) - (game.player.pos.y - e.pos.y);
        const int facing = (facingBias < 0.0f) ? -1 : 1;
        const bool stunned = e.hitSlowTime > 0.0f;
        const Color tint = stunned
            ? Color{ 255, 220, 220, 255 } : WHITE;

        renderer.Submit(depth, [screen, hasTexture, texture, frame, facing, tint, stunned]()
        {
            // Contact shadow first (same convention as the props and the player).
            DrawContactShadow(screen, 20.0f, 10.0f, 0.35f);

            if (hasTexture)
            {
                const float sourceWidth = (facing < 0) ? -kEnemyFrameSize : kEnemyFrameSize;
                const Rectangle source{ static_cast<float>(frame) * kEnemyFrameSize, 0.0f,
                                        sourceWidth, kEnemyFrameSize };
                const float side = kEnemyFrameSize * kEnemySpriteScale;
                const Rectangle dest{ screen.x, screen.y, side, side };
                const Vector2 origin{ side * 0.5f, side };
                DrawTexturePro(texture, source, dest, origin, 0.0f, tint);
            }
            else
            {
                const Color red     = stunned
                    ? Color{ 255, 112, 112, 255 } : Color{ 220, 60, 60, 255 };
                const Color darkRed = Color{ 160, 40, 40, 255 };
                const float halfW = kTileWidth * 0.35f;
                const float halfH = kTileHeight * 0.35f;

                DrawRectangleRounded(
                    Rectangle{ screen.x - halfW, screen.y - halfH - 20.0f,
                               halfW * 2.0f, halfH * 2.0f },
                    0.2f, 3, red
                );
                DrawRectangleRounded(
                    Rectangle{ screen.x - halfW + 3.0f, screen.y - halfH - 17.0f,
                               halfW * 2.0f - 6.0f, halfH * 2.0f - 6.0f },
                    0.2f, 2, darkRed
                );
            }
        });
    }
}
