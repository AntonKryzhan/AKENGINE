#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class FluidMediumKind : u32
    {
        FreshWater = 0,
        SeaWater = 1,
        Oil = 2,
        Mud = 3,
        Lava = 4,
        Gas = 5,
        Custom = 6
    };

    enum class FluidVolumeKind : u32
    {
        Plane = 0,
        Box = 1,
        Sphere = 2
    };

    enum class FluidInteractionKind : u32
    {
        Buoyancy = 0,
        Drag = 1,
        Flow = 2
    };

    struct FluidMediumDesc
    {
        FluidMediumKind kind = FluidMediumKind::FreshWater;
        float densityKgPerCubicMeter = 997.0f;
        float dynamicViscosityPascalSeconds = 0.001f;
        float linearDragCoefficient = 1.2f;
        float angularDragCoefficient = 0.2f;
        float buoyancyScale = 1.0f;
        float maxForceNewton = 1.0e6f;
        Vec3 flowVelocity{};
        bool breathable = false;
        bool damageOnContact = false;
        float damagePerSecond = 0.0f;
        std::string debugName = "fresh_water";
    };

    struct FluidVolume
    {
        u32 id = 0;
        FluidVolumeKind kind = FluidVolumeKind::Box;
        FluidMediumDesc medium{};
        Vec3 center{};
        Vec3 halfExtents{5.0f, 1.0f, 5.0f};
        float radius = 1.0f;
        float surfaceHeight = 0.0f;
        Vec3 surfaceNormal{0.0f, 1.0f, 0.0f};
        Vec3 flowVelocity{};
        bool enabled = true;
        std::string debugName;
    };

    struct FluidSample
    {
        bool inside = false;
        std::size_t volumeIndex = 0;
        u32 volumeId = 0;
        FluidVolumeKind volumeKind = FluidVolumeKind::Box;
        FluidMediumDesc medium{};
        Vec3 point{};
        Vec3 surfaceNormal{0.0f, 1.0f, 0.0f};
        Vec3 flowVelocity{};
        float depthMeters = 0.0f;
        float normalizedDepth = 0.0f;
    };

    struct FluidColliderInteraction
    {
        bool active = false;
        std::size_t volumeIndex = 0;
        std::size_t colliderIndex = 0;
        u32 bodyId = 0;
        float submergedFraction = 0.0f;
        float displacedVolumeCubicMeters = 0.0f;
        float densityKgPerCubicMeter = 0.0f;
        float buoyancyNewton = 0.0f;
        float dragNewton = 0.0f;
        float damagePerSecond = 0.0f;
        Vec3 buoyancyForce{};
        Vec3 dragForce{};
        Vec3 totalForce{};
        Vec3 applicationPoint{};
        Vec3 flowVelocity{};
    };

    struct FluidBodyComponent
    {
        u32 bodyId = 0;
        float buoyancyScale = 1.0f;
        float dragScale = 1.0f;
        bool enableBuoyancy = true;
        bool enableDrag = true;
        bool enabled = true;
    };

    struct FluidSystemStats
    {
        std::size_t volumeCount = 0;
        std::size_t enabledVolumeCount = 0;
        std::size_t testedColliderCount = 0;
        std::size_t interactionCount = 0;
        std::size_t buoyantColliderCount = 0;
        std::size_t draggedColliderCount = 0;
        std::size_t appliedForceCount = 0;
        float totalDisplacedVolumeCubicMeters = 0.0f;
        float totalBuoyancyNewton = 0.0f;
        float totalDragNewton = 0.0f;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct FluidProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        std::vector<FluidVolume> volumes;
        std::vector<FluidColliderInteraction> interactions;
        FluidSystemStats fluidStats{};
        PhysicsStepStats physicsStats{};
        FluidSample surfaceSample{};
    };

    const char* ToString(FluidMediumKind kind);
    const char* ToString(FluidVolumeKind kind);
    const char* ToString(FluidInteractionKind kind);

    FluidMediumDesc MakeFluidMedium(FluidMediumKind kind);
    FluidVolume MakeBoxFluidVolume(u32 id, Vec3 center, Vec3 halfExtents, FluidMediumDesc medium = MakeFluidMedium(FluidMediumKind::FreshWater));
    FluidVolume MakePlaneFluidVolume(u32 id, float surfaceHeight, FluidMediumDesc medium = MakeFluidMedium(FluidMediumKind::FreshWater));
    FluidVolume MakeSphereFluidVolume(u32 id, Vec3 center, float radius, FluidMediumDesc medium = MakeFluidMedium(FluidMediumKind::FreshWater));

    FluidMediumDesc SanitizeFluidMedium(FluidMediumDesc medium);
    FluidVolume SanitizeFluidVolume(FluidVolume volume);

    FluidSample SampleFluidAtPoint(const std::vector<FluidVolume>& volumes, Vec3 point);
    float EstimateColliderVolumeCubicMeters(const PhysicsCollider& collider);
    float EstimateSubmergedFraction(const PhysicsScene& scene, const PhysicsCollider& collider, const FluidVolume& volume);
    FluidColliderInteraction ComputeFluidInteraction(const PhysicsScene& scene, std::size_t colliderIndex, const FluidVolume& volume, std::size_t volumeIndex, float deltaSeconds);
    FluidSystemStats ApplyFluidForcesToPhysicsScene(PhysicsScene& scene, const std::vector<FluidVolume>& volumes, float deltaSeconds, std::vector<FluidColliderInteraction>* outInteractions = nullptr);

    std::string ToDebugString(const FluidMediumDesc& medium);
    std::string ToDebugString(const FluidVolume& volume);
    std::string ToDebugString(const FluidSample& sample);
    std::string ToDebugString(const FluidColliderInteraction& interaction);
    std::string ToDebugString(const FluidSystemStats& stats);
    std::string BuildFluidProbeSummary();
    FluidProbeResult BuildFluidProbe();
}
