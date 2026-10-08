#include "IsometricMath.hpp"

// -----------------------------------------------------------------------------
//  Forward projection
//      isoX = cartX - cartY
//      isoY = (cartX + cartY) / 2
// -----------------------------------------------------------------------------
Vector2 IsometricMath::CartesianToIsometric(Vector2 cartesian)
{
    return Vector2{
        cartesian.x - cartesian.y,            // isoX
        (cartesian.x + cartesian.y) * 0.5f    // isoY
    };
}

// -----------------------------------------------------------------------------
//  Inverse projection
//      Solve the forward system for cartX / cartY:
//          cartX = isoY + isoX / 2
//          cartY = isoY - isoX / 2
// -----------------------------------------------------------------------------
Vector2 IsometricMath::IsometricToCartesian(Vector2 isometric)
{
    return Vector2{
        isometric.y + isometric.x * 0.5f,     // cartX
        isometric.y - isometric.x * 0.5f      // cartY
    };
}
