// =============================================================================
//  Breakout - Phase 5 : Game Loop Completion & Polish
// -----------------------------------------------------------------------------
//  This translation unit owns the game loop and the finite state machine that
//  drives every screen of the game:
//
//      INIT -> MAIN_MENU -> PLAYING -> GAME_OVER -> MAIN_MENU -> ...
//                                   \-> VICTORY -> MAIN_MENU -> ...
//
//  Phase 5 additions, on top of the Phase 3 player / combat / lighting frame:
//
//    * StartLevel() resets EVERY transient pool at once - bullets, particles and
//      enemies - so a level transition can never leak state into the next level;
//    * the PLAYING state is now a complete loop: the enemy pool and the particle
//      pool are ticked right after the player and the bullets, the run ends in
//      GAME_OVER when the player's hit points reach zero, and touching the level
//      exit advances the level - rolling over into VICTORY past kTotalLevels;
//    * UpdateCameraFollow() is no longer glued to the player: it lerps the
//      camera target towards the player's screen position every frame;
//    * the HUD gained a health bar plus a system readout (enemies, particles);
//    * the (invisible) level exit is marked with a pulsing beacon, otherwise a
//      level could never be completed on purpose.
//
//  Frame contract (unchanged since Phase 1): BeginFrame / Flush are owned by
//  this file only. MapRenderer, CombatSystem, EnemySystem, PlayerSystem and
//  ParticleSystem merely Submit() into the shared Y-sort queue; every queued
//  lambda captures BY VALUE so it can outlive the submitting scope.
// =============================================================================

#include "Game.hpp"

