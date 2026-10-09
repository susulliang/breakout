#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Collision3D
{
struct Hit
{
    bool hit = false;
    float t = 1.0f;
    Vector3 point{};
};

inline float Dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 Subtract(Vector3 a, Vector3 b)
{
    return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z };
}

inline bool SweepSphere(Vector3 start, Vector3 end, float movingRadius,
                        Vector3 center, float targetRadius, Hit& hit)
{
    hit = Hit{};
    if (!std::isfinite(movingRadius) || !std::isfinite(targetRadius) ||
        movingRadius < 0.0f || targetRadius < 0.0f)
        return false;

    const Vector3 delta = Subtract(end, start);
    const Vector3 offset = Subtract(start, center);
    const float radius = movingRadius + targetRadius;
    const float a = Dot(delta, delta);
    const float c = Dot(offset, offset) - radius * radius;
    if (c <= 0.0f)
    {
        hit = Hit{ true, 0.0f, start };
        return true;
    }
    if (a <= 1e-12f)
        return false;

    const float b = 2.0f * Dot(offset, delta);
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return false;

    const float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;
    hit = Hit{ true, t, Vector3{ start.x + delta.x * t,
                                 start.y + delta.y * t,
                                 start.z + delta.z * t } };
    return true;
}

inline bool SweepAabb(Vector3 start, Vector3 end, float movingRadius,
                     BoundingBox target, Hit& hit)
{
    hit = Hit{};
    if (!std::isfinite(movingRadius) || movingRadius < 0.0f ||
        target.min.x > target.max.x || target.min.y > target.max.y ||
        target.min.z > target.max.z)
        return false;

    const Vector3 delta = Subtract(end, start);
    const Vector3 lo{ target.min.x - movingRadius, target.min.y - movingRadius,
                      target.min.z - movingRadius };
    const Vector3 hi{ target.max.x + movingRadius, target.max.y + movingRadius,
                      target.max.z + movingRadius };
    float enter = 0.0f;
    float exit = 1.0f;
    const float origins[] = { start.x, start.y, start.z };
    const float directions[] = { delta.x, delta.y, delta.z };
    const float mins[] = { lo.x, lo.y, lo.z };
    const float maxs[] = { hi.x, hi.y, hi.z };

    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(directions[axis]) <= 1e-8f)
        {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis])
                return false;
            continue;
        }
        float t0 = (mins[axis] - origins[axis]) / directions[axis];
        float t1 = (maxs[axis] - origins[axis]) / directions[axis];
        if (t0 > t1) std::swap(t0, t1);
        enter = std::max(enter, t0);
        exit = std::min(exit, t1);
        if (enter > exit)
            return false;
    }

    if (exit < 0.0f || enter > 1.0f)
        return false;
    const float t = std::max(0.0f, enter);
    hit = Hit{ true, t, Vector3{ start.x + delta.x * t,
                                 start.y + delta.y * t,
                                 start.z + delta.z * t } };
    return true;
}

inline bool Prefer(const Hit& candidate, const Hit& current, float epsilon = 1e-5f)
{
    return candidate.hit && (!current.hit || candidate.t < current.t - epsilon);
}
}
