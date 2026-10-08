#pragma once

#include "raylib.h"

#include <cmath>
#include <limits>

namespace World3D
{
constexpr float kPixelsPerWorldUnit = 45.254834f;
constexpr float kVerticalPixelsPerUnit = 39.191836f;
constexpr float kInitialWallHeight = 0.816497f;

inline Vector3 FromGround(Vector2 point, float height = 0.0f)
{
    return Vector3{ point.x, height, point.y };
}

inline Vector2 ToGround(Vector3 point)
{
    return Vector2{ point.x, point.z };
}

inline Vector2 CellCenter(int x, int gridY)
{
    return Vector2{ static_cast<float>(x) + 0.5f,
                    static_cast<float>(gridY) + 0.5f };
}

inline bool CellAt(Vector2 point, int& x, int& gridY)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
    {
        return false;
    }
    x = static_cast<int>(std::floor(point.x));
    gridY = static_cast<int>(std::floor(point.y));
    return true;
}

inline bool InCell(Vector2 point, int x, int gridY)
{
    return point.x >= static_cast<float>(x) && point.x < static_cast<float>(x + 1) &&
           point.y >= static_cast<float>(gridY) && point.y < static_cast<float>(gridY + 1);
}

inline BoundingBox WallBox(int x, int gridY, float height = kInitialWallHeight)
{
    return BoundingBox{
        Vector3{ static_cast<float>(x), 0.0f, static_cast<float>(gridY) },
        Vector3{ static_cast<float>(x + 1), height, static_cast<float>(gridY + 1) }
    };
}

inline bool IntersectGround(Ray ray, Vector3& point)
{
    if (!std::isfinite(ray.direction.y) || std::fabs(ray.direction.y) < 1e-6f)
    {
        return false;
    }
    const float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f || !std::isfinite(t))
    {
        return false;
    }
    point = Vector3{
        ray.position.x + t * ray.direction.x,
        0.0f,
        ray.position.z + t * ray.direction.z
    };
    return std::isfinite(point.x) && std::isfinite(point.z);
}
}