#include "CombatSystem.hpp"
#include "CameraSystem.hpp"
#include "EnemySystem.hpp"
#include "LightingSystem.hpp"
#include "MapGenerator.hpp"
#include "MapRenderer.hpp"
#include "ProceduralScene3D.hpp"
#include "SceneRenderer3D.hpp"
#include "ParticleSystem.hpp"
#include "PlayerSystem.hpp"
#include "Props.hpp"
#include "World.hpp"
#include "World3D.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace
{

// -----------------------------------------------------------------------------
//  Presentation constants
// -----------------------------------------------------------------------------
constexpr char  kWindowTitle[]  = "Breakout - Phase 5 (Game Loop & Polish)";

constexpr float kMaxFrameTime   = 0.05f;   ///< clamp spikes after a stalled frame
constexpr float kBootScreenTime = 1.25f;   ///< INIT state duration, in seconds

/// Phase 5: walking within this many tiles of the exit completes the level.
constexpr float kExitRadius = 1.0f;

// The particle burst sizes are private to the systems that fire them (wall
// sparks in CombatSystem, flesh hits / kills in EnemySystem); the game loop
// itself never spawns particles directly, it only ticks the pool.

// -----------------------------------------------------------------------------
//  Palette (Phase 6 art direction: flat graphic faces, high contrast accents)
// -----------------------------------------------------------------------------
const Color kColorBackground = Color{ 18,  20,  28,  255 };
const Color kColorVoid       = Color{ 12,  13,  19,  255 };
const Color kColorAccent     = Color{ 255, 214, 64,  255 };
const Color kColorText       = Color{ 232, 236, 245, 255 };
const Color kColorTextDim    = Color{ 138, 146, 166, 255 };
const Color kColorScrim      = Color{ 10,  11,  16,  205 };
const Color kColorExit       = Color{ 96,  226, 214, 255 };

/// Health bar ramp: green while healthy, amber under 65 %, red under 35 %.
const Color kColorHealthHigh = Color{ 96,  224, 130, 255 };
const Color kColorHealthMid  = Color{ 236, 186, 72,  255 };
const Color kColorHealthLow  = Color{ 232, 76,  72,  255 };
const Color kColorHealthBack = Color{ 46,  22,  26,  255 };

// -----------------------------------------------------------------------------
//  Globals
// -----------------------------------------------------------------------------
/// Single runtime instance; the loop below owns it end to end.
Game g_game{};

/// Phase 3.F: RenderTexture + GLSL lighting pipeline for the PLAYING state.
LightingSystem g_lighting{};
SceneRenderer3D g_scene3D{};
ProceduralScene3D g_proceduralScene3D{};

// -----------------------------------------------------------------------------
//  Small helpers
// -----------------------------------------------------------------------------

const char* TileName(int tile)
{
    switch (tile)
    {
        case kTileFloor: return "floor";
        case kTileWall:  return "wall";
        default:         return "void";
    }
}

/// Draws text horizontally centred around (centerX, y) at its natural width.
void DrawCenteredText(const char* text, int centerX, int y, int fontSize, Color color)
{
    DrawText(text, centerX - MeasureText(text, fontSize) / 2, y, fontSize, color);
}

/// Replaces the platform pointer with a high-contrast in-game aiming reticle.
void DrawCrosshair(const Game& game)
{
    const Vector2 mouse = game.input.mouse;
    const Color outline = Color{ 10, 11, 16, 240 };
    const bool confirmed = game.hitFeedbackTime > 0.0f;
    const float feedback = confirmed ? ClampFloat(game.hitFeedbackTime / 0.34f, 0.0f, 1.0f) : 0.0f;
    const float scale = confirmed ? (game.hitFeedbackHeadshot ? 1.8f : 1.35f) : 1.0f;
    const Color accent = confirmed ? Color{ 255, 54, 58, 255 } : Color{ 255, 214, 64, 255 };
    const float arm = 12.0f * scale;
    const float inner = 4.0f * scale;

    DrawCircleLinesV(mouse, 7.0f * scale, outline);
    DrawCircleLinesV(mouse, 5.0f * scale, accent);
    if (confirmed && game.hitFeedbackHeadshot)
        DrawCircleLinesV(mouse, 13.0f * scale, Fade(accent, feedback));
    DrawLineEx(Vector2{ mouse.x - arm, mouse.y }, Vector2{ mouse.x - inner, mouse.y },
               3.0f, outline);
    DrawLineEx(Vector2{ mouse.x + inner, mouse.y }, Vector2{ mouse.x + arm, mouse.y },
               3.0f, outline);
    DrawLineEx(Vector2{ mouse.x, mouse.y - arm }, Vector2{ mouse.x, mouse.y - inner },
               3.0f, outline);
    DrawLineEx(Vector2{ mouse.x, mouse.y + inner }, Vector2{ mouse.x, mouse.y + arm },
               3.0f, outline);
    DrawLineEx(Vector2{ mouse.x - arm, mouse.y }, Vector2{ mouse.x - inner, mouse.y },
               1.5f, accent);
    DrawLineEx(Vector2{ mouse.x + inner, mouse.y }, Vector2{ mouse.x + arm, mouse.y },
               1.5f, accent);
    DrawLineEx(Vector2{ mouse.x, mouse.y - arm }, Vector2{ mouse.x, mouse.y - inner },
               1.5f, accent);
    DrawLineEx(Vector2{ mouse.x, mouse.y + inner }, Vector2{ mouse.x, mouse.y + arm },
               1.5f, accent);
    DrawCircleV(mouse, 1.5f, accent);
}

void ResetRunInventory(Game& game)
{
    game.weapons.ammo = {{ -1, 0, 0, 0 }};
    game.weapons.selected = WeaponSlot::Slingshot;
    game.weapons.fireCooldown = 0.0f;
    game.player.medPacks = 3;
}

/// Phase 5: number of live particles, for the HUD readout.
int CountActiveParticles(const Game& game)
{
    int count = 0;
    for (const Particle& particle : game.particles)
    {
        if (particle.active)
        {
            ++count;
        }
    }
    return count;
}

/// Single funnel for state changes: keeps stateTime meaningful for the INIT
/// boot screen and the end-of-run screens.
void EnterState(Game& game, GameState next)
{
    game.state = next;
    game.stateTime = 0.0f;
}

// -----------------------------------------------------------------------------
//  Level lifecycle
// -----------------------------------------------------------------------------

/**
 * @brief Builds the current level from scratch and drops the player into it.
 *
 * Phase 5 hardening: every transient pool is reset here, in one place, so a
 * transition can never leave bullets, particles or enemies behind:
 *   1. regenerate the grid + rooms (MapGenerator),
 *   2. clear the bullet pool,
 *   3. clear the particle pool,
 *   4. re-scatter the enemies (PopulateEnemies clears the pool itself),
 *   5. re-scatter the decor and rebuild the light list,
 *   6. put the player back on the spawn tile at full health.
 */
void StartLevel(Game& game)
{
    MapGenerator::GenerateMap(game);
    g_scene3D.RebuildLevel(game);

    CombatSystem::ResetPool(game);          // Phase 5: bullets
    EnemySystem::ResetDeathPieces(game);
    CombatSystem::ResetLevelPickups(game);
    ParticleSystem::ResetParticles(game);   // Phase 5: effects
    game.hitFeedbackTime = 0.0f;
    game.hitFeedbackHeadshot = false;
    EnemySystem::PopulateEnemies(game);     // Phase 5: enemies (clears + respawns)
    CombatSystem::PopulateWeaponPickups(game);

    PropSystem::Populate(game);
    LightingSystem::PopulateLevelLights(game);

    game.player.pos      = game.playerSpawn;
    game.player.hp       = game.player.maxHp;
    game.player.hurtCooldown = 0.0f;    // Phase 5 fix: clear the immunity window
    game.playerFacing    = 1;
    game.playerAnimPhase = 0.0f;

    game.hoveredCellX = -1;
    game.hoveredCellY = -1;

    CameraSystem::Snap(game, GetScreenWidth(), GetScreenHeight());
    EnterState(game, GameState::PLAYING);
}

/**
 * @brief Phase 5: the player touched the exit of the current level.
 *
 * The last level does not generate another one - crossing it wins the run.
 */
void AdvanceLevel(Game& game)
{
    if (game.currentLevel >= kTotalLevels)
    {
        EnterState(game, GameState::VICTORY);
        return;
    }

    ++game.currentLevel;
    StartLevel(game);   // regenerates the map and resets every pool
}

// -----------------------------------------------------------------------------
//  State updates
// -----------------------------------------------------------------------------

void UpdatePlaying(Game& game, float deltaTime)
{
    // Camera, aim picking and drawing share one immutable frame snapshot.
    if (game.renderBackend == RenderBackend::World3D)
    {
        game.aim = CameraSystem::PickGround(game, game.input.mouse);
    }
    else
    {
        const Vector2 mouseWorld = GetScreenToWorld2D(game.input.mouse, game.camera);
        const Vector2 ground = ScreenToCartesian(mouseWorld);
        game.aim.valid = true;
        game.aim.point = Vector3{ ground.x, 0.0f, ground.y };
        game.aim.groundPoint = game.aim.point;
        game.aim.enemyIndex = -1;
        game.aim.pickedHead = false;
    }

    PlayerSystem::UpdatePlayer(game, deltaTime);
    CombatSystem::UpdateCombat(game, deltaTime);
    EnemySystem::UpdateEnemies(game, deltaTime);            // Phase 5: enemies tick
    ParticleSystem::UpdateParticles(game, deltaTime);       // Phase 5: effects tick

    // Mouse hover readout for the debug HUD.
    const Vector2 hovered{ game.aim.groundPoint.x, game.aim.groundPoint.z };
    int hoveredX = -1;
    int hoveredY = -1;
    World3D::CellAt(hovered, hoveredX, hoveredY);

    if (hoveredX >= 0 && hoveredX < game.mapWidth &&
        hoveredY >= 0 && hoveredY < game.mapHeight)
    {
        game.hoveredCellX = hoveredX;
        game.hoveredCellY = hoveredY;
    }
    else
    {
        game.hoveredCellX = -1;
        game.hoveredCellY = -1;
    }

    // --- Loss condition (Phase 5) --------------------------------------------
    if (game.player.hp <= 0)
    {
        game.player.hp = 0;
        EnterState(game, GameState::GAME_OVER);
        return;
    }

    // --- Win condition / level transition (Phase 5) --------------------------
    const Vector2 exitDelta{ game.player.pos.x - game.levelExit.x,
                             game.player.pos.y - game.levelExit.y };

    if (VectorLength(exitDelta) < kExitRadius)
    {
        AdvanceLevel(game);
        return;
    }

    // --- Debug / QA shortcuts ------------------------------------------------
    if (game.input.rebuildPressed)
    {
        StartLevel(game);                       // rebuild the current level
    }
    else if (game.input.previewGameOverPressed)
    {
        EnterState(game, GameState::GAME_OVER); // preview the loss screen
    }
    else if (game.input.previewVictoryPressed)
    {
        EnterState(game, GameState::VICTORY);   // preview the win screen
    }
    else if (game.input.menuPressed)
    {
        EnterState(game, GameState::MAIN_MENU);
    }
}

// -----------------------------------------------------------------------------
//  Scene submission (PLAYING and the frozen backdrops of the end screens)
// -----------------------------------------------------------------------------

/// Phase 5: the exit tile is otherwise invisible, so a pulsing beacon is queued
/// at its isometric Y - the same sort key the map and the entities use - which
/// turns "walk here" into something the player can actually aim for.
void SubmitExitBeacon(const Game& game)
{
    Renderer& renderer = const_cast<Game&>(game).renderer;

    const Vector2 screen = CartesianToScreen(game.levelExit);
    const float   depth  = IsometricMath::CartesianToIsometric(game.levelExit).y + 0.01f;
    const float   pulse  = 0.5f + 0.5f * std::sin(game.stateTime * 3.0f);

    renderer.Submit(depth, [screen, pulse]()
    {
        DrawEllipse(static_cast<int>(screen.x), static_cast<int>(screen.y),
                    kTileWidth * 0.30f, kTileHeight * 0.30f, Fade(kColorExit, 0.16f + 0.10f * pulse));

        const IsoDiamond ring = MakeDiamond(screen, kTileWidth * 0.42f, kTileHeight * 0.42f);
        DrawDiamondOutline(ring, 2.0f, Fade(kColorExit, 0.35f + 0.45f * pulse));

        DrawDiamond(MakeDiamond(screen, 9.0f, 4.5f), Fade(kColorExit, 0.45f + 0.35f * pulse));

        DrawLineEx(Vector2{ screen.x, screen.y - 6.0f },
                   Vector2{ screen.x, screen.y - 44.0f - 14.0f * pulse },
                   2.0f, Fade(kColorExit, 0.20f + 0.30f * pulse));
    });
}

/**
 * @brief Pushes the whole world into the Y-sort queue.
 *
 * The camera is activated by the caller (the frame is wrapped in the RenderTexture
 * when the lighting pipeline is live) - this function only submits, it never
 * opens or closes a frame.
 */
void DrawPlayingScene(const Game& game, const LightingSystem& lighting)
{
    (void)lighting;   // accepted for call-site symmetry; the queue does the work

    // Same rationale as MapRenderer::RenderMap(): the scene is read only, only
    // the shared Y-sort queue (a per-frame scratch buffer) is written through.
    Renderer& renderer = const_cast<Game&>(game).renderer;

    renderer.BeginFrame();

    MapRenderer::RenderMap(game);
    PropSystem::Render(game);
    SubmitExitBeacon(game);              // Phase 5: the level goal, now visible
    CombatSystem::RenderWeaponPickups(game);
    CombatSystem::RenderBullets(game);
    EnemySystem::RenderEnemies(game);    // Phase 4: enemies sorted with the world
    PlayerSystem::RenderPlayer(game);
    EnemySystem::RenderDeathPieces(game);
    ParticleSystem::RenderParticles(game); // Phase 5: effects on top of their tile

    renderer.Flush();
}

void DrawDiagnostic3D(const Game& game)
{
    const int minX = std::max(0, static_cast<int>(std::floor(game.player.pos.x - 25.0f)));
    const int maxX = std::min(game.mapWidth - 1, static_cast<int>(std::ceil(game.player.pos.x + 25.0f)));
    const int minY = std::max(0, static_cast<int>(std::floor(game.player.pos.y - 25.0f)));
    const int maxY = std::min(game.mapHeight - 1, static_cast<int>(std::ceil(game.player.pos.y + 25.0f)));

    BeginMode3D(game.camera3D);
    if (g_scene3D.IsAvailable())
    {
        g_scene3D.DrawMap();
    }
    else
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const int tile = TileAt(game, x, y);
                const Vector3 center{ static_cast<float>(x) + 0.5f,
                                      -0.035f,
                                      static_cast<float>(y) + 0.5f };
                if (tile == kTileFloor)
                {
                    const Color floor = ((x + y) % 2 == 0)
                        ? Color{ 62, 68, 82, 255 }
                        : Color{ 55, 60, 73, 255 };
                    DrawCubeV(center, Vector3{ 0.98f, 0.07f, 0.98f }, floor);
                }
                else if (tile == kTileWall)
                {
                    DrawCubeV(Vector3{ center.x, World3D::kInitialWallHeight * 0.5f, center.z },
                              Vector3{ 1.0f, World3D::kInitialWallHeight, 1.0f },
                              Color{ 142, 151, 171, 255 });
                }
            }
        }
    }

    if (g_proceduralScene3D.IsAvailable())
    {
        g_proceduralScene3D.Draw(game);
    }
    else
    {
        for (const Enemy& enemy : game.enemies)
        {
            if (enemy.active)
            {
                DrawCubeV(Vector3{ enemy.pos.x, 0.6f, enemy.pos.y },
                          Vector3{ 0.42f, 1.2f, 0.42f }, Color{ 204, 56, 64, 255 });
            }
        }
        DrawCubeV(Vector3{ game.player.pos.x, 0.6f, game.player.pos.y },
                  Vector3{ 0.45f, 1.2f, 0.45f }, Color{ 72, 145, 242, 255 });
        DrawCubeV(Vector3{ game.levelExit.x, 0.35f, game.levelExit.y },
                  Vector3{ 0.12f, 0.7f, 0.12f }, kColorExit);
    }

    const Vector3 axisOrigin{ game.player.pos.x, 0.02f, game.player.pos.y };
    DrawLine3D(axisOrigin, Vector3{ axisOrigin.x + 1.0f, axisOrigin.y, axisOrigin.z },
               Color{ 255, 80, 80, 255 });
    DrawLine3D(axisOrigin, Vector3{ axisOrigin.x, axisOrigin.y + 1.0f, axisOrigin.z },
               Color{ 96, 220, 255, 255 });
    DrawLine3D(axisOrigin, Vector3{ axisOrigin.x, axisOrigin.y, axisOrigin.z + 1.0f },
               Color{ 104, 255, 140, 255 });
    EndMode3D();
}

