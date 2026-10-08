#pragma once

#include "Game.hpp"

/**
 * @file CombatSystem.hpp
 * @brief Mouse aiming, bullet pool and bullet/wall collision (Phase 3.C).
 */
class CombatSystem
{
public:
    CombatSystem() = delete;
    ~CombatSystem() = delete;
    CombatSystem(const CombatSystem&) = delete;
    CombatSystem& operator=(const CombatSystem&) = delete;

    /**
     * @brief Spawns bullets on left click (unprojecting the cursor) and ticks the pool.
     *
     * Bullets are stepped in sub-steps so a fast projectile can never tunnel
     * through a one tile thick wall, and deactivate on wall hit or map exit.
     */
    static void UpdateCombat(Game& game, float deltaTime);

    /// Submits every active bullet into the Y-sorted queue.
    static void RenderBullets(const Game& game);

    /// Deactivates the whole pool (called when a level starts).
    static void ResetPool(Game& game);

    /// Number of active bullets; used by the HUD.
    static int ActiveCount(const Game& game);
};
