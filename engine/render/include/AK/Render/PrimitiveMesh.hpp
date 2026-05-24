#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/RHI/VulkanRHI.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class PrimitiveMeshKind : u32
    {
        Triangle = 0,
        Plane = 1,
        Cube = 2,
        UvSphere = 3,
        Cylinder = 4,
        Capsule = 5,
        Grid = 6
    };

    struct PrimitiveVertex
    {
        Vec3 position{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec2 uv{};
        Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct PrimitiveMeshBuildDesc
    {
        PrimitiveMeshKind kind = PrimitiveMeshKind::Cube;
        std::string name;
        float radius = 0.5f;
        float halfHeight = 0.5f;
        float size = 1.0f;
        u32 segments = 16;
        u32 rings = 8;
        Vec4 color{0.72f, 0.78f, 0.88f, 1.0f};
    };

    struct PrimitiveMesh
    {
        std::string name;
        PrimitiveMeshKind kind = PrimitiveMeshKind::Cube;
        std::vector<PrimitiveVertex> vertices;
        std::vector<u32> indices;
        AABB3 bounds{};
        bool valid = false;
    };

    struct PrimitiveSceneObject
    {
        std::string name;
        u32 meshIndex = 0;
        Vec3 position{};
        Vec3 rotationDegrees{};
        Vec3 scale{1.0f, 1.0f, 1.0f};
        Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        bool visible = true;
    };

    struct PrimitiveTestScene
    {
        std::vector<PrimitiveMesh> meshes;
        std::vector<PrimitiveSceneObject> objects;
        Vec3 cameraPosition{4.0f, 3.0f, 6.0f};
        Vec3 cameraTarget{0.0f, 0.4f, 0.0f};
        bool reversedZ = true;
        bool valid = false;
    };

    struct PrimitiveMeshStats
    {
        u32 meshes = 0;
        u32 objects = 0;
        u32 vertices = 0;
        u32 indices = 0;
        u32 triangles = 0;
        u32 drawCalls = 0;
        u32 validationWarnings = 0;
        bool finite = true;
        bool valid = false;
        AABB3 sceneBounds{};
        std::string summary;
    };

    PrimitiveMesh BuildPrimitiveMesh(const PrimitiveMeshBuildDesc& desc);
    PrimitiveTestScene BuildDefaultPrimitiveTestScene();
    PrimitiveMeshStats AnalyzePrimitiveTestScene(const PrimitiveTestScene& scene);
    std::vector<VulkanDrawCallPlan> BuildDrawCallPlan(const PrimitiveTestScene& scene);

    const char* ToString(PrimitiveMeshKind kind);
    std::string ToDebugString(const PrimitiveMeshStats& stats);
}
