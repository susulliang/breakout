#pragma once

#include "Game.hpp"

/**
 * @file PlayerSystem.hpp
 * @brief Player input, AABB wall-sliding collision and player rendering (Phase 3.B).
 *
 * Stateless by design (not instantiable): the system is a namespace of free
 * functions that operate on the flat `Game` aggregate, matching the flat
 * memory model of the project (no ECS, no components).
 */
class PlayerSystem
{
public:
    PlayerSystem() = delete;
    ~PlayerSystem() = delete;
    PlayerSystem(const PlayerSystem&) = delete;
    PlayerSystem& operator=(const PlayerSystem&) = delete;

    /// Loads assets/sprites/player.png when present (id stays 0 -> procedural fallback).
    static void LoadAssets(Game& game);

    /// Releases the optional sprite sheet.
    static void UnloadAssets(Game& game);

    /**
     * @brief Fixed-axis WASD movement with axis-separated AABB collision.
     *
     * WASD follows fixed screen-cardinal directions, converted to Cartesian
     * movement for collision. Diagonals are normalized; cursor aim only
     * controls player facing.
     */
    static void UpdatePlayer(Game& game, float deltaTime);

    /// Submits the player into the Y-sorted queue (isometric Y as sort key).
    static void RenderPlayer(const Game& game);

    /// Current walk cycle frame index (0 == idle), for debug overlays.
    static int CurrentFrame(const Game& game);
};
