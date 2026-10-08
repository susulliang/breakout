#pragma once

#include "raylib.h"

/**
 * @file IsometricMath.hpp
 * @brief Stateless cartesian <-> isometric coordinate conversions.
 *
 * The whole game world (physics, collision, generation) is simulated on a flat
 * 2D cartesian grid. The isometric projection is applied strictly as a
 * rendering step, and inverted whenever a screen-space position (typically the
 * mouse cursor) has to be mapped back onto the grid.
 *
 * Unit space convention (one unit == one grid cell):
 * @code
 *     isoX = cartX - cartY
 *     isoY = (cartX + cartY) / 2
 * @endcode
 *
 * To turn the result into pixels, multiply isoX by half the tile width and
 * isoY by the full tile height (see CartesianToScreen() in main.cpp).
 *
 * @note Pure utility class - it is not instantiable, call the static members.
 */
class IsometricMath
{
public:
    IsometricMath() = delete;
    ~IsometricMath() = delete;
    IsometricMath(const IsometricMath&) = delete;
    IsometricMath& operator=(const IsometricMath&) = delete;

    /**
     * @brief Projects a cartesian grid position into isometric space.
     * @param cartesian Position in grid units (x / y).
     * @return Unscaled isometric position (isoX / isoY).
     */
    static Vector2 CartesianToIsometric(Vector2 cartesian);

    /**
     * @brief Inverse projection: maps isometric space back onto the grid.
     *
     * Used for mouse picking, i.e. resolving the grid cell the player aims at.
     *
     * @param isometric Position in isometric unit space (isoX / isoY).
     * @return Position in grid units (x / y).
     */
    static Vector2 IsometricToCartesian(Vector2 isometric);
};
