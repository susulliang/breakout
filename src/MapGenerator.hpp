#pragma once

#include "Game.hpp"

/**
 * @file MapGenerator.hpp
 * @brief Phase 2 - procedural level generation (rooms + L-shaped corridors).
 *
 * Generates a complete cartesian level into an existing Game:
 *
 *  1. linear difficulty scaling, level 1 .. 10
 *     Level  1 ->  30x30 grid,  3 rooms
 *     Level 10 -> 100x100 grid, 15 rooms
 *  2. random, non-overlapping rectangular rooms (1 free cell of padding)
 *  3. 1-cell-wide L-shaped corridors joining consecutive room centres
 *  4. walls: every void cell 8-adjacent to a floor cell becomes kTileWall
 *  5. spawn points: left-most room centre -> Game::playerSpawn,
 *                   right-most room centre -> Game::levelExit
 *
 * @note Pure utility class - call the static member, do not instantiate.
 */
class MapGenerator
{
public:
    MapGenerator() = delete;
    ~MapGenerator() = delete;
    MapGenerator(const MapGenerator&) = delete;
    MapGenerator& operator=(const MapGenerator&) = delete;

    /**
     * @brief Fills @p game.gridMap / mapWidth / mapHeight / playerSpawn /
     *        levelExit from @p game.currentLevel.
     *
     * The whole map is rebuilt from scratch, so calling this again is safe and
     * is exactly what a level transition (Phase 5) will do.
     *
     * @param game Game whose level fields are (re)generated.
     */
    static void GenerateMap(Game& game);
};
