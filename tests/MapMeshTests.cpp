#include "MapMeshBuilder.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
int g_checks = 0;

void Check(bool condition, const char* expression, int line)
{
    ++g_checks;
    if (!condition)
    {
        std::cerr << "MapMeshTests:" << line << ": check failed: "
                  << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)
}

int main()
{
    const MapMeshBuilder::ChunkData block = MapMeshBuilder::BuildChunk(
        { kTileWall, kTileWall, kTileWall, kTileWall }, 2, 2, 0, 0, 0.816497f);
    CHECK(block.floors.positions.empty());
    CHECK(block.walls.positions.size() == 48);
    CHECK(block.walls.normals.size() == block.walls.positions.size());
    CHECK(block.walls.colors.size() == block.walls.positions.size());
    CHECK(block.walls.indices.size() == 72);
    for (const std::uint16_t index : block.walls.indices)
        CHECK(index < block.walls.positions.size());

    const MapMeshBuilder::ChunkData floor = MapMeshBuilder::BuildChunk(
        { kTileFloor }, 1, 1, 0, 0, 0.816497f);
    CHECK(floor.floors.positions.size() == 8);
    CHECK(floor.floors.indices.size() == 12);
    CHECK(floor.walls.positions.empty());

    std::vector<int> splitWalls(static_cast<std::size_t>(17 * 3), kTileVoid);
    splitWalls[static_cast<std::size_t>(1 * 17 + 15)] = kTileWall;
    splitWalls[static_cast<std::size_t>(1 * 17 + 16)] = kTileWall;
    const auto leftChunk = MapMeshBuilder::BuildChunk(
        splitWalls, 17, 3, 0, 0, 0.816497f);
    const auto rightChunk = MapMeshBuilder::BuildChunk(
        splitWalls, 17, 3, 16, 0, 0.816497f);
    CHECK(leftChunk.walls.positions.size() == 16);
    CHECK(rightChunk.walls.positions.size() == 16);

    const auto invalid = MapMeshBuilder::BuildChunk(
        { kTileFloor }, 2, 2, 0, 0, 0.816497f);
    CHECK(invalid.floors.positions.empty() && invalid.walls.positions.empty());

    std::cout << "MapMeshTests passed " << g_checks << " checks\n";
    return EXIT_SUCCESS;
}