void DrawPlayingHud(const Game& game, const LightingSystem& lighting);

void DrawWorld3DFrame(const Game& game, const LightingSystem& lighting)
{
    DrawDiagnostic3D(game);
    DrawPlayingHud(game, lighting);
}

// -----------------------------------------------------------------------------
//  Screens
// -----------------------------------------------------------------------------

void DrawMainMenu(const Game& game)
{
    const int centerX = GetScreenWidth() / 2;
    const int titleY  = GetScreenHeight() / 2 - 120;

    const char* title    = "BREAKOUT";
    const char* subtitle = "Phase 5 - Game Loop & Polish";

    // Phase 5: a slow bob keeps the title alive without any asset.
    const float bob = std::sin(game.stateTime * 1.6f) * 4.0f;

    DrawCenteredText(title, centerX, static_cast<int>(titleY + bob), 72, kColorAccent);
    DrawCenteredText(subtitle, centerX, titleY + 78, 20, kColorTextDim);

    DrawCenteredText("W/A/S/D - move up / left / down / right", centerX, titleY + 150, 20, kColorText);
    DrawCenteredText("Mouse aim and fire   |   1-4 switch weapons", centerX, titleY + 180, 20, kColorText);
    DrawCenteredText("5 - medkit restores 40 HP (3 per run)", centerX, titleY + 210, 20, kColorText);
    DrawCenteredText("Reach the cyan beacon to clear the level", centerX, titleY + 250, 20, kColorText);
    DrawCenteredText("Survive all 10 levels to win", centerX, titleY + 280, 20, kColorTextDim);

    // Blinking prompt.
    if (static_cast<int>(game.stateTime * 2.0f) % 2 == 0)
    {
        DrawCenteredText("Press ENTER or SPACE to start", centerX, titleY + 320, 24, kColorAccent);
    }
}

