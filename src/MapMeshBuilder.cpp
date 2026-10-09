#include "MapMeshBuilder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
const Color kFloorGrout{ 20, 23, 30, 255 };
const Color kFloorA{ 62, 68, 82, 255 };
const Color kFloorB{ 55, 60, 73, 255 };
const Color kWallTop{ 206, 213, 224, 255 };
const Color kWallX{ 112, 120, 138, 255 };
const Color kWallZ{ 74, 81, 97, 255 };

bool AppendQuad(MapMeshBuilder::MeshData& mesh, Vector3 a, Vector3 b,
                Vector3 c, Vector3 d, Vector3 normal, Color color)
{
    const std::size_t base = mesh.positions.size();
    if (base + 4 > std::numeric_limits<std::uint16_t>::max())
    {
        return false;
    }

    mesh.positions.insert(mesh.positions.end(), { a, b, c, d });
    mesh.normals.insert(mesh.normals.end(), 4, normal);
    mesh.colors.insert(mesh.colors.end(), 4, color);
    mesh.texcoords.insert(mesh.texcoords.end(), {
        Vector2{ 0.0f, 0.0f }, Vector2{ 0.0f, 1.0f },
        Vector2{ 1.0f, 1.0f }, Vector2{ 1.0f, 0.0f }
    });
    const auto first = static_cast<std::uint16_t>(base);
    mesh.indices.insert(mesh.indices.end(), {
        first, static_cast<std::uint16_t>(first + 1), static_cast<std::uint16_t>(first + 2),
        first, static_cast<std::uint16_t>(first + 2), static_cast<std::uint16_t>(first + 3)
    });
    return true;
}

int TileAt(const std::vector<int>& grid, int width, int height, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
    {
        return kTileVoid;
    }
    return grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)];
}

void AddFloor(MapMeshBuilder::MeshData& mesh, int x, int y)
{
    const float left = static_cast<float>(x);
    const float right = left + 1.0f;
    const float back = static_cast<float>(y);
    const float front = back + 1.0f;
    const float inset = 0.04f;
    const Color tileColor = ((x + y) % 2 == 0) ? kFloorA : kFloorB;

    AppendQuad(mesh, Vector3{ left, 0.0f, back }, Vector3{ left, 0.0f, front },
               Vector3{ right, 0.0f, front }, Vector3{ right, 0.0f, back },
               Vector3{ 0.0f, 1.0f, 0.0f }, kFloorGrout);
    AppendQuad(mesh, Vector3{ left + inset, 0.003f, back + inset },
               Vector3{ left + inset, 0.003f, front - inset },
               Vector3{ right - inset, 0.003f, front - inset },
               Vector3{ right - inset, 0.003f, back + inset },
               Vector3{ 0.0f, 1.0f, 0.0f }, tileColor);
}

void AddWall(MapMeshBuilder::MeshData& mesh, const std::vector<int>& grid,
             int mapWidth, int mapHeight, int x, int y, float height)
{
    const float left = static_cast<float>(x);
    const float right = left + 1.0f;
    const float back = static_cast<float>(y);
    const float front = back + 1.0f;

    AppendQuad(mesh, Vector3{ left, height, back }, Vector3{ left, height, front },
               Vector3{ right, height, front }, Vector3{ right, height, back },
               Vector3{ 0.0f, 1.0f, 0.0f }, kWallTop);

    if (TileAt(grid, mapWidth, mapHeight, x + 1, y) != kTileWall)
    {
        AppendQuad(mesh, Vector3{ right, 0.0f, back }, Vector3{ right, height, back },
                   Vector3{ right, height, front }, Vector3{ right, 0.0f, front },
                   Vector3{ 1.0f, 0.0f, 0.0f }, kWallX);
    }
    if (TileAt(grid, mapWidth, mapHeight, x - 1, y) != kTileWall)
    {
        AppendQuad(mesh, Vector3{ left, 0.0f, front }, Vector3{ left, height, front },
                   Vector3{ left, height, back }, Vector3{ left, 0.0f, back },
                   Vector3{ -1.0f, 0.0f, 0.0f }, kWallX);
    }
    if (TileAt(grid, mapWidth, mapHeight, x, y + 1) != kTileWall)
    {
        AppendQuad(mesh, Vector3{ right, 0.0f, front }, Vector3{ right, height, front },
                   Vector3{ left, height, front }, Vector3{ left, 0.0f, front },
                   Vector3{ 0.0f, 0.0f, 1.0f }, kWallZ);
    }
    if (TileAt(grid, mapWidth, mapHeight, x, y - 1) != kTileWall)
    {
        AppendQuad(mesh, Vector3{ left, 0.0f, back }, Vector3{ left, height, back },
                   Vector3{ right, height, back }, Vector3{ right, 0.0f, back },
                   Vector3{ 0.0f, 0.0f, -1.0f }, kWallZ);
    }
}
}

MapMeshBuilder::ChunkData MapMeshBuilder::BuildChunk(
    const std::vector<int>& grid, int mapWidth, int mapHeight,
    int originX, int originY, float wallHeight)
{
    ChunkData result{};
    if (mapWidth <= 0 || mapHeight <= 0 || originX < 0 || originY < 0 ||
        originX >= mapWidth || originY >= mapHeight || wallHeight <= 0.0f ||
        grid.size() != static_cast<std::size_t>(mapWidth) * static_cast<std::size_t>(mapHeight))
    {
        return result;
    }

    const int endX = (originX / kChunkSize) * kChunkSize + kChunkSize;
    const int endY = (originY / kChunkSize) * kChunkSize + kChunkSize;
    for (int y = originY; y < endY && y < mapHeight; ++y)
    {
        for (int x = originX; x < endX && x < mapWidth; ++x)
        {
            const int tile = TileAt(grid, mapWidth, mapHeight, x, y);
            if (tile == kTileFloor)
            {
                AddFloor(result.floors, x, y);
            }
            else if (tile == kTileWall)
            {
                AddWall(result.walls, grid, mapWidth, mapHeight, x, y, wallHeight);
            }
        }
    }
    return result;
}
