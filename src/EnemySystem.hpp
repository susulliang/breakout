#pragma once

#include "Game.hpp"
#include "IsometricMath.hpp"
#include "World.hpp"

#include "raylib.h"

/**
 * @file EnemySystem.hpp
 * @brief Enemy population, AI (Bresenham line-of-sight + chase state machine),
 *        bullet-enemy combat, and sprite rendering with contact shadow.
 *
 * Enemies are a flat array of 200 slots in Game; systems mutate active slots
 * directly. No ECS, no globals, no heap allocation during play.
 *
 * Difficulty scales linearly: Level 1 = 5 enemies at speed 1.0x / hp 10;
 * Level 3 = 12 / 1.2x / 15; Level 5 = 25 / 1.4x / 25; Level 8 = 45 / 1.7x / 40;
 * Level 10 = 70 / 2.0x / 60. Intermediate levels interpolate.
 */
class EnemySystem
{
public:
    EnemySystem() = delete;
    ~EnemySystem() = delete;
    EnemySystem(const EnemySystem&) = delete;
    EnemySystem& operator=(const EnemySystem&) = delete;

    /// Load the optional enemy sprite sheet, keeping the procedural fallback.
    static void LoadAssets(Game& game);

    /// Release the optional enemy sprite sheet.
    static void UnloadAssets(Game& game);

    /// Clear enemy pool and scatter N enemies based on current level.
    static void PopulateEnemies(Game& game);

    /// Update AI, wall-slide movement, enemy overlap, bullet combat, and debris.
    static void UpdateEnemies(Game& game, float deltaTime);

    /// Render every active enemy as a Y-sorted sprite with contact shadow.
    static void RenderEnemies(const Game& game);

    /// Render detached, settling and fading enemy parts in the shared draw queue.
    static void RenderDeathPieces(const Game& game);

    /// Clear live enemies and detached pieces when a new level starts.
    static void ResetDeathPieces(Game& game);
};
