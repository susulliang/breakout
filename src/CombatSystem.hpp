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

    /// Load and release the generated weapon icon atlas.
    static void LoadAssets(Game& game);
    static void UnloadAssets(Game& game);

    /// Roll level-specific pickup chances and place weapons on safe floor cells.
    static void PopulateWeaponPickups(Game& game);

    /**
     * @brief Handles weapon selection/firing, pickups, and bullet integration.
     *
     * Bullets are stepped in sub-steps so a fast projectile can never tunnel
     * through a one tile thick wall, and deactivate on wall hit or map exit.
     */
    static void UpdateCombat(Game& game, float deltaTime);

    /// Submits every active bullet into the Y-sorted queue.
    static void RenderBullets(const Game& game);
    static void RenderWeaponPickups(const Game& game);

    /// Deactivates the whole pool (called when a level starts).
    static void ResetPool(Game& game);

    /// Clear transient pickups and death-independent weapon effects for a level.
    static void ResetLevelPickups(Game& game);

    /// Number of active bullets; used by the HUD.
    static int ActiveCount(const Game& game);
};
