#pragma once

#include <AK/Core/Guid.hpp>
#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>
#include <AK/Terrain/Terrain.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace AK
{
    enum class SurfaceMaterialKind : u32
    {
        Default = 0,
        Concrete = 1,
        Wood = 2,
        Metal = 3,
        Glass = 4,
        TerrainSoil = 5,
        Grass = 6,
        Rock = 7,
        Sand = 8,
        Ice = 9,
        Water = 10,
        Rubber = 11,
        Custom = 12
    };

    enum class SurfaceDomain : u32
    {
        Generic = 0,
        Terrain = 1,
        Planet = 2,
        Destructible = 3,
        Fluid = 4,
        Character = 5
    };

    enum class SurfaceBlendMode : u32
    {
        Dominant = 0,
        WeightedAverage = 1
    };

    enum SurfaceMaterialFlags : u32
    {
        SurfaceMaterialFlag_None = 0,
        SurfaceMaterialFlag_Terrain = 1u << 0u,
        SurfaceMaterialFlag_Planet = 1u << 1u,
        SurfaceMaterialFlag_Destructible = 1u << 2u,
        SurfaceMaterialFlag_Fluid = 1u << 3u,
        SurfaceMaterialFlag_Slippery = 1u << 4u,
        SurfaceMaterialFlag_Climbable = 1u << 5u,
        SurfaceMaterialFlag_Footstep = 1u << 6u
    };

    struct SurfaceMaterialId
    {
        AssetGuid guid{};
        u32 slot = 0;

        constexpr bool IsValid() const
        {
            return guid.IsValid() || slot != 0;
        }
    };

    constexpr bool operator==(SurfaceMaterialId a, SurfaceMaterialId b)
    {
        return a.guid == b.guid && a.slot == b.slot;
    }

    constexpr bool operator!=(SurfaceMaterialId a, SurfaceMaterialId b)
    {
        return !(a == b);
    }

    struct SurfacePhysicsDesc
    {
        float densityKgPerCubicMeter = 1000.0f;
        float staticFriction = 0.7f;
        float dynamicFriction = 0.55f;
        float restitution = 0.05f;
        float rollingResistance = 0.02f;
        float hardness = 1.0f;
        float destructionResistance = 1.0f;
        float traction = 1.0f;
        float footstepLoudness = 1.0f;
    };

    struct SurfaceRenderDesc
    {
        Vec3 baseColor{0.8f, 0.8f, 0.8f};
        float roughness = 0.8f;
        float metallic = 0.0f;
        float alpha = 1.0f;
        bool alphaTest = false;
    };

    struct SurfaceMaterialDesc
    {
        SurfaceMaterialId id{};
        SurfaceMaterialKind kind = SurfaceMaterialKind::Default;
        SurfaceDomain domain = SurfaceDomain::Generic;
        std::string name = "default";
        SurfacePhysicsDesc physics{};
        SurfaceRenderDesc render{};
        u32 flags = SurfaceMaterialFlag_Footstep;
        bool valid = true;
    };

    struct SurfaceLayer
    {
        SurfaceMaterialId material{};
        float weight = 1.0f;
    };

    struct SurfaceSample
    {
        SurfaceDomain domain = SurfaceDomain::Generic;
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float heightMeters = 0.0f;
        float slopeDegrees = 0.0f;
        std::vector<SurfaceLayer> layers;
        SurfaceBlendMode blendMode = SurfaceBlendMode::WeightedAverage;
        bool hole = false;
        bool finite = true;
    };

    struct SurfaceContactMaterial
    {
        float staticFriction = 0.7f;
        float dynamicFriction = 0.55f;
        float restitution = 0.05f;
        float rollingResistance = 0.02f;
        float densityKgPerCubicMeter = 1000.0f;
        float hardness = 1.0f;
        float destructionResistance = 1.0f;
        float traction = 1.0f;
        u32 flags = SurfaceMaterialFlag_None;
        bool valid = true;
    };

    struct SurfaceValidationReport
    {
        bool ok = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct SurfaceRegistryStats
    {
        std::size_t materialCount = 0;
        std::size_t terrainMaterialCount = 0;
        std::size_t destructibleMaterialCount = 0;
        std::size_t fluidMaterialCount = 0;
        bool hasDefault = false;
        bool ok = false;
    };

    class SurfaceMaterialRegistry final
    {
    public:
        SurfaceMaterialId Add(SurfaceMaterialDesc material);
        bool Remove(SurfaceMaterialId id);
        const SurfaceMaterialDesc* Find(SurfaceMaterialId id) const;
        const SurfaceMaterialDesc* FindByKind(SurfaceMaterialKind kind) const;
        const SurfaceMaterialDesc* FindByName(std::string_view name) const;
        std::size_t Count() const;
        void Clear();
        SurfaceRegistryStats Stats() const;
        const std::vector<SurfaceMaterialDesc>& Materials() const;

    private:
        std::vector<SurfaceMaterialDesc> mMaterials;
        u32 mNextSlot = 1;
    };

    struct SurfaceProbeResult
    {
        bool ok = false;
        SurfaceRegistryStats registryStats{};
        SurfaceSample grassSample{};
        SurfaceSample steepSample{};
        SurfaceSample planetSample{};
        SurfaceContactMaterial rubberConcreteContact{};
        PhysicsStepStats physicsStats{};
        float lateralVelocityBefore = 0.0f;
        float lateralVelocityAfter = 0.0f;
        std::string summary;
    };

    const char* ToString(SurfaceMaterialKind kind);
    const char* ToString(SurfaceDomain domain);
    const char* ToString(SurfaceBlendMode mode);

    SurfaceMaterialId BuildSurfaceMaterialId(std::string_view stableName, u32 slot = 0);
    SurfaceMaterialDesc MakeSurfaceMaterial(SurfaceMaterialKind kind);
    SurfacePhysicsDesc SanitizeSurfacePhysics(SurfacePhysicsDesc physics);
    SurfaceMaterialDesc SanitizeSurfaceMaterial(SurfaceMaterialDesc material);
    SurfaceValidationReport ValidateSurfaceMaterial(const SurfaceMaterialDesc& material);
    SurfaceMaterialRegistry BuildDefaultSurfaceMaterialRegistry();

    SurfaceMaterialKind SelectTerrainSurfaceKind(const TerrainSample& sample);
    SurfaceSample BuildSurfaceSampleFromTerrain(const TerrainSample& sample, SurfaceDomain domain = SurfaceDomain::Terrain);
    SurfaceContactMaterial ResolveSurfaceSample(const SurfaceMaterialRegistry& registry, const SurfaceSample& sample);
    SurfaceContactMaterial CombineSurfaceContacts(const SurfaceContactMaterial& a, const SurfaceContactMaterial& b);
    SurfaceContactMaterial ContactMaterialFromSurface(const SurfaceMaterialDesc& material);

    void ApplySurfaceToPhysicsBody(PhysicsBody& body, const SurfaceContactMaterial& material, float volumeCubicMeters = 0.0f, bool updateMass = false);
    void ApplySurfaceToPhysicsBody(PhysicsBody& body, const SurfaceMaterialDesc& material, float volumeCubicMeters = 0.0f, bool updateMass = false);

    std::string ToDebugString(SurfaceMaterialId id);
    std::string ToDebugString(const SurfacePhysicsDesc& physics, int precision = 3);
    std::string ToDebugString(const SurfaceMaterialDesc& material, int precision = 3);
    std::string ToDebugString(const SurfaceContactMaterial& material, int precision = 3);
    std::string ToDebugString(const SurfaceSample& sample, int precision = 3);
    std::string ToDebugString(const SurfaceValidationReport& report);
    std::string ToDebugString(const SurfaceRegistryStats& stats);

    SurfaceProbeResult BuildSurfaceProbe();
    std::string BuildSurfaceProbeSummary();
}

namespace std
{
    template <>
    struct hash<AK::SurfaceMaterialId>
    {
        std::size_t operator()(AK::SurfaceMaterialId value) const noexcept
        {
            const std::uint64_t mixedGuid = value.guid.high ^ (value.guid.low + 0x9e3779b97f4a7c15ull + (value.guid.high << 6u) + (value.guid.high >> 2u));
            const std::uint64_t mixed = mixedGuid ^ (static_cast<std::uint64_t>(value.slot) + 0x9e3779b97f4a7c15ull + (mixedGuid << 6u) + (mixedGuid >> 2u));
            return std::hash<std::uint64_t>{}(mixed);
        }
    };
}
