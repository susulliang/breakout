#include "LightingSystem.hpp"

#include "IsometricMath.hpp"
#include "World.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstddef>
#include <vector>

/**
 * @file LightingSystem.cpp
 * @brief RenderTexture pipeline + GLSL point light upload.
 *
 * Light positions are authored in cartesian tile units; they are converted to
 * screen pixels with the same projection the scene uses, so the shader only has
 * to compare pixel distances. Radii are authored in tiles and scaled by
 * kPixelsPerTile so a radius of e.g. 8 covers eight tiles horizontally.
 */

namespace
{
/// Global ambient level: the underground facility is deliberately dark, but the
/// floor must never fall to pure black. Raised from 0.13 to 0.34 in the Phase 5
/// fix so a room stays readable even when none of its lights are in range yet;
/// lighting.fs enforces its own ambient floor as a second line of defence.
constexpr float kAmbientLevel = 0.34f;

/// The player carries a head torch, otherwise the rooms would be unreadable.
constexpr float kPlayerTorchRadius    = 8.5f;    // tiles
constexpr float kPlayerTorchIntensity = 1.15f;
const Color     kPlayerTorchColor     = Color{ 255, 226, 186, 255 };

/// Room light colours per zone type (cool medical blue / warm workshop red).
const Color kMedicalLightColor  = Color{  96, 170, 255, 255 };
const Color kWorkshopLightColor = Color{ 255, 140,  90, 255 };

/// Accent light colours matching the high saturation props.
const Color kNeonGreenLight = Color{ 104, 255, 140, 255 };
const Color kScreenBlueLight = Color{  96, 196, 255, 255 };

/// Alpha of the scene clear colour (the void outside the map).
const Color kSceneClearColor = Color{ 8, 10, 14, 255 };
}   // namespace

LightingSystem::~LightingSystem()
{
    Shutdown();
}

bool LightingSystem::Initialize()
{
    m_width  = (GetScreenWidth() > 0) ? GetScreenWidth() : 1;
    m_height = (GetScreenHeight() > 0) ? GetScreenHeight() : 1;

    m_target = LoadRenderTexture(m_width, m_height);
    SetTextureFilter(m_target.texture, TEXTURE_FILTER_POINT);   // keep the pixel grid crisp

    // The shader is optional: resolve it from the build dir or the source tree.
    const char* candidates[] = { "shaders/lighting.fs", "lighting.fs" };
    for (const char* candidate : candidates)
    {
        const std::string resolved = ResolveAssetPath(candidate);
        if (resolved.empty())
        {
            continue;
        }

        m_shader = LoadShader(nullptr, resolved.c_str());
        // raylib has no IsShaderValid() helper in 5.0: a failed build leaves the
        // GL program id at 0 (and logs its own warning), which is the check used
        // both here and on the fallback path below.
        if (m_shader.id != 0)
        {
            m_shaderPath = resolved;
            m_available  = true;

            // Cache the uniform locations once.
            m_locAmbient     = GetShaderLocation(m_shader, "ambientLevel");
            m_locLightCount  = GetShaderLocation(m_shader, "lightCount");
            m_locResolution  = GetShaderLocation(m_shader, "resolution");
            m_locLightPos    = GetShaderLocation(m_shader, "lightPos");
            if (m_locLightPos < 0)    m_locLightPos    = GetShaderLocation(m_shader, "lightPos[0]");
            m_locLightColor  = GetShaderLocation(m_shader, "lightColor");
            if (m_locLightColor < 0)  m_locLightColor  = GetShaderLocation(m_shader, "lightColor[0]");
            m_locLightRadius = GetShaderLocation(m_shader, "lightRadius");
            if (m_locLightRadius < 0) m_locLightRadius = GetShaderLocation(m_shader, "lightRadius[0]");

            TraceLog(LOG_INFO, "LightingSystem: shader loaded (%s)", resolved.c_str());
            break;
        }

        // Invalid program (compilation/link failure): release and fall back.
        if (m_shader.id != 0)
        {
            UnloadShader(m_shader);
        }
        m_shader = Shader{};
        TraceLog(LOG_WARNING, "LightingSystem: shader at %s failed to build, using the lit fallback",
                 resolved.c_str());
    }

    if (!m_available)
    {
        TraceLog(LOG_WARNING, "LightingSystem: assets/shaders/lighting.fs not found - "
                              "scene is rendered without dynamic lighting");
    }

    if (m_target.id == 0)
    {
        TraceLog(LOG_ERROR, "LightingSystem: failed to allocate the scene render texture");
        return false;
    }
    return true;
}

void LightingSystem::Shutdown()
{
    if (m_shader.id != 0)
    {
        UnloadShader(m_shader);
        m_shader = Shader{};
    }
    if (m_target.id != 0)
    {
        UnloadRenderTexture(m_target);
        m_target = RenderTexture2D{};
    }
    m_available = false;
}

void LightingSystem::Resize(int width, int height)
{
    if (width <= 0 || height <= 0 || (width == m_width && height == m_height))
    {
        return;
    }

    if (m_target.id != 0)
    {
        UnloadRenderTexture(m_target);
    }

    m_width  = width;
    m_height = height;
    m_target = LoadRenderTexture(m_width, m_height);
    SetTextureFilter(m_target.texture, TEXTURE_FILTER_POINT);
}

const char* LightingSystem::StatusText() const
{
    return m_available ? "shader ON" : "FALLBACK (unlit)";
}

