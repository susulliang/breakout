#pragma once

#include "Game.hpp"

/**
 * @file Props.hpp
 * @brief Modular, non collidable decor layer (Phase 3.H, Task 6.4).
 *
 * Props are pure visuals: they never enter the collision pass and never modify
 * the pathfinding grid (`gridMap`). They live in `Game::props` and are drawn
 * through the very same Y-sorted queue as the map, so a prop behind a wall is
 * occluded exactly like any other entity.
 */
class PropSystem
{
public:
    PropSystem() = delete;
    ~PropSystem() = delete;
    PropSystem(const PropSystem&) = delete;
    PropSystem& operator=(const PropSystem&) = delete;

    /// Scatters decor into the rooms of the current level (deterministic per level).
    static void Populate(Game& game);

    /// Submits every prop into the Y-sorted render queue.
    static void Render(const Game& game);
};
