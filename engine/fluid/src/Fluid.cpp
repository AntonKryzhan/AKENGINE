#include <AK/Fluid/Fluid.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float MinimumFluidDensity = 0.001f;
        constexpr float MinimumFluidVolume = 1.0e-8f;
        constexpr float MinimumDeltaSeconds = 1.0e-6f;
        constexpr float MaximumFluidForce = 1.0e8f;
        constexpr float GravityFallbackMagnitude = 9.80665f;

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        Vec3 Negate(Vec3 value)
        {
            return {-value.x, -value.y, -value.z};
        }

        Vec3 ClampLength(Vec3 value, float maxLength)
        {
            const float length = Length(value);
            if (length <= maxLength || length <= FloatEpsilon)
            {
                return value;
            }
            return Multiply(value, maxLength / length);
        }


        float BoundsVolume(AABB3 bounds)
        {
            if (!IsValid(bounds))
            {
                return 0.0f;
            }
            const Vec3 size = Size(bounds);
            return std::max(0.0f, size.x) * std::max(0.0f, size.y) * std::max(0.0f, size.z);
        }

        float ColliderBoundsSurfaceFractionY(AABB3 bounds, float surfaceY)
        {
            if (!IsValid(bounds))
            {
                return 0.0f;
            }
            if (surfaceY <= bounds.min.y)
            {
                return 0.0f;
            }
            if (surfaceY >= bounds.max.y)
            {
                return 1.0f;
            }
            return std::clamp((surfaceY - bounds.min.y) / std::max(FloatEpsilon, bounds.max.y - bounds.min.y), 0.0f, 1.0f);
        }

        float SphereCapFraction(float radius, float depth)
        {
            if (radius <= FloatEpsilon)
            {
                return 0.0f;
            }
            if (depth <= 0.0f)
            {
                return 0.0f;
            }
            if (depth >= 2.0f * radius)
            {
                return 1.0f;
            }
            const float h = std::clamp(depth, 0.0f, 2.0f * radius);
            const float capVolume = Pi32 * h * h * (3.0f * radius - h) / 3.0f;
            const float fullVolume = 4.0f * Pi32 * radius * radius * radius / 3.0f;
            return std::clamp(capVolume / std::max(FloatEpsilon, fullVolume), 0.0f, 1.0f);
        }

        bool VolumeContainsPoint(const FluidVolume& volume, Vec3 point, float* outDepth, float* outNormalizedDepth, Vec3* outNormal)
        {
            const FluidVolume sanitized = SanitizeFluidVolume(volume);
            if (!sanitized.enabled)
            {
                return false;
            }

            if (sanitized.kind == FluidVolumeKind::Plane)
            {
                const Vec3 normal = Normalize(sanitized.surfaceNormal, {0.0f, 1.0f, 0.0f});
                const float signedAbove = Dot(Subtract(point, {0.0f, sanitized.surfaceHeight, 0.0f}), normal);
                const float depth = -signedAbove;
                if (depth <= 0.0f)
                {
                    return false;
                }
                if (outDepth)
                {
                    *outDepth = depth;
                }
                if (outNormalizedDepth)
                {
                    *outNormalizedDepth = std::clamp(depth / 4.0f, 0.0f, 1.0f);
                }
                if (outNormal)
                {
                    *outNormal = normal;
                }
                return true;
            }

            if (sanitized.kind == FluidVolumeKind::Box)
            {
                const AABB3 bounds = MakeAABB3FromCenterExtents(sanitized.center, sanitized.halfExtents);
                if (!Contains(bounds, point))
                {
                    return false;
                }
                const float surfaceY = bounds.max.y;
                const float depth = std::max(0.0f, surfaceY - point.y);
                if (outDepth)
                {
                    *outDepth = depth;
                }
                if (outNormalizedDepth)
                {
                    *outNormalizedDepth = std::clamp(depth / std::max(FloatEpsilon, 2.0f * sanitized.halfExtents.y), 0.0f, 1.0f);
                }
                if (outNormal)
                {
                    *outNormal = {0.0f, 1.0f, 0.0f};
                }
                return true;
            }

            const Vec3 delta = Subtract(point, sanitized.center);
            const float distance = Length(delta);
            if (distance >= sanitized.radius)
            {
                return false;
            }
            const float depth = sanitized.radius - distance;
            if (outDepth)
            {
                *outDepth = depth;
            }
            if (outNormalizedDepth)
            {
                *outNormalizedDepth = std::clamp(depth / std::max(FloatEpsilon, sanitized.radius), 0.0f, 1.0f);
            }
            if (outNormal)
            {
                *outNormal = Normalize(delta, {0.0f, 1.0f, 0.0f});
            }
            return true;
        }

        float GravityMagnitude(const PhysicsScene& scene)
        {
            const float magnitude = Length(scene.config.gravity);
            if (magnitude <= FloatEpsilon || !IsFinite(scene.config.gravity))
            {
                return GravityFallbackMagnitude;
            }
            return magnitude;
        }

        Vec3 UpFromGravity(const PhysicsScene& scene)
        {
            if (LengthSquared(scene.config.gravity) <= FloatEpsilon || !IsFinite(scene.config.gravity))
            {
                return {0.0f, 1.0f, 0.0f};
            }
            return Normalize(Negate(scene.config.gravity), {0.0f, 1.0f, 0.0f});
        }

        const PhysicsBody* BodyForCollider(const PhysicsScene& scene, const PhysicsCollider& collider)
        {
            return FindBody(scene, collider.bodyId);
        }

        PhysicsBody* BodyForCollider(PhysicsScene& scene, const PhysicsCollider& collider)
        {
            return FindBody(scene, collider.bodyId);
        }
    }

    const char* ToString(FluidMediumKind kind)
    {
        switch (kind)
        {
        case FluidMediumKind::FreshWater:
            return "FreshWater";
        case FluidMediumKind::SeaWater:
            return "SeaWater";
        case FluidMediumKind::Oil:
            return "Oil";
        case FluidMediumKind::Mud:
            return "Mud";
        case FluidMediumKind::Lava:
            return "Lava";
        case FluidMediumKind::Gas:
            return "Gas";
        case FluidMediumKind::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    const char* ToString(FluidVolumeKind kind)
    {
        switch (kind)
        {
        case FluidVolumeKind::Plane:
            return "Plane";
        case FluidVolumeKind::Box:
            return "Box";
        case FluidVolumeKind::Sphere:
            return "Sphere";
        default:
            return "Unknown";
        }
    }

    const char* ToString(FluidInteractionKind kind)
    {
        switch (kind)
        {
        case FluidInteractionKind::Buoyancy:
            return "Buoyancy";
        case FluidInteractionKind::Drag:
            return "Drag";
        case FluidInteractionKind::Flow:
            return "Flow";
        default:
            return "Unknown";
        }
    }

    FluidMediumDesc MakeFluidMedium(FluidMediumKind kind)
    {
        FluidMediumDesc medium{};
        medium.kind = kind;
        switch (kind)
        {
        case FluidMediumKind::FreshWater:
            medium.densityKgPerCubicMeter = 997.0f;
            medium.dynamicViscosityPascalSeconds = 0.001f;
            medium.linearDragCoefficient = 1.2f;
            medium.angularDragCoefficient = 0.2f;
            medium.buoyancyScale = 1.0f;
            medium.debugName = "fresh_water";
            break;
        case FluidMediumKind::SeaWater:
            medium.densityKgPerCubicMeter = 1025.0f;
            medium.dynamicViscosityPascalSeconds = 0.00108f;
            medium.linearDragCoefficient = 1.35f;
            medium.angularDragCoefficient = 0.24f;
            medium.buoyancyScale = 1.02f;
            medium.debugName = "sea_water";
            break;
        case FluidMediumKind::Oil:
            medium.densityKgPerCubicMeter = 850.0f;
            medium.dynamicViscosityPascalSeconds = 0.08f;
            medium.linearDragCoefficient = 2.2f;
            medium.angularDragCoefficient = 0.45f;
            medium.buoyancyScale = 0.85f;
            medium.debugName = "oil";
            break;
        case FluidMediumKind::Mud:
            medium.densityKgPerCubicMeter = 1650.0f;
            medium.dynamicViscosityPascalSeconds = 1.5f;
            medium.linearDragCoefficient = 5.0f;
            medium.angularDragCoefficient = 1.2f;
            medium.buoyancyScale = 1.15f;
            medium.debugName = "mud";
            break;
        case FluidMediumKind::Lava:
            medium.densityKgPerCubicMeter = 2800.0f;
            medium.dynamicViscosityPascalSeconds = 80.0f;
            medium.linearDragCoefficient = 12.0f;
            medium.angularDragCoefficient = 2.5f;
            medium.buoyancyScale = 1.0f;
            medium.damageOnContact = true;
            medium.damagePerSecond = 100.0f;
            medium.debugName = "lava";
            break;
        case FluidMediumKind::Gas:
            medium.densityKgPerCubicMeter = 1.2f;
            medium.dynamicViscosityPascalSeconds = 1.8e-5f;
            medium.linearDragCoefficient = 0.05f;
            medium.angularDragCoefficient = 0.01f;
            medium.buoyancyScale = 0.02f;
            medium.breathable = true;
            medium.debugName = "gas";
            break;
        case FluidMediumKind::Custom:
        default:
            medium.debugName = "custom";
            break;
        }
        return medium;
    }

    FluidVolume MakeBoxFluidVolume(u32 id, Vec3 center, Vec3 halfExtents, FluidMediumDesc medium)
    {
        FluidVolume volume{};
        volume.id = id;
        volume.kind = FluidVolumeKind::Box;
        volume.center = center;
        volume.halfExtents = halfExtents;
        volume.surfaceHeight = center.y + halfExtents.y;
        volume.medium = SanitizeFluidMedium(medium);
        volume.debugName = volume.medium.debugName + "_box";
        return SanitizeFluidVolume(volume);
    }

    FluidVolume MakePlaneFluidVolume(u32 id, float surfaceHeight, FluidMediumDesc medium)
    {
        FluidVolume volume{};
        volume.id = id;
        volume.kind = FluidVolumeKind::Plane;
        volume.surfaceHeight = surfaceHeight;
        volume.surfaceNormal = {0.0f, 1.0f, 0.0f};
        volume.medium = SanitizeFluidMedium(medium);
        volume.debugName = volume.medium.debugName + "_plane";
        return SanitizeFluidVolume(volume);
    }

    FluidVolume MakeSphereFluidVolume(u32 id, Vec3 center, float radius, FluidMediumDesc medium)
    {
        FluidVolume volume{};
        volume.id = id;
        volume.kind = FluidVolumeKind::Sphere;
        volume.center = center;
        volume.radius = radius;
        volume.surfaceHeight = center.y + radius;
        volume.medium = SanitizeFluidMedium(medium);
        volume.debugName = volume.medium.debugName + "_sphere";
        return SanitizeFluidVolume(volume);
    }

    FluidMediumDesc SanitizeFluidMedium(FluidMediumDesc medium)
    {
        medium.densityKgPerCubicMeter = std::clamp(medium.densityKgPerCubicMeter, MinimumFluidDensity, 50000.0f);
        medium.dynamicViscosityPascalSeconds = std::clamp(medium.dynamicViscosityPascalSeconds, 0.0f, 100000.0f);
        medium.linearDragCoefficient = std::clamp(medium.linearDragCoefficient, 0.0f, 1000.0f);
        medium.angularDragCoefficient = std::clamp(medium.angularDragCoefficient, 0.0f, 1000.0f);
        medium.buoyancyScale = std::clamp(medium.buoyancyScale, 0.0f, 10.0f);
        medium.maxForceNewton = std::clamp(medium.maxForceNewton, 1.0f, MaximumFluidForce);
        medium.damagePerSecond = std::clamp(medium.damagePerSecond, 0.0f, 100000.0f);
        if (!IsFinite(medium.flowVelocity))
        {
            medium.flowVelocity = {};
        }
        if (medium.debugName.empty())
        {
            medium.debugName = ToString(medium.kind);
        }
        return medium;
    }

    FluidVolume SanitizeFluidVolume(FluidVolume volume)
    {
        volume.medium = SanitizeFluidMedium(volume.medium);
        if (!IsFinite(volume.center))
        {
            volume.center = {};
        }
        if (!IsFinite(volume.halfExtents))
        {
            volume.halfExtents = {1.0f, 1.0f, 1.0f};
        }
        volume.halfExtents.x = std::max(0.001f, std::abs(volume.halfExtents.x));
        volume.halfExtents.y = std::max(0.001f, std::abs(volume.halfExtents.y));
        volume.halfExtents.z = std::max(0.001f, std::abs(volume.halfExtents.z));
        volume.radius = std::clamp(std::abs(volume.radius), 0.001f, 1000000.0f);
        if (!IsFinite(volume.surfaceNormal) || LengthSquared(volume.surfaceNormal) <= FloatEpsilon)
        {
            volume.surfaceNormal = {0.0f, 1.0f, 0.0f};
        }
        volume.surfaceNormal = Normalize(volume.surfaceNormal, {0.0f, 1.0f, 0.0f});
        if (!IsFinite(volume.surfaceHeight))
        {
            volume.surfaceHeight = volume.center.y + volume.halfExtents.y;
        }
        if (!IsFinite(volume.flowVelocity))
        {
            volume.flowVelocity = {};
        }
        if (volume.kind == FluidVolumeKind::Box)
        {
            volume.surfaceHeight = volume.center.y + volume.halfExtents.y;
        }
        if (volume.kind == FluidVolumeKind::Sphere)
        {
            volume.surfaceHeight = volume.center.y + volume.radius;
        }
        if (volume.debugName.empty())
        {
            volume.debugName = volume.medium.debugName;
        }
        return volume;
    }

    FluidSample SampleFluidAtPoint(const std::vector<FluidVolume>& volumes, Vec3 point)
    {
        FluidSample best{};
        best.point = point;
        for (std::size_t index = 0; index < volumes.size(); ++index)
        {
            const FluidVolume volume = SanitizeFluidVolume(volumes[index]);
            float depth = 0.0f;
            float normalizedDepth = 0.0f;
            Vec3 normal{0.0f, 1.0f, 0.0f};
            if (!VolumeContainsPoint(volume, point, &depth, &normalizedDepth, &normal))
            {
                continue;
            }

            if (!best.inside || depth > best.depthMeters)
            {
                best.inside = true;
                best.volumeIndex = index;
                best.volumeId = volume.id;
                best.volumeKind = volume.kind;
                best.medium = volume.medium;
                best.surfaceNormal = normal;
                best.flowVelocity = Add(volume.medium.flowVelocity, volume.flowVelocity);
                best.depthMeters = depth;
                best.normalizedDepth = normalizedDepth;
            }
        }
        return best;
    }

    float EstimateColliderVolumeCubicMeters(const PhysicsCollider& collider)
    {
        switch (collider.kind)
        {
        case PhysicsColliderKind::Sphere:
            return 4.0f * Pi32 * collider.radius * collider.radius * collider.radius / 3.0f;
        case PhysicsColliderKind::Capsule:
        {
            const float r = std::max(0.0f, collider.radius);
            const float cylinderHeight = std::max(0.0f, 2.0f * collider.halfHeight);
            return Pi32 * r * r * cylinderHeight + 4.0f * Pi32 * r * r * r / 3.0f;
        }
        case PhysicsColliderKind::Box:
            return std::max(0.0f, 8.0f * collider.halfExtents.x * collider.halfExtents.y * collider.halfExtents.z);
        case PhysicsColliderKind::ProxyAABB:
        case PhysicsColliderKind::Heightfield:
        default:
            return BoundsVolume(collider.localBounds);
        }
    }

    float EstimateSubmergedFraction(const PhysicsScene& scene, const PhysicsCollider& collider, const FluidVolume& volume)
    {
        const PhysicsBody* body = BodyForCollider(scene, collider);
        if (!body || !body->enabled || !collider.enabled)
        {
            return 0.0f;
        }

        const FluidVolume sanitized = SanitizeFluidVolume(volume);
        const AABB3 bounds = ComputeColliderWorldBounds(*body, collider);
        if (!IsValid(bounds))
        {
            return 0.0f;
        }

        if (sanitized.kind == FluidVolumeKind::Plane)
        {
            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                const Vec3 center = Center(bounds);
                return SphereCapFraction(collider.radius, sanitized.surfaceHeight - (center.y - collider.radius));
            }
            return ColliderBoundsSurfaceFractionY(bounds, sanitized.surfaceHeight);
        }

        if (sanitized.kind == FluidVolumeKind::Box)
        {
            const AABB3 volumeBounds = MakeAABB3FromCenterExtents(sanitized.center, sanitized.halfExtents);
            const AABB3 overlap = MakeAABB3(Max(bounds.min, volumeBounds.min), Min(bounds.max, volumeBounds.max));
            if (!IsValid(overlap) || !Intersects(bounds, volumeBounds))
            {
                return 0.0f;
            }
            return std::clamp(BoundsVolume(overlap) / std::max(FloatEpsilon, BoundsVolume(bounds)), 0.0f, 1.0f);
        }

        const Vec3 center = Center(bounds);
        const float boundsRadius = 0.5f * Length(Size(bounds));
        const float distance = Length(Subtract(center, sanitized.center));
        if (distance >= sanitized.radius + boundsRadius)
        {
            return 0.0f;
        }
        if (distance + boundsRadius <= sanitized.radius)
        {
            return 1.0f;
        }
        const float overlapDepth = sanitized.radius + boundsRadius - distance;
        return std::clamp(overlapDepth / std::max(FloatEpsilon, 2.0f * boundsRadius), 0.0f, 1.0f);
    }

    FluidColliderInteraction ComputeFluidInteraction(const PhysicsScene& scene, std::size_t colliderIndex, const FluidVolume& volume, std::size_t volumeIndex, float deltaSeconds)
    {
        FluidColliderInteraction interaction{};
        interaction.volumeIndex = volumeIndex;
        interaction.colliderIndex = colliderIndex;
        if (deltaSeconds <= MinimumDeltaSeconds || colliderIndex >= scene.colliders.size())
        {
            return interaction;
        }

        const PhysicsCollider& collider = scene.colliders[colliderIndex];
        const PhysicsBody* body = BodyForCollider(scene, collider);
        if (!body || !body->enabled || !collider.enabled || body->kind != PhysicsBodyKind::Dynamic)
        {
            return interaction;
        }

        const FluidVolume sanitized = SanitizeFluidVolume(volume);
        const float fraction = EstimateSubmergedFraction(scene, collider, sanitized);
        if (fraction <= FloatEpsilon)
        {
            return interaction;
        }

        const AABB3 bounds = ComputeColliderWorldBounds(*body, collider);
        const float displacedVolume = std::max(MinimumFluidVolume, EstimateColliderVolumeCubicMeters(collider) * fraction);
        const float density = sanitized.medium.densityKgPerCubicMeter;
        const float gravity = GravityMagnitude(scene);
        const Vec3 up = UpFromGravity(scene);
        const Vec3 flow = Add(sanitized.medium.flowVelocity, sanitized.flowVelocity);
        const Vec3 relativeVelocity = Subtract(body->velocity, flow);
        const Vec3 rawDrag = Multiply(relativeVelocity, -sanitized.medium.linearDragCoefficient * fraction * std::max(1.0f, density * displacedVolume * 0.01f));
        const float buoyancyNewton = density * displacedVolume * gravity * sanitized.medium.buoyancyScale;
        const Vec3 rawBuoyancy = Multiply(up, buoyancyNewton);
        const float maxForce = sanitized.medium.maxForceNewton;
        interaction.active = true;
        interaction.bodyId = collider.bodyId;
        interaction.submergedFraction = fraction;
        interaction.displacedVolumeCubicMeters = displacedVolume;
        interaction.densityKgPerCubicMeter = density;
        interaction.buoyancyForce = ClampLength(rawBuoyancy, maxForce);
        interaction.dragForce = ClampLength(rawDrag, maxForce);
        interaction.totalForce = Add(interaction.buoyancyForce, interaction.dragForce);
        interaction.buoyancyNewton = Length(interaction.buoyancyForce);
        interaction.dragNewton = Length(interaction.dragForce);
        interaction.damagePerSecond = sanitized.medium.damageOnContact ? sanitized.medium.damagePerSecond * fraction : 0.0f;
        interaction.applicationPoint = IsValid(bounds) ? Center(bounds) : body->position;
        interaction.flowVelocity = flow;
        return interaction;
    }

    FluidSystemStats ApplyFluidForcesToPhysicsScene(PhysicsScene& scene, const std::vector<FluidVolume>& volumes, float deltaSeconds, std::vector<FluidColliderInteraction>* outInteractions)
    {
        FluidSystemStats stats{};
        stats.volumeCount = volumes.size();
        if (outInteractions)
        {
            outInteractions->clear();
        }

        if (deltaSeconds <= MinimumDeltaSeconds)
        {
            AddWarning(stats.warnings, "delta seconds too small");
            stats.finite = false;
            return stats;
        }

        for (const FluidVolume& volume : volumes)
        {
            if (SanitizeFluidVolume(volume).enabled)
            {
                ++stats.enabledVolumeCount;
            }
        }

        for (std::size_t colliderIndex = 0; colliderIndex < scene.colliders.size(); ++colliderIndex)
        {
            const PhysicsCollider& collider = scene.colliders[colliderIndex];
            const PhysicsBody* body = BodyForCollider(scene, collider);
            if (!body || body->kind != PhysicsBodyKind::Dynamic || !collider.enabled)
            {
                continue;
            }
            ++stats.testedColliderCount;

            for (std::size_t volumeIndex = 0; volumeIndex < volumes.size(); ++volumeIndex)
            {
                const FluidVolume volume = SanitizeFluidVolume(volumes[volumeIndex]);
                if (!volume.enabled)
                {
                    continue;
                }

                FluidColliderInteraction interaction = ComputeFluidInteraction(scene, colliderIndex, volume, volumeIndex, deltaSeconds);
                if (!interaction.active)
                {
                    continue;
                }

                PhysicsBody* mutableBody = BodyForCollider(scene, collider);
                if (mutableBody)
                {
                    ApplyForce(*mutableBody, interaction.totalForce);
                    if (mutableBody->sleeping && LengthSquared(interaction.totalForce) > 1.0f)
                    {
                        mutableBody->sleeping = false;
                        mutableBody->sleepTimerSeconds = 0.0f;
                    }
                }

                ++stats.interactionCount;
                ++stats.appliedForceCount;
                if (interaction.buoyancyNewton > FloatEpsilon)
                {
                    ++stats.buoyantColliderCount;
                }
                if (interaction.dragNewton > FloatEpsilon)
                {
                    ++stats.draggedColliderCount;
                }
                stats.totalDisplacedVolumeCubicMeters += interaction.displacedVolumeCubicMeters;
                stats.totalBuoyancyNewton += interaction.buoyancyNewton;
                stats.totalDragNewton += interaction.dragNewton;
                stats.finite = stats.finite && IsFinite(interaction.totalForce) && IsFinite(interaction.applicationPoint);
                if (outInteractions)
                {
                    outInteractions->push_back(interaction);
                }
            }
        }

        if (!stats.finite)
        {
            AddWarning(stats.warnings, "non-finite fluid interaction");
        }
        return stats;
    }

    std::string ToDebugString(const FluidMediumDesc& medium)
    {
        std::ostringstream out;
        out << "fluid_medium kind=" << ToString(medium.kind)
            << " density=" << medium.densityKgPerCubicMeter
            << " viscosity=" << medium.dynamicViscosityPascalSeconds
            << " drag=" << medium.linearDragCoefficient
            << " buoyancy=" << medium.buoyancyScale
            << " damage=" << medium.damagePerSecond
            << " name=" << medium.debugName;
        return out.str();
    }

    std::string ToDebugString(const FluidVolume& volume)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "fluid_volume id=" << volume.id
            << " kind=" << ToString(volume.kind)
            << " medium=" << ToString(volume.medium.kind)
            << " center=" << ToDebugString(volume.center, 3)
            << " half=" << ToDebugString(volume.halfExtents, 3)
            << " radius=" << volume.radius
            << " surface=" << volume.surfaceHeight
            << " flow=" << ToDebugString(Add(volume.medium.flowVelocity, volume.flowVelocity), 3)
            << " enabled=" << (volume.enabled ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const FluidSample& sample)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "fluid_sample inside=" << (sample.inside ? "true" : "false")
            << " volume=" << sample.volumeId
            << " kind=" << ToString(sample.volumeKind)
            << " depth=" << sample.depthMeters
            << " normalized=" << sample.normalizedDepth
            << " flow=" << ToDebugString(sample.flowVelocity, 3)
            << " normal=" << ToDebugString(sample.surfaceNormal, 3);
        return out.str();
    }

    std::string ToDebugString(const FluidColliderInteraction& interaction)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "fluid_interaction active=" << (interaction.active ? "true" : "false")
            << " volume=" << interaction.volumeIndex
            << " collider=" << interaction.colliderIndex
            << " body=" << interaction.bodyId
            << " submerged=" << interaction.submergedFraction
            << " displaced=" << interaction.displacedVolumeCubicMeters
            << " buoyancy=" << interaction.buoyancyNewton
            << " drag=" << interaction.dragNewton
            << " total=" << ToDebugString(interaction.totalForce, 3)
            << " flow=" << ToDebugString(interaction.flowVelocity, 3)
            << " damage=" << interaction.damagePerSecond;
        return out.str();
    }

    std::string ToDebugString(const FluidSystemStats& stats)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "fluid_system volumes=" << stats.volumeCount
            << " enabled=" << stats.enabledVolumeCount
            << " tested=" << stats.testedColliderCount
            << " interactions=" << stats.interactionCount
            << " buoyant=" << stats.buoyantColliderCount
            << " dragged=" << stats.draggedColliderCount
            << " forces=" << stats.appliedForceCount
            << " displaced=" << stats.totalDisplacedVolumeCubicMeters
            << " buoyancy=" << stats.totalBuoyancyNewton
            << " drag=" << stats.totalDragNewton
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return out.str();
    }

    std::string BuildFluidProbeSummary()
    {
        FluidProbeResult probe = BuildFluidProbe();
        return probe.summary;
    }

    FluidProbeResult BuildFluidProbe()
    {
        FluidProbeResult probe{};
        probe.scene.config.fixedDeltaSeconds = 1.0f / 60.0f;
        probe.scene.config.maxSubsteps = 4;
        probe.scene.config.enableSleeping = false;

        PhysicsBody boat = MakeDynamicBody(1u, {0.0f, 0.15f, 0.0f}, 120.0f);
        boat.linearDamping = 0.02f;
        boat.velocity = {1.2f, -0.4f, 0.0f};
        PhysicsCollider boatCollider = MakeBoxCollider(1u, {1.0f, 0.25f, 0.55f});
        boatCollider.material = MakePhysicsMaterial(0.6f, 0.4f, 0.05f, 550.0f);

        PhysicsBody ball = MakeDynamicBody(2u, {2.0f, -0.35f, 0.0f}, 8.0f);
        ball.velocity = {-0.4f, -1.1f, 0.0f};
        PhysicsCollider ballCollider = MakeSphereCollider(2u, 0.35f);
        ballCollider.material = MakePhysicsMaterial(0.7f, 0.45f, 0.2f, 780.0f);

        PhysicsBody rock = MakeDynamicBody(3u, {-1.5f, -0.7f, 0.2f}, 80.0f);
        rock.velocity = {0.0f, -2.0f, 0.0f};
        PhysicsCollider rockCollider = MakeCapsuleCollider(3u, 0.25f, 0.45f);
        rockCollider.material = MakePhysicsMaterial(0.9f, 0.65f, 0.02f, 2400.0f);

        probe.scene.bodies.push_back(boat);
        probe.scene.bodies.push_back(ball);
        probe.scene.bodies.push_back(rock);
        probe.scene.colliders.push_back(boatCollider);
        probe.scene.colliders.push_back(ballCollider);
        probe.scene.colliders.push_back(rockCollider);

        FluidMediumDesc sea = MakeFluidMedium(FluidMediumKind::SeaWater);
        sea.flowVelocity = {0.35f, 0.0f, 0.1f};
        FluidVolume water = MakeBoxFluidVolume(1u, {0.0f, -0.65f, 0.0f}, {5.0f, 0.85f, 5.0f}, sea);
        water.flowVelocity = {0.15f, 0.0f, 0.0f};
        probe.volumes.push_back(water);

        FluidVolume lava = MakeSphereFluidVolume(2u, {-1.5f, -0.8f, 0.2f}, 0.75f, MakeFluidMedium(FluidMediumKind::Lava));
        probe.volumes.push_back(lava);

        const float dt = 1.0f / 60.0f;
        probe.surfaceSample = SampleFluidAtPoint(probe.volumes, {0.0f, -0.2f, 0.0f});
        probe.fluidStats = ApplyFluidForcesToPhysicsScene(probe.scene, probe.volumes, dt, &probe.interactions);
        probe.physicsStats = StepPhysics(probe.scene, dt);

        probe.ok = probe.fluidStats.finite && probe.physicsStats.finite && probe.fluidStats.interactionCount >= 3 && probe.fluidStats.totalBuoyancyNewton > 1.0f && probe.surfaceSample.inside;

        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << "[ ok ] fluid/buoyancy foundation volumes=" << probe.fluidStats.volumeCount
            << " interactions=" << probe.fluidStats.interactionCount
            << " buoyant=" << probe.fluidStats.buoyantColliderCount
            << " dragged=" << probe.fluidStats.draggedColliderCount
            << " displaced=" << probe.fluidStats.totalDisplacedVolumeCubicMeters
            << " buoyancy=" << probe.fluidStats.totalBuoyancyNewton
            << " drag=" << probe.fluidStats.totalDragNewton
            << " sample_depth=" << probe.surfaceSample.depthMeters
            << " physics_contacts=" << probe.physicsStats.contactCount
            << " finite=" << (probe.fluidStats.finite && probe.physicsStats.finite ? "true" : "false")
            << " warnings=" << (probe.fluidStats.warnings.size() + probe.physicsStats.warnings.size());
        probe.summary = out.str();
        if (!probe.ok)
        {
            probe.summary.replace(2, 2, "fail");
        }
        return probe;
    }
}
