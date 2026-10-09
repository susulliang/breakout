#pragma once

#include "ActorPose.hpp"
#include "Game.hpp"

#include <array>
#include <cstddef>

class ProceduralScene3D
{
public:
    ProceduralScene3D() = default;
    ~ProceduralScene3D() = default;
    ProceduralScene3D(const ProceduralScene3D&) = delete;
    ProceduralScene3D& operator=(const ProceduralScene3D&) = delete;

    bool Initialize();
    void Draw(const Game& game) const;
    void Shutdown();
    bool IsAvailable() const { return m_ready; }

private:
    static constexpr std::size_t kActorPartCount = 6;
    static constexpr std::size_t kWeaponCount = 4;
    static constexpr std::size_t kPropCount = 6;

    void DrawActor(const Game& game, const ActorPose::Pose& pose,
                   bool enemy, float recoil) const;

    std::array<Mesh, kActorPartCount> m_playerParts{};
    std::array<Mesh, kActorPartCount> m_enemyParts{};
    std::array<Mesh, kWeaponCount> m_weapons{};
    std::array<Mesh, kPropCount> m_props{};
    Mesh m_beacon{};
    Material m_material{};
    bool m_ready = false;
};
