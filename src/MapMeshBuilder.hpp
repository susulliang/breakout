#pragma once

#include "Game.hpp"

#include <cstdint>
#include <vector>

namespace MapMeshBuilder
{
constexpr int kChunkSize = 16;

struct MeshData
{
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Color> colors;
    std::vector<Vector2> texcoords;
    std::vector<std::uint16_t> indices;
};

struct ChunkData
{
    MeshData floors;
    MeshData walls;
};

ChunkData BuildChunk(const std::vector<int>& grid, int mapWidth, int mapHeight,
                     int originX, int originY, float wallHeight);
}
