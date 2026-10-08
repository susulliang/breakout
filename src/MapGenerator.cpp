#include "MapGenerator.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

// -----------------------------------------------------------------------------
//  Generation constants (file-local)
// -----------------------------------------------------------------------------
namespace {

// Difficulty curve: everything is a linear interpolation over levels 1 .. 10.
constexpr int kFirstLevel   = 1;
constexpr int kLastLevel    = 10;
constexpr int kMinMapSize   = 30;   ///< level  1: 30x30
constexpr int kMaxMapSize   = 100;  ///< level 10: 100x100
constexpr int kMinRoomCount = 3;    ///< level  1: 3 rooms
constexpr int kMaxRoomCount = 15;   ///< level 10: 15 rooms

constexpr int kRoomSizeMin     = 4;   ///< smallest room edge, in cells
constexpr int kRoomSizeMaxCap  = 16;  ///< largest room edge, in cells
constexpr int kRoomPadding     = 1;   ///< free cells kept between two rooms
constexpr int kRoomMargin      = 1;   ///< free cells kept along the map border
constexpr int kAttemptsPerRoom = 400; ///< rejection-sampling budget per room

/// Base seed. The level index is mixed in, which makes every level reproduce
/// the exact same layout on every run (handy for debugging and for verifying
/// spawn points). Change this constant to reshuffle all ten levels at once.
constexpr std::uint32_t kSeedBase = 0xB7EA6C0Du;
constexpr std::uint32_t kSeedStride = 2654435761u;   ///< Knuth multiplicative hash

/**
 * @brief Linear map of a level index onto [minValue, maxValue].
 *
 * LerpLevel(30, 100, 1) == 30, LerpLevel(30, 100, 10) == 100.
 */
int LerpLevel(int minValue, int maxValue, int level)
{
    const int clamped = std::clamp(level, kFirstLevel, kLastLevel);
    constexpr int kSteps = kLastLevel - kFirstLevel;          // == 9
    return minValue + (maxValue - minValue) * (clamped - kFirstLevel) / kSteps;
}

/// Writes a floor cell; out-of-bounds coordinates are silently ignored.
void CarveCell(std::vector<int>& map, int mapWidth, int mapHeight, int x, int y)
{
    if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) {
        return;
    }
    map[static_cast<std::size_t>(y) * mapWidth + x] = kTileFloor;
}

/**
 * @brief Carves a 1-cell-wide L-shaped corridor from (x1,y1) to (x2,y2).
 *
 * @param horizontalFirst true -> walk x first then y, false -> the other way.
 *        Randomising the elbow keeps the generated levels from all looking the
 *        same; both variants are valid L shapes over the same endpoints.
 */
void CarveCorridor(std::vector<int>& map, int mapWidth, int mapHeight,
                   int x1, int y1, int x2, int y2, bool horizontalFirst)
{
    const int stepX = (x2 >= x1) ? 1 : -1;
    const int stepY = (y2 >= y1) ? 1 : -1;

    if (horizontalFirst)
    {
        // (x1,y1) -> (x2,y1) -> (x2,y2)
        for (int x = x1;; x += stepX) {
            CarveCell(map, mapWidth, mapHeight, x, y1);
            if (x == x2) { break; }
        }
        for (int y = y1;; y += stepY) {
            CarveCell(map, mapWidth, mapHeight, x2, y);
            if (y == y2) { break; }
        }
    }
    else
    {
        // (x1,y1) -> (x1,y2) -> (x2,y2)
        for (int y = y1;; y += stepY) {
            CarveCell(map, mapWidth, mapHeight, x1, y);
            if (y == y2) { break; }
        }
        for (int x = x1;; x += stepX) {
            CarveCell(map, mapWidth, mapHeight, x, y2);
            if (x == x2) { break; }
        }
    }
}

/// Stamps a room's full rectangle as floor.
void FillRoom(std::vector<int>& map, int mapWidth, const RoomRect& room)
{
    for (int y = room.y; y < room.y + room.height; ++y)
    {
        for (int x = room.x; x < room.x + room.width; ++x)
        {
            map[static_cast<std::size_t>(y) * mapWidth + x] = kTileFloor;
        }
    }
}

/**
 * @brief Adds the wall shell.
 *
 * Reads @p source and writes into @p target, so the original floor layout is
 * untouched while the scan is running: any kTileVoid cell with a kTileFloor
 * neighbour (8-way, diagonals included) turns into kTileWall.
 */
void BuildWalls(const std::vector<int>& source, std::vector<int>& target,
                int mapWidth, int mapHeight)
{
    static const int kNeighbourOffsets[8][2] = {
        { -1, -1 }, {  0, -1 }, {  1, -1 },
        { -1,  0 },             {  1,  0 },
        { -1,  1 }, {  0,  1 }, {  1,  1 }
    };

    for (int y = 0; y < mapHeight; ++y)
    {
        for (int x = 0; x < mapWidth; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y) * mapWidth + x;
            if (source[index] != kTileVoid) {
                continue;
            }

            for (const auto& offset : kNeighbourOffsets)
            {
                const int nx = x + offset[0];
                const int ny = y + offset[1];
                if (nx < 0 || nx >= mapWidth || ny < 0 || ny >= mapHeight) {
                    continue;
                }
                if (source[static_cast<std::size_t>(ny) * mapWidth + nx] == kTileFloor) {
                    target[index] = kTileWall;
                    break;
                }
            }
        }
    }
}

} // namespace

// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------
void MapGenerator::GenerateMap(Game& game)
{
    // --- 1. Linear scaling from level 1 to level 10 --------------------------
    const int mapSize    = LerpLevel(kMinMapSize, kMaxMapSize, game.currentLevel);
    const int roomTarget = LerpLevel(kMinRoomCount, kMaxRoomCount, game.currentLevel);

    game.mapWidth  = mapSize;
    game.mapHeight = mapSize;
    game.gridMap.assign(static_cast<std::size_t>(mapSize) * static_cast<std::size_t>(mapSize),
                        kTileVoid);

    std::mt19937 rng(kSeedBase ^ (static_cast<std::uint32_t>(game.currentLevel) * kSeedStride));

    // --- 2. Room placement: rejection sampling, 1 cell of padding ------------
    // Rooms never exceed a quarter of the map, which keeps placement easy and
    // guarantees the random coordinate range below stays valid.
    const int roomSizeMax = std::max(kRoomSizeMin + 2, std::min(mapSize / 4, kRoomSizeMaxCap));

    std::uniform_int_distribution<int> sizeDistribution(kRoomSizeMin, roomSizeMax);

    std::vector<RoomRect> rooms;
    rooms.reserve(static_cast<std::size_t>(roomTarget));

    for (int attempt = 0;
         attempt < roomTarget * kAttemptsPerRoom && static_cast<int>(rooms.size()) < roomTarget;
         ++attempt)
    {
        const int width  = sizeDistribution(rng);
        const int height = sizeDistribution(rng);

        std::uniform_int_distribution<int> xDistribution(kRoomMargin, mapSize - kRoomMargin - width);
        std::uniform_int_distribution<int> yDistribution(kRoomMargin, mapSize - kRoomMargin - height);

        const int rx = xDistribution(rng);
        const int ry = yDistribution(rng);

        // Overlap test with kRoomPadding cells of breathing room on every side.
        bool overlaps = false;
        for (const RoomRect& other : rooms)
        {
            if (rx < other.x + other.width + kRoomPadding &&
                rx + width + kRoomPadding > other.x &&
                ry < other.y + other.height + kRoomPadding &&
                ry + height + kRoomPadding > other.y)
            {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            continue;
        }

        RoomRect room;
        room.x       = rx;
        room.y       = ry;
        room.width   = width;
        room.height  = height;
        room.centerX = static_cast<float>(rx + width / 2) + 0.5f;
        room.centerY = static_cast<float>(ry + height / 2) + 0.5f;
        rooms.push_back(room);
    }

    if (rooms.empty())
    {
        // Safety net (practically unreachable): one room in the middle.
        const int size = std::min(kRoomSizeMin + 2, mapSize - 2 * kRoomMargin);
        const int rx   = (mapSize - size) / 2;

        RoomRect room;
        room.x       = rx;
        room.y       = rx;
        room.width   = size;
        room.height  = size;
        room.centerX = static_cast<float>(rx + size / 2) + 0.5f;
        room.centerY = static_cast<float>(rx + size / 2) + 0.5f;
        rooms.push_back(room);
    }

    // --- 3. Stamp the rooms as floor ----------------------------------------
    for (const RoomRect& room : rooms) {
        FillRoom(game.gridMap, game.mapWidth, room);
    }

    // --- 4. Connect the room centres with L-shaped corridors -----------------
    // Consecutive rooms are always joined, so the resulting layout is a single
    // connected level no matter where the random placement put the rooms.
    std::bernoulli_distribution horizontalFirst(0.5);
    for (std::size_t i = 0; i + 1 < rooms.size(); ++i)
    {
        const int fromX = static_cast<int>(rooms[i].centerX);
        const int fromY = static_cast<int>(rooms[i].centerY);
        const int toX   = static_cast<int>(rooms[i + 1].centerX);
        const int toY   = static_cast<int>(rooms[i + 1].centerY);

        CarveCorridor(game.gridMap, game.mapWidth, game.mapHeight,
                      fromX, fromY, toX, toY, horizontalFirst(rng));
    }

    // --- 5. Wall shell -------------------------------------------------------
    std::vector<int> walled = game.gridMap;
    BuildWalls(game.gridMap, walled, game.mapWidth, game.mapHeight);
    game.gridMap.swap(walled);

    // --- 6. Spawn points -----------------------------------------------------
    // Player spawn: the room whose centre sits furthest left.
    // Level exit  : the room whose centre sits furthest right.
    const auto leftMost = std::min_element(rooms.begin(), rooms.end(),
        [](const RoomRect& a, const RoomRect& b) { return a.centerX < b.centerX; });
    const auto rightMost = std::max_element(rooms.begin(), rooms.end(),
        [](const RoomRect& a, const RoomRect& b) { return a.centerX < b.centerX; });

    game.playerSpawn = Vector2{ leftMost->centerX, leftMost->centerY };
    game.levelExit   = Vector2{ rightMost->centerX, rightMost->centerY };

    // --- 7. Export the rooms ------------------------------------------------
    // Phase 3 consumes this list twice: PropSystem scatters decor inside the
    // rooms and LightingSystem drops one ceiling light on every room centre.
    game.rooms = rooms;
}