void DrawEndScreen(const Game& game, const LightingSystem& lighting, bool victory)
{
    if (game.renderBackend == RenderBackend::World3D)
        DrawDiagnostic3D(game);
    else
        DrawPlayingScene(game, lighting);

    const int centerX = GetScreenWidth() / 2;
    const int centerY = GetScreenHeight() / 2;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), kColorScrim);

    const char* headline = victory ? "VICTORY" : "GAME OVER";
    const Color accent   = victory ? kColorHealthHigh : kColorHealthLow;

    DrawCenteredText(headline, centerX, centerY - 110, 64, accent);

    const char* detail = victory
        ? TextFormat("All %d levels cleared", kTotalLevels)
        : TextFormat("You fell on level %d of %d", game.currentLevel, kTotalLevels);

    DrawCenteredText(detail, centerX, centerY - 24, 26, kColorText);

    DrawCenteredText(TextFormat("Enemies remaining: %d   Particles alive: %d",
                                game.enemyCount, CountActiveParticles(game)),
                     centerX, centerY + 14, 18, kColorTextDim);

    if (!victory)
    {
        DrawCenteredText("Tip: keep moving - enemies only chase what they can see",
                         centerX, centerY + 46, 18, kColorTextDim);
    }

    if (static_cast<int>(game.stateTime * 2.0f) % 2 == 0)
    {
        DrawCenteredText("Press ENTER or SPACE for the main menu", centerX, centerY + 110, 24, kColorText);
    }
}

