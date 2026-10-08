#pragma once

#include "Game.hpp"

#include <vector>

class SceneRenderer3D
{
public:
    SceneRenderer3D() = default;
    ~SceneRenderer3D() = default;
    SceneRenderer3D(const SceneRenderer3D&) = delete;
    SceneRenderer3D& operator=(const SceneRenderer3D&) = delete;

    bool Initialize();
    void RebuildLevel(const Game& game);
    void DrawMap() const;
    void Shutdown();
    bool IsAvailable() const { return m_ready; }

private:
    struct Chunk
    {
        Mesh floors{};
        Mesh walls{};
    };

    std::vector<Chunk> m_chunks;
    Material m_material{};
    bool m_ready = false;
};
