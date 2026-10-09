#pragma once

#include "raylib.h"

#include <array>
#include <cstddef>
#include <cmath>

namespace ActorPose
{
enum class PartId : unsigned char
{
    Head,
    Torso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg
};

constexpr std::size_t kPartCount = 6;
constexpr float kWalkCycleSeconds = 0.66f;
constexpr float kCharacterHeight = 1.30f;
constexpr float kHeadRadius = 0.145f;
constexpr float kBodyRadius = 0.235f;

struct PartPose
{
    PartId id{};
    Vector3 position{}; // World-space center.
    Vector3 rotation{}; // Euler radians: X, Y, Z.
    Vector3 size{};     // World-space dimensions.
};

struct Input
{
    Vector2 groundPosition{};
    Vector2 forward{ 0.0f, 1.0f };
    float walkPhase = 0.0f;       // Normalized cycles.
    float movementBlend = 0.0f;  // [0, 1].
    float elapsed = 0.0f;
    float stagger = 0.0f;         // [0, 1].
    float recoil = 0.0f;          // [0, 1].
};

struct Pose
{
    Vector3 root{};
    float yaw = 0.0f;
    std::array<PartPose, kPartCount> parts{};
    Vector3 grip{};
    Vector3 muzzle{};
    Vector3 headCenter{};
    float headRadius = kHeadRadius;
    Vector3 bodyCenter{};
    float bodyRadius = kBodyRadius;
};

inline float Clamp01(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline Vector3 RotateLocal(Vector3 point, float yaw)
{
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return Vector3{ point.x * c + point.z * s,
                    point.y,
                    point.z * c - point.x * s };
}

inline Pose Evaluate(const Input& input, float weaponLength)
{
    Pose pose{};
    const float aimLength = std::sqrt(input.forward.x * input.forward.x +
                                      input.forward.y * input.forward.y);
    const float aimX = aimLength > 1e-5f ? input.forward.x / aimLength : 0.0f;
    const float aimZ = aimLength > 1e-5f ? input.forward.y / aimLength : 1.0f;
    pose.yaw = std::atan2(aimX, aimZ);
    pose.root = Vector3{ input.groundPosition.x, 0.0f, input.groundPosition.y };

    const float move = Clamp01(input.movementBlend);
    const float stagger = Clamp01(input.stagger);
    const float recoil = Clamp01(input.recoil);
    const float phase = input.walkPhase * 6.28318530718f;
    const float gait = std::sin(phase) * move;
    const float breath = std::sin(input.elapsed * 2.617993878f) * 0.014f * (1.0f - move);
    const float bob = std::fabs(std::sin(phase)) * 0.018f * move;

    const std::array<Vector3, kPartCount> localPositions{{
        Vector3{ 0.0f, 1.105f + breath + bob, 0.0f },
        Vector3{ 0.0f, 0.755f + breath + bob, 0.0f },
        Vector3{ -0.235f, 0.765f + bob, 0.105f },
        Vector3{ 0.235f, 0.735f + bob, 0.145f },
        Vector3{ -0.115f, 0.275f + bob, 0.0f },
        Vector3{ 0.115f, 0.275f + bob, 0.0f }
    }};
    const std::array<Vector3, kPartCount> sizes{{
        Vector3{ 0.245f, 0.265f, 0.235f },
        Vector3{ 0.365f, 0.465f, 0.245f },
        Vector3{ 0.145f, 0.425f, 0.165f },
        Vector3{ 0.145f, 0.405f, 0.165f },
        Vector3{ 0.165f, 0.515f, 0.205f },
        Vector3{ 0.165f, 0.515f, 0.205f }
    }};

    std::array<Vector3, kPartCount> localRotations{};
    localRotations[1].x = -0.15f * stagger;
    localRotations[2].x = 0.45f - gait * 0.48f - 0.35f * stagger;
    localRotations[3].x = 0.72f + gait * 0.40f - 0.25f * stagger - 0.48f * recoil;
    localRotations[4].x = gait * 0.48f;
    localRotations[5].x = -gait * 0.48f;
    localRotations[4].z = 0.035f * stagger;
    localRotations[5].z = -0.035f * stagger;

    for (std::size_t i = 0; i < kPartCount; ++i)
    {
        const Vector3 rotated = RotateLocal(localPositions[i], pose.yaw);
        pose.parts[i] = PartPose{
            static_cast<PartId>(i),
            Vector3{ pose.root.x + rotated.x, rotated.y, pose.root.z + rotated.z },
            Vector3{ localRotations[i].x, pose.yaw + localRotations[i].y,
                     localRotations[i].z },
            sizes[i]
        };
    }

    const Vector3 gripLocal{ 0.245f, 0.705f + bob, 0.305f - 0.08f * recoil };
    const Vector3 gripOffset = RotateLocal(gripLocal, pose.yaw);
    pose.grip = Vector3{ pose.root.x + gripOffset.x, gripOffset.y,
                         pose.root.z + gripOffset.z };
    pose.muzzle = Vector3{ pose.grip.x + aimX * weaponLength,
                           pose.grip.y,
                           pose.grip.z + aimZ * weaponLength };
    const Vector3 headOffset = RotateLocal(localPositions[0], pose.yaw);
    pose.headCenter = Vector3{ pose.root.x + headOffset.x, headOffset.y,
                              pose.root.z + headOffset.z };
    const Vector3 bodyOffset = RotateLocal(localPositions[1], pose.yaw);
    pose.bodyCenter = Vector3{ pose.root.x + bodyOffset.x, bodyOffset.y,
                               pose.root.z + bodyOffset.z };
    return pose;
}
}
