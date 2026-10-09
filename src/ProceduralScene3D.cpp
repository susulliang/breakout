#include "ProceduralScene3D.hpp"

#include "ActorPose.hpp"
#include "World3D.hpp"

#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
struct MeshData
{
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Color> colors;
    std::vector<std::uint16_t> indices;
};

constexpr float kPi = 3.14159265359f;
constexpr float kWeaponLengths[4] = { 0.38f, 0.56f, 0.78f, 0.68f };

Color Shade(Color color, float amount)
{
    return Color{
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.r * amount), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.g * amount), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.b * amount), 0, 255)),
        color.a
    };
}

bool AppendQuad(MeshData& mesh, Vector3 a, Vector3 b, Vector3 c, Vector3 d,
                Vector3 normal, Color color)
{
    const std::size_t base = mesh.positions.size();
    if (base + 4 > std::numeric_limits<std::uint16_t>::max())
        return false;

    mesh.positions.insert(mesh.positions.end(), { a, b, c, d });
    mesh.normals.insert(mesh.normals.end(), 4, normal);
    mesh.colors.insert(mesh.colors.end(), 4, color);
    const auto first = static_cast<std::uint16_t>(base);
    mesh.indices.insert(mesh.indices.end(), {
        first, static_cast<std::uint16_t>(first + 1), static_cast<std::uint16_t>(first + 2),
        first, static_cast<std::uint16_t>(first + 2), static_cast<std::uint16_t>(first + 3)
    });
    return true;
}

void AppendBox(MeshData& mesh, Vector3 lo, Vector3 hi, Color color)
{
    AppendQuad(mesh, Vector3{ hi.x, lo.y, lo.z }, Vector3{ hi.x, hi.y, lo.z },
               Vector3{ hi.x, hi.y, hi.z }, Vector3{ hi.x, lo.y, hi.z },
               Vector3{ 1, 0, 0 }, Shade(color, 0.82f));
    AppendQuad(mesh, Vector3{ lo.x, lo.y, hi.z }, Vector3{ lo.x, hi.y, hi.z },
               Vector3{ lo.x, hi.y, lo.z }, Vector3{ lo.x, lo.y, lo.z },
               Vector3{ -1, 0, 0 }, Shade(color, 0.64f));
    AppendQuad(mesh, Vector3{ lo.x, hi.y, hi.z }, Vector3{ hi.x, hi.y, hi.z },
               Vector3{ hi.x, hi.y, lo.z }, Vector3{ lo.x, hi.y, lo.z },
               Vector3{ 0, 1, 0 }, Shade(color, 1.12f));
    AppendQuad(mesh, Vector3{ lo.x, lo.y, lo.z }, Vector3{ hi.x, lo.y, lo.z },
               Vector3{ hi.x, lo.y, hi.z }, Vector3{ lo.x, lo.y, hi.z },
               Vector3{ 0, -1, 0 }, Shade(color, 0.52f));
    AppendQuad(mesh, Vector3{ lo.x, lo.y, hi.z }, Vector3{ hi.x, lo.y, hi.z },
               Vector3{ hi.x, hi.y, hi.z }, Vector3{ lo.x, hi.y, hi.z },
               Vector3{ 0, 0, 1 }, color);
    AppendQuad(mesh, Vector3{ hi.x, lo.y, lo.z }, Vector3{ lo.x, lo.y, lo.z },
               Vector3{ lo.x, hi.y, lo.z }, Vector3{ hi.x, hi.y, lo.z },
               Vector3{ 0, 0, -1 }, Shade(color, 0.72f));
}

