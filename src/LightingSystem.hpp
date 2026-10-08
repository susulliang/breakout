#pragma once

#include "Game.hpp"

#include "raylib.h"

#include <string>

/**
 * @file LightingSystem.hpp
 * @brief 2D dynamic lighting: RenderTexture pipeline + GLSL point light shader (Phase 3.F).
 *
 * Pipeline:
 * @code
 *   lighting.BeginScene();          // scene -> RenderTexture2D
 *   BeginMode2D(camera);
 *       ... Y-sorted submissions + Flush ...
 *   EndMode2D();
 *   lighting.EndScene(game, camera); // RenderTexture + lighting.fs -> screen
 * @endcode
 *
 * The system degrades gracefully: when assets/shaders/lighting.fs is missing or
 * fails to compile, the RenderTexture is still used but presented unlit, so the
 * game keeps running (never crashes) - IsAvailable() reports which path is live.
 */
class LightingSystem
{
public:
    LightingSystem() = default;
    ~LightingSystem();

    LightingSystem(const LightingSystem&) = delete;
    LightingSystem& operator=(const LightingSystem&) = delete;

    /// Loads the shader (optional) and allocates the scene target. Call after InitWindow.
    bool Initialize();

    /// Releases the shader and the render target.
    void Shutdown();

    /// Reallocates the target after a window resize.
    void Resize(int width, int height);

    /// True when the GLSL lighting shader is loaded and usable.
    bool IsAvailable() const { return m_available; }

    /// Number of lights uploaded during the last EndScene() call.
    int UploadedLightCount() const { return m_uploadedLights; }

    /// Resolved shader path (empty when the shader was not found).
    const std::string& ShaderPath() const { return m_shaderPath; }

    /// Short status string for the on-screen debug readout.
    const char* StatusText() const;

    /// Builds the level light list: one room light per room + prop accent lights.
    static void PopulateLevelLights(Game& game);

    /// Binds the scene render target and clears it with the ambient void colour.
    void BeginScene();

    /// Presents the scene target, applying the lighting shader when available.
    void EndScene(const Game& game, const Camera2D& camera);

private:
    /// Selects the nearest lights (level lights + the player head torch) and uploads them.
    void UploadLights(const Game& game, const Camera2D& camera);

    Shader          m_shader{};
    RenderTexture2D m_target{};
    int             m_width  = 0;
    int             m_height = 0;
    bool            m_available   = false;
    int             m_uploadedLights = 0;

    int m_locAmbient    = -1;
    int m_locLightCount = -1;
    int m_locResolution = -1;
    int m_locLightPos   = -1;
    int m_locLightColor = -1;
    int m_locLightRadius = -1;

    std::string m_shaderPath{};

    // Scratch uniform buffers (float based, uploaded with SetShaderValueV).
    float m_lightPosData[kMaxShaderLights * 2]{};
    float m_lightColorData[kMaxShaderLights * 4]{};
    float m_lightRadiusData[kMaxShaderLights]{};
};