// -----------------------------------------------------------------------------
//  HUD
// -----------------------------------------------------------------------------

/// Phase 5: the player health bar - backdrop, frame, scaled fill and numeric
/// readout. The fill width is driven by hp / maxHp, so it degrades gracefully
/// for any maxHp the design picks later.
void DrawHealthBar(const Game& game, int x, int y)
{
    constexpr int kBarWidth  = 260;
    constexpr int kBarHeight = 18;

    const float ratio = (game.player.maxHp > 0)
        ? ClampFloat(static_cast<float>(game.player.hp) / static_cast<float>(game.player.maxHp), 0.0f, 1.0f)
        : 0.0f;

    DrawRectangle(x - 3, y - 3, kBarWidth + 6, kBarHeight + 6, Fade(kColorBackground, 0.9f));
    DrawRectangleLines(x - 3, y - 3, kBarWidth + 6, kBarHeight + 6, Fade(kColorTextDim, 0.85f));
    DrawRectangle(x, y, kBarWidth, kBarHeight, kColorHealthBack);

    Color fill = kColorHealthHigh;
    if (ratio < 0.35f)
    {
        fill = kColorHealthLow;
    }
    else if (ratio < 0.65f)
    {
        fill = kColorHealthMid;
    }

    const int filled = static_cast<int>(static_cast<float>(kBarWidth) * ratio);
    if (filled > 0)
    {
        DrawRectangle(x, y, filled, kBarHeight, fill);
        DrawRectangle(x, y, filled, kBarHeight / 3, Fade(WHITE, 0.20f));   // glossy top
    }

    DrawText(TextFormat("HP %d / %d", game.player.hp, game.player.maxHp),
             x, y + kBarHeight + 6, 16, kColorText);
}

