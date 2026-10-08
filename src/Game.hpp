#pragma once

#include "Renderer.hpp"

#include "raylib.h"

#include <array>
#include <cstddef>
#include <vector>

/**
 * @file Game.hpp
 * @brief The single mutable state container of the game.
 *
 * Everything that changes while the game runs lives in Game: the finite state
 * machine, the render queue, the camera, the transient input state, the
 * procedurally generated level (Phase 2), the player / bullet pool / dynamic
 * light list / decor layer (Phase 3), the enemy pool (Phase 4) and - new in
 * Phase 5 - the player hit points, the particle pool (1000 slots) and the level
 * progression constants that drive the win / loss conditions.
 *
 * The struct is deliberately a plain aggregate: systems such as MapGenerator,
 * MapRenderer, PlayerSystem, CombatSystem, EnemySystem, ParticleSystem,
 * PropSystem and LightingSystem all take it by reference and mutate the fields
 * they own. No hidden singletons, no globals, no ECS - the flat memory model of
 * Phase 1 and Phase 2 is preserved.
 *
 * @note The world is simulated in cartesian grid space (physics, collision,
 *       generation). The isometric projection is applied only when a position
 *       is turned into pixels, i.e. at submission time.
 */

// -----------------------------------------------------------------------------
//  Tile semantics - the alphabet of Game::gridMap
// -----------------------------------------------------------------------------
enum TileType : int
{
    kTileVoid  = 0,   ///< Empty space outside the level - nothing is drawn.
    kTileFloor = 1,   ///< Walkable floor (rooms + corridors).
    kTileWall  = 2    ///< Solid wall: any void cell touching a floor cell.
};

// -----------------------------------------------------------------------------
//  Isometric tile metrics, in pixels.
//  Shared by MapRenderer, the systems and the world<->screen helpers so that the
//  generated geometry, the camera target and every sprite agree on where a grid
//  cell lands on screen.
// -----------------------------------------------------------------------------
constexpr float kTileWidth  = 64.0f;   ///< Full width of one diamond.
constexpr float kTileHeight = 32.0f;   ///< Full height of one diamond (2:1).

/// Vertical extrusion applied to a wall tile, making it read as a cube.
constexpr float kWallHeight = kTileHeight;

/// Horizontal pixels travelled per cartesian tile; the unit lighting radii are
/// converted with (points on the screen x axis move half a tile width per cell).
constexpr float kPixelsPerTile = kTileWidth * 0.5f;

// -----------------------------------------------------------------------------
//  Phase 3 budgets
// -----------------------------------------------------------------------------
/// Fixed size bullet pool: no allocation during play (flat memory model).
constexpr int kMaxBullets = 500;

/// Maximum number of point lights handed to lighting.fs (mirrors MAX_LIGHTS in
/// assets/shaders/lighting.fs - keep the two in sync).
constexpr int kMaxShaderLights = 16;

// -----------------------------------------------------------------------------
//  Phase 5 budgets
// -----------------------------------------------------------------------------
/// Fixed size particle pool: no allocation during play (flat memory model).
constexpr int kMaxParticles = 1000;

/// Number of procedurally generated levels of the run; reaching the exit of
/// level kTotalLevels ends the game in VICTORY.
constexpr int kTotalLevels = 10;

// =============================================================================
//  Finite state machine
// =============================================================================
enum class GameState
{
    INIT,
    MAIN_MENU,
    PLAYING,
    GAME_OVER,
    VICTORY
};

/// Human readable state name, used by the debug HUD.
inline const char* StateName(GameState state)
{
    switch (state)
    {
        case GameState::INIT:      return "INIT";
        case GameState::MAIN_MENU: return "MAIN_MENU";
        case GameState::PLAYING:   return "PLAYING";
        case GameState::GAME_OVER: return "GAME_OVER";
        case GameState::VICTORY:   return "VICTORY";
    }
    return "UNKNOWN";
}

// =============================================================================
//  Phase 2 - level data
// =============================================================================

/**
 * @brief One generated room, exported by MapGenerator for the decor and the
 *        lighting pass (Phase 3.H / 3.F consume the same rectangles).
 *
 * The cell fields stay integral (they index gridMap), while the centre is kept
 * as float because it is used directly as a cartesian position: that avoids a
 * narrowing conversion at every Vector2{...} construction site.
 */
struct RoomRect
{
    int   x       = 0;      ///< left column (inclusive)
    int   y       = 0;      ///< top row (inclusive)
    int   width   = 0;      ///< cells along x
    int   height  = 0;      ///< cells along y
    float centerX = 0.0f;   ///< x of the room centre, always inside the room
    float centerY = 0.0f;   ///< y of the room centre, always inside the room
};

// =============================================================================
//  Phase 3.A - player, bullets, lights, decor
// =============================================================================

/// The one controllable character. Position lives in cartesian tile units.
struct Player
{
    Vector2 pos{ 3.0f, 3.0f };   ///< cartesian grid position (tile units)
    float   speed  = 4.0f;       ///< tiles per second
    float   radius = 0.3f;       ///< AABB half extent, in cartesian tile units

    // --- Phase 5: survivability ---------------------------------------------
    int hp    = 100;             ///< current hit points; <= 0 -> GAME_OVER
    int maxHp = 100;             ///< full-health ceiling, also the HUD bar scale

    /// Seconds of contact-damage immunity left (Phase 5 fix). Refilled by
    /// EnemySystem on every hit, so a body parked on the player drains the bar
    /// at a readable, reactable pace instead of once per rendered frame.
    float hurtCooldown = 0.0f;
};

