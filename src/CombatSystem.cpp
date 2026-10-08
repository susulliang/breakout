#include "CombatSystem.hpp"

#include "IsometricMath.hpp"
#include "ParticleSystem.hpp"
#include "World.hpp"
#include "GameplayRules.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

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
constexpr float kPickupRadius = 0.72f;

const Color kWeaponColors[4] = {
    Color{ 255, 214, 64, 255 }, Color{ 255, 142, 76, 255 },
    Color{ 112, 210, 255, 255 }, Color{ 180, 255, 104, 255 }
};

struct WeaponStats
{
    float speed;
    float damage;
    float cooldown;
    float radius;
    int   ammoCost;
};

WeaponStats StatsFor(WeaponSlot slot)
{
    switch (slot)
    {
        case WeaponSlot::Shotgun: return { 11.0f, 0.72f, 0.42f, 0.105f, 1 };
        case WeaponSlot::Marksman: return { 21.0f, 3.5f, 0.62f, 0.13f, 1 };
        case WeaponSlot::Gatling: return { 15.0f, 0.48f, 0.105f, 0.08f, 1 };
        default: return { 13.5f, 1.0f, 0.28f, 0.09f, 0 };
    }
}

int PickupAmmo(WeaponSlot slot)
{
    switch (slot)
    {
        case WeaponSlot::Shotgun: return 12;
        case WeaponSlot::Marksman: return 8;
        case WeaponSlot::Gatling: return 48;
        default: return 0;
    }
}

const char* WeaponName(WeaponSlot slot)
{
    switch (slot)
    {
        case WeaponSlot::Shotgun: return "SHOTGUN";
        case WeaponSlot::Marksman: return "MARKSMAN";
        case WeaponSlot::Gatling: return "GATLING";
        default: return "SLINGSHOT";
    }
}

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

bool OwnsWeapon(const Game& game, WeaponSlot slot)
{
    return slot == WeaponSlot::Slingshot ||
           game.weapons.ammo[static_cast<std::size_t>(slot)] > 0;
}

void SpawnBullet(Game& game, Vector2 direction, WeaponSlot weapon, Vector2 aimScreen)
{
    Bullet* slot = FindFreeSlot(game);
    if (!slot) return;

    const WeaponStats stats = StatsFor(weapon);
    slot->active = true;
    slot->pos = game.player.pos;
    slot->previousPos = slot->pos;
    slot->vel = Vector2{ direction.x * stats.speed, direction.y * stats.speed };
    slot->speed = stats.speed;
    slot->radius = stats.radius;
    slot->damage = stats.damage;
    slot->weapon = weapon;
    slot->aimScreen = aimScreen;
}

