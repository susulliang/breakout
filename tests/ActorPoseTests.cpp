#include "ActorPose.hpp"

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
        std::cerr << "ActorPoseTests:" << line << ": check failed: "
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
    ActorPose::Input input{};
    input.groundPosition = Vector2{ 2.5f, 3.5f };
    input.forward = Vector2{ 0.0f, 1.0f };
    const ActorPose::Pose north = ActorPose::Evaluate(input, 0.5f);
    CHECK(north.parts.size() == ActorPose::kPartCount);
    CHECK(Near(north.root.x, 2.5f));
    CHECK(Near(north.root.z, 3.5f));
    CHECK(Near(north.headCenter.y, 1.105f));
    CHECK(Near(north.muzzle.x, north.grip.x));
    CHECK(north.muzzle.z > north.grip.z);

    input.forward = Vector2{ 1.0f, 0.0f };
    const ActorPose::Pose east = ActorPose::Evaluate(input, 0.5f);
    CHECK(Near(east.yaw, 1.5707963f));
    CHECK(east.muzzle.x > east.grip.x);
    CHECK(Near(east.muzzle.z, east.grip.z));
    CHECK(Near(east.headCenter.x, input.groundPosition.x));

    input.forward = Vector2{ 0.0f, 1.0f };
    input.movementBlend = 1.0f;
    input.walkPhase = 0.25f;
    const ActorPose::Pose stepping = ActorPose::Evaluate(input, 0.5f);
    CHECK(std::fabs(stepping.parts[4].rotation.x) > 0.4f);
    CHECK(std::fabs(stepping.parts[5].rotation.x) > 0.4f);
    CHECK(stepping.parts[4].rotation.x * stepping.parts[5].rotation.x < 0.0f);

    input.movementBlend = 0.0f;
    input.stagger = 1.0f;
    input.recoil = 1.0f;
    const ActorPose::Pose hit = ActorPose::Evaluate(input, 0.5f);
    CHECK(hit.parts[3].rotation.x < stepping.parts[3].rotation.x);
    CHECK(hit.grip.z < stepping.grip.z);

    std::cout << "ActorPoseTests passed " << g_checks << " checks\n";
    return EXIT_SUCCESS;
}
