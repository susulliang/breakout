#pragma once

#include "Game.hpp"
#include "IsometricMath.hpp"
#include "World.hpp"

#include "raylib.h"

/**
 * @file ParticleSystem.hpp
 * @brief Fixed-size particle pool (1000 slots) for explosion effects.
 *
 * Particles are spawned at cartesian positions on bullet-enemy / bullet-wall
 * hits, then driven by per-tile velocity for a short lifetime before fading
 * out. All rendering uses Y-sort depth with `Renderer::Submit` value capture.
 *
 * Flat memory model, no ECS, no heap allocation during play.
 */
class ParticleSystem
{
public:
    ParticleSystem() = delete;
    ~ParticleSystem() = delete;
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    /// Spawn an explosion at `cartesianPos`; screenLift raises wall sparks above the floor.
    static void SpawnExplosion(Game& game, Vector2 cartesianPos, int count, Color color,
                               float screenLift = 0.0f);

    /// Update all active particles: integrate velocity, decay life.
    static void UpdateParticles(Game& game, float deltaTime);

    /// Render active particles as small isometric shapes with fade-out.
    static void RenderParticles(const Game& game);

    /// Reset all particle slots.
    static void ResetParticles(Game& game);
};
