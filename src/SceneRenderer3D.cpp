#include "SceneRenderer3D.hpp"

#include "MapMeshBuilder.hpp"
#include "World3D.hpp"

#include "raymath.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace
{
bool UploadMeshData(const MapMeshBuilder::MeshData& data, Mesh& mesh)
{
    if (data.positions.empty())
    {
        mesh = Mesh{};
        return true;
    }
    if (data.positions.size() > 65535 || data.normals.size() != data.positions.size() ||
        data.colors.size() != data.positions.size() ||
        data.texcoords.size() != data.positions.size() || data.indices.empty())
    {
        return false;
    }

    mesh = Mesh{};
    mesh.vertexCount = static_cast<int>(data.positions.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3);
    const std::size_t vertexCount = data.positions.size();
    mesh.vertices = static_cast<float*>(MemAlloc(vertexCount * 3 * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(vertexCount * 3 * sizeof(float)));
    mesh.texcoords = static_cast<float*>(MemAlloc(vertexCount * 2 * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(vertexCount * 4));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(data.indices.size() * sizeof(unsigned short)));
    if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.colors || !mesh.indices)
    {
        if (mesh.vertices) MemFree(mesh.vertices);
        if (mesh.normals) MemFree(mesh.normals);
        if (mesh.texcoords) MemFree(mesh.texcoords);
        if (mesh.colors) MemFree(mesh.colors);
        if (mesh.indices) MemFree(mesh.indices);
        mesh = Mesh{};
        return false;
    }

    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        mesh.vertices[i * 3] = data.positions[i].x;
        mesh.vertices[i * 3 + 1] = data.positions[i].y;
        mesh.vertices[i * 3 + 2] = data.positions[i].z;
        mesh.normals[i * 3] = data.normals[i].x;
        mesh.normals[i * 3 + 1] = data.normals[i].y;
        mesh.normals[i * 3 + 2] = data.normals[i].z;
        mesh.texcoords[i * 2] = data.texcoords[i].x;
        mesh.texcoords[i * 2 + 1] = data.texcoords[i].y;
        mesh.colors[i * 4] = data.colors[i].r;
        mesh.colors[i * 4 + 1] = data.colors[i].g;
        mesh.colors[i * 4 + 2] = data.colors[i].b;
        mesh.colors[i * 4 + 3] = data.colors[i].a;
    }
    std::memcpy(mesh.indices, data.indices.data(), data.indices.size() * sizeof(unsigned short));
    UploadMesh(&mesh, false);
    return true;
}

void ReleaseMesh(Mesh& mesh)
{
    if (mesh.vertexCount > 0 || mesh.vaoId != 0 || mesh.vboId != nullptr)
    {
        UnloadMesh(mesh);
    }
    mesh = Mesh{};
}
}

