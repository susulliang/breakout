#include "IsometricMath.hpp"
#include "CameraSystem.hpp"
#include "World3D.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
int g_checks = 0;

void Check(bool condition, const char* expression, int line)
{
    ++g_checks;
    if (!condition)
    {
        std::cerr << "PortMathTests:" << line << ": check failed: "
                  << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool Near(float a, float b, float epsilon = 1e-5f)
{
    return std::fabs(a - b) <= epsilon;
}
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

int main()
{
    const Vector2 center = World3D::CellCenter(2, 3);
    CHECK(Near(center.x, 2.5f) && Near(center.y, 3.5f));
    int cellX = -1;
    int cellY = -1;
    CHECK(World3D::CellAt(center, cellX, cellY) && cellX == 2 && cellY == 3);
    CHECK(World3D::InCell(Vector2{ 2.999f, 3.999f }, 2, 3));
    CHECK(!World3D::InCell(Vector2{ 3.0f, 3.5f }, 2, 3));

    const Vector3 world = World3D::FromGround(center, 1.25f);
    const Vector2 restoredGround = World3D::ToGround(world);
    CHECK(Near(world.x, 2.5f) && Near(world.y, 1.25f) && Near(world.z, 3.5f));
    CHECK(Near(restoredGround.x, center.x) && Near(restoredGround.y, center.y));

    Vector3 groundHit{};
    CHECK(World3D::IntersectGround(
        Ray{ Vector3{ 2.0f, 5.0f, 3.0f }, Vector3{ 0.0f, -1.0f, 0.0f } }, groundHit));
    CHECK(Near(groundHit.x, 2.0f) && Near(groundHit.y, 0.0f) && Near(groundHit.z, 3.0f));
    CHECK(!World3D::IntersectGround(
        Ray{ Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f } }, groundHit));
    CHECK(!World3D::IntersectGround(
        Ray{ Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f } }, groundHit));

    const float speed = 4.0f;
    for (const Vector2 screen : { Vector2{ 1.0f, 0.0f }, Vector2{ -1.0f, 0.0f },
                                  Vector2{ 0.0f, 1.0f }, Vector2{ 0.0f, -1.0f },
                                  Vector2{ 0.70710678f, 0.70710678f } })
    {
        const Vector2 groundVelocity = CameraSystem::ScreenMovement(screen, speed);
        const Vector2 projectedPixels{
            32.0f * (groundVelocity.x - groundVelocity.y),
            16.0f * (groundVelocity.x + groundVelocity.y)
        };
        CHECK(Near(std::sqrt(projectedPixels.x * projectedPixels.x +
                             projectedPixels.y * projectedPixels.y),
                   32.0f * speed));
    }

    for (int x = -20; x <= 20; ++x)
    {
        for (int y = -20; y <= 20; ++y)
        {
            const Vector2 cartesian{ static_cast<float>(x) * 0.37f,
                                     static_cast<float>(y) * 0.29f };
            const Vector2 projected = IsometricMath::CartesianToIsometric(cartesian);
            const Vector2 restored = IsometricMath::IsometricToCartesian(projected);
            CHECK(Near(restored.x, cartesian.x));
            CHECK(Near(restored.y, cartesian.y));
        }
    }

    std::cout << "PortMathTests passed " << g_checks << " checks\n";
    return EXIT_SUCCESS;
}
