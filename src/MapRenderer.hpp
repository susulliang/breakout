#pragma once

#include "Game.hpp"

/**
 * @file MapRenderer.hpp
 * @brief Phase 2 - draws Game::gridMap as Y-sorted isometric geometry.
 *
 * Floors become flat 64x32 diamonds, walls become extrude cubes (top face
 * raised by kWallHeight with the two visible side faces connecting it back to
 * the base diamond). Every tile is pushed into the shared Renderer queue with
 * its isometric Y as the sorting key, so the map, the player and any other
 * entity submitted afterwards are painted back-to-front together.
 *
 * @note RenderMap() only QUEUES commands - it never calls BeginFrame() or
 *       Flush(). The caller (main.cpp) owns the frame:
 *       @code
 *           renderer.BeginFrame();
 *           MapRenderer::RenderMap(game);
 *           renderer.Submit(playerDepth, ...);   // same queue -> correct overlap
 *           renderer.Flush();
 *       @endcode
 *
 * @note Pure utility class - call the static member, do not instantiate.
 */
class MapRenderer
{
public:
    MapRenderer() = delete;
    ~MapRenderer() = delete;
    MapRenderer(const MapRenderer&) = delete;
    MapRenderer& operator=(const MapRenderer&) = delete;

    /**
     * @brief Queues every visible tile of @p game.gridMap.
     *
     * Takes the game by const reference (the public contract): the level data
     * is only read. The render queue itself is a mutable scratch buffer rather
     * than logical game state, so the implementation casts that constness away
     * for the single member it has to touch.
     *
     * @param game Game holding the generated grid; nothing is drawn when the
     *             grid is empty.
     */
    static void RenderMap(const Game& game);
};