void CollectPickups(Game& game)
{
    for (WeaponPickup& pickup : game.weaponPickups)
    {
        if (!pickup.active) continue;
        const Vector2 delta{ pickup.pos.x - game.player.pos.x,
                             pickup.pos.y - game.player.pos.y };
        if (VectorLengthSqr(delta) > kPickupRadius * kPickupRadius) continue;

        const std::size_t slot = static_cast<std::size_t>(pickup.slot);
        game.weapons.ammo[slot] += PickupAmmo(pickup.slot);
        game.weapons.selected = pickup.slot;
        game.hitFeedbackTime = 0.35f;
        game.hitFeedbackHeadshot = false;
        pickup.active = false;
        TraceLog(LOG_INFO, "Picked up %s", WeaponName(pickup.slot));
    }
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

void CombatSystem::LoadAssets(Game& game)
{
    const std::string path = ResolveAssetPath("sprites/weapons.png");
    if (!path.empty())
    {
        game.weaponTexture = LoadTexture(path.c_str());
        TraceLog(LOG_INFO, "CombatSystem: weapon atlas loaded from %s", path.c_str());
    }
    if (game.weaponTexture.id == 0)
    {
        TraceLog(LOG_WARNING, "CombatSystem: weapon atlas unavailable, using procedural icons");
    }
}

void CombatSystem::UnloadAssets(Game& game)
{
    if (game.weaponTexture.id != 0)
    {
        UnloadTexture(game.weaponTexture);
        game.weaponTexture = Texture2D{};
    }
}

void CombatSystem::PopulateWeaponPickups(Game& game)
{
    ResetLevelPickups(game);

    constexpr uint32_t kPickupSalt = 0x51A7C3D9u;
    std::mt19937 rng(kPickupSalt ^ (static_cast<uint32_t>(game.currentLevel) * 0x9E3779B9u));
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_int_distribution<int> roomPick(0, std::max(0, static_cast<int>(game.rooms.size()) - 1));

    const WeaponSlot slots[] = { WeaponSlot::Shotgun, WeaponSlot::Marksman, WeaponSlot::Gatling };
    const float chances[] = { 0.90f, 0.50f, 0.20f };
    std::vector<Vector2> occupied;
    occupied.push_back(game.playerSpawn);
    occupied.push_back(game.levelExit);

    for (std::size_t i = 0; i < 3 && !game.rooms.empty(); ++i)
    {
        if (chance(rng) > chances[i]) continue;
        const RoomRect& room = game.rooms[static_cast<std::size_t>(roomPick(rng))];
        for (int attempt = 0; attempt < 40; ++attempt)
        {
            std::uniform_int_distribution<int> xPick(room.x, room.x + room.width - 1);
            std::uniform_int_distribution<int> yPick(room.y, room.y + room.height - 1);
            const int cellX = xPick(rng);
            const int cellY = yPick(rng);
            const Vector2 pos{ static_cast<float>(cellX) + 0.5f,
                               static_cast<float>(cellY) + 0.5f };
            if (TileAt(game, cellX, cellY) != kTileFloor)
                continue;

            bool crowded = false;
            for (const Vector2 other : occupied)
            {
                const Vector2 delta{ pos.x - other.x, pos.y - other.y };
                if (VectorLengthSqr(delta) < 9.0f) { crowded = true; break; }
            }
            if (crowded) continue;

            WeaponPickup& pickup = game.weaponPickups[i];
            pickup.active = true;
            pickup.slot = slots[i];
            pickup.pos = pos;
            occupied.push_back(pos);
            break;
        }
    }
}

void CombatSystem::ResetLevelPickups(Game& game)
{
    for (WeaponPickup& pickup : game.weaponPickups) pickup.active = false;
}

void CombatSystem::ResetPool(Game& game)
{
    for (Bullet& bullet : game.bullets)
    {
        bullet.active = false;
        bullet.vel    = Vector2{ 0.0f, 0.0f };
        bullet.previousPos = bullet.pos;
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
    if (game.input.selectedSlot >= 0)
    {
        const WeaponSlot requested = static_cast<WeaponSlot>(game.input.selectedSlot);
        if (OwnsWeapon(game, requested)) game.weapons.selected = requested;
    }

    game.weapons.fireCooldown = std::max(0.0f, game.weapons.fireCooldown - deltaTime);
    CollectPickups(game);
    if (game.input.healPressed)
        GameplayRules::TryUseMedPack(game.player.hp, game.player.maxHp,
                                     game.player.medPacks);

    const WeaponSlot selected = game.weapons.selected;
    const bool firing = selected == WeaponSlot::Gatling
        ? game.input.fireHeld
        : game.input.firePressed;
    if (firing && game.weapons.fireCooldown <= 0.0f && OwnsWeapon(game, selected))
    {
        const Vector2 mouseWorld{ game.aim.groundPoint.x, game.aim.groundPoint.z };
        const Vector2 aimPoint = mouseWorld;
        const Vector2 toAim{ aimPoint.x - game.player.pos.x,
                             aimPoint.y - game.player.pos.y };
        if (VectorLength(toAim) > kMinAimLength)
        {
            const Vector2 baseDirection = VectorNormalized(toAim);
            const WeaponStats stats = StatsFor(selected);
            const int pellets = selected == WeaponSlot::Shotgun ? 5 : 1;
            for (int i = 0; i < pellets; ++i)
            {
                float angle = 0.0f;
                if (pellets > 1)
                    angle = (static_cast<float>(i) - 2.0f) * 0.105f;
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                const Vector2 direction{ baseDirection.x * c - baseDirection.y * s,
                                         baseDirection.x * s + baseDirection.y * c };
                SpawnBullet(game, direction, selected, mouseWorld);
            }
            if (selected != WeaponSlot::Slingshot)
                --game.weapons.ammo[static_cast<std::size_t>(selected)];
            game.weapons.fireCooldown = stats.cooldown;
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
        bullet.previousPos = bullet.pos;

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
                    ParticleSystem::SpawnExplosion(game, bullet.pos, kWallSparkCount, kWallSparkColor,
                                                   kBulletLift);
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
        const Vector2 previous = CartesianToScreen(bullet.previousPos);
        const float   depth  = IsometricMath::CartesianToIsometric(bullet.pos).y + 0.03f;
        const WeaponSlot weapon = bullet.weapon;
        const Color color = kWeaponColors[static_cast<int>(weapon)];

        renderer.Submit(depth, [screen, previous, weapon, color]()
        {
            const Vector2 core{ screen.x, screen.y - kBulletLift };
            if (weapon == WeaponSlot::Slingshot)
            {
                const Vector2 tail{ previous.x, previous.y - kBulletLift };
                DrawLineEx(tail, core, 2.5f, Fade(color, 0.72f));
                DrawCircleV(core, 3.2f, Fade(color, 0.24f));
                DrawCircleV(core, 1.8f, WHITE);
            }
            else
            {
                const float radius = weapon == WeaponSlot::Shotgun ? 2.8f : 3.4f;
                DrawCircleV(core, kGlowRadius, Fade(color, 0.16f));
                DrawCircleV(core, radius, color);
                DrawCircleV(core, 1.4f, WHITE);
            }
        });
    }
}

void CombatSystem::RenderWeaponPickups(const Game& game)
{
    Renderer& renderer = const_cast<Game&>(game).renderer;
    const bool hasAtlas = game.weaponTexture.id != 0;
    const Texture2D atlas = game.weaponTexture;
    for (const WeaponPickup& pickup : game.weaponPickups)
    {
        if (!pickup.active) continue;
        const Vector2 screen = CartesianToScreen(pickup.pos);
        const float depth = IsometricMath::CartesianToIsometric(pickup.pos).y + 0.025f;
        const int icon = static_cast<int>(pickup.slot);
        const Color color = kWeaponColors[icon];
        const float bob = std::sin(game.stateTime * 4.0f + pickup.pos.x) * 3.0f;
        renderer.Submit(depth, [screen, hasAtlas, atlas, icon, color, bob]()
        {
            DrawContactShadow(screen, 13.0f, 6.0f, 0.45f);
            DrawCircleV(Vector2{ screen.x, screen.y - 18.0f - bob }, 13.0f,
                        Fade(color, 0.18f));
            if (hasAtlas)
            {
                const Rectangle source{ static_cast<float>(icon) * 64.0f, 0.0f, 64.0f, 64.0f };
                const Rectangle dest{ screen.x, screen.y - 20.0f - bob, 30.0f, 30.0f };
                DrawTexturePro(atlas, source, dest, Vector2{ 15.0f, 15.0f }, 0.0f, WHITE);
            }
            else
            {
                DrawRectangleRounded(Rectangle{ screen.x - 10.0f, screen.y - 23.0f - bob,
                                                 20.0f, 7.0f }, 0.3f, 3, color);
                DrawRectangle(screen.x - 3.0f, screen.y - 19.0f - bob, 5.0f, 11.0f,
                              Color{ 54, 58, 70, 255 });
            }
        });
    }
}
