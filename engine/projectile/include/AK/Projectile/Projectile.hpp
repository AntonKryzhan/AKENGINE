#pragma once

#include <AK/Core/Types.hpp>
#include <AK/CSG/Boolean.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Gravity/GravityField.hpp>
#include <AK/Physics/Physics.hpp>
#include <AK/Surface/Surface.hpp>
#include <AK/World/WorldCoordinates.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class ProjectileState : u32
    {
        Flying = 0,
        Ricocheted = 1,
        Penetrated = 2,
        Embedded = 3,
        Stuck = 4,
        Fragmented = 5,
        Melted = 6,
        Destroyed = 7
    };

    enum class ProjectileMedium : u32
    {
        Air = 0,
        Water = 1,
        Vacuum = 2
    };

    enum class ProjectileEnvironmentPreset : u32
    {
        OpenField = 0,
        Forest = 1,
        Urban = 2,
        Indoor = 3,
        Mountains = 4,
        Desert = 5,
        Storm = 6,
        HighAltitude = 7,
        Jungle = 8,
        Arctic = 9,
        CityHighRise = 10,
        IndoorLargeHall = 11,
        Cave = 12,
        Coastal = 13,
        Snowstorm = 14,
        UnderwaterShallow = 15,
        UnderwaterDeep = 16,
        Space = 17,
        SpaceStation = 18
    };

    enum class ProjectileDamageType : u32
    {
        Arcane = 0,
        Chemical = 1,
        Electric = 2,
        Fire = 3,
        Ice = 4,
        Light = 5,
        Physical = 6,
        Psionic = 7,
        Radiation = 8
    };

    enum class ProjectileWeaponType : u32
    {
        Pistol = 0,
        Shotgun = 1,
        SMG = 2,
        AssaultRifle = 3,
        AutomaticRifle = 4,
        SniperRifle = 5,
        GrenadeLauncher = 6
    };

    enum class ProjectileImpactOutcome : u32
    {
        None = 0,
        Ricochet = 1,
        Penetration = 2,
        Embedded = 3,
        Stopped = 4
    };

    enum class ProjectileTrajectoryEventType : u32
    {
        None = 0,
        Penetrated = 1,
        NotPenetrated = 2,
        Ricocheted = 3,
        Destroyed = 4,
        Stuck = 5,
        Melted = 6
    };

    enum class ProjectileThicknessShapeKind : u32
    {
        Override = 0,
        AABB = 1,
        OrientedBox = 2,
        Sphere = 3,
        PhysicsCollider = 4,
        VoxelGrid = 5
    };

    struct ProjectilePreset
    {
        ProjectileDamageType damageType = ProjectileDamageType::Physical;
        ProjectileWeaponType weaponType = ProjectileWeaponType::AssaultRifle;
        float radiusMeters = 0.00278f;
        float lengthMeters = 0.023f;
        float densityKgPerCubicMeter = 11340.0f;
        float heatCapacityJPerKgK = 1200.0f;
        float meltTemperatureKelvin = 600.61f;
        float initialSpeedMetersPerSecond = 920.0f;
        float rotationalLengthMeters = 0.1778f;
        float aeroMomentCoef = 1.0e-6f;
        float emissivity = 0.28f;
        float baseDragCoefficient = 0.255f;
        float turbulenceIntensity = 0.1f;
        float latentHeatJPerKg = 871000.0f;
        float maxTemperatureKelvin = 2000.0f;
        float gravityMetersPerSecondSquared = 9.81f;
        float characteristicHeightMeters = 50.0f;
        float angleStabilityCoef = 0.1f;
    };

    struct ProjectileTurbulencePreset
    {
        std::string name = "Open Field";
        ProjectileMedium medium = ProjectileMedium::Air;
        float turbulenceIntensity = 0.08f;
        float turbulenceTauSeconds = 0.25f;
        float turbulenceSigmaBaseMetersPerSecond = 1.8f;
        float turbulenceMaxStdMetersPerSecond = 6.0f;
        float turbulenceAltitudeDecayMeters = 1800.0f;
        float baseWindSpeedMetersPerSecond = 2.0f;
        Vec3 baseWindDirection{1.0f, 0.0f, 0.0f};
        bool overrideDensity = false;
        float densityOverrideKgPerCubicMeter = 1.225f;
        bool overrideSpeedOfSound = false;
        float speedOfSoundOverrideMetersPerSecond = 343.2f;
    };

    struct ProjectileEnvironment
    {
        ProjectileMedium medium = ProjectileMedium::Air;
        ProjectileEnvironmentPreset preset = ProjectileEnvironmentPreset::OpenField;
        float densityKgPerCubicMeter = 1.225f;
        float speedOfSoundMetersPerSecond = 343.2f;
        float pressurePascals = 101325.0f;
        float ambientTemperatureKelvin = 288.15f;
        float referenceAmbientTemperatureKelvin = 288.15f;
        Vec3 gravityMetersPerSecondSquared{0.0f, -9.80665f, 0.0f};
        Vec3 baseWindDirection{1.0f, 0.0f, 0.0f};
        float baseWindSpeedMetersPerSecond = 2.0f;
        Vec3 meanWindMetersPerSecond{};
        Vec3 turbulenceMetersPerSecond{};
        Vec3 windMetersPerSecond{};
        float turbulenceIntensity = 0.08f;
        float turbulenceTauSeconds = 0.25f;
        float turbulenceSigmaMetersPerSecond = 1.8f;
        float turbulenceMaxStdMetersPerSecond = 6.0f;
        float turbulenceAltitudeDecayMeters = 1800.0f;
        bool densityOverridden = false;
        bool speedOfSoundOverridden = false;
        bool turbulenceEnabled = true;
        u32 randomSeed = 0xA17B4115u;
        float timeSeconds = 0.0f;
    };

    struct ProjectileBody
    {
        ProjectileState state = ProjectileState::Flying;
        Vec3 position{};
        Vec3 previousPosition{};
        Vec3 velocity{0.0f, 0.0f, 920.0f};
        Vec3 angularVelocity{};
        Vec3 forward{0.0f, 0.0f, 1.0f};
        float massKilograms = 0.006f;
        float initialMassKilograms = 0.006f;
        float radiusMeters = 0.00278f;
        float lengthMeters = 0.023f;
        float densityKgPerCubicMeter = 11340.0f;
        float heatCapacityJPerKgK = 1200.0f;
        float latentHeatJPerKg = 871000.0f;
        float emissivity = 0.28f;
        float temperatureKelvin = 288.15f;
        float ambientReferenceTemperatureKelvin = 288.15f;
        float thermalEnergyJoules = 0.0f;
        float meltTemperatureKelvin = 600.61f;
        float maxTemperatureKelvin = 2000.0f;
        float criticalAngularVelocityRadiansPerSecond = 1000.0f;
        float aeroMomentCoef = 1.0e-6f;
        float baseDragCoefficient = 0.255f;
        float ageSeconds = 0.0f;
        float stateTimerSeconds = 0.0f;
        float embeddedLifetimeSeconds = 30.0f;
        float embedNoseOffsetMeters = 0.004f;
        float maxEmbedDepthFallbackMeters = 0.012f;
        float heavySimulationDisableAfterSeconds = 1.0f;
        bool heavySimulationEnabled = true;
        bool enableMagnus = true;
        bool enableCoriolis = true;
        bool enableTurbulence = true;
        bool enableThermalSimulation = true;
    };

    struct ProjectileWorldState
    {
        WorldPosition position{};
        WorldPosition previousPosition{};
        WorldPosition physicsIslandOrigin{};
        DVec3 velocityMetersPerSecond{};
        DVec3 angularVelocityRadiansPerSecond{};
        double cellSizeMeters = DefaultWorldCellSizeMeters;
        double physicsRebaseThresholdMeters = DefaultWorldRebaseThresholdMeters;
        bool authoritativeLargeWorld = true;
        bool originRebasedThisStep = false;
        bool finite = true;
    };

    struct ProjectileStepSettings
    {
        float deltaSeconds = 1.0f / 120.0f;
        float maxSimulationSeconds = 30.0f;
        float stopSpeedMetersPerSecond = 0.1f;
        float maxAbsPositionMeters = 10000.0f;
        float maxSpeedMetersPerSecond = 2500.0f;
        float maxStepSeconds = 0.1f;
        bool enableDrag = true;
        bool enableGravity = true;
        bool enableWind = true;
        bool enableSpinStabilization = true;
        bool enableMagnus = true;
        bool enableCoriolis = true;
        bool enableTurbulence = true;
        bool enableThermal = true;
        bool enableMassErosion = true;
        bool destroyOnStop = true;
    };

    struct ProjectileStepResult
    {
        bool ok = false;
        bool finite = true;
        ProjectileState state = ProjectileState::Flying;
        Vec3 previousPosition{};
        Vec3 newPosition{};
        Vec3 accelerationMetersPerSecondSquared{};
        Vec3 windMetersPerSecond{};
        float speedMetersPerSecond = 0.0f;
        float kineticEnergyJoules = 0.0f;
        float mach = 0.0f;
        float dragCoefficient = 0.0f;
        float massKilograms = 0.0f;
        float temperatureKelvin = 0.0f;
        float massLostKilograms = 0.0f;
        std::vector<std::string> warnings;
    };

    struct ProjectileNativeStepSettings
    {
        ProjectileStepSettings step{};
        LargeWorldConfig largeWorld{};
        GravitySolverConfig gravity{};
        bool sampleGravityFields = true;
        bool syncBodyFromLargeWorld = true;
        bool syncLargeWorldFromBody = true;
        bool rebasePhysicsOrigin = true;
    };

    struct ProjectileNativeStepResult
    {
        bool ok = false;
        ProjectileStepResult step{};
        GravitySample gravity{};
        CameraRelativePosition physicsLocal{};
        WorldPosition previousWorldPosition{};
        WorldPosition newWorldPosition{};
        bool originRebased = false;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct ProjectileImpactMaterial
    {
        std::string name = "default";
        float densityKgPerCubicMeter = 1000.0f;
        float speedOfSoundMetersPerSecond = 1200.0f;
        float yieldStrengthPascals = 25.0e6f;
        float youngModulusPascals = 2.0e9f;
        float ricochetEnergyLoss = 0.25f;
        float maxDeformationMeters = 0.01f;
        float energyDissipation = 0.1f;
        float destructionResistance = 1.0f;
        float thicknessOverrideMeters = 0.0f;
        bool softNoRicochet = false;
        bool valid = true;
    };

    struct ProjectileMaterialDamageState
    {
        std::string name = "default";
        float originalYieldStrengthPascals = 25.0e6f;
        float currentYieldStrengthPascals = 25.0e6f;
        float originalDensityKgPerCubicMeter = 1000.0f;
        float currentDensityKgPerCubicMeter = 1000.0f;
        float accumulatedDamagePercent = 0.0f;
        bool valid = true;
    };

    struct ProjectileThicknessShape
    {
        ProjectileThicknessShapeKind kind = ProjectileThicknessShapeKind::AABB;
        AABB3 bounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        Vec3 boxCenter{};
        Vec3 boxAxisX{1.0f, 0.0f, 0.0f};
        Vec3 boxAxisY{0.0f, 1.0f, 0.0f};
        Vec3 boxAxisZ{0.0f, 0.0f, 1.0f};
        Vec3 boxSizeMeters{1.0f, 1.0f, 1.0f};
        Sphere3 sphere{};
        PhysicsCollider physicsCollider{};
        const CsgVoxelGrid* voxelGrid = nullptr;
        float overrideThicknessMeters = 0.0f;
        float shellEpsilonMeters = 0.003f;
    };

    struct ProjectileThicknessInput
    {
        ProjectileThicknessShape shape{};
        Vec3 contactPoint{};
        Vec3 surfaceNormal{0.0f, 1.0f, 0.0f};
        Vec3 travelDirection{0.0f, 0.0f, 1.0f};
        float minThicknessMeters = 0.001f;
        float fallbackThicknessMeters = 0.02f;
    };

    struct ProjectileThicknessResult
    {
        bool valid = false;
        float thicknessMeters = 0.02f;
        Vec3 exitPoint{};
        std::string method = "fallback";
    };

    struct ProjectileImpactInput
    {
        ProjectileBody projectile{};
        ProjectileImpactMaterial material{};
        ProjectileMaterialDamageState materialDamage{};
        Vec3 hitPoint{};
        Vec3 surfaceNormal{0.0f, 1.0f, 0.0f};
        float thicknessMeters = 0.1f;
        Vec3 exitPoint{};
        float shellEpsilonMeters = 0.003f;
    };

    struct ProjectileImpactResult
    {
        ProjectileImpactOutcome outcome = ProjectileImpactOutcome::None;
        ProjectileState nextState = ProjectileState::Flying;
        ProjectileTrajectoryEventType eventType = ProjectileTrajectoryEventType::None;
        Vec3 outPosition{};
        Vec3 outVelocity{};
        Vec3 outAngularVelocity{};
        float obliquityDegrees = 0.0f;
        float impactAngleFromPlaneDegrees = 0.0f;
        float kineticEnergyJoules = 0.0f;
        float penetrationEnergyJoules = 0.0f;
        float residualEnergyJoules = 0.0f;
        float embedDepthMeters = 0.0f;
        float impactForceNewtons = 0.0f;
        float damagePercent = 0.0f;
        float massLossKilograms = 0.0f;
        ProjectileMaterialDamageState materialDamage{};
        bool valid = false;
    };

    struct ProjectilePhysicsHit
    {
        bool hit = false;
        std::size_t colliderIndex = 0;
        u32 bodyId = 0;
        PhysicsColliderKind colliderKind = PhysicsColliderKind::Box;
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float distanceMeters = 0.0f;
        ProjectileThicknessResult thickness{};
    };

    struct ProjectileSweepSettings
    {
        float shellRadiusMeters = 0.003f;
        float maxDistanceMeters = 10000.0f;
        u32 maxHits = 8;
        bool sortByDistance = true;
        bool includeTriggers = false;
        bool computeThickness = true;
    };

    struct ProjectileMultiHitResult
    {
        bool hit = false;
        std::vector<ProjectilePhysicsHit> hits;
        bool clippedByMaxHits = false;
        bool finite = true;
    };

    struct ProjectileIntegratedStepResult
    {
        bool ok = false;
        ProjectileStepResult step{};
        ProjectilePhysicsHit hit{};
        ProjectileImpactResult impact{};
        bool collisionResolved = false;
        std::vector<std::string> warnings;
    };

    struct VoxelTunnelSettings
    {
        float radiusMeters = 0.1f;
        float maxDepthMeters = 1.0f;
        bool removeUnsupportedAfterCut = true;
        bool supportFromBottomY = true;
        bool createDebrisCandidates = true;
    };

    struct VoxelDebrisCandidate
    {
        Vec3 center{};
        Vec3 size{};
        Vec3 initialImpulse{};
        float massKilograms = 1.0f;
        bool supported = false;
    };

    struct VoxelDamageStats
    {
        u64 visitedTunnelCells = 0;
        u64 clearedVoxels = 0;
        u64 unsupportedRemovedVoxels = 0;
        u64 debrisCandidateVoxels = 0;
        u64 exposedFacesAfter = 0;
        bool clippedByBounds = false;
        bool ok = false;
        std::vector<std::string> warnings;
        std::vector<VoxelDebrisCandidate> debrisCandidates;
    };

    struct VoxelRleRun
    {
        u8 value = 0;
        u32 count = 0;
    };

    struct VoxelBitsetOccupancy
    {
        u32 resolutionX = 0;
        u32 resolutionY = 0;
        u32 resolutionZ = 0;
        std::vector<u64> words;
        u64 solidCount = 0;
        bool valid = false;
    };

    struct VoxelChunkDirtyRange
    {
        u32 minX = 0;
        u32 minY = 0;
        u32 minZ = 0;
        u32 maxX = 0;
        u32 maxY = 0;
        u32 maxZ = 0;
        bool valid = false;
    };

    struct VoxelDamageWorkset
    {
        VoxelBitsetOccupancy occupancy{};
        VoxelChunkDirtyRange dirtyRange{};
        u32 chunkSize = 8;
        u64 chunkCount = 0;
        u64 dirtyChunkCount = 0;
        u64 bytesUsed = 0;
        bool valid = false;
    };

    struct ProjectileTrajectorySample
    {
        Vec3 position{};
        float timeSeconds = 0.0f;
    };

    struct ProjectileTrajectoryEvent
    {
        Vec3 position{};
        ProjectileTrajectoryEventType type = ProjectileTrajectoryEventType::None;
        float timeSeconds = 0.0f;
    };

    struct ProjectileTrajectory
    {
        std::vector<ProjectileTrajectorySample> samples;
        std::vector<ProjectileTrajectoryEvent> events;
        u32 maxSamples = 512;
    };

    struct ProjectileComponent
    {
        ProjectileBody body{};
        ProjectileWorldState world{};
        ProjectileEnvironment environment{};
        ProjectileImpactMaterial impactMaterial{};
        ProjectileTrajectory trajectory{};
        bool active = true;
    };

    struct BallisticBodyComponent
    {
        ProjectileImpactMaterial material{};
        ProjectileMaterialDamageState damage{};
        bool receiveProjectileDamage = true;
    };

    struct DestructibleVoxelComponent
    {
        CsgVoxelGrid* grid = nullptr;
        VoxelDamageWorkset workset{};
        bool dirty = false;
    };

    struct ProjectileSystemStats
    {
        u32 activeProjectiles = 0;
        u32 steppedProjectiles = 0;
        u32 collisionQueries = 0;
        u32 impacts = 0;
        u32 penetrations = 0;
        u32 ricochets = 0;
        u32 stopped = 0;
        u32 largeWorldRebases = 0;
        u32 gravitySamples = 0;
        u32 warnings = 0;
        bool ok = false;
    };

    struct ProjectileProbeResult
    {
        bool ok = false;
        ProjectileBody projectile{};
        ProjectileEnvironment environment{};
        ProjectileStepResult step{};
        ProjectilePhysicsHit physicsHit{};
        ProjectileImpactResult impact{};
        ProjectileIntegratedStepResult integrated{};
        ProjectileNativeStepResult nativeStep{};
        ProjectileMultiHitResult multiHit{};
        VoxelDamageStats damage{};
        VoxelDamageWorkset voxelWorkset{};
        ProjectileSystemStats systemStats{};
        std::vector<VoxelRleRun> compressedRuns;
        ProjectileTrajectory trajectory{};
        CsgVoxelGrid damagedGrid{};
        std::string summary;
    };

    const char* ToString(ProjectileState state);
    const char* ToString(ProjectileMedium medium);
    const char* ToString(ProjectileEnvironmentPreset preset);
    const char* ToString(ProjectileDamageType type);
    const char* ToString(ProjectileWeaponType type);
    const char* ToString(ProjectileImpactOutcome outcome);
    const char* ToString(ProjectileTrajectoryEventType type);
    const char* ToString(ProjectileThicknessShapeKind kind);

    ProjectilePreset MakeDefaultRifleProjectilePreset();
    ProjectilePreset MakeProjectilePreset(ProjectileDamageType damageType, ProjectileWeaponType weaponType);
    ProjectilePreset ApplyWeaponTypeToProjectilePreset(ProjectilePreset preset, ProjectileWeaponType weaponType);
    ProjectileTurbulencePreset MakeProjectileTurbulencePreset(ProjectileEnvironmentPreset preset);
    ProjectileEnvironment MakeProjectileEnvironment(ProjectileEnvironmentPreset preset);
    ProjectileBody MakeProjectileBody(const ProjectilePreset& preset, Vec3 position, Vec3 forward);
    ProjectileWorldState MakeProjectileWorldState(WorldPosition position, Vec3 velocityMetersPerSecond = {}, double cellSizeMeters = DefaultWorldCellSizeMeters);
    void SyncProjectileBodyFromWorldState(ProjectileBody& projectile, const ProjectileWorldState& world);
    void SyncProjectileWorldStateFromBody(const ProjectileBody& projectile, ProjectileWorldState& world);

    float ComputeProjectileMassKilograms(const ProjectilePreset& preset);
    float ComputeProjectileAreaSquareMeters(const ProjectileBody& projectile);
    float ComputeProjectileSurfaceAreaSquareMeters(const ProjectileBody& projectile);
    float ComputeProjectileKineticEnergyJoules(const ProjectileBody& projectile);
    float ProjectileCdFromMach(float mach);

    void SetProjectileState(ProjectileBody& projectile, ProjectileState state);
    void UpdateProjectileAtmosphere(ProjectileEnvironment& environment, Vec3 positionMeters);
    void UpdateProjectileWind(ProjectileEnvironment& environment, float deltaSeconds, Vec3 positionMeters, bool enableTurbulence);
    Vec3 ComputeProjectileMagnusForce(const ProjectileBody& projectile, const ProjectileEnvironment& environment, Vec3 flowDirection, float speedMetersPerSecond);
    Vec3 ComputeProjectileCoriolisForce(const ProjectileBody& projectile);
    Vec3 ComputeProjectileAcceleration(const ProjectileBody& projectile, const ProjectileEnvironment& environment, const ProjectileStepSettings& settings);
    void UpdateProjectileAngular(ProjectileBody& projectile, float deltaSeconds);
    void UpdateProjectileThermal(ProjectileBody& projectile, const ProjectileEnvironment& environment, float speedMetersPerSecond, float deltaSeconds);
    float UpdateProjectileMassErosion(ProjectileBody& projectile, const ProjectileEnvironment& environment, float deltaSeconds);
    float ComputeProjectileTemperatureYieldStrengthPascals(const ProjectileBody& projectile, float referenceYieldStrengthPascals = 250.0e6f);
    ProjectileStepResult StepProjectile(ProjectileBody& projectile, const ProjectileEnvironment& environment, const ProjectileStepSettings& settings = {});
    ProjectileStepResult StepProjectileAdvanced(ProjectileBody& projectile, ProjectileEnvironment& environment, const ProjectileStepSettings& settings = {});
    GravitySample SampleProjectileGravity(const ProjectileWorldState& world, const ProjectileEnvironment& environment, const std::vector<GravityFieldDesc>& gravityFields = {}, const GravitySolverConfig& config = {});
    ProjectileNativeStepResult StepProjectileNative(ProjectileBody& projectile, ProjectileWorldState& world, ProjectileEnvironment& environment, const std::vector<GravityFieldDesc>& gravityFields = {}, const ProjectileNativeStepSettings& settings = {});

    ProjectileImpactMaterial MakeProjectileImpactMaterial(const SurfaceMaterialDesc& material);
    ProjectileImpactMaterial SanitizeProjectileImpactMaterial(ProjectileImpactMaterial material);
    ProjectileMaterialDamageState MakeProjectileMaterialDamageState(const ProjectileImpactMaterial& material);
    float ComputePenetrationEnergyJoules(const ProjectileBody& projectile, const ProjectileImpactMaterial& material, float obliquityDegrees, float thicknessMeters);
    float ComputeRicochetThresholdDegrees(const ProjectileBody& projectile, const ProjectileImpactMaterial& material);
    float ComputeProjectileImpactForceNewtons(const ProjectileBody& projectile, const ProjectileImpactMaterial& material, float thicknessMeters, float impactAngleFromPlaneDegrees);
    ProjectileMaterialDamageState ApplyProjectileMaterialDeformation(ProjectileMaterialDamageState state, const ProjectileImpactMaterial& material, const ProjectileBody& projectile, float impactForceNewtons, float thicknessMeters);
    ProjectileThicknessResult ComputeProjectileThickness(const ProjectileThicknessInput& input);
    ProjectileImpactResult ResolveProjectileImpact(const ProjectileImpactInput& input);
    void ApplyProjectileImpact(ProjectileBody& projectile, const ProjectileImpactResult& impact);

    ProjectilePhysicsHit SweepProjectileAgainstPhysicsScene(const PhysicsScene& scene, Vec3 from, Vec3 to, float shellEpsilonMeters = 0.003f);
    ProjectileMultiHitResult SweepProjectileMultiHitAgainstPhysicsScene(const PhysicsScene& scene, Vec3 from, Vec3 to, const ProjectileSweepSettings& settings = {});
    ProjectileIntegratedStepResult StepProjectileIntegrated(ProjectileBody& projectile, PhysicsScene& physicsScene, ProjectileEnvironment& environment, const ProjectileImpactMaterial& material, const ProjectileStepSettings& settings = {});

    VoxelDamageStats RemoveVoxelTunnelDda(CsgVoxelGrid& grid, Vec3 localStart, Vec3 localDirection, const VoxelTunnelSettings& settings = {});
    u64 RemoveUnsupportedVoxelsFloodFill(CsgVoxelGrid& grid, bool supportFromBottomY = true, std::vector<VoxelDebrisCandidate>* outDebris = nullptr);
    u64 CountVoxelExposedFaces(const CsgVoxelGrid& grid);
    std::vector<VoxelRleRun> CompressVoxelGridRLE(const CsgVoxelGrid& grid);
    bool DecompressVoxelGridRLE(CsgVoxelGrid& grid, const std::vector<VoxelRleRun>& runs);
    VoxelBitsetOccupancy BuildVoxelBitsetOccupancy(const CsgVoxelGrid& grid);
    bool ApplyVoxelBitsetOccupancy(CsgVoxelGrid& grid, const VoxelBitsetOccupancy& occupancy);
    bool GetVoxelBit(const VoxelBitsetOccupancy& occupancy, u64 index);
    void SetVoxelBit(VoxelBitsetOccupancy& occupancy, u64 index, bool solid);
    VoxelDamageWorkset BuildVoxelDamageWorkset(const CsgVoxelGrid& grid, const VoxelDamageStats& stats = {}, u32 chunkSize = 8);

    void AddProjectileTrajectorySample(ProjectileTrajectory& trajectory, Vec3 position, float timeSeconds);
    void AddProjectileTrajectoryEvent(ProjectileTrajectory& trajectory, Vec3 position, ProjectileTrajectoryEventType type, float timeSeconds);
    std::vector<Vec3> BuildProjectileCatmullRomTrajectory(const ProjectileTrajectory& trajectory, u32 interpolationSteps = 20);
    ProjectileSystemStats ProjectileSystemFixedUpdate(std::vector<ProjectileComponent>& projectiles, PhysicsScene& physicsScene, const std::vector<GravityFieldDesc>& gravityFields, const ProjectileNativeStepSettings& settings = {});

    std::string ToDebugString(const ProjectileStepResult& result);
    std::string ToDebugString(const ProjectileImpactResult& result);
    std::string ToDebugString(const ProjectilePhysicsHit& hit);
    std::string ToDebugString(const VoxelDamageStats& stats);
    std::string ToDebugString(const ProjectileProbeResult& probe);
    ProjectileProbeResult BuildProjectileProbe();
    std::string BuildProjectileProbeSummary();
}
