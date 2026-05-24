#include <AK/Render/PrimitiveMesh.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;

        u32 ClampSegments(u32 value, u32 minValue = 3, u32 maxValue = 96)
        {
            return std::clamp(value, minValue, maxValue);
        }

        void PushVertex(PrimitiveMesh& mesh, Vec3 p, Vec3 n, Vec2 uv, Vec4 color)
        {
            PrimitiveVertex v{};
            v.position = p;
            v.normal = Normalize(n, {0.0f, 1.0f, 0.0f});
            v.uv = uv;
            v.color = color;
            mesh.vertices.push_back(v);
            mesh.bounds = Expand(mesh.bounds, p);
        }

        void PushTriangle(PrimitiveMesh& mesh, u32 a, u32 b, u32 c)
        {
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
        }

        void PushQuad(PrimitiveMesh& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, Vec4 color)
        {
            const u32 base = static_cast<u32>(mesh.vertices.size());
            PushVertex(mesh, a, normal, {0.0f, 0.0f}, color);
            PushVertex(mesh, b, normal, {1.0f, 0.0f}, color);
            PushVertex(mesh, c, normal, {1.0f, 1.0f}, color);
            PushVertex(mesh, d, normal, {0.0f, 1.0f}, color);
            PushTriangle(mesh, base + 0, base + 1, base + 2);
            PushTriangle(mesh, base + 0, base + 2, base + 3);
        }

        PrimitiveMesh MakeTriangle(const PrimitiveMeshBuildDesc& desc)
        {
            PrimitiveMesh mesh{};
            mesh.name = desc.name.empty() ? "triangle" : desc.name;
            mesh.kind = PrimitiveMeshKind::Triangle;
            mesh.bounds = MakeEmptyAABB3();
            const float s = desc.size <= 0.0f ? 1.0f : desc.size;
            PushVertex(mesh, {-0.5f * s, -0.35f * s, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, desc.color);
            PushVertex(mesh, { 0.5f * s, -0.35f * s, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, desc.color);
            PushVertex(mesh, { 0.0f,       0.55f * s, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}, desc.color);
            PushTriangle(mesh, 0, 1, 2);
            return mesh;
        }

        PrimitiveMesh MakePlane(const PrimitiveMeshBuildDesc& desc)
        {
            PrimitiveMesh mesh{};
            mesh.name = desc.name.empty() ? "plane" : desc.name;
            mesh.kind = PrimitiveMeshKind::Plane;
            mesh.bounds = MakeEmptyAABB3();
            const float h = std::max(desc.size, 0.001f) * 0.5f;
            PushQuad(mesh, {-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, desc.color);
            return mesh;
        }

        PrimitiveMesh MakeCube(const PrimitiveMeshBuildDesc& desc)
        {
            PrimitiveMesh mesh{};
            mesh.name = desc.name.empty() ? "cube" : desc.name;
            mesh.kind = PrimitiveMeshKind::Cube;
            mesh.bounds = MakeEmptyAABB3();
            const float h = std::max(desc.size, 0.001f) * 0.5f;
            PushQuad(mesh, {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h}, {0.0f,0.0f,1.0f}, desc.color);
            PushQuad(mesh, { h,-h,-h}, {-h,-h,-h}, {-h, h,-h}, { h, h,-h}, {0.0f,0.0f,-1.0f}, desc.color);
            PushQuad(mesh, {-h, h, h}, { h, h, h}, { h, h,-h}, {-h, h,-h}, {0.0f,1.0f,0.0f}, desc.color);
            PushQuad(mesh, {-h,-h,-h}, { h,-h,-h}, { h,-h, h}, {-h,-h, h}, {0.0f,-1.0f,0.0f}, desc.color);
            PushQuad(mesh, { h,-h, h}, { h,-h,-h}, { h, h,-h}, { h, h, h}, {1.0f,0.0f,0.0f}, desc.color);
            PushQuad(mesh, {-h,-h,-h}, {-h,-h, h}, {-h, h, h}, {-h, h,-h}, {-1.0f,0.0f,0.0f}, desc.color);
            return mesh;
        }

        PrimitiveMesh MakeUvSphere(const PrimitiveMeshBuildDesc& desc)
        {
            PrimitiveMesh mesh{};
            mesh.name = desc.name.empty() ? "sphere" : desc.name;
            mesh.kind = PrimitiveMeshKind::UvSphere;
            mesh.bounds = MakeEmptyAABB3();
            const u32 segments = ClampSegments(desc.segments, 6, 96);
            const u32 rings = std::clamp(desc.rings, 4u, 48u);
            const float radius = std::max(desc.radius, 0.001f);

            for (u32 y = 0; y <= rings; ++y)
            {
                const float v = static_cast<float>(y) / static_cast<float>(rings);
                const float theta = v * Pi;
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);
                for (u32 x = 0; x <= segments; ++x)
                {
                    const float u = static_cast<float>(x) / static_cast<float>(segments);
                    const float phi = u * Pi * 2.0f;
                    Vec3 n{std::cos(phi) * sinTheta, cosTheta, std::sin(phi) * sinTheta};
                    PushVertex(mesh, Multiply(n, radius), n, {u, v}, desc.color);
                }
            }

            for (u32 y = 0; y < rings; ++y)
            {
                for (u32 x = 0; x < segments; ++x)
                {
                    const u32 row = segments + 1;
                    const u32 a = y * row + x;
                    const u32 b = a + 1;
                    const u32 c = a + row;
                    const u32 d = c + 1;
                    PushTriangle(mesh, a, c, b);
                    PushTriangle(mesh, b, c, d);
                }
            }
            return mesh;
        }

        PrimitiveMesh MakeCylinder(const PrimitiveMeshBuildDesc& desc)
        {
            PrimitiveMesh mesh{};
            mesh.name = desc.name.empty() ? "cylinder" : desc.name;
            mesh.kind = PrimitiveMeshKind::Cylinder;
            mesh.bounds = MakeEmptyAABB3();
            const u32 segments = ClampSegments(desc.segments, 6, 96);
            const float radius = std::max(desc.radius, 0.001f);
            const float h = std::max(desc.halfHeight, 0.001f);

            for (u32 i = 0; i < segments; ++i)
            {
                const float a0 = (static_cast<float>(i) / segments) * Pi * 2.0f;
                const float a1 = (static_cast<float>(i + 1) / segments) * Pi * 2.0f;
                Vec3 p0{std::cos(a0) * radius, -h, std::sin(a0) * radius};
                Vec3 p1{std::cos(a1) * radius, -h, std::sin(a1) * radius};
                Vec3 p2{std::cos(a1) * radius,  h, std::sin(a1) * radius};
                Vec3 p3{std::cos(a0) * radius,  h, std::sin(a0) * radius};
                PushQuad(mesh, p0, p1, p2, p3, Normalize(Vec3{p0.x + p1.x, 0.0f, p0.z + p1.z}), desc.color);

                const u32 top = static_cast<u32>(mesh.vertices.size());
                PushVertex(mesh, {0.0f, h, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f,0.5f}, desc.color);
                PushVertex(mesh, p3, {0.0f, 1.0f, 0.0f}, {0.0f,0.0f}, desc.color);
                PushVertex(mesh, p2, {0.0f, 1.0f, 0.0f}, {1.0f,0.0f}, desc.color);
                PushTriangle(mesh, top, top + 1, top + 2);

                const u32 bottom = static_cast<u32>(mesh.vertices.size());
                PushVertex(mesh, {0.0f, -h, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f,0.5f}, desc.color);
                PushVertex(mesh, p1, {0.0f, -1.0f, 0.0f}, {1.0f,0.0f}, desc.color);
                PushVertex(mesh, p0, {0.0f, -1.0f, 0.0f}, {0.0f,0.0f}, desc.color);
                PushTriangle(mesh, bottom, bottom + 1, bottom + 2);
            }
            return mesh;
        }

        PrimitiveMesh MakeCapsule(const PrimitiveMeshBuildDesc& desc)
        {
            PrimitiveMesh mesh = MakeUvSphere(desc);
            mesh.name = desc.name.empty() ? "capsule" : desc.name;
            mesh.kind = PrimitiveMeshKind::Capsule;
            const float halfHeight = std::max(desc.halfHeight, 0.001f);
            mesh.bounds = MakeEmptyAABB3();
            for (PrimitiveVertex& v : mesh.vertices)
            {
                v.position.y += v.position.y >= 0.0f ? halfHeight : -halfHeight;
                mesh.bounds = Expand(mesh.bounds, v.position);
            }

            PrimitiveMesh cylinder = MakeCylinder({PrimitiveMeshKind::Cylinder, "capsule_body", desc.radius, halfHeight, desc.size, desc.segments, desc.rings, desc.color});
            const u32 base = static_cast<u32>(mesh.vertices.size());
            mesh.vertices.insert(mesh.vertices.end(), cylinder.vertices.begin(), cylinder.vertices.end());
            for (u32 index : cylinder.indices)
            {
                mesh.indices.push_back(base + index);
            }
            mesh.bounds = Union(mesh.bounds, cylinder.bounds);
            return mesh;
        }
    }

    PrimitiveMesh BuildPrimitiveMesh(const PrimitiveMeshBuildDesc& desc)
    {
        PrimitiveMesh mesh{};
        switch (desc.kind)
        {
            case PrimitiveMeshKind::Triangle: mesh = MakeTriangle(desc); break;
            case PrimitiveMeshKind::Plane: mesh = MakePlane(desc); break;
            case PrimitiveMeshKind::Cube: mesh = MakeCube(desc); break;
            case PrimitiveMeshKind::UvSphere: mesh = MakeUvSphere(desc); break;
            case PrimitiveMeshKind::Cylinder: mesh = MakeCylinder(desc); break;
            case PrimitiveMeshKind::Capsule: mesh = MakeCapsule(desc); break;
            case PrimitiveMeshKind::Grid: mesh = MakePlane(desc); mesh.kind = PrimitiveMeshKind::Grid; mesh.name = desc.name.empty() ? "grid" : desc.name; break;
            default: mesh = MakeCube(desc); break;
        }
        mesh.valid = !mesh.vertices.empty() && mesh.indices.size() >= 3 && IsValid(mesh.bounds);
        return mesh;
    }

    PrimitiveTestScene BuildDefaultPrimitiveTestScene()
    {
        PrimitiveTestScene scene{};
        scene.reversedZ = true;
        scene.cameraPosition = {4.0f, 3.0f, 6.0f};
        scene.cameraTarget = {0.0f, 0.4f, 0.0f};
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::Triangle, "triangle", 0.5f, 0.5f, 1.0f, 3, 3, {1.0f, 0.36f, 0.25f, 1.0f}}));
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::Plane, "floor", 0.5f, 0.5f, 8.0f, 2, 2, {0.35f, 0.37f, 0.39f, 1.0f}}));
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::Cube, "cube", 0.5f, 0.5f, 1.0f, 4, 4, {0.25f, 0.58f, 1.0f, 1.0f}}));
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::UvSphere, "sphere", 0.5f, 0.5f, 1.0f, 24, 12, {0.95f, 0.75f, 0.25f, 1.0f}}));
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::Cylinder, "cylinder", 0.45f, 0.65f, 1.0f, 24, 8, {0.38f, 0.95f, 0.48f, 1.0f}}));
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::Capsule, "capsule", 0.35f, 0.55f, 1.0f, 20, 8, {0.78f, 0.48f, 1.0f, 1.0f}}));
        scene.meshes.push_back(BuildPrimitiveMesh({PrimitiveMeshKind::Grid, "debug_grid", 0.5f, 0.5f, 10.0f, 10, 10, {0.18f, 0.22f, 0.28f, 1.0f}}));

        scene.objects.push_back({"floor", 1, {0.0f, -0.55f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}, {0.35f,0.37f,0.39f,1.0f}, true});
        scene.objects.push_back({"triangle", 0, {-2.2f, 0.25f, 0.0f}, {}, {1.0f,1.0f,1.0f}, {1.0f,0.36f,0.25f,1.0f}, true});
        scene.objects.push_back({"cube", 2, {-1.0f, 0.0f, 0.0f}, {}, {1.0f,1.0f,1.0f}, {0.25f,0.58f,1.0f,1.0f}, true});
        scene.objects.push_back({"sphere", 3, {0.35f, 0.0f, 0.0f}, {}, {1.0f,1.0f,1.0f}, {0.95f,0.75f,0.25f,1.0f}, true});
        scene.objects.push_back({"cylinder", 4, {1.65f, 0.1f, 0.0f}, {}, {1.0f,1.0f,1.0f}, {0.38f,0.95f,0.48f,1.0f}, true});
        scene.objects.push_back({"capsule", 5, {2.85f, 0.25f, 0.0f}, {}, {1.0f,1.0f,1.0f}, {0.78f,0.48f,1.0f,1.0f}, true});
        scene.valid = AnalyzePrimitiveTestScene(scene).valid;
        return scene;
    }

    PrimitiveMeshStats AnalyzePrimitiveTestScene(const PrimitiveTestScene& scene)
    {
        PrimitiveMeshStats stats{};
        stats.meshes = static_cast<u32>(scene.meshes.size());
        stats.objects = static_cast<u32>(scene.objects.size());
        stats.sceneBounds = MakeEmptyAABB3();

        for (const PrimitiveMesh& mesh : scene.meshes)
        {
            if (!mesh.valid)
            {
                ++stats.validationWarnings;
            }
            stats.vertices += static_cast<u32>(mesh.vertices.size());
            stats.indices += static_cast<u32>(mesh.indices.size());
            stats.triangles += static_cast<u32>(mesh.indices.size() / 3u);
            stats.sceneBounds = Union(stats.sceneBounds, mesh.bounds);
            for (const PrimitiveVertex& v : mesh.vertices)
            {
                stats.finite = stats.finite && IsFinite(v.position) && IsFinite(v.normal);
            }
        }

        for (const PrimitiveSceneObject& object : scene.objects)
        {
            if (object.meshIndex >= scene.meshes.size() || !object.visible)
            {
                ++stats.validationWarnings;
            }
            else
            {
                ++stats.drawCalls;
            }
            stats.finite = stats.finite && IsFinite(object.position) && IsFinite(object.scale);
        }

        stats.valid = stats.meshes > 0 && stats.drawCalls > 0 && stats.vertices > 0 && stats.indices > 0 && stats.triangles > 0 && stats.validationWarnings == 0 && stats.finite;
        std::ostringstream out;
        out << "primitive_scene meshes=" << stats.meshes
            << " objects=" << stats.objects
            << " draw_calls=" << stats.drawCalls
            << " vertices=" << stats.vertices
            << " indices=" << stats.indices
            << " triangles=" << stats.triangles
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.validationWarnings;
        stats.summary = out.str();
        return stats;
    }

    std::vector<VulkanDrawCallPlan> BuildDrawCallPlan(const PrimitiveTestScene& scene)
    {
        std::vector<VulkanDrawCallPlan> result;
        result.reserve(scene.objects.size());
        for (const PrimitiveSceneObject& object : scene.objects)
        {
            if (!object.visible || object.meshIndex >= scene.meshes.size())
            {
                continue;
            }
            const PrimitiveMesh& mesh = scene.meshes[object.meshIndex];
            VulkanDrawCallPlan draw{};
            draw.name = object.name;
            draw.vertexCount = static_cast<u32>(mesh.vertices.size());
            draw.indexCount = static_cast<u32>(mesh.indices.size());
            draw.instanceCount = 1;
            draw.indexed = true;
            draw.valid = mesh.valid;
            result.push_back(draw);
        }
        return result;
    }

    const char* ToString(PrimitiveMeshKind kind)
    {
        switch (kind)
        {
            case PrimitiveMeshKind::Triangle: return "Triangle";
            case PrimitiveMeshKind::Plane: return "Plane";
            case PrimitiveMeshKind::Cube: return "Cube";
            case PrimitiveMeshKind::UvSphere: return "UvSphere";
            case PrimitiveMeshKind::Cylinder: return "Cylinder";
            case PrimitiveMeshKind::Capsule: return "Capsule";
            case PrimitiveMeshKind::Grid: return "Grid";
            default: return "Unknown";
        }
    }

    std::string ToDebugString(const PrimitiveMeshStats& stats)
    {
        return stats.summary;
    }
}