bool SceneRenderer3D::Initialize()
{
    if (m_ready)
    {
        return true;
    }
    m_floorMaterial = LoadMaterialDefault();
    m_wallMaterial = LoadMaterialDefault();
    if (m_floorMaterial.maps != nullptr && m_wallMaterial.maps != nullptr)
    {
        const Texture2D floor = CreateSurfaceTexture(true);
        const Texture2D wall = CreateSurfaceTexture(false);
        if (floor.id != 0)
            m_floorMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = floor;
        if (wall.id != 0)
            m_wallMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = wall;
        if (floor.id != 0 && wall.id != 0)
        {
            SetTextureFilter(floor, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(wall, TEXTURE_FILTER_BILINEAR);
            m_ready = IsMaterialReady(m_floorMaterial) && IsMaterialReady(m_wallMaterial);
        }
    }
    if (!m_ready)
    {
        TraceLog(LOG_WARNING, "SceneRenderer3D: material or generated texture unavailable");
        if (m_floorMaterial.maps != nullptr) UnloadMaterial(m_floorMaterial);
        if (m_wallMaterial.maps != nullptr) UnloadMaterial(m_wallMaterial);
        m_floorMaterial = Material{};
        m_wallMaterial = Material{};
    }
    return m_ready;
}

Texture2D SceneRenderer3D::CreateSurfaceTexture(bool floor) const
{
    constexpr int kTextureSize = 128;
    Image image = GenImageColor(kTextureSize, kTextureSize, WHITE);
    if (image.data == nullptr)
        return Texture2D{};

    Color* pixels = static_cast<Color*>(image.data);
    for (int y = 0; y < kTextureSize; ++y)
    {
        for (int x = 0; x < kTextureSize; ++x)
        {
            const int repeatX = x % 32;
            const int repeatY = y % 32;
            const unsigned int hash = static_cast<unsigned int>(repeatX * 73856093) ^
                                      static_cast<unsigned int>(repeatY * 19349663) ^
                                      (floor ? 0xA341316Cu : 0xC8013EA4u);
            const int noise = static_cast<int>((hash ^ (hash >> 13)) % 15u) - 7;
            int shade = floor ? 234 + noise : 226 + noise;
            const bool nearXSeam = (x % 64) < 2 || (x % 64) >= 62;
            const bool nearYSeam = (y % 64) < 2 || (y % 64) >= 62;
            if (floor && (nearXSeam || nearYSeam))
                shade -= 12;
            if (!floor && (nearYSeam || nearXSeam))
                shade -= 20;
            const unsigned char value = static_cast<unsigned char>(std::clamp(shade, 0, 255));
            pixels[static_cast<std::size_t>(y * kTextureSize + x)] =
                Color{ value, static_cast<unsigned char>(std::min(255, value + 2)),
                       static_cast<unsigned char>(std::min(255, value + 6)), 255 };
        }
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

void SceneRenderer3D::RebuildLevel(const Game& game)
{
    for (Chunk& chunk : m_chunks)
    {
        ReleaseMesh(chunk.floors);
        ReleaseMesh(chunk.walls);
    }
    m_chunks.clear();
    if (!m_ready || game.mapWidth <= 0 || game.mapHeight <= 0)
    {
        return;
    }

    const int chunksX = (game.mapWidth + MapMeshBuilder::kChunkSize - 1) /
                        MapMeshBuilder::kChunkSize;
    const int chunksY = (game.mapHeight + MapMeshBuilder::kChunkSize - 1) /
                        MapMeshBuilder::kChunkSize;
    m_chunks.reserve(static_cast<std::size_t>(chunksX) * static_cast<std::size_t>(chunksY));

    for (int chunkY = 0; chunkY < chunksY; ++chunkY)
    {
        for (int chunkX = 0; chunkX < chunksX; ++chunkX)
        {
            const int originX = chunkX * MapMeshBuilder::kChunkSize;
            const int originY = chunkY * MapMeshBuilder::kChunkSize;
            const MapMeshBuilder::ChunkData data = MapMeshBuilder::BuildChunk(
                game.gridMap, game.mapWidth, game.mapHeight, originX, originY,
                World3D::kInitialWallHeight);
            Chunk chunk{};
            if (!UploadMeshData(data.floors, chunk.floors) ||
                !UploadMeshData(data.walls, chunk.walls))
            {
                ReleaseMesh(chunk.floors);
                ReleaseMesh(chunk.walls);
                TraceLog(LOG_WARNING, "SceneRenderer3D: skipped chunk (%d, %d)", chunkX, chunkY);
                continue;
            }
            m_chunks.push_back(chunk);
        }
    }
}

void SceneRenderer3D::DrawMap() const
{
    if (!m_ready)
    {
        return;
    }
    const Matrix identity = MatrixIdentity();
    for (const Chunk& chunk : m_chunks)
    {
        if (chunk.floors.vertexCount > 0)
        {
            DrawMesh(chunk.floors, m_floorMaterial, identity);
        }
        if (chunk.walls.vertexCount > 0)
        {
            DrawMesh(chunk.walls, m_wallMaterial, identity);
        }
    }
}

void SceneRenderer3D::Shutdown()
{
    for (Chunk& chunk : m_chunks)
    {
        ReleaseMesh(chunk.floors);
        ReleaseMesh(chunk.walls);
    }
    m_chunks.clear();
    if (m_floorMaterial.maps != nullptr) UnloadMaterial(m_floorMaterial);
    if (m_wallMaterial.maps != nullptr) UnloadMaterial(m_wallMaterial);
    m_floorMaterial = Material{};
    m_wallMaterial = Material{};
    m_ready = false;
}
