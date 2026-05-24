#include <AK/Render/MeshOptimization.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace AK
{
    namespace
    {
        constexpr u32 InvalidU32 = 0xFFFFFFFFu;

        struct QuantizedVertexKey
        {
            i32 px = 0;
            i32 py = 0;
            i32 pz = 0;
            i32 nx = 0;
            i32 ny = 0;
            i32 nz = 0;
            i32 u = 0;
            i32 v = 0;
            u32 color = 0;
            u32 material = 0;

            bool operator<(const QuantizedVertexKey& rhs) const
            {
                return std::tie(px, py, pz, nx, ny, nz, u, v, color, material) <
                       std::tie(rhs.px, rhs.py, rhs.pz, rhs.nx, rhs.ny, rhs.nz, rhs.u, rhs.v, rhs.color, rhs.material);
            }
        };

        struct LocalVertex
        {
            PrimitiveVertex vertex{};
            u32 materialId = 0;
        };

        u16 PackUnorm16(float value)
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return static_cast<u16>(std::lround(clamped * 65535.0f));
        }

        float UnpackUnorm16(u16 value)
        {
            return static_cast<float>(value) / 65535.0f;
        }

        i16 PackSnorm16(float value)
        {
            const float clamped = std::clamp(value, -1.0f, 1.0f);
            return static_cast<i16>(std::lround(clamped * 32767.0f));
        }

        float UnpackSnorm16(i16 value)
        {
            return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
        }

        u32 PackRgba8(Vec4 c)
        {
            const u32 r = static_cast<u32>(std::lround(Saturate(c.x) * 255.0f));
            const u32 g = static_cast<u32>(std::lround(Saturate(c.y) * 255.0f));
            const u32 b = static_cast<u32>(std::lround(Saturate(c.z) * 255.0f));
            const u32 a = static_cast<u32>(std::lround(Saturate(c.w) * 255.0f));
            return r | (g << 8u) | (b << 16u) | (a << 24u);
        }

        Vec4 UnpackRgba8(u32 c)
        {
            return {
                static_cast<float>(c & 0xFFu) / 255.0f,
                static_cast<float>((c >> 8u) & 0xFFu) / 255.0f,
                static_cast<float>((c >> 16u) & 0xFFu) / 255.0f,
                static_cast<float>((c >> 24u) & 0xFFu) / 255.0f
            };
        }

        u32 PackOctNormal(Vec3 n)
        {
            n = Normalize(n, {0.0f, 1.0f, 0.0f});
            const float invL1 = SafeDivide(1.0f, std::abs(n.x) + std::abs(n.y) + std::abs(n.z), 1.0f);
            float x = n.x * invL1;
            float y = n.y * invL1;

            if (n.z < 0.0f)
            {
                const float oldX = x;
                x = (1.0f - std::abs(y)) * (oldX >= 0.0f ? 1.0f : -1.0f);
                y = (1.0f - std::abs(oldX)) * (y >= 0.0f ? 1.0f : -1.0f);
            }

            const u16 px = static_cast<u16>(PackSnorm16(x));
            const u16 py = static_cast<u16>(PackSnorm16(y));
            return static_cast<u32>(px) | (static_cast<u32>(py) << 16u);
        }

        Vec3 UnpackOctNormal(u32 packed)
        {
            const i16 sx = static_cast<i16>(packed & 0xFFFFu);
            const i16 sy = static_cast<i16>((packed >> 16u) & 0xFFFFu);
            float x = UnpackSnorm16(sx);
            float y = UnpackSnorm16(sy);
            float z = 1.0f - std::abs(x) - std::abs(y);

            if (z < 0.0f)
            {
                const float oldX = x;
                x = (1.0f - std::abs(y)) * (oldX >= 0.0f ? 1.0f : -1.0f);
                y = (1.0f - std::abs(oldX)) * (y >= 0.0f ? 1.0f : -1.0f);
            }

            return Normalize({x, y, z}, {0.0f, 1.0f, 0.0f});
        }

        QuantizedVertexKey MakeKey(const PrimitiveVertex& v, u32 materialId, const RenderMeshOptimizationConfig& config)
        {
            const float pStep = std::max(config.quantizationPositionStep, 1.0e-7f);
            const float nStep = std::max(config.quantizationNormalStep, 1.0e-7f);
            QuantizedVertexKey key{};
            key.px = static_cast<i32>(std::llround(v.position.x / pStep));
            key.py = static_cast<i32>(std::llround(v.position.y / pStep));
            key.pz = static_cast<i32>(std::llround(v.position.z / pStep));
            key.nx = static_cast<i32>(std::llround(v.normal.x / nStep));
            key.ny = static_cast<i32>(std::llround(v.normal.y / nStep));
            key.nz = static_cast<i32>(std::llround(v.normal.z / nStep));
            key.u = static_cast<i32>(std::llround(v.uv.x * 65535.0f));
            key.v = static_cast<i32>(std::llround(v.uv.y * 65535.0f));
            key.color = PackRgba8(v.color);
            key.material = materialId;
            return key;
        }

        Vec3 TransformSceneVertex(const PrimitiveVertex& v, const PrimitiveSceneObject& object)
        {
            Vec3 p{v.position.x * object.scale.x, v.position.y * object.scale.y, v.position.z * object.scale.z};
            p = Add(p, object.position);
            return p;
        }

        Vec3 BlendColor(Vec4 a, Vec4 b)
        {
            return {a.x * b.x, a.y * b.y, a.z * b.z};
        }

        PrimitiveVertex MakeObjectVertex(const PrimitiveVertex& source, const PrimitiveSceneObject& object)
        {
            PrimitiveVertex out = source;
            out.position = TransformSceneVertex(source, object);
            out.normal = Normalize(source.normal, {0.0f, 1.0f, 0.0f});
            const Vec3 color = BlendColor(source.color, object.color);
            out.color = {color.x, color.y, color.z, source.color.w * object.color.w};
            return out;
        }

        float TriangleCacheScore(const std::vector<u32>& indices, u32 cacheSize)
        {
            if (indices.empty() || cacheSize == 0)
            {
                return 0.0f;
            }

            std::vector<u32> cache;
            cache.reserve(cacheSize);
            u32 misses = 0;
            for (u32 idx : indices)
            {
                const auto it = std::find(cache.begin(), cache.end(), idx);
                if (it == cache.end())
                {
                    ++misses;
                    cache.insert(cache.begin(), idx);
                    if (cache.size() > cacheSize)
                    {
                        cache.pop_back();
                    }
                }
                else
                {
                    const u32 value = *it;
                    cache.erase(it);
                    cache.insert(cache.begin(), value);
                }
            }

            return SafeDivide(static_cast<float>(misses), static_cast<float>(indices.size() / 3u), 0.0f);
        }

        std::vector<u32> OptimizeTriangleOrderGreedy(const std::vector<u32>& indices, u32 cacheSize)
        {
            if (indices.size() < 6 || cacheSize == 0)
            {
                return indices;
            }

            const u32 triangleCount = static_cast<u32>(indices.size() / 3u);
            std::vector<u8> used(triangleCount, 0);
            std::vector<u32> result;
            result.reserve(indices.size());
            std::vector<u32> cache;
            cache.reserve(cacheSize);

            u32 remaining = triangleCount;
            u32 scanStart = 0;
            while (remaining > 0)
            {
                i32 best = -1;
                i32 bestScore = -1;

                const u32 scanLimit = std::min(triangleCount, scanStart + 512u);
                for (u32 t = scanStart; t < scanLimit; ++t)
                {
                    if (used[t])
                    {
                        continue;
                    }

                    i32 score = 0;
                    for (u32 c = 0; c < cache.size(); ++c)
                    {
                        const u32 cv = cache[c];
                        for (u32 k = 0; k < 3; ++k)
                        {
                            if (indices[t * 3u + k] == cv)
                            {
                                score += static_cast<i32>(cacheSize - std::min<u32>(c, cacheSize - 1u));
                            }
                        }
                    }

                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = static_cast<i32>(t);
                    }
                }

                if (best < 0)
                {
                    for (u32 t = 0; t < triangleCount; ++t)
                    {
                        if (!used[t])
                        {
                            best = static_cast<i32>(t);
                            break;
                        }
                    }
                }

                const u32 tri = static_cast<u32>(best);
                used[tri] = 1;
                --remaining;
                scanStart = tri;

                for (u32 k = 0; k < 3; ++k)
                {
                    const u32 idx = indices[tri * 3u + k];
                    result.push_back(idx);
                    const auto it = std::find(cache.begin(), cache.end(), idx);
                    if (it != cache.end())
                    {
                        cache.erase(it);
                    }
                    cache.insert(cache.begin(), idx);
                    if (cache.size() > cacheSize)
                    {
                        cache.pop_back();
                    }
                }
            }

            return result;
        }

        std::vector<u32> BuildLodIndices(const std::vector<u32>& source, u32 level)
        {
            if (level == 0 || source.size() < 12)
            {
                return source;
            }

            const u32 stride = 1u << std::min(level, 4u);
            std::vector<u32> result;
            result.reserve(source.size() / stride + 3u);
            const u32 triangleCount = static_cast<u32>(source.size() / 3u);
            for (u32 t = 0; t < triangleCount; t += stride)
            {
                result.push_back(source[t * 3u + 0u]);
                result.push_back(source[t * 3u + 1u]);
                result.push_back(source[t * 3u + 2u]);
            }

            if (result.size() < 3)
            {
                result.assign(source.begin(), source.begin() + std::min<std::size_t>(source.size(), 3));
            }
            return result;
        }

        u32 AddOrReuseVertex(std::vector<LocalVertex>& localVertices,
                             std::vector<PackedRenderVertex32>& packedVertices,
                             std::map<QuantizedVertexKey, u32>& weldMap,
                             const PrimitiveVertex& vertex,
                             u32 materialId,
                             const RenderMeshOptimizationConfig& config,
                             u32& weldedVertices)
        {
            const QuantizedVertexKey key = MakeKey(vertex, materialId, config);
            if (config.weldVertices)
            {
                const auto found = weldMap.find(key);
                if (found != weldMap.end())
                {
                    ++weldedVertices;
                    return found->second;
                }
            }

            const u32 index = static_cast<u32>(packedVertices.size());
            localVertices.push_back({vertex, materialId});
            packedVertices.push_back(PackRenderVertex32(vertex, materialId, 0));
            weldMap.emplace(key, index);
            return index;
        }

        void BuildMeshletsForSection(const OptimizedRenderGeometry& geometry,
                                     const RenderMeshSection& section,
                                     u32 sectionIndex,
                                     const RenderMeshOptimizationConfig& config,
                                     std::vector<RenderMeshlet>& outMeshlets)
        {
            if (!config.generateMeshlets || section.indexCount < 3)
            {
                return;
            }

            const u32 maxTriangles = std::max(config.maxMeshletTriangles, 1u);
            const u32 end = section.firstIndex + section.indexCount;
            u32 cursor = section.firstIndex;
            while (cursor < end)
            {
                RenderMeshlet meshlet{};
                meshlet.sectionIndex = sectionIndex;
                meshlet.firstIndex = cursor;
                meshlet.bounds = MakeEmptyAABB3();
                std::set<u32> unique;
                Vec3 normalSum{};
                u32 triCount = 0;

                while (cursor + 2u < end && triCount < maxTriangles)
                {
                    const u32 a = geometry.indices[cursor + 0u];
                    const u32 b = geometry.indices[cursor + 1u];
                    const u32 c = geometry.indices[cursor + 2u];
                    std::set<u32> test = unique;
                    test.insert(a);
                    test.insert(b);
                    test.insert(c);
                    if (!unique.empty() && test.size() > config.maxMeshletVertices)
                    {
                        break;
                    }
                    unique.swap(test);
                    PrimitiveVertex va = UnpackRenderVertex32(geometry.vertices[a]);
                    PrimitiveVertex vb = UnpackRenderVertex32(geometry.vertices[b]);
                    PrimitiveVertex vc = UnpackRenderVertex32(geometry.vertices[c]);
                    meshlet.bounds = Expand(meshlet.bounds, va.position);
                    meshlet.bounds = Expand(meshlet.bounds, vb.position);
                    meshlet.bounds = Expand(meshlet.bounds, vc.position);
                    normalSum = Add(normalSum, Normalize(Cross(Subtract(vb.position, va.position), Subtract(vc.position, va.position)), {0.0f, 1.0f, 0.0f}));
                    cursor += 3u;
                    ++triCount;
                }

                meshlet.indexCount = cursor - meshlet.firstIndex;
                meshlet.triangleCount = triCount;
                meshlet.vertexCount = static_cast<u32>(unique.size());
                meshlet.firstVertex = unique.empty() ? 0u : *unique.begin();
                meshlet.coneAxis = Normalize(normalSum, {0.0f, 1.0f, 0.0f});
                meshlet.coneCutoff = -0.2f;
                meshlet.valid = triCount > 0 && IsValid(meshlet.bounds);
                outMeshlets.push_back(meshlet);
            }
        }

        u64 BytesForFullVertices(u32 count)
        {
            return static_cast<u64>(count) * static_cast<u64>(sizeof(PrimitiveVertex));
        }

        bool IsFiniteVertex(const PrimitiveVertex& v)
        {
            return IsFinite(v.position) && IsFinite(v.normal) && IsFinite(v.uv.x) && IsFinite(v.uv.y) && IsFinite(v.color.x) && IsFinite(v.color.y) && IsFinite(v.color.z) && IsFinite(v.color.w);
        }
    }

    RenderMeshOptimizationConfig MakeDefaultRenderMeshOptimizationConfig()
    {
        RenderMeshOptimizationConfig config{};
        return config;
    }

    MeshingSourceStudyReport BuildMeshingMainStudyReport()
    {
        MeshingSourceStudyReport report{};
        report.usefulIdeaNames = {
            "height/min-max ranges avoid scanning empty space",
            "six-direction visited masks avoid duplicate face work",
            "greedy contiguous face/run merging",
            "temporary mesh arenas recycled after upload",
            "dirty chunk rebuild policy",
            "front-to-back traversal from camera direction",
            "combined buffer upload after CPU generation",
            "compact vertex layout and shader-derived face UVs"
        };
        report.akEngineAdaptations = {
            "generic polygon mesh sections instead of fixed voxel chunks",
            "combined Vulkan vertex/index buffer plan for all scene primitives",
            "32-byte packed vertex contract instead of 48-byte full-float vertex",
            "deterministic vertex welding with quantized keys",
            "meshlet ranges for future GPU culling/indirect draws",
            "LOD index ranges for future streaming and visibility budgets",
            "front-to-back draw command sorting independent of OpenGL state",
            "dirty section states ready for partial buffer updates"
        };
        report.usefulIdeas = static_cast<u32>(report.usefulIdeaNames.size());
        report.rejectedUnityOrOpenGlLimits = 4;
        std::ostringstream out;
        out << "meshing-main study useful=" << report.usefulIdeas
            << " adapted_to_polygons=" << (report.adaptedToPolygonMeshes ? "true" : "false")
            << " vulkan_friendly=" << (report.vulkanFriendly ? "true" : "false")
            << " rejected_api_limits=" << report.rejectedUnityOrOpenGlLimits;
        report.summary = out.str();
        return report;
    }

    PackedRenderVertex32 PackRenderVertex32(const PrimitiveVertex& vertex, u32 materialId, u32 flags)
    {
        PackedRenderVertex32 packed{};
        packed.px = vertex.position.x;
        packed.py = vertex.position.y;
        packed.pz = vertex.position.z;
        packed.normalOctSnorm16x2 = PackOctNormal(vertex.normal);
        const u32 u = PackUnorm16(vertex.uv.x);
        const u32 v = PackUnorm16(vertex.uv.y);
        packed.uvUnorm16x2 = u | (v << 16u);
        packed.colorRgba8 = PackRgba8(vertex.color);
        packed.materialAndFlags = (materialId & 0xFFFFu) | ((flags & 0xFFFFu) << 16u);
        return packed;
    }

    PrimitiveVertex UnpackRenderVertex32(const PackedRenderVertex32& packed)
    {
        PrimitiveVertex v{};
        v.position = {packed.px, packed.py, packed.pz};
        v.normal = UnpackOctNormal(packed.normalOctSnorm16x2);
        v.uv = {UnpackUnorm16(static_cast<u16>(packed.uvUnorm16x2 & 0xFFFFu)), UnpackUnorm16(static_cast<u16>((packed.uvUnorm16x2 >> 16u) & 0xFFFFu))};
        v.color = UnpackRgba8(packed.colorRgba8);
        return v;
    }

    OptimizedRenderGeometry OptimizePrimitiveSceneForVulkan(const PrimitiveTestScene& scene, const RenderMeshOptimizationConfig& config)
    {
        OptimizedRenderGeometry geometry{};
        geometry.bounds = MakeEmptyAABB3();
        std::vector<LocalVertex> localVertices;
        std::map<QuantizedVertexKey, u32> weldMap;
        u32 welded = 0;
        (void)welded;

        for (u32 objectIndex = 0; objectIndex < scene.objects.size(); ++objectIndex)
        {
            const PrimitiveSceneObject& object = scene.objects[objectIndex];
            if (!object.visible || object.meshIndex >= scene.meshes.size())
            {
                continue;
            }

            const PrimitiveMesh& mesh = scene.meshes[object.meshIndex];
            if (!mesh.valid || mesh.indices.size() < 3)
            {
                continue;
            }

            RenderMeshSection section{};
            section.name = object.name;
            section.meshIndex = object.meshIndex;
            section.objectIndex = objectIndex;
            section.firstVertex = static_cast<u32>(geometry.vertices.size());
            section.firstIndex = static_cast<u32>(geometry.indices.size());
            section.materialId = object.meshIndex + 1u;
            section.state = RenderSectionState::Dirty;
            section.bounds = MakeEmptyAABB3();
            std::map<u32, u32> remap;
            std::vector<u32> localIndices;
            localIndices.reserve(mesh.indices.size());

            for (u32 sourceIndex : mesh.indices)
            {
                if (sourceIndex >= mesh.vertices.size())
                {
                    continue;
                }

                const auto found = remap.find(sourceIndex);
                if (found != remap.end())
                {
                    localIndices.push_back(found->second);
                    continue;
                }

                PrimitiveVertex vertex = MakeObjectVertex(mesh.vertices[sourceIndex], object);
                section.bounds = Expand(section.bounds, vertex.position);
                geometry.bounds = Expand(geometry.bounds, vertex.position);
                const u32 globalIndex = AddOrReuseVertex(localVertices, geometry.vertices, weldMap, vertex, section.materialId, config, welded);
                remap.emplace(sourceIndex, globalIndex);
                localIndices.push_back(globalIndex);
            }

            if (config.optimizeTriangleOrder)
            {
                std::vector<u32> optimizedIndices = OptimizeTriangleOrderGreedy(localIndices, config.targetVertexCacheSize);
                const float beforeScore = TriangleCacheScore(localIndices, config.targetVertexCacheSize);
                const float afterScore = TriangleCacheScore(optimizedIndices, config.targetVertexCacheSize);
                if (afterScore <= beforeScore + 0.01f)
                {
                    localIndices = std::move(optimizedIndices);
                }
            }

            for (u32 idx : localIndices)
            {
                geometry.indices.push_back(idx);
            }

            section.vertexCount = static_cast<u32>(geometry.vertices.size()) - section.firstVertex;
            section.indexCount = static_cast<u32>(geometry.indices.size()) - section.firstIndex;
            section.valid = section.indexCount >= 3 && IsValid(section.bounds);
            const u32 sectionIndex = static_cast<u32>(geometry.sections.size());
            geometry.sections.push_back(section);

            const u32 maxLods = config.generateLodIndices ? std::max(1u, config.lodLevels) : 1u;
            for (u32 level = 0; level < maxLods; ++level)
            {
                RenderMeshLodRange lod{};
                lod.level = level;
                lod.screenHeightThreshold = config.lod0ScreenHeight * std::pow(std::max(config.lodTransitionFactor, 0.01f), static_cast<float>(level));

                if (level == 0)
                {
                    lod.firstIndex = section.firstIndex;
                    lod.indexCount = section.indexCount;
                }
                else
                {
                    std::vector<u32> lodIndices = BuildLodIndices(localIndices, level);
                    lod.firstIndex = static_cast<u32>(geometry.indices.size());
                    lod.indexCount = static_cast<u32>(lodIndices.size());
                    geometry.indices.insert(geometry.indices.end(), lodIndices.begin(), lodIndices.end());
                }

                lod.triangleCount = lod.indexCount / 3u;
                lod.generated = lod.indexCount >= 3;
                geometry.lodRanges.push_back(lod);

                RenderDrawCommand draw{};
                draw.name = object.name + ":lod" + std::to_string(level);
                draw.sectionIndex = sectionIndex;
                draw.lodLevel = level;
                draw.firstIndex = lod.firstIndex;
                draw.indexCount = lod.indexCount;
                draw.vertexOffset = 0;
                draw.instanceCount = 1;
                draw.materialId = section.materialId;
                draw.cameraDistance = Length(Subtract(Center(section.bounds), scene.cameraPosition));
                draw.indexed = true;
                draw.valid = section.valid && lod.generated;
                geometry.drawCommands.push_back(draw);
            }

            BuildMeshletsForSection(geometry, section, sectionIndex, config, geometry.meshlets);
        }

        if (config.frontToBackSort)
        {
            std::stable_sort(geometry.drawCommands.begin(), geometry.drawCommands.end(), [](const RenderDrawCommand& a, const RenderDrawCommand& b)
            {
                if (a.lodLevel != b.lodLevel)
                {
                    return a.lodLevel < b.lodLevel;
                }
                if (a.materialId != b.materialId)
                {
                    return a.materialId < b.materialId;
                }
                return a.cameraDistance < b.cameraDistance;
            });
        }

        geometry.valid = !geometry.vertices.empty() && !geometry.indices.empty() && !geometry.drawCommands.empty() && IsValid(geometry.bounds);
        return geometry;
    }

    RenderMeshOptimizationStats AnalyzeOptimizedRenderGeometry(const PrimitiveTestScene& source, const OptimizedRenderGeometry& geometry, const RenderMeshOptimizationConfig& config)
    {
        RenderMeshOptimizationStats stats{};
        stats.sourceMeshes = static_cast<u32>(source.meshes.size());
        stats.sourceObjects = static_cast<u32>(source.objects.size());
        stats.sections = static_cast<u32>(geometry.sections.size());
        stats.lodRanges = static_cast<u32>(geometry.lodRanges.size());
        stats.meshlets = static_cast<u32>(geometry.meshlets.size());
        stats.drawCommands = static_cast<u32>(geometry.drawCommands.size());
        stats.outputVertices = static_cast<u32>(geometry.vertices.size());
        stats.outputIndices = static_cast<u32>(geometry.indices.size());
        stats.outputTriangles = stats.outputIndices / 3u;
        stats.combinedBuffers = config.combineVertexBuffers && config.combineIndexBuffers;
        stats.packedVertices = config.vertexPacking == RenderVertexPackingMode::Packed32Byte;
        stats.lodGenerated = config.generateLodIndices && stats.lodRanges > stats.sections;

        std::vector<u32> sourceIndices;
        sourceIndices.reserve(4096);
        for (const PrimitiveMesh& mesh : source.meshes)
        {
            stats.sourceVertices += static_cast<u32>(mesh.vertices.size());
            stats.sourceIndices += static_cast<u32>(mesh.indices.size());
            stats.sourceTriangles += static_cast<u32>(mesh.indices.size() / 3u);
            sourceIndices.insert(sourceIndices.end(), mesh.indices.begin(), mesh.indices.end());
            for (const PrimitiveVertex& v : mesh.vertices)
            {
                stats.finite = stats.finite && IsFiniteVertex(v);
            }
        }

        for (const PackedRenderVertex32& v : geometry.vertices)
        {
            PrimitiveVertex unpacked = UnpackRenderVertex32(v);
            stats.finite = stats.finite && IsFiniteVertex(unpacked);
        }

        for (const RenderMeshSection& section : geometry.sections)
        {
            if (!section.valid)
            {
                ++stats.validationWarnings;
            }
            if (section.state == RenderSectionState::Clean && config.dirtySectionSkipping)
            {
                ++stats.skippedCleanSections;
            }
        }

        for (const RenderDrawCommand& draw : geometry.drawCommands)
        {
            if (!draw.valid)
            {
                ++stats.validationWarnings;
            }
        }

        for (const RenderMeshlet& meshlet : geometry.meshlets)
        {
            if (!meshlet.valid)
            {
                ++stats.validationWarnings;
            }
        }

        stats.sourceVertexBytes = BytesForFullVertices(stats.sourceVertices);
        stats.packedVertexBytes = static_cast<u64>(geometry.vertices.size()) * static_cast<u64>(sizeof(PackedRenderVertex32));
        stats.indexBytes = static_cast<u64>(geometry.indices.size()) * static_cast<u64>(sizeof(u32));
        stats.weldedVertices = stats.sourceVertices > stats.outputVertices ? stats.sourceVertices - stats.outputVertices : 0u;
        stats.vertexByteSavingsPercent = stats.sourceVertexBytes > 0
            ? 100.0 * (1.0 - static_cast<double>(stats.packedVertexBytes) / static_cast<double>(stats.sourceVertexBytes))
            : 0.0;
        std::vector<u32> lod0Indices;
        for (const RenderMeshSection& section : geometry.sections)
        {
            const u32 end = std::min<u32>(section.firstIndex + section.indexCount, static_cast<u32>(geometry.indices.size()));
            for (u32 i = section.firstIndex; i < end; ++i)
            {
                lod0Indices.push_back(geometry.indices[i]);
            }
        }
        stats.cacheScoreBefore = TriangleCacheScore(sourceIndices, config.targetVertexCacheSize);
        stats.cacheScoreAfter = TriangleCacheScore(lod0Indices, config.targetVertexCacheSize);

        stats.valid = geometry.valid && stats.finite && stats.validationWarnings == 0 && stats.outputVertices > 0 && stats.outputIndices > 0 && stats.drawCommands > 0;
        std::ostringstream out;
        out << "mesh_opt source_v=" << stats.sourceVertices
            << " packed_v=" << stats.outputVertices
            << " indices=" << stats.outputIndices
            << " draws=" << stats.drawCommands
            << " meshlets=" << stats.meshlets
            << " lods=" << stats.lodRanges
            << " vertex_bytes=" << stats.sourceVertexBytes << "->" << stats.packedVertexBytes
            << " save=" << static_cast<int>(std::round(stats.vertexByteSavingsPercent)) << "%"
            << " cache=" << stats.cacheScoreBefore << "->" << stats.cacheScoreAfter
            << " warnings=" << stats.validationWarnings;
        stats.summary = out.str();
        return stats;
    }

    RenderMeshOptimizationProbe BuildRenderMeshOptimizationProbe()
    {
        RenderMeshOptimizationProbe probe{};
        probe.sourceStudy = BuildMeshingMainStudyReport();
        probe.config = MakeDefaultRenderMeshOptimizationConfig();
        probe.inputScene = BuildDefaultPrimitiveTestScene();
        probe.geometry = OptimizePrimitiveSceneForVulkan(probe.inputScene, probe.config);
        probe.stats = AnalyzeOptimizedRenderGeometry(probe.inputScene, probe.geometry, probe.config);
        probe.ok = probe.sourceStudy.studied && probe.sourceStudy.adaptedToPolygonMeshes && probe.geometry.valid && probe.stats.valid;
        std::ostringstream out;
        out << "render mesh optimization probe ok=" << (probe.ok ? "true" : "false")
            << " ideas=" << probe.sourceStudy.usefulIdeas
            << " draw_commands=" << probe.stats.drawCommands
            << " meshlets=" << probe.stats.meshlets
            << " vertex_savings=" << static_cast<int>(std::round(probe.stats.vertexByteSavingsPercent)) << "%";
        probe.summary = out.str();
        return probe;
    }

    const char* ToString(RenderGeometryOptimizationStrategy strategy)
    {
        switch (strategy)
        {
            case RenderGeometryOptimizationStrategy::Conservative: return "Conservative";
            case RenderGeometryOptimizationStrategy::Aggressive: return "Aggressive";
            case RenderGeometryOptimizationStrategy::StreamingWorld: return "StreamingWorld";
            case RenderGeometryOptimizationStrategy::NaniteInspired: return "NaniteInspired";
            default: return "Unknown";
        }
    }

    const char* ToString(RenderVertexPackingMode mode)
    {
        switch (mode)
        {
            case RenderVertexPackingMode::FullFloat: return "FullFloat";
            case RenderVertexPackingMode::Packed32Byte: return "Packed32Byte";
            case RenderVertexPackingMode::PackedPosition16Planned: return "PackedPosition16Planned";
            default: return "Unknown";
        }
    }

    const char* ToString(RenderLodPolicy policy)
    {
        switch (policy)
        {
            case RenderLodPolicy::Disabled: return "Disabled";
            case RenderLodPolicy::TriangleBudget: return "TriangleBudget";
            case RenderLodPolicy::ScreenError: return "ScreenError";
            case RenderLodPolicy::QemPlanned: return "QemPlanned";
            default: return "Unknown";
        }
    }

    const char* ToString(RenderSectionState state)
    {
        switch (state)
        {
            case RenderSectionState::Clean: return "Clean";
            case RenderSectionState::Dirty: return "Dirty";
            case RenderSectionState::RebuildRequired: return "RebuildRequired";
            case RenderSectionState::Evictable: return "Evictable";
            default: return "Unknown";
        }
    }

    std::string ToDebugString(const MeshingSourceStudyReport& report)
    {
        std::ostringstream out;
        out << report.summary;
        if (!report.usefulIdeaNames.empty())
        {
            out << " ideas=[";
            for (std::size_t i = 0; i < report.usefulIdeaNames.size(); ++i)
            {
                if (i > 0) out << "; ";
                out << report.usefulIdeaNames[i];
            }
            out << "]";
        }
        return out.str();
    }

    std::string ToDebugString(const RenderMeshOptimizationStats& stats)
    {
        return stats.summary;
    }

    std::string ToDebugString(const RenderMeshOptimizationProbe& probe)
    {
        return probe.summary;
    }
}
