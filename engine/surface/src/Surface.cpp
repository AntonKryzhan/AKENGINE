#include <AK/Surface/Surface.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr float MinimumDensity = 0.001f;
        constexpr float MaximumDensity = 100000.0f;
        constexpr float MinimumVolume = 1.0e-6f;

        float ClampNonNegative(float value)
        {
            return IsFinite(value) && value > 0.0f ? value : 0.0f;
        }

        float ClampUnit(float value)
        {
            if (!IsFinite(value))
            {
                return 0.0f;
            }
            return std::clamp(value, 0.0f, 1.0f);
        }

        float ClampPositive(float value, float fallback)
        {
            return IsFinite(value) && value > 0.0f ? value : fallback;
        }

        float MixWeighted(float current, float value, float weight)
        {
            return current + value * weight;
        }

        bool HasFlag(u32 flags, u32 flag)
        {
            return (flags & flag) == flag;
        }

        bool MatchesSurfaceMaterialId(SurfaceMaterialId stored, SurfaceMaterialId query)
        {
            if (stored == query)
            {
                return true;
            }
            if (stored.guid.IsValid() && query.guid.IsValid() && stored.guid == query.guid)
            {
                return stored.slot == 0 || query.slot == 0 || stored.slot == query.slot;
            }
            return false;
        }

        void PushError(SurfaceValidationReport& report, std::string message)
        {
            report.ok = false;
            report.errors.push_back(std::move(message));
        }

        void PushWarning(SurfaceValidationReport& report, std::string message)
        {
            report.warnings.push_back(std::move(message));
        }

        const SurfaceMaterialDesc* FallbackMaterial(const SurfaceMaterialRegistry& registry)
        {
            if (const SurfaceMaterialDesc* material = registry.FindByKind(SurfaceMaterialKind::Default))
            {
                return material;
            }
            return registry.Materials().empty() ? nullptr : &registry.Materials().front();
        }
    }

    const char* ToString(SurfaceMaterialKind kind)
    {
        switch (kind)
        {
            case SurfaceMaterialKind::Default:
                return "default";
            case SurfaceMaterialKind::Concrete:
                return "concrete";
            case SurfaceMaterialKind::Wood:
                return "wood";
            case SurfaceMaterialKind::Metal:
                return "metal";
            case SurfaceMaterialKind::Glass:
                return "glass";
            case SurfaceMaterialKind::TerrainSoil:
                return "terrain_soil";
            case SurfaceMaterialKind::Grass:
                return "grass";
            case SurfaceMaterialKind::Rock:
                return "rock";
            case SurfaceMaterialKind::Sand:
                return "sand";
            case SurfaceMaterialKind::Ice:
                return "ice";
            case SurfaceMaterialKind::Water:
                return "water";
            case SurfaceMaterialKind::Rubber:
                return "rubber";
            case SurfaceMaterialKind::Custom:
                return "custom";
            default:
                return "unknown";
        }
    }

    const char* ToString(SurfaceDomain domain)
    {
        switch (domain)
        {
            case SurfaceDomain::Generic:
                return "generic";
            case SurfaceDomain::Terrain:
                return "terrain";
            case SurfaceDomain::Planet:
                return "planet";
            case SurfaceDomain::Destructible:
                return "destructible";
            case SurfaceDomain::Fluid:
                return "fluid";
            case SurfaceDomain::Character:
                return "character";
            default:
                return "unknown";
        }
    }

    const char* ToString(SurfaceBlendMode mode)
    {
        switch (mode)
        {
            case SurfaceBlendMode::Dominant:
                return "dominant";
            case SurfaceBlendMode::WeightedAverage:
                return "weighted-average";
            default:
                return "unknown";
        }
    }

    SurfaceMaterialId BuildSurfaceMaterialId(std::string_view stableName, u32 slot)
    {
        std::string normalized = "ak://surface/";
        normalized.append(stableName.begin(), stableName.end());
        return {BuildAssetGuidFromNormalizedPath(normalized), slot};
    }

    SurfacePhysicsDesc SanitizeSurfacePhysics(SurfacePhysicsDesc physics)
    {
        physics.densityKgPerCubicMeter = std::clamp(ClampPositive(physics.densityKgPerCubicMeter, 1000.0f), MinimumDensity, MaximumDensity);
        physics.staticFriction = std::clamp(ClampNonNegative(physics.staticFriction), 0.0f, 4.0f);
        physics.dynamicFriction = std::clamp(ClampNonNegative(physics.dynamicFriction), 0.0f, physics.staticFriction);
        physics.restitution = ClampUnit(physics.restitution);
        physics.rollingResistance = std::clamp(ClampNonNegative(physics.rollingResistance), 0.0f, 1.0f);
        physics.hardness = std::clamp(ClampNonNegative(physics.hardness), 0.0f, 100.0f);
        physics.destructionResistance = std::clamp(ClampNonNegative(physics.destructionResistance), 0.0f, 100.0f);
        physics.traction = std::clamp(ClampNonNegative(physics.traction), 0.0f, 4.0f);
        physics.footstepLoudness = std::clamp(ClampNonNegative(physics.footstepLoudness), 0.0f, 8.0f);
        return physics;
    }

    SurfaceMaterialDesc MakeSurfaceMaterial(SurfaceMaterialKind kind)
    {
        SurfaceMaterialDesc material{};
        material.kind = kind;
        material.name = ToString(kind);
        material.id = BuildSurfaceMaterialId(material.name);
        material.physics = SanitizeSurfacePhysics(material.physics);
        material.flags = SurfaceMaterialFlag_Footstep;

        switch (kind)
        {
            case SurfaceMaterialKind::Concrete:
                material.domain = SurfaceDomain::Destructible;
                material.physics.densityKgPerCubicMeter = 2400.0f;
                material.physics.staticFriction = 0.85f;
                material.physics.dynamicFriction = 0.70f;
                material.physics.restitution = 0.05f;
                material.physics.hardness = 8.0f;
                material.physics.destructionResistance = 1.2f;
                material.render.baseColor = {0.55f, 0.55f, 0.52f};
                material.render.roughness = 0.92f;
                material.flags |= SurfaceMaterialFlag_Destructible;
                break;
            case SurfaceMaterialKind::Wood:
                material.domain = SurfaceDomain::Destructible;
                material.physics.densityKgPerCubicMeter = 650.0f;
                material.physics.staticFriction = 0.65f;
                material.physics.dynamicFriction = 0.50f;
                material.physics.restitution = 0.12f;
                material.physics.hardness = 3.0f;
                material.physics.destructionResistance = 0.55f;
                material.render.baseColor = {0.58f, 0.36f, 0.18f};
                material.render.roughness = 0.75f;
                material.flags |= SurfaceMaterialFlag_Destructible;
                break;
            case SurfaceMaterialKind::Metal:
                material.domain = SurfaceDomain::Destructible;
                material.physics.densityKgPerCubicMeter = 7850.0f;
                material.physics.staticFriction = 0.60f;
                material.physics.dynamicFriction = 0.45f;
                material.physics.restitution = 0.08f;
                material.physics.hardness = 12.0f;
                material.physics.destructionResistance = 3.0f;
                material.render.baseColor = {0.65f, 0.66f, 0.68f};
                material.render.roughness = 0.35f;
                material.render.metallic = 1.0f;
                material.flags |= SurfaceMaterialFlag_Destructible;
                break;
            case SurfaceMaterialKind::Glass:
                material.domain = SurfaceDomain::Destructible;
                material.physics.densityKgPerCubicMeter = 2500.0f;
                material.physics.staticFriction = 0.35f;
                material.physics.dynamicFriction = 0.25f;
                material.physics.restitution = 0.02f;
                material.physics.hardness = 6.0f;
                material.physics.destructionResistance = 0.2f;
                material.render.baseColor = {0.72f, 0.9f, 1.0f};
                material.render.roughness = 0.02f;
                material.render.alpha = 0.35f;
                material.flags |= SurfaceMaterialFlag_Destructible;
                break;
            case SurfaceMaterialKind::TerrainSoil:
                material.domain = SurfaceDomain::Terrain;
                material.physics.densityKgPerCubicMeter = 1600.0f;
                material.physics.staticFriction = 0.90f;
                material.physics.dynamicFriction = 0.75f;
                material.physics.restitution = 0.0f;
                material.physics.hardness = 1.0f;
                material.physics.destructionResistance = 0.7f;
                material.render.baseColor = {0.34f, 0.22f, 0.12f};
                material.render.roughness = 1.0f;
                material.flags |= SurfaceMaterialFlag_Terrain | SurfaceMaterialFlag_Destructible;
                break;
            case SurfaceMaterialKind::Grass:
                material.domain = SurfaceDomain::Terrain;
                material.physics.densityKgPerCubicMeter = 900.0f;
                material.physics.staticFriction = 0.95f;
                material.physics.dynamicFriction = 0.82f;
                material.physics.restitution = 0.0f;
                material.physics.hardness = 0.35f;
                material.physics.destructionResistance = 0.25f;
                material.physics.footstepLoudness = 0.55f;
                material.render.baseColor = {0.15f, 0.45f, 0.12f};
                material.render.roughness = 1.0f;
                material.flags |= SurfaceMaterialFlag_Terrain;
                break;
            case SurfaceMaterialKind::Rock:
                material.domain = SurfaceDomain::Terrain;
                material.physics.densityKgPerCubicMeter = 2700.0f;
                material.physics.staticFriction = 1.05f;
                material.physics.dynamicFriction = 0.88f;
                material.physics.restitution = 0.03f;
                material.physics.hardness = 9.0f;
                material.physics.destructionResistance = 2.0f;
                material.render.baseColor = {0.42f, 0.42f, 0.40f};
                material.render.roughness = 0.9f;
                material.flags |= SurfaceMaterialFlag_Terrain | SurfaceMaterialFlag_Destructible | SurfaceMaterialFlag_Climbable;
                break;
            case SurfaceMaterialKind::Sand:
                material.domain = SurfaceDomain::Terrain;
                material.physics.densityKgPerCubicMeter = 1550.0f;
                material.physics.staticFriction = 0.78f;
                material.physics.dynamicFriction = 0.62f;
                material.physics.restitution = 0.0f;
                material.physics.hardness = 0.2f;
                material.physics.destructionResistance = 0.25f;
                material.physics.traction = 0.55f;
                material.render.baseColor = {0.76f, 0.66f, 0.42f};
                material.render.roughness = 1.0f;
                material.flags |= SurfaceMaterialFlag_Terrain;
                break;
            case SurfaceMaterialKind::Ice:
                material.domain = SurfaceDomain::Terrain;
                material.physics.densityKgPerCubicMeter = 917.0f;
                material.physics.staticFriction = 0.08f;
                material.physics.dynamicFriction = 0.04f;
                material.physics.restitution = 0.02f;
                material.physics.hardness = 1.4f;
                material.physics.destructionResistance = 0.4f;
                material.physics.traction = 0.1f;
                material.render.baseColor = {0.75f, 0.9f, 1.0f};
                material.render.roughness = 0.08f;
                material.flags |= SurfaceMaterialFlag_Terrain | SurfaceMaterialFlag_Slippery;
                break;
            case SurfaceMaterialKind::Water:
                material.domain = SurfaceDomain::Fluid;
                material.physics.densityKgPerCubicMeter = 997.0f;
                material.physics.staticFriction = 0.0f;
                material.physics.dynamicFriction = 0.0f;
                material.physics.restitution = 0.0f;
                material.physics.rollingResistance = 0.0f;
                material.physics.hardness = 0.0f;
                material.physics.destructionResistance = 0.0f;
                material.physics.traction = 0.0f;
                material.physics.footstepLoudness = 0.25f;
                material.render.baseColor = {0.10f, 0.24f, 0.45f};
                material.render.roughness = 0.02f;
                material.render.alpha = 0.55f;
                material.flags = SurfaceMaterialFlag_Fluid;
                break;
            case SurfaceMaterialKind::Rubber:
                material.physics.densityKgPerCubicMeter = 1100.0f;
                material.physics.staticFriction = 1.15f;
                material.physics.dynamicFriction = 0.95f;
                material.physics.restitution = 0.75f;
                material.physics.hardness = 0.75f;
                material.physics.destructionResistance = 0.35f;
                material.render.baseColor = {0.02f, 0.02f, 0.02f};
                material.render.roughness = 0.82f;
                break;
            case SurfaceMaterialKind::Custom:
                material.name = "custom";
                material.id = BuildSurfaceMaterialId(material.name);
                break;
            case SurfaceMaterialKind::Default:
            default:
                material.physics.densityKgPerCubicMeter = 1000.0f;
                material.physics.staticFriction = 0.70f;
                material.physics.dynamicFriction = 0.55f;
                material.physics.restitution = 0.05f;
                material.physics.hardness = 1.0f;
                material.physics.destructionResistance = 1.0f;
                material.render.baseColor = {0.8f, 0.8f, 0.8f};
                break;
        }

        material.physics = SanitizeSurfacePhysics(material.physics);
        return material;
    }

    SurfaceMaterialDesc SanitizeSurfaceMaterial(SurfaceMaterialDesc material)
    {
        if (material.name.empty())
        {
            material.name = ToString(material.kind);
        }
        if (!material.id.IsValid())
        {
            material.id = BuildSurfaceMaterialId(material.name);
        }
        material.physics = SanitizeSurfacePhysics(material.physics);
        material.render.roughness = ClampUnit(material.render.roughness);
        material.render.metallic = ClampUnit(material.render.metallic);
        material.render.alpha = ClampUnit(material.render.alpha);
        material.valid = ValidateSurfaceMaterial(material).ok;
        return material;
    }

    SurfaceValidationReport ValidateSurfaceMaterial(const SurfaceMaterialDesc& material)
    {
        SurfaceValidationReport report{};
        if (material.name.empty())
        {
            PushError(report, "surface material name is empty");
        }
        if (!material.id.IsValid())
        {
            PushError(report, "surface material id is invalid");
        }
        if (!IsFinite(material.physics.densityKgPerCubicMeter) || material.physics.densityKgPerCubicMeter <= 0.0f)
        {
            PushError(report, "surface material density must be positive");
        }
        if (material.physics.dynamicFriction > material.physics.staticFriction)
        {
            PushWarning(report, "dynamic friction is greater than static friction; sanitize will clamp it");
        }
        if (HasFlag(material.flags, SurfaceMaterialFlag_Fluid) && material.physics.hardness > 0.001f)
        {
            PushWarning(report, "fluid surface should usually have zero hardness");
        }
        return report;
    }

    SurfaceMaterialId SurfaceMaterialRegistry::Add(SurfaceMaterialDesc material)
    {
        material = SanitizeSurfaceMaterial(std::move(material));
        if (material.id.slot == 0)
        {
            material.id.slot = mNextSlot++;
        }
        else
        {
            mNextSlot = std::max(mNextSlot, material.id.slot + 1u);
        }

        if (SurfaceMaterialDesc* existing = const_cast<SurfaceMaterialDesc*>(Find(material.id)))
        {
            *existing = material;
            return existing->id;
        }

        mMaterials.push_back(material);
        return mMaterials.back().id;
    }

    bool SurfaceMaterialRegistry::Remove(SurfaceMaterialId id)
    {
        const auto it = std::remove_if(mMaterials.begin(), mMaterials.end(), [id](const SurfaceMaterialDesc& material)
        {
            return MatchesSurfaceMaterialId(material.id, id);
        });
        const bool removed = it != mMaterials.end();
        mMaterials.erase(it, mMaterials.end());
        return removed;
    }

    const SurfaceMaterialDesc* SurfaceMaterialRegistry::Find(SurfaceMaterialId id) const
    {
        const auto it = std::find_if(mMaterials.begin(), mMaterials.end(), [id](const SurfaceMaterialDesc& material)
        {
            return MatchesSurfaceMaterialId(material.id, id);
        });
        return it == mMaterials.end() ? nullptr : &*it;
    }

    const SurfaceMaterialDesc* SurfaceMaterialRegistry::FindByKind(SurfaceMaterialKind kind) const
    {
        const auto it = std::find_if(mMaterials.begin(), mMaterials.end(), [kind](const SurfaceMaterialDesc& material)
        {
            return material.kind == kind;
        });
        return it == mMaterials.end() ? nullptr : &*it;
    }

    const SurfaceMaterialDesc* SurfaceMaterialRegistry::FindByName(std::string_view name) const
    {
        const auto it = std::find_if(mMaterials.begin(), mMaterials.end(), [name](const SurfaceMaterialDesc& material)
        {
            return material.name == name;
        });
        return it == mMaterials.end() ? nullptr : &*it;
    }

    std::size_t SurfaceMaterialRegistry::Count() const
    {
        return mMaterials.size();
    }

    void SurfaceMaterialRegistry::Clear()
    {
        mMaterials.clear();
        mNextSlot = 1;
    }

    SurfaceRegistryStats SurfaceMaterialRegistry::Stats() const
    {
        SurfaceRegistryStats stats{};
        stats.materialCount = mMaterials.size();
        for (const SurfaceMaterialDesc& material : mMaterials)
        {
            if (material.kind == SurfaceMaterialKind::Default)
            {
                stats.hasDefault = true;
            }
            if (HasFlag(material.flags, SurfaceMaterialFlag_Terrain))
            {
                ++stats.terrainMaterialCount;
            }
            if (HasFlag(material.flags, SurfaceMaterialFlag_Destructible))
            {
                ++stats.destructibleMaterialCount;
            }
            if (HasFlag(material.flags, SurfaceMaterialFlag_Fluid))
            {
                ++stats.fluidMaterialCount;
            }
        }
        stats.ok = stats.materialCount > 0 && stats.hasDefault && stats.terrainMaterialCount > 0;
        return stats;
    }

    const std::vector<SurfaceMaterialDesc>& SurfaceMaterialRegistry::Materials() const
    {
        return mMaterials;
    }

    SurfaceMaterialRegistry BuildDefaultSurfaceMaterialRegistry()
    {
        SurfaceMaterialRegistry registry{};
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Default));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Concrete));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Wood));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Metal));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Glass));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::TerrainSoil));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Grass));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Rock));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Sand));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Ice));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Water));
        registry.Add(MakeSurfaceMaterial(SurfaceMaterialKind::Rubber));
        return registry;
    }

    SurfaceMaterialKind SelectTerrainSurfaceKind(const TerrainSample& sample)
    {
        if (sample.hole)
        {
            return SurfaceMaterialKind::Water;
        }
        if (!sample.finite)
        {
            return SurfaceMaterialKind::Default;
        }
        if (sample.heightMeters < -1.0)
        {
            return SurfaceMaterialKind::Sand;
        }
        if (sample.slopeDegrees >= 45.0)
        {
            return SurfaceMaterialKind::Rock;
        }
        if (sample.slopeDegrees >= 28.0)
        {
            return SurfaceMaterialKind::TerrainSoil;
        }
        return SurfaceMaterialKind::Grass;
    }

    SurfaceSample BuildSurfaceSampleFromTerrain(const TerrainSample& sample, SurfaceDomain domain)
    {
        SurfaceSample surface{};
        surface.domain = domain;
        surface.normal = ToFloatVec3(sample.normal);
        surface.heightMeters = static_cast<float>(std::clamp(sample.heightMeters, static_cast<double>(-std::numeric_limits<float>::max()), static_cast<double>(std::numeric_limits<float>::max())));
        surface.slopeDegrees = static_cast<float>(std::clamp(sample.slopeDegrees, 0.0, 90.0));
        surface.hole = sample.hole;
        surface.finite = sample.finite && IsFinite(surface.normal) && IsFinite(surface.heightMeters) && IsFinite(surface.slopeDegrees);
        surface.blendMode = SurfaceBlendMode::WeightedAverage;

        const SurfaceMaterialKind dominant = SelectTerrainSurfaceKind(sample);
        const SurfaceMaterialId dominantId = BuildSurfaceMaterialId(ToString(dominant));
        surface.layers.push_back({dominantId, 1.0f});

        if (!sample.hole && sample.slopeDegrees > 20.0 && sample.slopeDegrees < 45.0)
        {
            const float rockWeight = static_cast<float>((sample.slopeDegrees - 20.0) / 25.0);
            surface.layers.front().weight = 1.0f - rockWeight;
            surface.layers.push_back({BuildSurfaceMaterialId(ToString(SurfaceMaterialKind::Rock)), rockWeight});
        }

        return surface;
    }

    SurfaceContactMaterial ContactMaterialFromSurface(const SurfaceMaterialDesc& material)
    {
        SurfaceContactMaterial contact{};
        contact.staticFriction = material.physics.staticFriction;
        contact.dynamicFriction = material.physics.dynamicFriction;
        contact.restitution = material.physics.restitution;
        contact.rollingResistance = material.physics.rollingResistance;
        contact.densityKgPerCubicMeter = material.physics.densityKgPerCubicMeter;
        contact.hardness = material.physics.hardness;
        contact.destructionResistance = material.physics.destructionResistance;
        contact.traction = material.physics.traction;
        contact.flags = material.flags;
        contact.valid = material.valid;
        return contact;
    }

    SurfaceContactMaterial ResolveSurfaceSample(const SurfaceMaterialRegistry& registry, const SurfaceSample& sample)
    {
        const SurfaceMaterialDesc* fallback = FallbackMaterial(registry);
        if (!fallback)
        {
            return {};
        }

        if (sample.layers.empty())
        {
            return ContactMaterialFromSurface(*fallback);
        }

        if (sample.blendMode == SurfaceBlendMode::Dominant)
        {
            const SurfaceLayer* dominant = &sample.layers.front();
            for (const SurfaceLayer& layer : sample.layers)
            {
                if (layer.weight > dominant->weight)
                {
                    dominant = &layer;
                }
            }
            const SurfaceMaterialDesc* material = registry.Find(dominant->material);
            return ContactMaterialFromSurface(material ? *material : *fallback);
        }

        float totalWeight = 0.0f;
        for (const SurfaceLayer& layer : sample.layers)
        {
            totalWeight += ClampNonNegative(layer.weight);
        }
        if (totalWeight <= 0.0f)
        {
            return ContactMaterialFromSurface(*fallback);
        }

        SurfaceContactMaterial result{};
        result.staticFriction = 0.0f;
        result.dynamicFriction = 0.0f;
        result.restitution = 0.0f;
        result.rollingResistance = 0.0f;
        result.densityKgPerCubicMeter = 0.0f;
        result.hardness = 0.0f;
        result.destructionResistance = 0.0f;
        result.traction = 0.0f;
        result.flags = SurfaceMaterialFlag_None;
        result.valid = true;

        for (const SurfaceLayer& layer : sample.layers)
        {
            const float weight = ClampNonNegative(layer.weight) / totalWeight;
            const SurfaceMaterialDesc* material = registry.Find(layer.material);
            const SurfaceContactMaterial contact = ContactMaterialFromSurface(material ? *material : *fallback);
            result.staticFriction = MixWeighted(result.staticFriction, contact.staticFriction, weight);
            result.dynamicFriction = MixWeighted(result.dynamicFriction, contact.dynamicFriction, weight);
            result.restitution = MixWeighted(result.restitution, contact.restitution, weight);
            result.rollingResistance = MixWeighted(result.rollingResistance, contact.rollingResistance, weight);
            result.densityKgPerCubicMeter = MixWeighted(result.densityKgPerCubicMeter, contact.densityKgPerCubicMeter, weight);
            result.hardness = MixWeighted(result.hardness, contact.hardness, weight);
            result.destructionResistance = MixWeighted(result.destructionResistance, contact.destructionResistance, weight);
            result.traction = MixWeighted(result.traction, contact.traction, weight);
            result.flags |= contact.flags;
            result.valid = result.valid && contact.valid;
        }

        result.staticFriction = std::clamp(result.staticFriction, 0.0f, 4.0f);
        result.dynamicFriction = std::clamp(result.dynamicFriction, 0.0f, result.staticFriction);
        result.restitution = ClampUnit(result.restitution);
        result.rollingResistance = ClampUnit(result.rollingResistance);
        result.densityKgPerCubicMeter = std::clamp(result.densityKgPerCubicMeter, MinimumDensity, MaximumDensity);
        result.hardness = std::clamp(result.hardness, 0.0f, 100.0f);
        result.destructionResistance = std::clamp(result.destructionResistance, 0.0f, 100.0f);
        result.traction = std::clamp(result.traction, 0.0f, 4.0f);
        return result;
    }

    SurfaceContactMaterial CombineSurfaceContacts(const SurfaceContactMaterial& a, const SurfaceContactMaterial& b)
    {
        SurfaceContactMaterial result{};
        result.staticFriction = std::sqrt(std::max(0.0f, a.staticFriction) * std::max(0.0f, b.staticFriction));
        result.dynamicFriction = std::sqrt(std::max(0.0f, a.dynamicFriction) * std::max(0.0f, b.dynamicFriction));
        result.restitution = std::max(ClampUnit(a.restitution), ClampUnit(b.restitution));
        result.rollingResistance = std::max(ClampUnit(a.rollingResistance), ClampUnit(b.rollingResistance));
        result.densityKgPerCubicMeter = (a.densityKgPerCubicMeter + b.densityKgPerCubicMeter) * 0.5f;
        result.hardness = std::min(a.hardness, b.hardness);
        result.destructionResistance = std::min(a.destructionResistance, b.destructionResistance);
        result.traction = std::sqrt(std::max(0.0f, a.traction) * std::max(0.0f, b.traction));
        result.flags = a.flags | b.flags;
        result.valid = a.valid && b.valid;
        return result;
    }

    void ApplySurfaceToPhysicsBody(PhysicsBody& body, const SurfaceContactMaterial& material, float volumeCubicMeters, bool updateMass)
    {
        body.friction = std::clamp(material.dynamicFriction, 0.0f, 4.0f);
        body.restitution = ClampUnit(material.restitution);
        if (updateMass && IsDynamic(body))
        {
            const float volume = std::max(MinimumVolume, ClampPositive(volumeCubicMeters, MinimumVolume));
            body.massKilograms = std::max(0.001f, material.densityKgPerCubicMeter * volume);
            body.inverseMass = 1.0f / body.massKilograms;
        }
    }

    void ApplySurfaceToPhysicsBody(PhysicsBody& body, const SurfaceMaterialDesc& material, float volumeCubicMeters, bool updateMass)
    {
        ApplySurfaceToPhysicsBody(body, ContactMaterialFromSurface(material), volumeCubicMeters, updateMass);
    }

    std::string ToDebugString(SurfaceMaterialId id)
    {
        std::ostringstream out;
        out << "surface_id=" << ToString(id.guid) << ":" << id.slot;
        return out.str();
    }

    std::string ToDebugString(const SurfacePhysicsDesc& physics, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "density=" << physics.densityKgPerCubicMeter
            << " static_friction=" << physics.staticFriction
            << " dynamic_friction=" << physics.dynamicFriction
            << " restitution=" << physics.restitution
            << " hardness=" << physics.hardness
            << " destruction=" << physics.destructionResistance
            << " traction=" << physics.traction;
        return out.str();
    }

    std::string ToDebugString(const SurfaceMaterialDesc& material, int precision)
    {
        std::ostringstream out;
        out << "surface material name=" << material.name
            << " kind=" << ToString(material.kind)
            << " domain=" << ToString(material.domain)
            << " flags=" << material.flags
            << " valid=" << (material.valid ? "yes" : "no")
            << " " << ToDebugString(material.physics, precision);
        return out.str();
    }

    std::string ToDebugString(const SurfaceContactMaterial& material, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "surface contact static_friction=" << material.staticFriction
            << " dynamic_friction=" << material.dynamicFriction
            << " restitution=" << material.restitution
            << " density=" << material.densityKgPerCubicMeter
            << " hardness=" << material.hardness
            << " destruction=" << material.destructionResistance
            << " traction=" << material.traction
            << " flags=" << material.flags
            << " valid=" << (material.valid ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const SurfaceSample& sample, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "surface sample domain=" << ToString(sample.domain)
            << " layers=" << sample.layers.size()
            << " slope=" << sample.slopeDegrees
            << " height=" << sample.heightMeters
            << " hole=" << (sample.hole ? "yes" : "no")
            << " finite=" << (sample.finite ? "yes" : "no")
            << " blend=" << ToString(sample.blendMode);
        if (!sample.layers.empty())
        {
            out << " first_weight=" << sample.layers.front().weight;
        }
        return out.str();
    }

    std::string ToDebugString(const SurfaceValidationReport& report)
    {
        std::ostringstream out;
        out << "surface validation ok=" << (report.ok ? "yes" : "no")
            << " warnings=" << report.warnings.size()
            << " errors=" << report.errors.size();
        return out.str();
    }

    std::string ToDebugString(const SurfaceRegistryStats& stats)
    {
        std::ostringstream out;
        out << "surface registry materials=" << stats.materialCount
            << " terrain=" << stats.terrainMaterialCount
            << " destructible=" << stats.destructibleMaterialCount
            << " fluids=" << stats.fluidMaterialCount
            << " default=" << (stats.hasDefault ? "yes" : "no")
            << " ok=" << (stats.ok ? "yes" : "no");
        return out.str();
    }

    SurfaceProbeResult BuildSurfaceProbe()
    {
        SurfaceProbeResult probe{};
        SurfaceMaterialRegistry registry = BuildDefaultSurfaceMaterialRegistry();
        probe.registryStats = registry.Stats();

        const TerrainSample grassTerrain = SampleHeightfieldTerrain(4.0, 8.0);
        TerrainSample steepTerrain = grassTerrain;
        steepTerrain.slopeDegrees = 52.0;
        steepTerrain.normal = Normalize(DVec3{0.8, 0.6, 0.0});

        const PlanetTerrainDesc planetDesc{};
        const TerrainSample planetTerrain = SamplePlanetTerrain({0.35, 0.15, 0.0}, planetDesc);

        probe.grassSample = BuildSurfaceSampleFromTerrain(grassTerrain, SurfaceDomain::Terrain);
        probe.steepSample = BuildSurfaceSampleFromTerrain(steepTerrain, SurfaceDomain::Terrain);
        probe.planetSample = BuildSurfaceSampleFromTerrain(planetTerrain, SurfaceDomain::Planet);

        const SurfaceContactMaterial grassContact = ResolveSurfaceSample(registry, probe.grassSample);
        const SurfaceContactMaterial rockContact = ResolveSurfaceSample(registry, probe.steepSample);
        const SurfaceContactMaterial planetContact = ResolveSurfaceSample(registry, probe.planetSample);
        const SurfaceMaterialDesc* rubber = registry.FindByKind(SurfaceMaterialKind::Rubber);
        const SurfaceMaterialDesc* concrete = registry.FindByKind(SurfaceMaterialKind::Concrete);
        probe.rubberConcreteContact = CombineSurfaceContacts(
            rubber ? ContactMaterialFromSurface(*rubber) : SurfaceContactMaterial{},
            concrete ? ContactMaterialFromSurface(*concrete) : SurfaceContactMaterial{});

        PhysicsScene scene{};
        scene.config.enableGravity = false;
        scene.config.contactSlop = 0.0f;

        PhysicsBody floor = MakeStaticBody(1, {0.0f, 0.0f, 0.0f});
        PhysicsBody dynamic = MakeDynamicBody(2, {0.0f, 0.26f, 0.0f}, 1.0f);
        dynamic.velocity = {3.0f, -1.0f, 0.0f};
        if (concrete)
        {
            ApplySurfaceToPhysicsBody(floor, *concrete);
        }
        if (rubber)
        {
            ApplySurfaceToPhysicsBody(dynamic, *rubber, 0.125f, true);
        }
        probe.lateralVelocityBefore = std::fabs(dynamic.velocity.x);

        scene.bodies.push_back(floor);
        scene.bodies.push_back(dynamic);
        scene.colliders.push_back(MakeBoxCollider(1, {1.0f, 0.1f, 1.0f}));
        scene.colliders.push_back(MakeBoxCollider(2, {0.25f, 0.25f, 0.25f}));

        const std::vector<PhysicsBroadphasePair> pairs = BuildBroadphasePairs(scene);
        const std::vector<PhysicsContact> contacts = BuildContacts(scene, pairs);
        for (const PhysicsContact& contact : contacts)
        {
            ResolveContact(scene, contact);
        }
        probe.physicsStats = StepPhysics(scene, 1.0f / 60.0f);
        const PhysicsBody* after = FindBody(scene, 2);
        probe.lateralVelocityAfter = after ? std::fabs(after->velocity.x) : probe.lateralVelocityBefore;

        probe.ok = probe.registryStats.ok
            && grassContact.valid
            && rockContact.valid
            && planetContact.valid
            && probe.rubberConcreteContact.valid
            && !contacts.empty()
            && probe.lateralVelocityAfter < probe.lateralVelocityBefore
            && probe.physicsStats.finite;

        std::ostringstream out;
        out << "Surface material probe: " << (probe.ok ? "ok" : "failed")
            << " materials=" << probe.registryStats.materialCount
            << " terrain=" << probe.registryStats.terrainMaterialCount
            << " destructible=" << probe.registryStats.destructibleMaterialCount
            << " fluids=" << probe.registryStats.fluidMaterialCount
            << " grass_layers=" << probe.grassSample.layers.size()
            << " steep_layers=" << probe.steepSample.layers.size()
            << " friction_before=" << probe.lateralVelocityBefore
            << " friction_after=" << probe.lateralVelocityAfter
            << " physics_contacts=" << contacts.size()
            << " step_contacts=" << probe.physicsStats.contactCount;
        probe.summary = out.str();
        return probe;
    }

    std::string BuildSurfaceProbeSummary()
    {
        return BuildSurfaceProbe().summary;
    }
}
