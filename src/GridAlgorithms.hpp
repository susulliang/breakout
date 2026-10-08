#pragma once

#include "Game.hpp"

#include <cstdlib>
#include <vector>

namespace GridAlgorithms
{
/// Tests every intermediate cell along a grid line; endpoints are excluded.
inline bool HasFloorLineOfSight(const std::vector<int>& grid, int width, int height,
                                int x0, int y0, int x1, int y1)
{
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int x = x0;
    int y = y0;
    int error = dx - dy;

    while (x != x1 || y != y1)
    {
        if ((x != x0 || y != y0) && (x != x1 || y != y1))
        {
            if (x < 0 || x >= width || y < 0 || y >= height) return false;
            const std::size_t index = static_cast<std::size_t>(y) *
                                      static_cast<std::size_t>(width) +
                                      static_cast<std::size_t>(x);
            if (index >= grid.size() || grid[index] != kTileFloor) return false;
        }

        const int twiceError = 2 * error;
        if (twiceError > -dy) { error -= dy; x += sx; }
        if (twiceError < dx) { error += dx; y += sy; }
    }
    return true;
}
}