void DrawPlayingHud(const Game& game, const LightingSystem& lighting)
{
    // ---- top panel ----------------------------------------------------------
    DrawRectangle(0, 0, 720, 76, Fade(kColorBackground, 0.82f));
    DrawLine(0, 76, 720, 76, Fade(kColorAccent, 0.45f));

    DrawText(kWindowTitle, 18, 14, 20, kColorText);
    DrawText(TextFormat("state %s   level %d/%d   map %dx%d   %s",
                        StateName(game.state), game.currentLevel, kTotalLevels,
                        game.mapWidth, game.mapHeight,
                        game.renderBackend == RenderBackend::World3D ? "3D" : "2D"),
             18, 44, 18, kColorAccent);

    // ---- player readout -----------------------------------------------------
    DrawText(TextFormat("player  x %.2f  y %.2f  facing %s",
                        game.player.pos.x, game.player.pos.y,
                        (game.playerFacing < 0 ? "left" : "right")),
             736, 14, 18, kColorTextDim);
    DrawText(TextFormat("fps %d", GetFPS()), 736, 40, 18, kColorTextDim);

    // ---- Phase 5: health bar ------------------------------------------------
    DrawHealthBar(game, 18, 96);
    DrawText(TextFormat("MEDKIT x%d  [5]", game.player.medPacks),
             300, 98, 18, kColorAccent);

    // ---- system readout -----------------------------------------------------
    const bool lit = lighting.IsAvailable();

    DrawText(TextFormat("enemies %d", game.enemyCount), 736, 66, 18, kColorTextDim);
    DrawText(TextFormat("bullets %d/%d", CombatSystem::ActiveCount(game), kMaxBullets),
             736, 92, 18, kColorTextDim);
    DrawText(TextFormat("particles %d/%d", CountActiveParticles(game), kMaxParticles),
             736, 118, 18, kColorTextDim);
    DrawText(TextFormat("lights %d/%d  shader %s",
                        static_cast<int>(game.activeLights.size()), kMaxShaderLights,
                        (lit ? "on" : "off")),
             736, 144, 18, kColorTextDim);

    const int selected = static_cast<int>(game.weapons.selected);
    const char* slotNames[] = { "SLINGSHOT", "SHOTGUN", "MARKSMAN", "GATLING" };
    const int slotX = 18;
    const int slotY = GetScreenHeight() - 94;
    for (int i = 0; i < 4; ++i)
    {
        const int x = slotX + i * 174;
        const bool active = i == selected;
        const Color edge = active ? kColorAccent : Fade(kColorTextDim, 0.65f);
        const char* ammo = (i == 0) ? "INF" :
            TextFormat("%d", game.weapons.ammo[static_cast<std::size_t>(i)]);
        DrawRectangle(x, slotY, 162, 30, Fade(kColorBackground, 0.90f));
        DrawRectangleLines(x, slotY, 162, 30, edge);
        DrawText(TextFormat("%d  %s  %s", i + 1, slotNames[i], ammo),
                 x + 8, slotY + 8, 14, active ? kColorAccent : kColorTextDim);
    }

    // ---- hover readout + controls ------------------------------------------
    if (game.hoveredCellX >= 0)
    {
        DrawText(TextFormat("hover  cell (%d, %d)  %s",
                            game.hoveredCellX, game.hoveredCellY,
                            TileName(TileAt(game, game.hoveredCellX, game.hoveredCellY))),
                 18, GetScreenHeight() - 58, 20, kColorTextDim);
    }

    DrawText("WASD move  |  Mouse aim/fire  |  1-4 weapons  |  5 medkit +40 HP  |  Reach beacon  |  R rebuild  ESC menu",
             18, GetScreenHeight() - 30, 16, kColorTextDim);
}