/// One slot of the fixed size bullet pool. Inactive slots are dormant memory.
struct Bullet
{
    bool    active = false;      ///< false -> slot is free for reuse
    Vector2 pos{};               ///< cartesian grid position (tile units)
    Vector2 vel{};               ///< cartesian velocity in tiles per second
    float   speed  = 12.0f;      ///< muzzle speed, tiles per second
    float   radius = 0.1f;       ///< collision probe radius, tile units
};

/// A point light authored in cartesian tile units; consumed by LightingSystem.
struct Light
{
    Vector2 pos{};                                  ///< cartesian tile units
    float   radius    = 8.0f;                       ///< falloff radius in tiles
    Color   color     = Color{ 255, 255, 255, 255 };///< tint (zone colour coded)
    float   intensity = 1.0f;                       ///< linear brightness scale
};

/// The modular decor vocabulary (Phase 3.H, Task 6.4). Pure visuals - a prop
/// never enters the collision pass and never touches gridMap.
enum class PropKind : int
{
    PartitionScreen,   ///< screen with a blue curtain
    WireFence,         ///< metal mesh fence
    PartsTable,        ///< table with scattered parts
    Locker,            ///< bright orange storage locker
    VendingMachine,    ///< neon green vending machine
    ScreenPanel        ///< bright blue terminal screen
};

/// One scattered decor object, Y-sorted with everything else.
struct Prop
{
    PropKind kind    = PropKind::PartitionScreen;
    Vector2  pos{};              ///< cartesian tile units (cell centre)
    float    scale   = 1.0f;     ///< 0.92 .. 1.08 size jitter
    float    variant = 0.0f;     ///< 0..1 deterministic variation seed
};

// =============================================================================
//  Phase 4 - enemies
// =============================================================================

enum class EnemyState : int
{
    IDLE,   ///< wandering or patrolling the room
    CHASE   ///< player sighted, moving to intercept
};

/// An enemy unit. Position lives in cartesian tile units.
struct Enemy
{
    bool         active = false;   ///< false -> slot is dormant
    Vector2      pos{};            ///< cartesian grid position (tile units)
    float        hp       = 10.0f; ///< hit points (damage scale with difficulty)
    float        speed    = 1.0f;  ///< tiles per second at difficulty 1.0x
    float        radius   = 0.3f;  ///< AABB half extent, same as player
    EnemyState   state    = EnemyState::IDLE;
};

// =============================================================================
//  Phase 5 - particles
// =============================================================================

/**
 * @brief One slot of the fixed size particle pool.
 *
 * A particle is a purely visual, short lived billboard: position and velocity
 * are expressed in cartesian tile units, exactly like every other entity, so
 * the isometric projection is applied once, at submission time, through
 * CartesianToScreen(). Inactive slots are dormant memory - the pool never
 * allocates while the game runs.
 */
struct Particle
{
    bool    active  = false;                          ///< false -> slot is free
    Vector2 pos{};                                    ///< cartesian tile units
    Vector2 vel{};                                    ///< tiles per second
    float   life    = 0.0f;                           ///< seconds left
    float   maxLife = 0.3f;                           ///< lifetime it spawned with
    Color   color   = Color{ 255, 255, 255, 255 };    ///< tint, faded by life
};

// =============================================================================
//  Runtime context - all mutable game state lives here
// =============================================================================
struct Game
{
    // --- Phase 1: engine foundation -----------------------------------------
    GameState state = GameState::INIT;
    Renderer  renderer{};
    Camera2D  camera{};

    int     hoveredCellX = -1;              ///< mouse-picked cell, grid space
    int     hoveredCellY = -1;

    float stateTime = 0.0f;                 ///< seconds spent in the current state
    bool  running   = true;                 ///< false -> leave the main loop

    // --- Phase 2: level data -------------------------------------------------
    int currentLevel = 1;                   ///< 1 .. kTotalLevels, driven by the level manager

    /// Row-major Cartesian grid, mapWidth * mapHeight entries of TileType.
    /// Invariant after MapGenerator::GenerateMap(): size() == mapWidth * mapHeight.
    std::vector<int> gridMap;

    int mapWidth  = 0;
    int mapHeight = 0;

    Vector2 playerSpawn{};                  ///< grid position of the left-most room centre
    Vector2 levelExit{};                    ///< grid position of the right-most room centre

    /// Rooms exported by MapGenerator; PropSystem scatters decor inside them and
    /// LightingSystem places one ceiling light per room.
    std::vector<RoomRect> rooms;

    // --- Phase 3.A: player, combat, lighting, decor --------------------------
    Player player{};

    /// Fixed size pool: no heap traffic while shooting (kMaxBullets slots).
    std::array<Bullet, kMaxBullets> bullets{};

    /// Point lights of the current level (room lights + prop accents).
    std::vector<Light> activeLights;

    /// Non collidable decor objects of the current level.
    std::vector<Prop> props;

    // --- Phase 3.E: optional sprite sheet ------------------------------------
    /// assets/sprites/player.png when present; id == 0 selects the procedural
    /// polygon fallback drawn by PlayerSystem.
    Texture2D playerTexture{};

    float playerAnimPhase = 0.0f;           ///< walk cycle timer (seconds)
    int   playerFacing    = 1;              ///< -1 == facing left, +1 == facing right

    // --- Phase 4: enemy pool -------------------------------------------------
    /// Fixed size enemy pool (200 slots, flat memory model).
    std::array<Enemy, 200> enemies{};

    int  enemyCount = 0;                    ///< active enemies in the current level

    // --- Phase 5: effects ----------------------------------------------------
    /// Fixed size particle pool (kMaxParticles slots, flat memory model).
    /// Filled by ParticleSystem::SpawnExplosion(), drained by
    /// ParticleSystem::UpdateParticles() and reset on every level start.
    std::array<Particle, kMaxParticles> particles{};
};
