#include "CameraSystem.hpp"

#include "IsometricMath.hpp"
#include "World.hpp"
#include "World3D.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kFollowRate = 5.0f;
constexpr float kCameraDistance = 30.0f;

Vector3 CameraOffset()
{
    constexpr float kHorizontal = 0.612372436f;
    constexpr float kVertical = 0.5f;
    return Vector3{ kCameraDistance * kHorizontal,
                    kCameraDistance * kVertical,
                    kCameraDistance * kHorizontal };
}

void ConfigureCamera(Game& game, int width, int height, bool snap)
{
    const int safeHeight = std::max(1, height);
    const float targetLift = 0.05f * static_cast<float>(safeHeight) /
                             World3D::kVerticalPixelsPerUnit;
    const Vector3 target{ game.player.pos.x, targetLift, game.player.pos.y };
    const Vector3 eyeOffset = CameraOffset();

    if (snap || !game.camera3DReady)
    {
        game.camera3D.target = target;
    }
    game.camera3D.position = Vector3{
        game.camera3D.target.x + eyeOffset.x,
        game.camera3D.target.y + eyeOffset.y,
        game.camera3D.target.z + eyeOffset.z
    };
    game.camera3D.up = Vector3{ 0.0f, 1.0f, 0.0f };
    game.camera3D.fovy = static_cast<float>(safeHeight) / World3D::kPixelsPerWorldUnit;
    game.camera3D.projection = CAMERA_ORTHOGRAPHIC;
    game.camera3DReady = true;

}
}

void CameraSystem::Snap(Game& game, int width, int height)
{
    ConfigureCamera(game, width, height, true);
    game.camera.offset = Vector2{ static_cast<float>(std::max(1, width)) * 0.5f,
                                  static_cast<float>(std::max(1, height)) * 0.55f };
    game.camera.target = CartesianToScreen(game.player.pos);
    game.camera.rotation = 0.0f;
    game.camera.zoom = 1.0f;
}

void CameraSystem::Update(Game& game, float deltaTime, int width, int height)
{
    const int safeHeight = std::max(1, height);
    const float targetLift = 0.05f * static_cast<float>(safeHeight) /
                             World3D::kVerticalPixelsPerUnit;
    const Vector3 target{ game.player.pos.x, targetLift, game.player.pos.y };
    const float t = std::clamp(kFollowRate * deltaTime, 0.0f, 1.0f);

    if (!game.camera3DReady)
    {
        Snap(game, width, height);
    }
    else
    {
        game.camera3D.target.x += (target.x - game.camera3D.target.x) * t;
        game.camera3D.target.y += (target.y - game.camera3D.target.y) * t;
        game.camera3D.target.z += (target.z - game.camera3D.target.z) * t;
        ConfigureCamera(game, width, height, false);

        const Vector2 targetScreen = CartesianToScreen(game.player.pos);
        game.camera.offset = Vector2{ static_cast<float>(std::max(1, width)) * 0.5f,
                                      static_cast<float>(std::max(1, height)) * 0.55f };
        game.camera.target.x += (targetScreen.x - game.camera.target.x) * t;
        game.camera.target.y += (targetScreen.y - game.camera.target.y) * t;
        game.camera.rotation = 0.0f;
        game.camera.zoom = 1.0f;
    }
}

AimResult CameraSystem::PickGround(const Game& game, Vector2 mouse)
{
    AimResult result{};
    const Ray ray = GetMouseRay(mouse, game.camera3D);
    Vector3 point{};
    if (!World3D::IntersectGround(ray, point))
    {
        return result;
    }
    result.valid = true;
    result.point = point;
    result.groundPoint = point;
    return result;
}

Vector2 CameraSystem::ScreenMovement(Vector2 normalizedInput, float speed)
{
    return Vector2{
        (normalizedInput.y + 0.5f * normalizedInput.x) * speed,
        (normalizedInput.y - 0.5f * normalizedInput.x) * speed
    };
}