// -----------------------------------------------------------------------------
//  Frame
// -----------------------------------------------------------------------------

/// Runs one fully lit PLAYING frame: world -> RenderTexture -> (lighting.fs) ->
/// screen, then the HUD on top. Mirrors the LightingSystem.hpp pipeline.
void DrawLitPlayingFrame(const Game& game)
{
    if (game.renderBackend == RenderBackend::World3D)
    {
        DrawWorld3DFrame(game, g_lighting);
        return;
    }

    // Phase 5 fix: the world MUST be rendered into the lighting render target
    // first. Without this BeginScene() the scene was drawn straight into the
    // default framebuffer and then immediately painted over by the still-black
    // RenderTexture presented at the end of EndScene() - the "first map is all
    // black" bug, since the presented texture never received the scene.
    g_lighting.BeginScene();

    BeginMode2D(game.camera);
        DrawPlayingScene(game, g_lighting);
    EndMode2D();

    g_lighting.EndScene(game, game.camera);
    DrawPlayingHud(game, g_lighting);
}

}   // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--renderer=3d") == 0)
            g_game.renderBackend = RenderBackend::World3D;
        else if (std::strcmp(argv[i], "--renderer=2d") == 0)
            g_game.renderBackend = RenderBackend::Legacy2D;
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, kWindowTitle);
    SetExitKey(KEY_NULL);
    HideCursor();
    SetTargetFPS(60);

    // Phase 3.F: shader + scene target. Degrades to an unlit RenderTexture
    // (never crashes) when assets/shaders/lighting.fs is missing.
    g_lighting.Initialize();
    g_scene3D.Initialize();
    g_proceduralScene3D.Initialize();

    PlayerSystem::LoadAssets(g_game);
    EnemySystem::LoadAssets(g_game);
    CombatSystem::LoadAssets(g_game);

    while (g_game.running && !WindowShouldClose())
    {
        // Tick time even while paused: END stands for an exclusive step flag,
        // TICK for "the world must keep being simulated".
        const float deltaTime = std::min(GetFrameTime(), kMaxFrameTime);
        g_game.input = CaptureInputFrame();
        if (g_game.input.toggleRendererPressed)
        {
            g_game.renderBackend = g_game.renderBackend == RenderBackend::Legacy2D
                ? RenderBackend::World3D : RenderBackend::Legacy2D;
        }
        if (g_game.camera3DReady)
        {
            CameraSystem::Update(g_game, deltaTime, GetScreenWidth(), GetScreenHeight());
        }
        g_game.stateTime += deltaTime;

        // ---- state:specific logic (before drawing) --------------------------
        switch (g_game.state)
        {
            case GameState::INIT:
            {
                if (g_game.stateTime >= kBootScreenTime)
                {
                    g_game.currentLevel = 1;
                    ResetRunInventory(g_game);
                    StartLevel(g_game);
                }
                break;
            }

            case GameState::MAIN_MENU:
            {
                // Existing level shown behind the title; rebuild for level 1.
                if (g_game.input.startPressed)
                {
                    g_game.currentLevel = 1;
                    ResetRunInventory(g_game);
                    StartLevel(g_game);
                }
                break;
            }

            case GameState::PLAYING:
            {
                UpdatePlaying(g_game, deltaTime);
                break;
            }

            case GameState::GAME_OVER:
            case GameState::VICTORY:
            {
                if (g_game.input.startPressed)
                {
                    EnterState(g_game, GameState::MAIN_MENU);
                }
                break;
            }
        }

        // ---- rendering ------------------------------------------------------
        BeginDrawing();
        ClearBackground(kColorVoid);

        switch (g_game.state)
        {
            case GameState::INIT:
            {
                DrawText("Initializing...", 18, 18, 20, kColorTextDim);
                break;
            }

            case GameState::MAIN_MENU:
            {
                DrawMainMenu(g_game);
                break;
            }

            case GameState::PLAYING:
            {
                DrawLitPlayingFrame(g_game);
                break;
            }

            case GameState::GAME_OVER:
            {
                DrawEndScreen(g_game, g_lighting, false);
                break;
            }

            case GameState::VICTORY:
            {
                DrawEndScreen(g_game, g_lighting, true);
                break;
            }
        }

        DrawCrosshair(g_game);
        EndDrawing();
    }

    EnemySystem::UnloadAssets(g_game);
    CombatSystem::UnloadAssets(g_game);
    PlayerSystem::UnloadAssets(g_game);
    g_proceduralScene3D.Shutdown();
    g_scene3D.Shutdown();
    g_lighting.Shutdown();
    ShowCursor();
    CloseWindow();
    return 0;
}
