#include "ParticleSystem.hpp"

#include "IsometricMath.hpp"
#include "World.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>

namespace
{
/// Particle lifetime in seconds: short enough to read as a single impact, long
/// enough to survive a couple of frames even on a slow tick.
constexpr float kParticleLifetime = 0.3f;

/// Speed band of a spawned particle (tiles per second). The outward burst is
/// randomised inside this band so it does not travel as one rigid ring.
constexpr float kMinParticleSpeed = 1.2f;
constexpr float kMaxParticleSpeed = 3.0f;

/// Burst size used when the caller does not ask for one, and the hard cap: a
/// single call must never eat the whole 1000 slot pool.
constexpr int kDefaultBurst = 12;
constexpr int kMaxBurst     = 64;

constexpr float kTwoPi = 6.28318530717958647692f;

/**
 * @brief Deterministic RNG for the burst directions.
 *
 * Particles are pure eye candy, but they must not depend on the C library's
 * global rand() state (shared with anything else in the process) and they must
 * not turn the same event into a differently shaped burst on every run.
 */
std::mt19937& Rng()
{
    static std::mt19937 rng{ 0x5EED1234u };
    return rng;
}

/**
 * @brief Find an inactive particle slot. Returns nullptr if pool exhausted.
 */
Particle* FindFreeSlot(Game& game)
{
    for (std::size_t i = 0; i < game.particles.size(); ++i)
    {
        if (!game.particles[i].active)
        {
            return &game.particles[i];
        }
    }
    return nullptr;
}
}   // namespace

void ParticleSystem::SpawnExplosion(Game& game, Vector2 cartesianPos, int count, Color color,
                                    float screenLift)
{
    if (count <= 0)
    {
        count = kDefaultBurst;
    }
    count = ClampInt(count, 1, kMaxBurst);

    std::mt19937&                         rng       = Rng();
    std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
    std::uniform_real_distribution<float> speedDist(kMinParticleSpeed, kMaxParticleSpeed);

    for (int i = 0; i < count; ++i)
    {
        Particle* p = FindFreeSlot(game);
        if (!p)
        {
            // Pool exhausted - the burst is truncated, never overflowed.
            break;
        }

        const float angle = angleDist(rng);
        const float speed = speedDist(rng);

        p->active  = true;
        p->pos     = cartesianPos;
        p->vel     = Vector2{ std::cos(angle) * speed, std::sin(angle) * speed };
        p->screenLift = screenLift;
        p->maxLife = kParticleLifetime;
        p->life    = kParticleLifetime;
        p->color   = color;
    }
}

void ParticleSystem::UpdateParticles(Game& game, float deltaTime)
{
    for (Particle& p : game.particles)
    {
        if (!p.active)
        {
            continue;
        }

        // Integrate velocity.
        p.pos.x += p.vel.x * deltaTime;
        p.pos.y += p.vel.y * deltaTime;

        // Decay life.
        p.life -= deltaTime;

        // If life is <= 0, mark inactive.
        if (p.life <= 0.0f)
        {
            p.active = false;
        }
    }
}

void ParticleSystem::RenderParticles(const Game& game)
{
    Renderer& renderer = const_cast<Game&>(game).renderer;

    for (const Particle& p : game.particles)
    {
        if (!p.active)
        {
            continue;
        }

        const Vector2 screen = CartesianToScreen(p.pos);
        const float depth = IsometricMath::CartesianToIsometric(p.pos).y + 0.03f;
        const float screenLift = p.screenLift;

        // Fade based on life ratio.
        const float fadeRatio = std::max(0.0f, p.life / p.maxLife);
        const Color fadedColor = Fade(p.color, fadeRatio);

        // Draw small isometric shape (diamond) with size proportional to particle
        renderer.Submit(depth, [screen, screenLift, fadedColor, fadeRatio]()
        {
            const float size = 4.0f * fadeRatio + 2.0f;
            const Vector2 elevated{ screen.x, screen.y - screenLift };
            const IsoDiamond diamond = MakeDiamond(elevated, size, size * 0.5f);
            DrawDiamond(diamond, fadedColor);
        });
    }
}

void ParticleSystem::ResetParticles(Game& game)
{
    for (Particle& p : game.particles)
    {
        p.active = false;
        p.vel  = Vector2{ 0.0f, 0.0f };
    }
}
