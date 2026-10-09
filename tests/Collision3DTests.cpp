#include "Collision3D.hpp"

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
        std::cerr << "Collision3DTests:" << line << ": check failed: "
                  << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

bool Near(float a, float b, float epsilon = 1e-4f)
{
    return std::fabs(a - b) <= epsilon;
}
}

int main()
{
    Collision3D::Hit sphere{};
    CHECK(Collision3D::SweepSphere(Vector3{ 0, 0, 0 }, Vector3{ 4, 0, 0 }, 0.0f,
                                   Vector3{ 2, 0, 0 }, 0.5f, sphere));
    CHECK(Near(sphere.t, 0.375f));
    CHECK(Near(sphere.point.x, 1.5f));

    CHECK(Collision3D::SweepSphere(Vector3{ 2, 0, 0 }, Vector3{ 2, 0, 0 }, 0.1f,
                                   Vector3{ 2, 0, 0 }, 0.5f, sphere));
    CHECK(Near(sphere.t, 0.0f));
    CHECK(!Collision3D::SweepSphere(Vector3{ 0, 0, 0 }, Vector3{ 0, 0, 0 }, 0.0f,
                                    Vector3{ 2, 0, 0 }, 0.5f, sphere));
    CHECK(!Collision3D::SweepSphere(Vector3{ 0, 1, 0 }, Vector3{ 4, 1, 0 }, 0.0f,
                                    Vector3{ 2, 0, 0 }, 0.5f, sphere));

    const BoundingBox box{ Vector3{ 1, 0, -1 }, Vector3{ 2, 1, 1 } };
    Collision3D::Hit wall{};
    CHECK(Collision3D::SweepAabb(Vector3{ 0, 0.5f, 0 }, Vector3{ 3, 0.5f, 0 },
                                 0.0f, box, wall));
    CHECK(Near(wall.t, 1.0f / 3.0f));
    CHECK(!Collision3D::SweepAabb(Vector3{ 0, 2, 0 }, Vector3{ 3, 2, 0 },
                                  0.0f, box, wall));
    CHECK(Collision3D::SweepAabb(Vector3{ 1.5f, 0.5f, 0 },
                                 Vector3{ 1.5f, 0.5f, 0 }, 0.0f, box, wall));
    CHECK(Near(wall.t, 0.0f));

    Collision3D::Hit nearer{ true, 0.2f, Vector3{} };
    CHECK(Collision3D::Prefer(wall, nearer));
    CHECK(!Collision3D::Prefer(Collision3D::Hit{ true, 0.3f, Vector3{} }, nearer));
    CHECK(Collision3D::Prefer(Collision3D::Hit{ true, 0.1f, Vector3{} }, nearer));

    std::cout << "Collision3DTests passed " << g_checks << " checks\n";
    return EXIT_SUCCESS;
}
