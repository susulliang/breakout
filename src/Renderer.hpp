#pragma once

#include "raylib.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

/**
 * @file Renderer.hpp
 * @brief Minimal Y-sorting render queue for isometric scenes.
 *
 * In an isometric projection the painter's algorithm has to follow the depth
 * axis: entities with a SMALLER isometric Y (further "back" on screen) must be
 * painted first, entities with a LARGER Y last, so that the ones closer to the
 * camera correctly overlap everything behind them.
 *
 * Usage - always called between BeginMode2D()/EndMode2D() so the queued
 * commands are executed in world space:
 * @code
 *     renderer.BeginFrame();
 *     renderer.Submit(isoY.floor,   [&] { DrawDiamond(...); });
 *     renderer.Submit(isoY.player,  [&] { DrawCircleV(...); });
 *     renderer.Flush();   // stable-sort by depth, then execute
 * @endcode
 *
 * @note Not copyable: it owns the per-frame command buffer.
 */
class Renderer
{
public:
    /// A deferred draw call. Captures must be by value - the queue outlives
    /// the submitting scope.
    using DrawCommand = std::function<void()>;

    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Clears the queue. Call once per frame, before submitting anything.
    void BeginFrame();

    /**
     * @brief Queues a draw command.
     * @param depth Sorting key - normally the isometric Y of the entity.
     *              Smaller values are painted first (further back).
     * @param command The deferred draw call.
     */
    void Submit(float depth, DrawCommand command);

    /**
     * @brief Convenience overload taking the depth from a screen position.
     * @param screenPosition Entity position; its Y is used as sorting key.
     * @param command The deferred draw call.
     */
    void Submit(Vector2 screenPosition, DrawCommand command);

    /// Sorts the queue back-to-front (ascending depth) and executes it.
    void Flush();

    /// Number of commands currently waiting in the queue.
    std::size_t PendingCount() const noexcept;

    /// Number of commands executed by the most recent Flush() call.
    std::size_t LastDrawnCount() const noexcept;

private:
    struct Entry
    {
        float         depth    = 0.0f;
        std::uint64_t sequence = 0;   ///< Stable tie-breaker for equal depths.
        DrawCommand   command;
    };

    std::vector<Entry> m_queue;
    std::uint64_t      m_sequence       = 0;
    std::size_t        m_lastDrawnCount = 0;
};
