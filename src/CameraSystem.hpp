#pragma once

#include "Game.hpp"

class CameraSystem
{
public:
    static void Snap(Game& game, int width, int height);
    static void Update(Game& game, float deltaTime, int width, int height);
    static AimResult PickGround(const Game& game, Vector2 mouse);
    static Vector2 ScreenMovement(Vector2 normalizedInput, float speed);
};
