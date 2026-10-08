#include "Game.hpp"
#include "GridAlgorithms.hpp"
#include "GameplayRules.hpp"
#include "MapGenerator.hpp"
#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <vector>

namespace
{
int g_checks = 0;

void Check(bool condition, const char* expression, int line)
{
    ++g_checks;
    if (!condition)
    {
        std::cerr << "GameplayParityTests:" << line << ": check failed: "
                  << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

bool Near(float a, float b)
{
    return std::fabs(a - b) < 1e-5f;
}

bool ConnectedFloors(const Game& game)
{
    std::vector<bool> visited(game.gridMap.size(), false);
    std::queue<Vector2> pending;
    const int startX = static_cast<int>(game.playerSpawn.x);
    const int startY = static_cast<int>(game.playerSpawn.y);
    const std::size_t start = static_cast<std::size_t>(startY) *
                              static_cast<std::size_t>(game.mapWidth) +
                              static_cast<std::size_t>(startX);
    if (TileAt(game, startX, startY) != kTileFloor) return false;

    pending.push(Vector2{ static_cast<float>(startX), static_cast<float>(startY) });
    visited[start] = true;
    constexpr int offsets[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    while (!pending.empty())
    {
        const Vector2 current = pending.front();
        pending.pop();
        const int x = static_cast<int>(current.x);
        const int y = static_cast<int>(current.y);
        for (const auto& offset : offsets)
        {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (TileAt(game, nx, ny) != kTileFloor) continue;
            const std::size_t index = static_cast<std::size_t>(ny) *
                                      static_cast<std::size_t>(game.mapWidth) +
                                      static_cast<std::size_t>(nx);
            if (visited[index]) continue;
            visited[index] = true;
            pending.push(Vector2{ static_cast<float>(nx), static_cast<float>(ny) });
        }
    }

    for (std::size_t i = 0; i < game.gridMap.size(); ++i)
        if (game.gridMap[i] == kTileFloor && !visited[i]) return false;
    return true;
}

void CheckGeneration(int level)
{
    Game first;
    first.currentLevel = level;
    MapGenerator::GenerateMap(first);

    Game second;
    second.currentLevel = level;
    MapGenerator::GenerateMap(second);

    const int expectedSize = 30 + 70 * (level - 1) / 9;
    CHECK(first.mapWidth == expectedSize);
    CHECK(first.mapHeight == expectedSize);
    CHECK(first.gridMap.size() == static_cast<std::size_t>(expectedSize * expectedSize));
    CHECK(first.gridMap == second.gridMap);
    CHECK(first.playerSpawn.x >= 0.0f && first.playerSpawn.x < first.mapWidth);
    CHECK(first.playerSpawn.y >= 0.0f && first.playerSpawn.y < first.mapHeight);
    CHECK(first.levelExit.x >= 0.0f && first.levelExit.x < first.mapWidth);
    CHECK(first.levelExit.y >= 0.0f && first.levelExit.y < first.mapHeight);
    CHECK(TileAt(first, static_cast<int>(first.playerSpawn.x),
                 static_cast<int>(first.playerSpawn.y)) == kTileFloor);
    CHECK(TileAt(first, static_cast<int>(first.levelExit.x),
                 static_cast<int>(first.levelExit.y)) == kTileFloor);
    CHECK(IsWalkableFootprint(first, first.playerSpawn, 0.3f));
    CHECK(ConnectedFloors(first));
    CHECK(first.enemies.size() == 200);
    CHECK(first.weaponPickups.size() == 3);
}
}

int main()
{
    for (int level = 1; level <= kTotalLevels; ++level) CheckGeneration(level);

    const std::vector<int> horizontal{
        kTileFloor, kTileFloor, kTileWall, kTileFloor, kTileFloor
    };
    CHECK(!GridAlgorithms::HasFloorLineOfSight(horizontal, 5, 1, 0, 0, 4, 0));
    CHECK(GridAlgorithms::HasFloorLineOfSight(horizontal, 5, 1, 2, 0, 4, 0));

    const std::vector<int> vertical{
        kTileFloor, kTileFloor, kTileWall, kTileFloor, kTileFloor
    };
    CHECK(!GridAlgorithms::HasFloorLineOfSight(vertical, 1, 5, 0, 0, 0, 4));
    CHECK(GridAlgorithms::HasFloorLineOfSight(vertical, 1, 5, 0, 2, 0, 4));

    const std::vector<int> diagonal{
        kTileFloor, kTileVoid, kTileVoid,
        kTileVoid, kTileWall,  kTileVoid,
        kTileVoid, kTileVoid,  kTileFloor
    };
    CHECK(!GridAlgorithms::HasFloorLineOfSight(diagonal, 3, 3, 0, 0, 2, 2));
    CHECK(GridAlgorithms::HasFloorLineOfSight(diagonal, 3, 3, 0, 0, 1, 1));

    int hp = 60;
    int medPacks = 3;
    CHECK(GameplayRules::TryUseMedPack(hp, 100, medPacks));
    CHECK(hp == 100 && medPacks == 2);
    hp = 80;
    CHECK(GameplayRules::TryUseMedPack(hp, 100, medPacks));
    CHECK(hp == 100 && medPacks == 1);
    CHECK(!GameplayRules::TryUseMedPack(hp, 100, medPacks));
    CHECK(medPacks == 1);
    hp = 40;
    medPacks = 0;
    CHECK(!GameplayRules::TryUseMedPack(hp, 100, medPacks));
    CHECK(hp == 40 && medPacks == 0);

    Game corridor;
    corridor.mapWidth = 3;
    corridor.mapHeight = 3;
    corridor.gridMap = {
        kTileVoid,  kTileWall, kTileVoid,
        kTileFloor, kTileFloor, kTileFloor,
        kTileVoid,  kTileWall, kTileVoid
    };
    Vector2 actor{ 1.5f, 1.5f };
    CHECK(IsWalkableFootprint(corridor, actor, 0.3f));
    MoveGroundWithWallSlide(corridor, actor, Vector2{ 0.2f, 0.0f }, 0.3f);
    CHECK(actor.x > 1.5f && actor.x < 2.0f);
    MoveGroundWithWallSlide(corridor, actor, Vector2{ 0.0f, 0.4f }, 0.3f);
    CHECK(Near(actor.y, 1.5f));
    MoveGroundWithWallSlide(corridor, actor, Vector2{ 0.0f, -0.4f }, 0.3f);
    CHECK(Near(actor.y, 1.5f));

    Game corner;
    corner.mapWidth = 3;
    corner.mapHeight = 3;
    corner.gridMap = {
        kTileVoid, kTileVoid,  kTileVoid,
        kTileVoid, kTileFloor, kTileFloor,
        kTileVoid, kTileFloor, kTileVoid
    };
    Vector2 cornerActor{ 1.5f, 1.5f };
    MoveGroundWithWallSlide(corner, cornerActor, Vector2{ 0.4f, 0.4f }, 0.3f);
    CHECK(cornerActor.x > 1.5f);
    CHECK(Near(cornerActor.y, 1.5f));

    std::cout << "GameplayParityTests passed " << g_checks << " checks\n";
    return EXIT_SUCCESS;
}
