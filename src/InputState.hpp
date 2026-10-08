#pragma once

#include "raylib.h"

struct InputFrame
{
    Vector2 screenMove{};
    Vector2 mouse{};
    int selectedSlot = -1;
    bool firePressed = false;
    bool fireHeld = false;
    bool healPressed = false;
    bool rebuildPressed = false;
    bool toggleRendererPressed = false;
    bool startPressed = false;
    bool menuPressed = false;
    bool previewGameOverPressed = false;
    bool previewVictoryPressed = false;
};

struct AimResult
{
    bool valid = false;
    Vector3 point{};
    Vector3 groundPoint{};
    int enemyIndex = -1;
    bool pickedHead = false;
};

inline InputFrame CaptureInputFrame()
{
    InputFrame input{};
    input.screenMove = Vector2{
        static_cast<float>(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) -
            static_cast<float>(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
        static_cast<float>(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) -
            static_cast<float>(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
    };
    input.mouse = GetMousePosition();
    input.firePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.fireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.healPressed = IsKeyPressed(KEY_FIVE);
    input.rebuildPressed = IsKeyPressed(KEY_R);
    input.toggleRendererPressed = IsKeyPressed(KEY_F8);
    input.startPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    input.menuPressed = IsKeyPressed(KEY_ESCAPE);
    input.previewGameOverPressed = IsKeyPressed(KEY_G);
    input.previewVictoryPressed = IsKeyPressed(KEY_V);
    for (int key = KEY_ONE; key <= KEY_FOUR; ++key)
    {
        if (IsKeyPressed(key))
        {
            input.selectedSlot = key - KEY_ONE;
            break;
        }
    }
    return input;
}