void LightingSystem::PopulateLevelLights(Game& game)
{
    game.activeLights.clear();

    // One ceiling light per room: cool blue medical bays alternate with warm
    // red workshops (Phase 3.F colour rule).
    for (std::size_t i = 0; i < game.rooms.size(); ++i)
    {
        const RoomRect& room = game.rooms[i];
        const bool medicalBay = ((i % 2) == 0);

        Light light{};
        light.pos       = Vector2{ room.centerX, room.centerY };
        light.radius    = medicalBay ? 9.0f : 8.0f;
        light.color     = medicalBay ? kMedicalLightColor : kWorkshopLightColor;
        light.intensity = medicalBay ? 1.00f : 1.20f;
        game.activeLights.push_back(light);
    }

    // High saturation props emit their own small glow.
    for (const Prop& prop : game.props)
    {
        if (prop.kind != PropKind::VendingMachine && prop.kind != PropKind::ScreenPanel)
        {
            continue;
        }

        Light glow{};
        glow.pos       = prop.pos;
        glow.radius    = 4.5f;
        glow.color     = (prop.kind == PropKind::VendingMachine) ? kNeonGreenLight : kScreenBlueLight;
        glow.intensity = 0.90f;
        game.activeLights.push_back(glow);
    }
}

void LightingSystem::BeginScene()
{
    BeginTextureMode(m_target);
    ClearBackground(kSceneClearColor);
}

void LightingSystem::UploadLights(const Game& game, const Camera2D& camera)
{
    struct Candidate
    {
        float distanceSqr = 0.0f;
        bool  isPlayerTorch = false;
        std::size_t lightIndex = 0;
    };

    // Rank every light by distance to the camera centre so the uniform budget
    // is spent on the lights that are actually visible (truncation rule of 3.F).
    const Vector2 focus{ camera.target.x, camera.target.y };
    std::vector<Candidate> candidates;
    candidates.reserve(game.activeLights.size() + 1);

    for (std::size_t i = 0; i < game.activeLights.size(); ++i)
    {
        const Light& light = game.activeLights[i];
        Candidate candidate;
        candidate.distanceSqr = VectorLengthSqr(Vector2{ light.pos.x - focus.x, light.pos.y - focus.y });
        candidate.lightIndex  = i;
        candidates.push_back(candidate);
    }

    Candidate torch;
    torch.isPlayerTorch = true;
    torch.distanceSqr = VectorLengthSqr(Vector2{ game.player.pos.x - focus.x, game.player.pos.y - focus.y }) - 1.0f;
    candidates.push_back(torch);   // the head torch wins ties: the player is always lit

    const int kept = std::min(static_cast<int>(candidates.size()), kMaxShaderLights);
    std::partial_sort(candidates.begin(), candidates.begin() + kept, candidates.end(),
                      [](const Candidate& a, const Candidate& b) { return a.distanceSqr < b.distanceSqr; });

    int count = 0;
    for (int i = 0; i < kept; ++i)
    {
        const Candidate& candidate = candidates[static_cast<std::size_t>(i)];

        Vector2 cartesian{ 0.0f, 0.0f };
        Color   color     = WHITE;
        float   radius    = 1.0f;
        float   intensity = 1.0f;

        if (candidate.isPlayerTorch)
        {
            cartesian = game.player.pos;
            color     = kPlayerTorchColor;
            radius    = kPlayerTorchRadius;
            intensity = kPlayerTorchIntensity;
        }
        else
        {
            const Light& light = game.activeLights[candidate.lightIndex];
            cartesian = light.pos;
            color     = light.color;
            radius    = light.radius;
            intensity = light.intensity;
        }

        // Cartesian tiles -> world pixels -> screen pixels (shader space).
        const Vector2 screen = GetWorldToScreen2D(CartesianToScreen(cartesian), camera);

        m_lightPosData[count * 2 + 0] = screen.x;
        m_lightPosData[count * 2 + 1] = screen.y;

        m_lightColorData[count * 4 + 0] = static_cast<float>(color.r) / 255.0f;
        m_lightColorData[count * 4 + 1] = static_cast<float>(color.g) / 255.0f;
        m_lightColorData[count * 4 + 2] = static_cast<float>(color.b) / 255.0f;
        m_lightColorData[count * 4 + 3] = intensity;

        m_lightRadiusData[count] = radius * kPixelsPerTile;

        ++count;
    }

    m_uploadedLights = count;

    SetShaderValue(m_shader, m_locAmbient, &kAmbientLevel, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_shader, m_locLightCount, &count, SHADER_UNIFORM_INT);

    const float resolution[2] = { static_cast<float>(m_width), static_cast<float>(m_height) };
    SetShaderValueV(m_shader, m_locResolution, resolution, SHADER_UNIFORM_VEC2, 1);
    SetShaderValueV(m_shader, m_locLightPos, m_lightPosData, SHADER_UNIFORM_VEC2, count);
    SetShaderValueV(m_shader, m_locLightColor, m_lightColorData, SHADER_UNIFORM_VEC4, count);
    SetShaderValueV(m_shader, m_locLightRadius, m_lightRadiusData, SHADER_UNIFORM_FLOAT, count);
}

void LightingSystem::EndScene(const Game& game, const Camera2D& camera)
{
    EndTextureMode();

    if (m_available)
    {
        UploadLights(game, camera);
        BeginShaderMode(m_shader);
    }

    // Negative source height flips the render texture back into screen space.
    const Rectangle source{ 0.0f, 0.0f,
                            static_cast<float>(m_target.texture.width),
                            static_cast<float>(-m_target.texture.height) };
    DrawTextureRec(m_target.texture, source, Vector2{ 0.0f, 0.0f }, WHITE);

    if (m_available)
    {
        EndShaderMode();
    }
}