void AppendCenteredBox(MeshData& mesh, Vector3 center, Vector3 size, Color color)
{
    const Vector3 half{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
    AppendBox(mesh, Vector3{ center.x - half.x, center.y - half.y, center.z - half.z },
              Vector3{ center.x + half.x, center.y + half.y, center.z + half.z }, color);
}

void AppendTaperedBody(MeshData& mesh, float lowerWidth, float upperWidth,
                       float depth, Color color)
{
    const float lowerX = lowerWidth * 0.5f;
    const float upperX = upperWidth * 0.5f;
    const float halfDepth = depth * 0.5f;
    const Vector3 blf{ -lowerX, -0.5f, halfDepth };
    const Vector3 brf{  lowerX, -0.5f, halfDepth };
    const Vector3 trf{  upperX,  0.5f, halfDepth };
    const Vector3 tlf{ -upperX, 0.5f, halfDepth };
    const Vector3 blb{ -lowerX, -0.5f, -halfDepth };
    const Vector3 brb{  lowerX, -0.5f, -halfDepth };
    const Vector3 trb{  upperX,  0.5f, -halfDepth };
    const Vector3 tlb{ -upperX, 0.5f, -halfDepth };

    AppendQuad(mesh, blf, brf, trf, tlf, Vector3{ 0, 0, 1 }, color);
    AppendQuad(mesh, brb, blb, tlb, trb, Vector3{ 0, 0, -1 }, Shade(color, 0.72f));
    AppendQuad(mesh, brf, brb, trb, trf, Vector3{ 1, 0, 0 }, Shade(color, 0.82f));
    AppendQuad(mesh, blb, blf, tlf, tlb, Vector3{ -1, 0, 0 }, Shade(color, 0.62f));
    AppendQuad(mesh, tlf, trf, trb, tlb, Vector3{ 0, 1, 0 }, Shade(color, 1.12f));
    AppendQuad(mesh, blb, brb, brf, blf, Vector3{ 0, -1, 0 }, Shade(color, 0.52f));
}

std::array<MeshData, 6> BuildHumanoid(bool enemy)
{
    const Color main = enemy ? Color{ 190, 48, 59, 255 } : Color{ 54, 119, 218, 255 };
    const Color trim = enemy ? Color{ 239, 147, 64, 255 } : Color{ 88, 216, 232, 255 };
    const Color dark = enemy ? Color{ 74, 29, 39, 255 } : Color{ 30, 57, 93, 255 };
    std::array<MeshData, 6> parts{};

    AppendTaperedBody(parts[0], 0.88f, 0.78f, 0.82f, main);
    AppendCenteredBox(parts[0], Vector3{ 0, 0.03f, 0.43f }, Vector3{ 0.62f, 0.17f, 0.08f }, trim);
    AppendCenteredBox(parts[0], Vector3{ 0, -0.25f, 0.32f }, Vector3{ 0.55f, 0.12f, 0.12f }, dark);

    AppendTaperedBody(parts[1], 0.70f, 1.0f, 0.72f, main);
    AppendCenteredBox(parts[1], Vector3{ 0, 0.12f, 0.40f }, Vector3{ 0.76f, 0.35f, 0.12f }, trim);
    AppendCenteredBox(parts[1], Vector3{ 0, -0.28f, 0.38f }, Vector3{ 0.50f, 0.14f, 0.11f }, dark);

    for (int i = 2; i < 4; ++i)
    {
        AppendTaperedBody(parts[static_cast<std::size_t>(i)], 0.78f, 1.0f,
                          0.90f, main);
        AppendCenteredBox(parts[static_cast<std::size_t>(i)],
                          Vector3{ 0, -0.20f, 0.48f },
                          Vector3{ 0.84f, 0.19f, 0.08f }, trim);
    }

    for (int i = 4; i < 6; ++i)
    {
        AppendTaperedBody(parts[static_cast<std::size_t>(i)], 0.92f, 0.76f,
                          0.88f, main);
        AppendCenteredBox(parts[static_cast<std::size_t>(i)],
                          Vector3{ 0, -0.38f, 0.37f },
                          Vector3{ 1.02f, 0.20f, 0.18f }, dark);
        AppendCenteredBox(parts[static_cast<std::size_t>(i)],
                          Vector3{ 0, -0.08f, 0.40f },
                          Vector3{ 0.92f, 0.12f, 0.10f }, trim);
    }
    return parts;
}

MeshData BuildWeapon(WeaponSlot slot)
{
    MeshData mesh{};
    const Color dark{ 42, 49, 61, 255 };
    const Color steel{ 119, 132, 151, 255 };
    const Color accent = slot == WeaponSlot::Shotgun ? Color{ 236, 130, 65, 255 } :
                         slot == WeaponSlot::Marksman ? Color{ 83, 190, 226, 255 } :
                         slot == WeaponSlot::Gatling ? Color{ 159, 202, 86, 255 } :
                         Color{ 242, 198, 74, 255 };

    if (slot == WeaponSlot::Slingshot)
    {
        AppendBox(mesh, Vector3{ -0.045f, -0.13f, -0.04f }, Vector3{ 0.045f, 0.08f, 0.06f }, dark);
        AppendBox(mesh, Vector3{ -0.14f, 0.04f, 0.19f }, Vector3{ -0.075f, 0.24f, 0.25f }, accent);
        AppendBox(mesh, Vector3{ 0.075f, 0.04f, 0.19f }, Vector3{ 0.14f, 0.24f, 0.25f }, accent);
        AppendBox(mesh, Vector3{ -0.12f, 0.10f, 0.23f }, Vector3{ -0.09f, 0.14f, 0.36f }, steel);
        AppendBox(mesh, Vector3{ 0.09f, 0.10f, 0.23f }, Vector3{ 0.12f, 0.14f, 0.36f }, steel);
        AppendBox(mesh, Vector3{ -0.07f, 0.04f, 0.31f }, Vector3{ 0.07f, 0.12f, 0.36f }, dark);
    }
    else if (slot == WeaponSlot::Shotgun)
    {
        AppendBox(mesh, Vector3{ -0.07f, -0.06f, -0.17f }, Vector3{ 0.07f, 0.07f, 0.05f }, accent);
        AppendBox(mesh, Vector3{ -0.085f, -0.04f, 0.02f }, Vector3{ 0.085f, 0.09f, 0.31f }, dark);
        AppendBox(mesh, Vector3{ -0.055f, -0.015f, 0.30f }, Vector3{ 0.055f, 0.055f, 0.56f }, steel);
        AppendBox(mesh, Vector3{ -0.09f, -0.12f, 0.02f }, Vector3{ 0.09f, -0.04f, 0.18f }, accent);
        AppendBox(mesh, Vector3{ -0.025f, 0.08f, 0.13f }, Vector3{ 0.025f, 0.13f, 0.22f }, accent);
    }
    else if (slot == WeaponSlot::Marksman)
    {
        AppendBox(mesh, Vector3{ -0.055f, -0.055f, -0.20f }, Vector3{ 0.055f, 0.06f, 0.08f }, accent);
        AppendBox(mesh, Vector3{ -0.07f, -0.035f, 0.04f }, Vector3{ 0.07f, 0.07f, 0.38f }, dark);
        AppendBox(mesh, Vector3{ -0.035f, -0.008f, 0.36f }, Vector3{ 0.035f, 0.035f, 0.78f }, steel);
        AppendBox(mesh, Vector3{ -0.045f, 0.075f, 0.16f }, Vector3{ 0.045f, 0.12f, 0.28f }, accent);
        AppendBox(mesh, Vector3{ -0.09f, -0.12f, 0.06f }, Vector3{ 0.09f, -0.04f, 0.25f }, accent);
    }
    else
    {
        AppendBox(mesh, Vector3{ -0.12f, -0.06f, -0.18f }, Vector3{ 0.12f, 0.09f, 0.20f }, dark);
        AppendBox(mesh, Vector3{ -0.10f, -0.035f, 0.16f }, Vector3{ 0.10f, 0.07f, 0.42f }, accent);
        const float offsets[4][2] = {
            { -0.055f, -0.045f }, { 0.055f, -0.045f },
            { -0.055f, 0.045f }, { 0.055f, 0.045f }
        };
        for (const auto& offset : offsets)
        {
            AppendBox(mesh, Vector3{ offset[0] - 0.018f, offset[1] - 0.018f, 0.39f },
                      Vector3{ offset[0] + 0.018f, offset[1] + 0.018f, 0.68f }, steel);
        }
        AppendBox(mesh, Vector3{ -0.04f, 0.09f, 0.10f }, Vector3{ 0.04f, 0.14f, 0.23f }, accent);
    }
    return mesh;
}

MeshData BuildProp(PropKind kind)
{
    MeshData mesh{};
    const Color steel{ 104, 117, 137, 255 };
    const Color dark{ 45, 54, 68, 255 };
    const Color blue{ 48, 105, 178, 255 };
    const Color orange{ 229, 111, 55, 255 };
    const Color green{ 99, 190, 103, 255 };
    const Color cyan{ 80, 207, 229, 255 };
    switch (kind)
    {
        case PropKind::PartitionScreen:
            AppendBox(mesh, Vector3{ -0.90f, 0.02f, -0.06f }, Vector3{ 0.90f, 0.12f, 0.06f }, dark);
            AppendBox(mesh, Vector3{ -0.84f, 0.14f, -0.035f }, Vector3{ 0.84f, 1.06f, 0.035f }, blue);
            AppendBox(mesh, Vector3{ -0.90f, 1.06f, -0.06f }, Vector3{ 0.90f, 1.15f, 0.06f }, steel);
            AppendBox(mesh, Vector3{ -0.90f, 0.0f, -0.06f }, Vector3{ -0.82f, 1.15f, 0.06f }, steel);
            AppendBox(mesh, Vector3{ 0.82f, 0.0f, -0.06f }, Vector3{ 0.90f, 1.15f, 0.06f }, steel);
            break;
        case PropKind::WireFence:
            for (int i = 0; i <= 8; ++i)
            {
                const float x = -0.90f + static_cast<float>(i) * 0.225f;
                AppendBox(mesh, Vector3{ x - 0.012f, 0.0f, -0.025f },
                          Vector3{ x + 0.012f, 1.0f, 0.025f }, steel);
            }
            AppendBox(mesh, Vector3{ -0.92f, 0.12f, -0.035f }, Vector3{ 0.92f, 0.16f, 0.035f }, dark);
            AppendBox(mesh, Vector3{ -0.92f, 0.94f, -0.035f }, Vector3{ 0.92f, 0.98f, 0.035f }, dark);
            break;
        case PropKind::PartsTable:
            AppendBox(mesh, Vector3{ -0.47f, 0.52f, -0.34f }, Vector3{ 0.47f, 0.64f, 0.34f }, steel);
            for (float x : { -0.39f, 0.39f })
                for (float z : { -0.26f, 0.26f })
                    AppendBox(mesh, Vector3{ x - 0.035f, 0.0f, z - 0.035f },
                              Vector3{ x + 0.035f, 0.54f, z + 0.035f }, dark);
            AppendBox(mesh, Vector3{ -0.24f, 0.64f, -0.08f }, Vector3{ -0.05f, 0.72f, 0.10f }, orange);
            AppendBox(mesh, Vector3{ 0.10f, 0.64f, -0.20f }, Vector3{ 0.34f, 0.70f, -0.05f }, cyan);
            break;
        case PropKind::Locker:
            AppendBox(mesh, Vector3{ -0.22f, 0.0f, -0.24f }, Vector3{ 0.22f, 0.92f, 0.24f }, orange);
            AppendBox(mesh, Vector3{ -0.18f, 0.08f, 0.245f }, Vector3{ 0.18f, 0.84f, 0.27f }, Shade(orange, 1.12f));
            AppendBox(mesh, Vector3{ 0.12f, 0.42f, 0.275f }, Vector3{ 0.15f, 0.55f, 0.30f }, dark);
            break;
        case PropKind::VendingMachine:
            AppendBox(mesh, Vector3{ -0.29f, 0.0f, -0.24f }, Vector3{ 0.29f, 1.08f, 0.24f }, dark);
            AppendBox(mesh, Vector3{ -0.23f, 0.35f, 0.245f }, Vector3{ 0.12f, 0.93f, 0.27f }, green);
            AppendBox(mesh, Vector3{ -0.18f, 0.42f, 0.275f }, Vector3{ 0.07f, 0.82f, 0.30f }, cyan);
            AppendBox(mesh, Vector3{ 0.16f, 0.09f, 0.245f }, Vector3{ 0.23f, 0.31f, 0.27f }, orange);
            break;
        case PropKind::ScreenPanel:
            AppendBox(mesh, Vector3{ -0.22f, 0.0f, -0.18f }, Vector3{ 0.22f, 0.82f, 0.18f }, dark);
            AppendBox(mesh, Vector3{ -0.18f, 0.40f, 0.19f }, Vector3{ 0.18f, 0.76f, 0.22f }, steel);
            AppendBox(mesh, Vector3{ -0.14f, 0.46f, 0.225f }, Vector3{ 0.14f, 0.70f, 0.24f }, cyan);
            AppendBox(mesh, Vector3{ -0.09f, 0.12f, 0.19f }, Vector3{ 0.09f, 0.17f, 0.22f }, blue);
            break;
    }
    return mesh;
}

MeshData BuildBeacon()
{
    MeshData mesh{};
    AppendBox(mesh, Vector3{ -0.12f, 0.02f, -0.12f }, Vector3{ 0.12f, 0.10f, 0.12f },
              Color{ 42, 62, 77, 255 });
    AppendBox(mesh, Vector3{ -0.045f, 0.10f, -0.045f }, Vector3{ 0.045f, 0.76f, 0.045f },
              Color{ 68, 201, 195, 255 });
    AppendBox(mesh, Vector3{ -0.09f, 0.70f, -0.09f }, Vector3{ 0.09f, 0.86f, 0.09f },
              Color{ 91, 241, 218, 255 });
    return mesh;
}

bool UploadMeshData(const MeshData& data, Mesh& mesh)
{
    if (data.positions.empty() || data.positions.size() > 65535 ||
        data.normals.size() != data.positions.size() ||
        data.colors.size() != data.positions.size() || data.indices.empty())
        return false;

    mesh = Mesh{};
    mesh.vertexCount = static_cast<int>(data.positions.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3);
    const std::size_t count = data.positions.size();
    mesh.vertices = static_cast<float*>(MemAlloc(count * 3 * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(count * 3 * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(count * 4));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(data.indices.size() * sizeof(unsigned short)));
    if (!mesh.vertices || !mesh.normals || !mesh.colors || !mesh.indices)
    {
        if (mesh.vertices) MemFree(mesh.vertices);
        if (mesh.normals) MemFree(mesh.normals);
        if (mesh.colors) MemFree(mesh.colors);
        if (mesh.indices) MemFree(mesh.indices);
        mesh = Mesh{};
        return false;
    }

    for (std::size_t i = 0; i < count; ++i)
    {
        mesh.vertices[i * 3] = data.positions[i].x;
        mesh.vertices[i * 3 + 1] = data.positions[i].y;
        mesh.vertices[i * 3 + 2] = data.positions[i].z;
        mesh.normals[i * 3] = data.normals[i].x;
        mesh.normals[i * 3 + 1] = data.normals[i].y;
        mesh.normals[i * 3 + 2] = data.normals[i].z;
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
        UnloadMesh(mesh);
    mesh = Mesh{};
}

Matrix ComposeMatrix(Vector3 position, Vector3 rotation, Vector3 scale)
{
    const Matrix translate = MatrixTranslate(position.x, position.y, position.z);
    const Matrix rotate = MatrixRotateXYZ(rotation);
    const Matrix scaling = MatrixScale(scale.x, scale.y, scale.z);
    return MatrixMultiply(MatrixMultiply(translate, rotate), scaling);
}

Matrix YawTransform(Vector3 position, float yaw, Vector3 scale)
{
    return ComposeMatrix(position, Vector3{ 0.0f, yaw, 0.0f }, scale);
}

float WeaponLength(WeaponSlot slot)
{
    return kWeaponLengths[static_cast<std::size_t>(slot)];
}

}

bool ProceduralScene3D::Initialize()
{
    if (m_ready)
        return true;

    m_material = LoadMaterialDefault();
    if (m_material.maps == nullptr || !IsMaterialReady(m_material))
    {
        TraceLog(LOG_WARNING, "ProceduralScene3D: default material unavailable");
        return false;
    }

    const auto player = BuildHumanoid(false);
    const auto enemy = BuildHumanoid(true);
    bool ok = true;
    for (std::size_t i = 0; i < kActorPartCount; ++i)
    {
        ok = UploadMeshData(player[i], m_playerParts[i]) && ok;
        ok = UploadMeshData(enemy[i], m_enemyParts[i]) && ok;
    }
    for (std::size_t i = 0; i < kWeaponCount; ++i)
    {
        ok = UploadMeshData(BuildWeapon(static_cast<WeaponSlot>(i)), m_weapons[i]) && ok;
    }
    for (std::size_t i = 0; i < kPropCount; ++i)
    {
        ok = UploadMeshData(BuildProp(static_cast<PropKind>(i)), m_props[i]) && ok;
    }
    ok = UploadMeshData(BuildBeacon(), m_beacon) && ok;
    m_ready = ok;
    if (!m_ready)
    {
        TraceLog(LOG_WARNING, "ProceduralScene3D: one or more generated meshes failed to upload");
        Shutdown();
    }
    else
    {
        TraceLog(LOG_INFO, "ProceduralScene3D: generated 12 character parts, 4 weapons, 6 props and beacon");
    }
    return m_ready;
}

void ProceduralScene3D::DrawActor(const Game& game, const ActorPose::Pose& pose,
                                  bool enemy, float recoil) const
{
    const auto& parts = enemy ? m_enemyParts : m_playerParts;
    for (std::size_t i = 0; i < kActorPartCount; ++i)
    {
        const ActorPose::PartPose& part = pose.parts[i];
        if (parts[i].vertexCount <= 0)
            continue;
        DrawMesh(parts[i], m_material,
                 ComposeMatrix(part.position, part.rotation, part.size));
    }

    if (!enemy)
    {
        const std::size_t weapon = static_cast<std::size_t>(game.weapons.selected);
        if (weapon < m_weapons.size() && m_weapons[weapon].vertexCount > 0)
        {
            DrawMesh(m_weapons[weapon], m_material,
                     YawTransform(pose.grip, pose.yaw, Vector3{ 1, 1, 1 }));
        }
        if (recoil > 0.15f)
        {
            DrawSphere(pose.muzzle, 0.025f + recoil * 0.025f,
                       Color{ 255, 217, 106, 230 });
        }
    }
}

void ProceduralScene3D::Draw(const Game& game) const
{
    if (!m_ready)
        return;

    const float moveBlend = (std::fabs(game.input.screenMove.x) > 0.0f ||
                             std::fabs(game.input.screenMove.y) > 0.0f) ? 1.0f : 0.0f;
    const float recoil = std::max(0.0f, 1.0f - game.player.fireAnimationTime / 0.14f);
    ActorPose::Input playerInput{};
    playerInput.groundPosition = game.player.pos;
    playerInput.forward = game.player.aimDirection;
    playerInput.walkPhase = game.playerAnimPhase / ActorPose::kWalkCycleSeconds;
    playerInput.movementBlend = moveBlend;
    playerInput.elapsed = game.stateTime;
    playerInput.stagger = game.player.hurtCooldown > 0.55f
        ? (game.player.hurtCooldown - 0.55f) / 0.25f : 0.0f;
    playerInput.recoil = recoil;
    const ActorPose::Pose playerPose = ActorPose::Evaluate(
        playerInput, WeaponLength(game.weapons.selected));
    DrawActor(game, playerPose, false, recoil);

    for (const Enemy& enemy : game.enemies)
    {
        if (!enemy.active)
            continue;
        ActorPose::Input enemyInput{};
        enemyInput.groundPosition = enemy.pos;
        enemyInput.forward = Vector2{ game.player.pos.x - enemy.pos.x,
                                      game.player.pos.y - enemy.pos.y };
        enemyInput.walkPhase = game.stateTime / ActorPose::kWalkCycleSeconds +
                               enemy.pos.x * 0.31f + enemy.pos.y * 0.17f;
        enemyInput.movementBlend = enemy.state == EnemyState::CHASE ? 1.0f : 0.18f;
        enemyInput.elapsed = game.stateTime + enemy.pos.x;
        enemyInput.stagger = enemy.hitReactionTime / 0.28f;
        const ActorPose::Pose pose = ActorPose::Evaluate(enemyInput, 0.0f);
        DrawActor(game, pose, true, 0.0f);
    }

    for (const Prop& prop : game.props)
    {
        const std::size_t index = static_cast<std::size_t>(prop.kind);
        if (index >= m_props.size() || m_props[index].vertexCount <= 0)
            continue;
        const float yaw = prop.variant < 0.5f ? 0.0f : 0.5f * kPi;
        DrawMesh(m_props[index], m_material,
                 YawTransform(World3D::FromGround(prop.pos), yaw,
                              Vector3{ prop.scale, prop.scale, prop.scale }));
    }

    const float pulse = 0.94f + 0.06f * std::sin(game.stateTime * 4.0f);
    DrawMesh(m_beacon, m_material,
             YawTransform(World3D::FromGround(game.levelExit), 0.0f,
                          Vector3{ 1.0f, pulse, 1.0f }));

    for (const WeaponPickup& pickup : game.weaponPickups)
    {
        if (!pickup.active)
            continue;
        const std::size_t weapon = static_cast<std::size_t>(pickup.slot);
        if (weapon >= m_weapons.size())
            continue;
        const float yaw = game.stateTime * 0.8f + pickup.pos.x;
        const float bob = 0.20f + 0.035f * std::sin(game.stateTime * 4.0f + pickup.pos.y);
        const Vector3 position = World3D::FromGround(pickup.pos, bob);
        DrawCircle3D(Vector3{ position.x, 0.015f, position.z }, 0.25f,
                     Vector3{ 1, 0, 0 }, 90.0f, Color{ 42, 122, 150, 100 });
        DrawMesh(m_weapons[weapon], m_material,
                 YawTransform(position, yaw, Vector3{ 0.68f, 0.68f, 0.68f }));
    }

    for (const Bullet& bullet : game.bullets)
    {
        if (!bullet.active)
            continue;
        const Vector3 start = World3D::FromGround(bullet.previousPos, 0.76f);
        const Vector3 end = World3D::FromGround(bullet.pos, 0.76f);
        const Color color = bullet.weapon == WeaponSlot::Shotgun
            ? Color{ 255, 142, 76, 255 } :
            bullet.weapon == WeaponSlot::Marksman
                ? Color{ 112, 210, 255, 255 } :
            bullet.weapon == WeaponSlot::Gatling
                ? Color{ 180, 255, 104, 255 } :
                  Color{ 255, 214, 64, 255 };
        DrawLine3D(start, end, Fade(color, 0.65f));
        DrawSphere(end, 0.035f, color);
    }

    for (const DeathPiece& piece : game.deathPieces)
    {
        if (!piece.active)
            continue;
        std::size_t part = piece.spritePart == 0 ? 0u :
                           piece.spritePart == 1 ? 1u :
                           piece.spritePart == 2 ? 2u : 4u;
        const ActorPose::Pose pose = ActorPose::Evaluate(
            ActorPose::Input{ piece.pos, Vector2{ 0.0f, 1.0f } }, 0.0f);
        const float lift = piece.screenLift / World3D::kVerticalPixelsPerUnit;
        const Vector3 position{ piece.pos.x, lift + 0.07f, piece.pos.y };
        const float size = std::max(0.08f, piece.scale);
        DrawMesh(m_enemyParts[part], m_material,
                 ComposeMatrix(position, Vector3{ 0.0f, piece.rotation, 0.0f },
                               Vector3{ pose.parts[part].size.x * size,
                                        pose.parts[part].size.y * size,
                                        pose.parts[part].size.z * size }));
    }

    for (const Particle& particle : game.particles)
    {
        if (!particle.active)
            continue;
        const float alpha = particle.maxLife > 0.0f
            ? std::clamp(particle.life / particle.maxLife, 0.0f, 1.0f) : 0.0f;
        const Vector3 position = World3D::FromGround(
            particle.pos, particle.screenLift / World3D::kVerticalPixelsPerUnit);
        DrawSphere(position, 0.025f, Fade(particle.color, alpha));
    }
}

void ProceduralScene3D::Shutdown()
{
    for (Mesh& mesh : m_playerParts) ReleaseMesh(mesh);
    for (Mesh& mesh : m_enemyParts) ReleaseMesh(mesh);
    for (Mesh& mesh : m_weapons) ReleaseMesh(mesh);
    for (Mesh& mesh : m_props) ReleaseMesh(mesh);
    ReleaseMesh(m_beacon);
    if (m_material.maps != nullptr)
        UnloadMaterial(m_material);
    m_material = Material{};
    m_ready = false;
}
