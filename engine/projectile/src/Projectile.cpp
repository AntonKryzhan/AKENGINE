#include <AK/Projectile/Projectile.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr float MinimumMass = 1.0e-6f;
        constexpr float MinimumLength = 1.0e-6f;
        constexpr float MinimumArea = 1.0e-9f;
        constexpr float MinimumDirectionLength = 1.0e-8f;
        constexpr float StefanBoltzmann = 5.67e-8f;
        constexpr float EarthAngularVelocity = 7.2921159e-5f;
        constexpr std::array<float, 10> MachKnots{0.0f, 0.3f, 0.7f, 0.9f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 3.0f};
        constexpr std::array<float, 10> CdG7{0.195f, 0.195f, 0.205f, 0.235f, 0.255f, 0.230f, 0.205f, 0.190f, 0.182f, 0.178f};

        float ClampNonNegative(float value)
        {
            return IsFinite(value) && value > 0.0f ? value : 0.0f;
        }

        float ClampPositive(float value, float fallback)
        {
            return IsFinite(value) && value > 0.0f ? value : fallback;
        }

        Vec3 Negate(Vec3 value)
        {
            return {-value.x, -value.y, -value.z};
        }

        Vec3 AbsVec(Vec3 value)
        {
            return {std::abs(value.x), std::abs(value.y), std::abs(value.z)};
        }

        Vec3 Reflect(Vec3 direction, Vec3 normal)
        {
            const Vec3 n = Normalize(normal, {0.0f, 1.0f, 0.0f});
            return Subtract(direction, Multiply(n, 2.0f * Dot(direction, n)));
        }

        Vec3 ClampMagnitude(Vec3 value, float maxLength)
        {
            const float lengthSq = LengthSquared(value);
            const float maxSq = maxLength * maxLength;
            if (lengthSq <= maxSq || lengthSq <= MinimumDirectionLength || maxLength <= 0.0f)
            {
                return value;
            }
            return Multiply(value, maxLength / std::sqrt(lengthSq));
        }

        Vec3 ToVec3Checked(DVec3 value)
        {
            return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
        }

        DVec3 ToDVec3Checked(Vec3 value)
        {
            return {static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.z)};
        }

        DVec3 WorldPositionToAbsoluteMeters(WorldPosition position, double cellSizeMeters)
        {
            const double cell = cellSizeMeters > 0.0 ? cellSizeMeters : DefaultWorldCellSizeMeters;
            return {
                static_cast<double>(position.cell.x) * cell + position.localX,
                static_cast<double>(position.cell.y) * cell + position.localY,
                static_cast<double>(position.cell.z) * cell + position.localZ
            };
        }

        WorldPosition AddWorldOffsetD(WorldPosition position, DVec3 offsetMeters, double cellSizeMeters)
        {
            WorldPosition out = position;
            out.localX += offsetMeters.x;
            out.localY += offsetMeters.y;
            out.localZ += offsetMeters.z;
            return NormalizeWorldPosition(out, cellSizeMeters > 0.0 ? cellSizeMeters : DefaultWorldCellSizeMeters);
        }

        DVec3 RelativeWorldD(WorldPosition position, WorldPosition origin, double cellSizeMeters)
        {
            return Subtract(WorldPositionToAbsoluteMeters(position, cellSizeMeters), WorldPositionToAbsoluteMeters(origin, cellSizeMeters));
        }

        bool IsFiniteWorldState(const ProjectileWorldState& world)
        {
            return AK::IsFinite(world.position)
                && AK::IsFinite(world.previousPosition)
                && AK::IsFinite(world.physicsIslandOrigin)
                && AK::IsFinite(world.velocityMetersPerSecond)
                && AK::IsFinite(world.angularVelocityRadiansPerSecond)
                && AK::IsFinite(world.cellSizeMeters)
                && world.cellSizeMeters > 0.0;
        }

        u64 VoxelBitWordCount(u64 bitCount)
        {
            return (bitCount + 63u) / 64u;
        }

        bool IsVoxelBitsetShapeValid(const VoxelBitsetOccupancy& occupancy)
        {
            const u64 cellCount = static_cast<u64>(occupancy.resolutionX) * occupancy.resolutionY * occupancy.resolutionZ;
            return occupancy.valid && cellCount > 0 && occupancy.words.size() == static_cast<std::size_t>(VoxelBitWordCount(cellCount));
        }

        Vec3 ComputeAabbNormalAtPoint(AABB3 bounds, Vec3 point);
        bool SweepSegmentSphere(Vec3 from, Vec3 to, Sphere3 sphere, float* outT);

        bool SweepColliderForProjectile(const PhysicsScene& scene, std::size_t colliderIndex, Vec3 from, Vec3 to, float shellRadiusMeters, bool includeTriggers, ProjectilePhysicsHit& outHit)
        {
            if (colliderIndex >= scene.colliders.size())
            {
                return false;
            }

            const PhysicsCollider& collider = scene.colliders[colliderIndex];
            if (!collider.enabled || (collider.trigger && !includeTriggers))
            {
                return false;
            }

            const PhysicsBody* body = FindBody(scene, collider.bodyId);
            if (!body || !body->enabled)
            {
                return false;
            }

            const Vec3 segment = Subtract(to, from);
            const float segmentLength = Length(segment);
            if (segmentLength <= MinimumDirectionLength)
            {
                return false;
            }

            const Ray3 ray{from, Divide(segment, segmentLength)};
            float tMeters = 0.0f;
            bool hit = false;
            Vec3 normal{0.0f, 1.0f, 0.0f};
            Vec3 point{};

            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                const Sphere3 sphere{Add(body->position, collider.localCenter), collider.radius + shellRadiusMeters};
                float tNorm = 0.0f;
                if (SweepSegmentSphere(from, to, sphere, &tNorm))
                {
                    hit = true;
                    tMeters = tNorm * segmentLength;
                    point = Add(from, Multiply(segment, tNorm));
                    normal = Normalize(Subtract(point, sphere.center), {0.0f, 1.0f, 0.0f});
                }
            }
            else
            {
                const AABB3 bounds = InflateAABB(ComputeColliderWorldBounds(*body, collider), shellRadiusMeters);
                float tMin = 0.0f;
                float tMax = 0.0f;
                if (RayIntersectsAABB(ray, bounds, &tMin, &tMax) && tMin >= 0.0f && tMin <= segmentLength)
                {
                    hit = true;
                    tMeters = tMin;
                    point = Add(from, Multiply(ray.direction, tMin));
                    normal = ComputeAabbNormalAtPoint(bounds, point);
                }
            }

            if (!hit)
            {
                return false;
            }

            outHit.hit = true;
            outHit.colliderIndex = colliderIndex;
            outHit.bodyId = collider.bodyId;
            outHit.colliderKind = collider.kind;
            outHit.point = point;
            outHit.normal = normal;
            outHit.distanceMeters = tMeters;

            ProjectileThicknessInput thicknessInput{};
            thicknessInput.contactPoint = point;
            thicknessInput.surfaceNormal = normal;
            thicknessInput.travelDirection = ray.direction;
            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                thicknessInput.shape.kind = ProjectileThicknessShapeKind::Sphere;
                thicknessInput.shape.sphere = {Add(body->position, collider.localCenter), collider.radius};
            }
            else
            {
                thicknessInput.shape.kind = ProjectileThicknessShapeKind::AABB;
                thicknessInput.shape.bounds = ComputeColliderWorldBounds(*body, collider);
            }
            outHit.thickness = ComputeProjectileThickness(thicknessInput);
            return true;
        }

        bool IsFlying(ProjectileState state)
        {
            return state == ProjectileState::Flying || state == ProjectileState::Ricocheted || state == ProjectileState::Penetrated;
        }

        bool IsInsideGrid(const CsgVoxelGrid& grid, i32 x, i32 y, i32 z)
        {
            return x >= 0 && y >= 0 && z >= 0
                && static_cast<u32>(x) < grid.resolutionX
                && static_cast<u32>(y) < grid.resolutionY
                && static_cast<u32>(z) < grid.resolutionZ;
        }

        bool IsGridUsable(const CsgVoxelGrid& grid)
        {
            const u64 cellCount = CsgCellCount(grid);
            return IsValid(grid.bounds)
                && grid.resolutionX > 0
                && grid.resolutionY > 0
                && grid.resolutionZ > 0
                && grid.solid.size() == static_cast<std::size_t>(cellCount);
        }

        i32 CellIndexAlongAxis(float position, float minValue, float cellSize, u32 resolution)
        {
            if (cellSize <= 0.0f || !IsFinite(position))
            {
                return -1;
            }
            const float relative = (position - minValue) / cellSize;
            const i32 index = static_cast<i32>(std::floor(relative));
            if (index < 0 || index >= static_cast<i32>(resolution))
            {
                return -1;
            }
            return index;
        }

        float Hash01(u32 seed, u32 salt)
        {
            u32 x = seed ^ (salt * 0x9E3779B9u);
            x ^= x >> 16u;
            x *= 0x7FEB352Du;
            x ^= x >> 15u;
            x *= 0x846CA68Bu;
            x ^= x >> 16u;
            return static_cast<float>(x & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
        }

        float Smoothstep(float t)
        {
            t = Saturate(t);
            return t * t * (3.0f - 2.0f * t);
        }

        float ValueNoise1D(float x, u32 seed)
        {
            const float xf = std::floor(x);
            const u32 i0 = static_cast<u32>(static_cast<i32>(xf));
            const float t = x - xf;
            const float a = Hash01(seed, i0) * 2.0f - 1.0f;
            const float b = Hash01(seed, i0 + 1u) * 2.0f - 1.0f;
            return Lerp(a, b, Smoothstep(t));
        }

        float NextGaussianDeterministic(u32& seed)
        {
            seed = seed * 1664525u + 1013904223u;
            const float u1 = std::max(Hash01(seed, 17u), 1.0e-7f);
            seed = seed * 1664525u + 1013904223u;
            const float u2 = Hash01(seed, 29u);
            const float rad = std::sqrt(-2.0f * std::log(u1));
            return rad * std::cos(TwoPi32 * u2);
        }

        ProjectileTrajectoryEventType EventFromOutcome(ProjectileImpactOutcome outcome)
        {
            switch (outcome)
            {
            case ProjectileImpactOutcome::Ricochet:
                return ProjectileTrajectoryEventType::Ricocheted;
            case ProjectileImpactOutcome::Penetration:
                return ProjectileTrajectoryEventType::Penetrated;
            case ProjectileImpactOutcome::Embedded:
                return ProjectileTrajectoryEventType::NotPenetrated;
            case ProjectileImpactOutcome::Stopped:
                return ProjectileTrajectoryEventType::Stuck;
            default:
                return ProjectileTrajectoryEventType::None;
            }
        }

        Vec3 ComputeAabbNormalAtPoint(AABB3 bounds, Vec3 point)
        {
            const float dxMin = std::abs(point.x - bounds.min.x);
            const float dxMax = std::abs(point.x - bounds.max.x);
            const float dyMin = std::abs(point.y - bounds.min.y);
            const float dyMax = std::abs(point.y - bounds.max.y);
            const float dzMin = std::abs(point.z - bounds.min.z);
            const float dzMax = std::abs(point.z - bounds.max.z);

            float best = dxMin;
            Vec3 normal{-1.0f, 0.0f, 0.0f};
            if (dxMax < best) { best = dxMax; normal = {1.0f, 0.0f, 0.0f}; }
            if (dyMin < best) { best = dyMin; normal = {0.0f, -1.0f, 0.0f}; }
            if (dyMax < best) { best = dyMax; normal = {0.0f, 1.0f, 0.0f}; }
            if (dzMin < best) { best = dzMin; normal = {0.0f, 0.0f, -1.0f}; }
            if (dzMax < best) { normal = {0.0f, 0.0f, 1.0f}; }
            return normal;
        }

        bool SweepSegmentSphere(Vec3 from, Vec3 to, Sphere3 sphere, float* outT)
        {
            const Vec3 d = Subtract(to, from);
            const Vec3 m = Subtract(from, sphere.center);
            const float a = LengthSquared(d);
            const float b = 2.0f * Dot(m, d);
            const float c = LengthSquared(m) - sphere.radius * sphere.radius;
            if (a <= MinimumDirectionLength)
            {
                return false;
            }
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f)
            {
                return false;
            }
            const float sqrtDisc = std::sqrt(disc);
            const float inv = 1.0f / (2.0f * a);
            const float t0 = (-b - sqrtDisc) * inv;
            const float t1 = (-b + sqrtDisc) * inv;
            float t = std::numeric_limits<float>::max();
            if (t0 >= 0.0f && t0 <= 1.0f) t = t0;
            else if (t1 >= 0.0f && t1 <= 1.0f) t = t1;
            if (t == std::numeric_limits<float>::max())
            {
                return false;
            }
            if (outT) *outT = t;
            return true;
        }

        void PushDebrisCandidate(const CsgVoxelGrid& grid, u32 x, u32 y, u32 z, Vec3 impulse, std::vector<VoxelDebrisCandidate>* outDebris)
        {
            if (!outDebris)
            {
                return;
            }
            const Vec3 size = CsgCellSize(grid);
            VoxelDebrisCandidate candidate{};
            candidate.center = CsgCellCenter(grid, x, y, z);
            candidate.size = size;
            candidate.initialImpulse = impulse;
            candidate.massKilograms = std::max(0.001f, size.x * size.y * size.z * 1000.0f);
            candidate.supported = false;
            outDebris->push_back(candidate);
        }

        void ClearSphere(CsgVoxelGrid& grid, Vec3 center, float radius, VoxelDamageStats& stats, Vec3 impulse)
        {
            const Vec3 cell = CsgCellSize(grid);
            const float r = std::max(radius, std::min({cell.x, cell.y, cell.z}) * 0.05f);
            const float r2 = r * r;

            const i32 minX = std::max(0, CellIndexAlongAxis(center.x - r, grid.bounds.min.x, cell.x, grid.resolutionX));
            const i32 minY = std::max(0, CellIndexAlongAxis(center.y - r, grid.bounds.min.y, cell.y, grid.resolutionY));
            const i32 minZ = std::max(0, CellIndexAlongAxis(center.z - r, grid.bounds.min.z, cell.z, grid.resolutionZ));
            const i32 maxX = std::min(static_cast<i32>(grid.resolutionX) - 1, CellIndexAlongAxis(center.x + r, grid.bounds.min.x, cell.x, grid.resolutionX));
            const i32 maxY = std::min(static_cast<i32>(grid.resolutionY) - 1, CellIndexAlongAxis(center.y + r, grid.bounds.min.y, cell.y, grid.resolutionY));
            const i32 maxZ = std::min(static_cast<i32>(grid.resolutionZ) - 1, CellIndexAlongAxis(center.z + r, grid.bounds.min.z, cell.z, grid.resolutionZ));

            if (minX < 0 || minY < 0 || minZ < 0 || maxX < minX || maxY < minY || maxZ < minZ)
            {
                return;
            }

            for (i32 z = minZ; z <= maxZ; ++z)
            {
                for (i32 y = minY; y <= maxY; ++y)
                {
                    for (i32 x = minX; x <= maxX; ++x)
                    {
                        const Vec3 c = CsgCellCenter(grid, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z));
                        const Vec3 delta = Subtract(c, center);
                        if (LengthSquared(delta) > r2)
                        {
                            continue;
                        }

                        if (CsgIsSolid(grid, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z)))
                        {
                            CsgSetSolid(grid, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z), false);
                            ++stats.clearedVoxels;
                            ++stats.debrisCandidateVoxels;
                            PushDebrisCandidate(grid, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z), impulse, &stats.debrisCandidates);
                        }
                    }
                }
            }
        }

        u64 FloodFillFromSeeds(CsgVoxelGrid& grid, std::vector<u8>& visited, std::queue<u64>& queue)
        {
            u64 count = 0;
            constexpr std::array<std::array<i32, 3>, 6> offsets{{
                {{ 1,  0,  0}},
                {{-1,  0,  0}},
                {{ 0,  1,  0}},
                {{ 0, -1,  0}},
                {{ 0,  0,  1}},
                {{ 0,  0, -1}}
            }};

            while (!queue.empty())
            {
                const u64 current = queue.front();
                queue.pop();
                ++count;

                const u64 xy = static_cast<u64>(grid.resolutionX) * grid.resolutionY;
                const u32 z = static_cast<u32>(current / xy);
                const u64 rem = current % xy;
                const u32 y = static_cast<u32>(rem / grid.resolutionX);
                const u32 x = static_cast<u32>(rem % grid.resolutionX);

                for (const auto& offset : offsets)
                {
                    const i32 nx = static_cast<i32>(x) + offset[0];
                    const i32 ny = static_cast<i32>(y) + offset[1];
                    const i32 nz = static_cast<i32>(z) + offset[2];
                    if (!IsInsideGrid(grid, nx, ny, nz))
                    {
                        continue;
                    }

                    const u64 ni = CsgCellIndex(grid, static_cast<u32>(nx), static_cast<u32>(ny), static_cast<u32>(nz));
                    if (grid.solid[static_cast<std::size_t>(ni)] != 0 && visited[static_cast<std::size_t>(ni)] == 0)
                    {
                        visited[static_cast<std::size_t>(ni)] = 1;
                        queue.push(ni);
                    }
                }
            }
            return count;
        }
    }

    const char* ToString(ProjectileState state)
    {
        switch (state)
        {
        case ProjectileState::Flying: return "Flying";
        case ProjectileState::Ricocheted: return "Ricocheted";
        case ProjectileState::Penetrated: return "Penetrated";
        case ProjectileState::Embedded: return "Embedded";
        case ProjectileState::Stuck: return "Stuck";
        case ProjectileState::Fragmented: return "Fragmented";
        case ProjectileState::Melted: return "Melted";
        case ProjectileState::Destroyed: return "Destroyed";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileMedium medium)
    {
        switch (medium)
        {
        case ProjectileMedium::Air: return "Air";
        case ProjectileMedium::Water: return "Water";
        case ProjectileMedium::Vacuum: return "Vacuum";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileEnvironmentPreset preset)
    {
        switch (preset)
        {
        case ProjectileEnvironmentPreset::OpenField: return "OpenField";
        case ProjectileEnvironmentPreset::Forest: return "Forest";
        case ProjectileEnvironmentPreset::Urban: return "Urban";
        case ProjectileEnvironmentPreset::Indoor: return "Indoor";
        case ProjectileEnvironmentPreset::Mountains: return "Mountains";
        case ProjectileEnvironmentPreset::Desert: return "Desert";
        case ProjectileEnvironmentPreset::Storm: return "Storm";
        case ProjectileEnvironmentPreset::HighAltitude: return "HighAltitude";
        case ProjectileEnvironmentPreset::Jungle: return "Jungle";
        case ProjectileEnvironmentPreset::Arctic: return "Arctic";
        case ProjectileEnvironmentPreset::CityHighRise: return "CityHighRise";
        case ProjectileEnvironmentPreset::IndoorLargeHall: return "IndoorLargeHall";
        case ProjectileEnvironmentPreset::Cave: return "Cave";
        case ProjectileEnvironmentPreset::Coastal: return "Coastal";
        case ProjectileEnvironmentPreset::Snowstorm: return "Snowstorm";
        case ProjectileEnvironmentPreset::UnderwaterShallow: return "UnderwaterShallow";
        case ProjectileEnvironmentPreset::UnderwaterDeep: return "UnderwaterDeep";
        case ProjectileEnvironmentPreset::Space: return "Space";
        case ProjectileEnvironmentPreset::SpaceStation: return "SpaceStation";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileDamageType type)
    {
        switch (type)
        {
        case ProjectileDamageType::Arcane: return "Arcane";
        case ProjectileDamageType::Chemical: return "Chemical";
        case ProjectileDamageType::Electric: return "Electric";
        case ProjectileDamageType::Fire: return "Fire";
        case ProjectileDamageType::Ice: return "Ice";
        case ProjectileDamageType::Light: return "Light";
        case ProjectileDamageType::Physical: return "Physical";
        case ProjectileDamageType::Psionic: return "Psionic";
        case ProjectileDamageType::Radiation: return "Radiation";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileWeaponType type)
    {
        switch (type)
        {
        case ProjectileWeaponType::Pistol: return "Pistol";
        case ProjectileWeaponType::Shotgun: return "Shotgun";
        case ProjectileWeaponType::SMG: return "SMG";
        case ProjectileWeaponType::AssaultRifle: return "AssaultRifle";
        case ProjectileWeaponType::AutomaticRifle: return "AutomaticRifle";
        case ProjectileWeaponType::SniperRifle: return "SniperRifle";
        case ProjectileWeaponType::GrenadeLauncher: return "GrenadeLauncher";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileImpactOutcome outcome)
    {
        switch (outcome)
        {
        case ProjectileImpactOutcome::None: return "None";
        case ProjectileImpactOutcome::Ricochet: return "Ricochet";
        case ProjectileImpactOutcome::Penetration: return "Penetration";
        case ProjectileImpactOutcome::Embedded: return "Embedded";
        case ProjectileImpactOutcome::Stopped: return "Stopped";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileTrajectoryEventType type)
    {
        switch (type)
        {
        case ProjectileTrajectoryEventType::None: return "None";
        case ProjectileTrajectoryEventType::Penetrated: return "Penetrated";
        case ProjectileTrajectoryEventType::NotPenetrated: return "NotPenetrated";
        case ProjectileTrajectoryEventType::Ricocheted: return "Ricocheted";
        case ProjectileTrajectoryEventType::Destroyed: return "Destroyed";
        case ProjectileTrajectoryEventType::Stuck: return "Stuck";
        case ProjectileTrajectoryEventType::Melted: return "Melted";
        default: return "Unknown";
        }
    }

    const char* ToString(ProjectileThicknessShapeKind kind)
    {
        switch (kind)
        {
        case ProjectileThicknessShapeKind::Override: return "Override";
        case ProjectileThicknessShapeKind::AABB: return "AABB";
        case ProjectileThicknessShapeKind::OrientedBox: return "OrientedBox";
        case ProjectileThicknessShapeKind::Sphere: return "Sphere";
        case ProjectileThicknessShapeKind::PhysicsCollider: return "PhysicsCollider";
        case ProjectileThicknessShapeKind::VoxelGrid: return "VoxelGrid";
        default: return "Unknown";
        }
    }

    ProjectilePreset MakeDefaultRifleProjectilePreset()
    {
        return MakeProjectilePreset(ProjectileDamageType::Physical, ProjectileWeaponType::AssaultRifle);
    }

    ProjectilePreset MakeProjectilePreset(ProjectileDamageType damageType, ProjectileWeaponType weaponType)
    {
        ProjectilePreset preset{};
        preset.damageType = damageType;
        preset.weaponType = weaponType;

        switch (damageType)
        {
        case ProjectileDamageType::Arcane:
            preset.radiusMeters = 5.0028f;
            preset.lengthMeters = 5.023f;
            preset.densityKgPerCubicMeter = 20000.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 100.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 0.27f;
            preset.turbulenceIntensity = 2.1f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 1.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 1.1f;
            break;
        case ProjectileDamageType::Chemical:
            preset.radiusMeters = 1.0028f;
            preset.lengthMeters = 10.023f;
            preset.densityKgPerCubicMeter = 400.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 40.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 1.27f;
            preset.turbulenceIntensity = 1.1f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 9.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 120.1f;
            break;
        case ProjectileDamageType::Electric:
            preset.radiusMeters = 3.0028f;
            preset.lengthMeters = 5.023f;
            preset.densityKgPerCubicMeter = 100.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 90.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 0.27f;
            preset.turbulenceIntensity = 5.1f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 209.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 1.1f;
            break;
        case ProjectileDamageType::Fire:
            preset.radiusMeters = 5.0028f;
            preset.lengthMeters = 10.023f;
            preset.densityKgPerCubicMeter = 400.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 150.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 0.27f;
            preset.turbulenceIntensity = 7.1f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 9.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 1.1f;
            break;
        case ProjectileDamageType::Ice:
            preset.radiusMeters = 5.0028f;
            preset.lengthMeters = 5.023f;
            preset.densityKgPerCubicMeter = 9500.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 90.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 0.27f;
            preset.turbulenceIntensity = 1.1f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 59.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 1.1f;
            break;
        case ProjectileDamageType::Light:
            preset.radiusMeters = 5.0028f;
            preset.lengthMeters = 5.023f;
            preset.densityKgPerCubicMeter = 20000.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 200.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 0.0f;
            preset.turbulenceIntensity = 0.0f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 0.0f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 1.1f;
            break;
        case ProjectileDamageType::Psionic:
            preset.radiusMeters = 1.0028f;
            preset.lengthMeters = 30.023f;
            preset.densityKgPerCubicMeter = 400.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 50.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 1.27f;
            preset.turbulenceIntensity = 1.1f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 9.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 120.1f;
            break;
        case ProjectileDamageType::Radiation:
            preset.radiusMeters = 5.0028f;
            preset.lengthMeters = 5.023f;
            preset.densityKgPerCubicMeter = 20000.0f;
            preset.heatCapacityJPerKgK = 1500.0f;
            preset.meltTemperatureKelvin = 1200.0f;
            preset.initialSpeedMetersPerSecond = 200.0f;
            preset.rotationalLengthMeters = 0.18f;
            preset.aeroMomentCoef = 1.0e-1f;
            preset.emissivity = 5.4f;
            preset.baseDragCoefficient = 0.0f;
            preset.turbulenceIntensity = 0.0f;
            preset.latentHeatJPerKg = 1000000.0f;
            preset.maxTemperatureKelvin = 5500.0f;
            preset.gravityMetersPerSecondSquared = 0.81f;
            preset.characteristicHeightMeters = 50.0f;
            preset.angleStabilityCoef = 1.1f;
            break;
        case ProjectileDamageType::Physical:
        default:
            break;
        }

        return ApplyWeaponTypeToProjectilePreset(preset, weaponType);
    }

    ProjectilePreset ApplyWeaponTypeToProjectilePreset(ProjectilePreset preset, ProjectileWeaponType weaponType)
    {
        preset.weaponType = weaponType;
        switch (weaponType)
        {
        case ProjectileWeaponType::Pistol:
            preset.radiusMeters *= 0.9f;
            preset.initialSpeedMetersPerSecond *= 0.8f;
            preset.densityKgPerCubicMeter *= 0.95f;
            preset.lengthMeters *= 0.9f;
            break;
        case ProjectileWeaponType::Shotgun:
            preset.radiusMeters *= 1.2f;
            preset.initialSpeedMetersPerSecond *= 0.7f;
            preset.densityKgPerCubicMeter *= 1.1f;
            preset.lengthMeters *= 0.95f;
            break;
        case ProjectileWeaponType::SMG:
            preset.initialSpeedMetersPerSecond *= 0.85f;
            preset.radiusMeters *= 0.95f;
            break;
        case ProjectileWeaponType::AssaultRifle:
            preset.initialSpeedMetersPerSecond *= 1.05f;
            break;
        case ProjectileWeaponType::AutomaticRifle:
            preset.initialSpeedMetersPerSecond *= 1.1f;
            preset.radiusMeters *= 0.95f;
            preset.densityKgPerCubicMeter *= 0.95f;
            break;
        case ProjectileWeaponType::SniperRifle:
            preset.initialSpeedMetersPerSecond *= 1.3f;
            preset.baseDragCoefficient *= 0.9f;
            preset.radiusMeters *= 0.9f;
            preset.lengthMeters *= 1.1f;
            break;
        case ProjectileWeaponType::GrenadeLauncher:
            preset.radiusMeters *= 1.5f;
            preset.lengthMeters *= 1.2f;
            preset.initialSpeedMetersPerSecond *= 0.6f;
            preset.densityKgPerCubicMeter *= 1.2f;
            break;
        default:
            break;
        }
        return preset;
    }

    ProjectileTurbulencePreset MakeProjectileTurbulencePreset(ProjectileEnvironmentPreset preset)
    {
        ProjectileTurbulencePreset result{};
        switch (preset)
        {
        case ProjectileEnvironmentPreset::Forest:
            result = {"Forest", ProjectileMedium::Air, 0.14f, 0.20f, 2.2f, 7.0f, 900.0f, 0.8f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Urban:
            result = {"Urban", ProjectileMedium::Air, 0.18f, 0.22f, 2.6f, 8.0f, 1200.0f, 1.5f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Indoor:
            result = {"Indoor", ProjectileMedium::Air, 0.02f, 0.30f, 0.3f, 1.0f, 3000.0f, 0.1f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Mountains:
            result = {"Mountains", ProjectileMedium::Air, 0.20f, 0.18f, 3.0f, 9.0f, 1400.0f, 4.0f, {1.0f, 0.0f, 0.0f}, false, 1.1f, false, 330.0f};
            break;
        case ProjectileEnvironmentPreset::Desert:
            result = {"Desert", ProjectileMedium::Air, 0.16f, 0.22f, 2.4f, 8.0f, 1600.0f, 3.0f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Storm:
            result = {"Storm", ProjectileMedium::Air, 0.30f, 0.15f, 4.5f, 14.0f, 2000.0f, 12.0f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::HighAltitude:
            result = {"High Altitude", ProjectileMedium::Air, 0.12f, 0.20f, 2.0f, 7.0f, 1800.0f, 5.0f, {1.0f, 0.0f, 0.0f}, true, 0.9f, true, 320.0f};
            break;
        case ProjectileEnvironmentPreset::Jungle:
            result = {"Jungle", ProjectileMedium::Air, 0.22f, 0.18f, 2.8f, 9.0f, 800.0f, 0.7f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Arctic:
            result = {"Arctic", ProjectileMedium::Air, 0.14f, 0.22f, 2.0f, 7.5f, 2200.0f, 6.0f, {1.0f, 0.0f, 0.0f}, true, 1.35f, true, 330.0f};
            break;
        case ProjectileEnvironmentPreset::CityHighRise:
            result = {"City High-Rise", ProjectileMedium::Air, 0.24f, 0.20f, 3.2f, 10.5f, 1500.0f, 4.5f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::IndoorLargeHall:
            result = {"Indoor Large Hall", ProjectileMedium::Air, 0.05f, 0.35f, 0.6f, 2.0f, 4000.0f, 0.2f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Cave:
            result = {"Cave", ProjectileMedium::Air, 0.04f, 0.40f, 0.5f, 1.5f, 5000.0f, 0.3f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Coastal:
            result = {"Coastal", ProjectileMedium::Air, 0.18f, 0.18f, 3.0f, 9.0f, 2200.0f, 5.0f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::Snowstorm:
            result = {"Snowstorm", ProjectileMedium::Air, 0.28f, 0.16f, 3.8f, 12.0f, 2400.0f, 8.0f, {1.0f, 0.0f, 0.0f}, false, 1.225f, false, 343.2f};
            break;
        case ProjectileEnvironmentPreset::UnderwaterShallow:
            result = {"Underwater Shallow", ProjectileMedium::Water, 0.08f, 0.30f, 0.6f, 2.0f, 10000.0f, 0.5f, {1.0f, 0.0f, 0.0f}, true, 1025.0f, true, 1481.0f};
            break;
        case ProjectileEnvironmentPreset::UnderwaterDeep:
            result = {"Underwater Deep", ProjectileMedium::Water, 0.05f, 0.40f, 0.4f, 1.5f, 10000.0f, 0.2f, {1.0f, 0.0f, 0.0f}, true, 1030.0f, true, 1500.0f};
            break;
        case ProjectileEnvironmentPreset::Space:
            result = {"Space", ProjectileMedium::Vacuum, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, {1.0f, 0.0f, 0.0f}, true, 0.0f, true, 0.0f};
            break;
        case ProjectileEnvironmentPreset::SpaceStation:
            result = {"Space Station", ProjectileMedium::Air, 0.01f, 0.50f, 0.1f, 0.5f, 10000.0f, 0.1f, {1.0f, 0.0f, 0.0f}, true, 1.2f, true, 340.0f};
            break;
        case ProjectileEnvironmentPreset::OpenField:
        default:
            break;
        }
        return result;
    }

    ProjectileEnvironment MakeProjectileEnvironment(ProjectileEnvironmentPreset preset)
    {
        const ProjectileTurbulencePreset pr = MakeProjectileTurbulencePreset(preset);
        ProjectileEnvironment environment{};
        environment.preset = preset;
        environment.medium = pr.medium;
        environment.turbulenceIntensity = std::max(0.0f, pr.turbulenceIntensity);
        environment.turbulenceTauSeconds = std::max(0.02f, pr.turbulenceTauSeconds);
        environment.turbulenceSigmaMetersPerSecond = std::max(0.0f, pr.turbulenceSigmaBaseMetersPerSecond);
        environment.turbulenceMaxStdMetersPerSecond = std::max(0.0f, pr.turbulenceMaxStdMetersPerSecond);
        environment.turbulenceAltitudeDecayMeters = std::max(1.0f, pr.turbulenceAltitudeDecayMeters);
        environment.baseWindDirection = Normalize(pr.baseWindDirection, {1.0f, 0.0f, 0.0f});
        environment.baseWindSpeedMetersPerSecond = std::max(0.0f, pr.baseWindSpeedMetersPerSecond);
        environment.meanWindMetersPerSecond = Multiply(environment.baseWindDirection, environment.baseWindSpeedMetersPerSecond);
        environment.densityOverridden = pr.overrideDensity;
        environment.speedOfSoundOverridden = pr.overrideSpeedOfSound;
        if (pr.overrideDensity)
        {
            environment.densityKgPerCubicMeter = std::max(0.0f, pr.densityOverrideKgPerCubicMeter);
        }
        if (pr.overrideSpeedOfSound)
        {
            environment.speedOfSoundMetersPerSecond = std::max(0.0f, pr.speedOfSoundOverrideMetersPerSecond);
        }
        if (pr.medium == ProjectileMedium::Vacuum)
        {
            environment.turbulenceEnabled = false;
            environment.windMetersPerSecond = {};
            environment.meanWindMetersPerSecond = {};
            environment.turbulenceMetersPerSecond = {};
            environment.gravityMetersPerSecondSquared = {};
        }
        else
        {
            environment.windMetersPerSecond = environment.meanWindMetersPerSecond;
        }
        return environment;
    }

    ProjectileBody MakeProjectileBody(const ProjectilePreset& preset, Vec3 position, Vec3 forward)
    {
        ProjectileBody body{};
        body.position = position;
        body.previousPosition = position;
        body.forward = Normalize(forward, {0.0f, 0.0f, 1.0f});
        body.velocity = Multiply(body.forward, std::max(0.0f, preset.initialSpeedMetersPerSecond));
        body.radiusMeters = std::max(0.0f, preset.radiusMeters);
        body.lengthMeters = std::max(MinimumLength, preset.lengthMeters);
        body.densityKgPerCubicMeter = std::max(0.0f, preset.densityKgPerCubicMeter);
        body.massKilograms = ComputeProjectileMassKilograms(preset);
        body.initialMassKilograms = body.massKilograms;
        body.heatCapacityJPerKgK = std::max(1.0f, preset.heatCapacityJPerKgK);
        body.latentHeatJPerKg = std::max(0.0f, preset.latentHeatJPerKg);
        body.emissivity = std::max(0.0f, preset.emissivity);
        body.meltTemperatureKelvin = std::max(1.0f, preset.meltTemperatureKelvin);
        body.maxTemperatureKelvin = std::max(body.meltTemperatureKelvin, preset.maxTemperatureKelvin);
        body.aeroMomentCoef = std::max(0.0f, preset.aeroMomentCoef);
        body.baseDragCoefficient = std::max(0.0f, preset.baseDragCoefficient);
        const float spinLength = std::max(MinimumLength, preset.rotationalLengthMeters > 0.0f ? preset.rotationalLengthMeters : body.lengthMeters);
        body.angularVelocity = Multiply(body.forward, Length(body.velocity) / spinLength);
        body.temperatureKelvin = body.ambientReferenceTemperatureKelvin;
        body.thermalEnergyJoules = body.massKilograms * body.heatCapacityJPerKgK * body.temperatureKelvin;
        return body;
    }

    ProjectileWorldState MakeProjectileWorldState(WorldPosition position, Vec3 velocityMetersPerSecond, double cellSizeMeters)
    {
        ProjectileWorldState world{};
        world.cellSizeMeters = cellSizeMeters > 0.0 ? cellSizeMeters : DefaultWorldCellSizeMeters;
        world.position = NormalizeWorldPosition(position, world.cellSizeMeters);
        world.previousPosition = world.position;
        world.physicsIslandOrigin = BuildOriginFromCamera(world.position, world.cellSizeMeters).position;
        world.velocityMetersPerSecond = ToDVec3Checked(velocityMetersPerSecond);
        world.finite = IsFiniteWorldState(world);
        return world;
    }

    void SyncProjectileBodyFromWorldState(ProjectileBody& projectile, const ProjectileWorldState& world)
    {
        const DVec3 relative = RelativeWorldD(world.position, world.physicsIslandOrigin, world.cellSizeMeters);
        const DVec3 previousRelative = RelativeWorldD(world.previousPosition, world.physicsIslandOrigin, world.cellSizeMeters);
        projectile.position = ToVec3Checked(relative);
        projectile.previousPosition = ToVec3Checked(previousRelative);
        projectile.velocity = ToVec3Checked(world.velocityMetersPerSecond);
        projectile.angularVelocity = ToVec3Checked(world.angularVelocityRadiansPerSecond);
    }

    void SyncProjectileWorldStateFromBody(const ProjectileBody& projectile, ProjectileWorldState& world)
    {
        const DVec3 previousRelative = RelativeWorldD(world.position, world.physicsIslandOrigin, world.cellSizeMeters);
        const DVec3 newRelative = ToDVec3Checked(projectile.position);
        const DVec3 delta = Subtract(newRelative, previousRelative);
        world.previousPosition = world.position;
        world.position = AddWorldOffsetD(world.position, delta, world.cellSizeMeters);
        world.velocityMetersPerSecond = ToDVec3Checked(projectile.velocity);
        world.angularVelocityRadiansPerSecond = ToDVec3Checked(projectile.angularVelocity);
        world.finite = IsFiniteWorldState(world);
    }

    float ComputeProjectileMassKilograms(const ProjectilePreset& preset)
    {
        const float radius = std::max(0.0f, preset.radiusMeters);
        const float length = std::max(MinimumLength, preset.lengthMeters);
        const float density = std::max(MinimumMass, preset.densityKgPerCubicMeter);
        return std::max(0.0001f, Pi32 * radius * radius * length * density);
    }

    float ComputeProjectileAreaSquareMeters(const ProjectileBody& projectile)
    {
        const float radius = std::max(0.0f, projectile.radiusMeters);
        return std::max(MinimumArea, Pi32 * radius * radius);
    }

    float ComputeProjectileSurfaceAreaSquareMeters(const ProjectileBody& projectile)
    {
        const float radius = std::max(0.0f, projectile.radiusMeters);
        const float length = std::max(MinimumLength, projectile.lengthMeters);
        return std::max(MinimumArea, 2.0f * Pi32 * radius * length + 2.0f * Pi32 * radius * radius);
    }

    float ComputeProjectileKineticEnergyJoules(const ProjectileBody& projectile)
    {
        return 0.5f * std::max(MinimumMass, projectile.massKilograms) * LengthSquared(projectile.velocity);
    }

    float ProjectileCdFromMach(float mach)
    {
        if (!IsFinite(mach) || mach <= MachKnots.front())
        {
            return CdG7.front();
        }
        if (mach >= MachKnots.back())
        {
            return CdG7.back();
        }

        for (std::size_t i = 1; i < MachKnots.size(); ++i)
        {
            if (mach <= MachKnots[i])
            {
                const float t = (mach - MachKnots[i - 1]) / (MachKnots[i] - MachKnots[i - 1]);
                return Lerp(CdG7[i - 1], CdG7[i], t);
            }
        }
        return CdG7.back();
    }

    void SetProjectileState(ProjectileBody& projectile, ProjectileState state)
    {
        if (projectile.state == state)
        {
            projectile.stateTimerSeconds = 0.0f;
            return;
        }
        projectile.state = state;
        projectile.stateTimerSeconds = 0.0f;
        if (state == ProjectileState::Embedded || state == ProjectileState::Stuck || state == ProjectileState::Melted || state == ProjectileState::Destroyed)
        {
            projectile.velocity = {};
            projectile.angularVelocity = {};
        }
    }

    void UpdateProjectileAtmosphere(ProjectileEnvironment& environment, Vec3 positionMeters)
    {
        if (environment.medium == ProjectileMedium::Vacuum || environment.densityOverridden || environment.speedOfSoundOverridden)
        {
            return;
        }

        const float h = std::max(positionMeters.y, 0.0f);
        constexpr float T0 = 288.15f;
        constexpr float p0 = 101325.0f;
        constexpr float L = 0.0065f;
        constexpr float R = 287.05f;
        constexpr float g0 = 9.80665f;
        constexpr float gamma = 1.4f;

        environment.ambientTemperatureKelvin = std::max(190.0f, T0 - L * h);
        environment.pressurePascals = p0 * std::pow(environment.ambientTemperatureKelvin / T0, g0 / (L * R));
        environment.densityKgPerCubicMeter = environment.pressurePascals / (R * environment.ambientTemperatureKelvin);
        environment.speedOfSoundMetersPerSecond = std::sqrt(gamma * R * environment.ambientTemperatureKelvin);
    }

    void UpdateProjectileWind(ProjectileEnvironment& environment, float deltaSeconds, Vec3 positionMeters, bool enableTurbulence)
    {
        if (environment.medium == ProjectileMedium::Vacuum)
        {
            environment.windMetersPerSecond = {};
            environment.meanWindMetersPerSecond = {};
            environment.turbulenceMetersPerSecond = {};
            return;
        }

        const float dt = std::max(deltaSeconds, 0.0f);
        environment.timeSeconds += dt;
        const Vec3 baseWind = Multiply(Normalize(environment.baseWindDirection, {1.0f, 0.0f, 0.0f}), std::max(0.0f, environment.baseWindSpeedMetersPerSecond));

        const float t = environment.timeSeconds;
        const Vec3 gust{
            ValueNoise1D(t * 0.37f + 0.13f, environment.randomSeed ^ 0x81u),
            ValueNoise1D(t * 0.41f + 0.71f, environment.randomSeed ^ 0x289u),
            ValueNoise1D(t * 0.29f + 0.59f, environment.randomSeed ^ 0x541u)
        };
        const Vec3 gustVelocity = Multiply(gust, environment.turbulenceIntensity * 2.0f * dt);
        environment.meanWindMetersPerSecond = ClampMagnitude(Add(Add(Multiply(environment.meanWindMetersPerSecond, std::max(0.0f, 1.0f - 0.1f * dt)), Multiply(baseWind, 0.1f * dt)), gustVelocity), 30.0f);

        if (enableTurbulence && environment.turbulenceEnabled && environment.turbulenceMaxStdMetersPerSecond > 0.0f && environment.turbulenceSigmaMetersPerSecond > 0.0f)
        {
            const float tau = std::max(environment.turbulenceTauSeconds, 0.02f);
            const float h = std::max(positionMeters.y, 0.0f);
            const float altFactor = std::exp(-h / std::max(environment.turbulenceAltitudeDecayMeters, 1.0f));
            const float sigma = std::min(environment.turbulenceSigmaMetersPerSecond * std::max(environment.turbulenceIntensity, 0.0f) * altFactor, environment.turbulenceMaxStdMetersPerSecond);
            const float s = std::sqrt(std::max(dt, 1.0e-4f)) * std::sqrt(2.0f / tau) * sigma;
            u32 seed = environment.randomSeed ^ static_cast<u32>(environment.timeSeconds * 100000.0f + 23.0f);
            const Vec3 dW{NextGaussianDeterministic(seed) * s, NextGaussianDeterministic(seed) * s, NextGaussianDeterministic(seed) * s};
            environment.randomSeed = seed;
            environment.turbulenceMetersPerSecond = Add(Add(environment.turbulenceMetersPerSecond, Multiply(environment.turbulenceMetersPerSecond, -dt / tau)), dW);
            environment.turbulenceMetersPerSecond = ClampMagnitude(environment.turbulenceMetersPerSecond, 3.0f * sigma);
        }
        else
        {
            environment.turbulenceMetersPerSecond = Multiply(environment.turbulenceMetersPerSecond, std::max(0.0f, 1.0f - 2.0f * dt));
        }

        environment.windMetersPerSecond = Add(environment.meanWindMetersPerSecond, environment.turbulenceMetersPerSecond);
    }

    Vec3 ComputeProjectileMagnusForce(const ProjectileBody& projectile, const ProjectileEnvironment& environment, Vec3 flowDirection, float speedMetersPerSecond)
    {
        if (environment.medium == ProjectileMedium::Vacuum || speedMetersPerSecond <= 1.0e-3f)
        {
            return {};
        }
        const float spinParam = Length(projectile.angularVelocity) * projectile.radiusMeters / std::max(speedMetersPerSecond, 1.0e-3f);
        const float cl = std::clamp(0.35f * spinParam, 0.0f, 0.55f);
        const Vec3 spinAxis = LengthSquared(projectile.velocity) > 1.0e-6f ? Normalize(projectile.velocity, projectile.forward) : projectile.forward;
        const Vec3 liftDirection = Normalize(Cross(spinAxis, flowDirection), {0.0f, 0.0f, 0.0f});
        const float magnitude = 0.5f * environment.densityKgPerCubicMeter * speedMetersPerSecond * speedMetersPerSecond * ComputeProjectileAreaSquareMeters(projectile) * cl;
        return Multiply(liftDirection, magnitude);
    }

    Vec3 ComputeProjectileCoriolisForce(const ProjectileBody& projectile)
    {
        const Vec3 omega{0.0f, EarthAngularVelocity, 0.0f};
        return Multiply(Cross(omega, projectile.velocity), -2.0f * projectile.massKilograms);
    }

    Vec3 ComputeProjectileAcceleration(const ProjectileBody& projectile, const ProjectileEnvironment& environment, const ProjectileStepSettings& settings)
    {
        if (!IsFlying(projectile.state))
        {
            return {};
        }

        Vec3 force{};
        if (settings.enableGravity)
        {
            force = Add(force, Multiply(environment.gravityMetersPerSecondSquared, projectile.massKilograms));
        }

        const bool vacuum = environment.medium == ProjectileMedium::Vacuum;
        const Vec3 wind = settings.enableWind ? environment.windMetersPerSecond : Vec3{};
        const Vec3 relativeVelocity = Subtract(projectile.velocity, wind);
        const float speed = Length(relativeVelocity);
        if (speed > 1.0e-3f)
        {
            const Vec3 flowDir = Divide(relativeVelocity, speed);
            if (settings.enableDrag && !vacuum)
            {
                const float soundSpeed = std::max(1.0e-3f, environment.speedOfSoundMetersPerSecond);
                const float mach = speed / soundSpeed;
                const float cd = ProjectileCdFromMach(mach);
                const float dragForce = 0.5f * std::max(0.0f, environment.densityKgPerCubicMeter) * speed * speed * cd * ComputeProjectileAreaSquareMeters(projectile);
                force = Add(force, Multiply(flowDir, -dragForce));
            }
            if (projectile.heavySimulationEnabled)
            {
                if (settings.enableMagnus && projectile.enableMagnus && !vacuum)
                {
                    force = Add(force, ComputeProjectileMagnusForce(projectile, environment, flowDir, speed));
                }
                if (settings.enableCoriolis && projectile.enableCoriolis)
                {
                    force = Add(force, ComputeProjectileCoriolisForce(projectile));
                }
            }
        }

        return Divide(force, std::max(MinimumMass, projectile.massKilograms));
    }

    void UpdateProjectileAngular(ProjectileBody& projectile, float deltaSeconds)
    {
        const Vec3 velocityDir = LengthSquared(projectile.velocity) > 1.0e-6f ? Normalize(projectile.velocity, projectile.forward) : projectile.forward;
        const float cosAngle = std::clamp(Dot(projectile.forward, velocityDir), -1.0f, 1.0f);
        const float angle = std::acos(cosAngle);
        if (angle > 1.0e-4f)
        {
            const Vec3 axis = Normalize(Cross(projectile.forward, velocityDir), {0.0f, 1.0f, 0.0f});
            const float tau = 0.03f;
            const float targetRate = angle / std::max(tau, 1.0e-4f);
            const float rate = std::min(targetRate, 400.0f);
            projectile.angularVelocity = Multiply(axis, rate);
            projectile.forward = Normalize(Add(projectile.forward, Multiply(Cross(axis, projectile.forward), rate * deltaSeconds)), velocityDir);
        }
        else
        {
            projectile.forward = velocityDir;
        }
    }

    void UpdateProjectileThermal(ProjectileBody& projectile, const ProjectileEnvironment& environment, float, float deltaSeconds)
    {
        if (!projectile.enableThermalSimulation)
        {
            projectile.temperatureKelvin = Lerp(projectile.temperatureKelvin, environment.referenceAmbientTemperatureKelvin, 0.1f * deltaSeconds);
            return;
        }

        const float area = ComputeProjectileAreaSquareMeters(projectile);
        const float t4 = projectile.temperatureKelvin * projectile.temperatureKelvin * projectile.temperatureKelvin * projectile.temperatureKelvin;
        const float heatOutRadiation = projectile.emissivity * StefanBoltzmann * area * t4;
        const float heatOutConvection = 8.0f * area * std::max(projectile.temperatureKelvin - environment.referenceAmbientTemperatureKelvin, 0.0f);
        const float dQ = -(heatOutRadiation + heatOutConvection) * deltaSeconds;
        projectile.temperatureKelvin += dQ / std::max(projectile.massKilograms * std::max(projectile.heatCapacityJPerKgK, 1.0f), 1.0e-6f);
        projectile.temperatureKelvin = std::clamp(projectile.temperatureKelvin, environment.referenceAmbientTemperatureKelvin, projectile.maxTemperatureKelvin);
        projectile.thermalEnergyJoules = projectile.massKilograms * projectile.heatCapacityJPerKgK * projectile.temperatureKelvin;
        if (projectile.temperatureKelvin >= projectile.meltTemperatureKelvin && projectile.temperatureKelvin >= projectile.maxTemperatureKelvin)
        {
            SetProjectileState(projectile, ProjectileState::Melted);
        }
    }

    float UpdateProjectileMassErosion(ProjectileBody& projectile, const ProjectileEnvironment& environment, float deltaSeconds)
    {
        const float erosionCoefficient = 1.0e-14f;
        const float speed = Length(projectile.velocity);
        const float erosionRate = erosionCoefficient * std::max(0.0f, environment.densityKgPerCubicMeter) * speed * speed * speed * ComputeProjectileAreaSquareMeters(projectile);
        const float deltaMass = std::max(0.0f, erosionRate * deltaSeconds);
        if (deltaMass > 0.0f)
        {
            projectile.massKilograms = std::max(projectile.massKilograms - deltaMass, 0.0001f);
            projectile.thermalEnergyJoules = std::max(projectile.thermalEnergyJoules - projectile.latentHeatJPerKg * deltaMass, 0.0f);
        }
        return deltaMass;
    }

    float ComputeProjectileTemperatureYieldStrengthPascals(const ProjectileBody& projectile, float referenceYieldStrengthPascals)
    {
        constexpr float referenceTemperature = 293.0f;
        constexpr float softeningTemperature = 600.61f;
        constexpr float exponent = 1.5f;
        const float numerator = std::max(softeningTemperature - projectile.temperatureKelvin, 0.01f);
        const float denominator = std::max(softeningTemperature - referenceTemperature, 0.01f);
        return std::max(referenceYieldStrengthPascals * std::pow(numerator / denominator, exponent), 5.0e6f);
    }

    ProjectileStepResult StepProjectile(ProjectileBody& projectile, const ProjectileEnvironment& environment, const ProjectileStepSettings& settings)
    {
        ProjectileEnvironment copy = environment;
        return StepProjectileAdvanced(projectile, copy, settings);
    }

    ProjectileStepResult StepProjectileAdvanced(ProjectileBody& projectile, ProjectileEnvironment& environment, const ProjectileStepSettings& settings)
    {
        ProjectileStepResult result{};
        result.previousPosition = projectile.position;
        result.state = projectile.state;
        projectile.previousPosition = projectile.position;

        if (!IsFinite(settings.deltaSeconds) || settings.deltaSeconds <= 0.0f)
        {
            result.warnings.push_back("delta_seconds_not_positive");
            result.finite = false;
            return result;
        }
        if (!IsFinite(projectile.position) || !IsFinite(projectile.velocity) || !IsFinite(projectile.massKilograms))
        {
            SetProjectileState(projectile, ProjectileState::Destroyed);
            result.state = projectile.state;
            result.finite = false;
            result.warnings.push_back("non_finite_projectile");
            return result;
        }

        const float dt = std::min(settings.deltaSeconds, std::max(1.0e-4f, settings.maxStepSeconds));
        projectile.stateTimerSeconds += dt;

        if (projectile.state == ProjectileState::Embedded || projectile.state == ProjectileState::Stuck)
        {
            projectile.velocity = {};
            projectile.angularVelocity = {};
            if (projectile.embeddedLifetimeSeconds > 0.0f && projectile.stateTimerSeconds >= projectile.embeddedLifetimeSeconds)
            {
                SetProjectileState(projectile, ProjectileState::Destroyed);
            }
            result.ok = true;
            result.newPosition = projectile.position;
            result.state = projectile.state;
            return result;
        }
        if (!IsFlying(projectile.state))
        {
            result.ok = true;
            result.newPosition = projectile.position;
            result.speedMetersPerSecond = Length(projectile.velocity);
            result.kineticEnergyJoules = ComputeProjectileKineticEnergyJoules(projectile);
            return result;
        }

        projectile.ageSeconds += dt;
        if (projectile.ageSeconds >= projectile.heavySimulationDisableAfterSeconds && projectile.heavySimulationEnabled)
        {
            projectile.heavySimulationEnabled = false;
        }

        UpdateProjectileAtmosphere(environment, projectile.position);
        UpdateProjectileWind(environment, dt, projectile.position, settings.enableTurbulence && projectile.enableTurbulence);

        const Vec3 a0 = ComputeProjectileAcceleration(projectile, environment, settings);
        const Vec3 vMid = Add(projectile.velocity, Multiply(a0, 0.5f * dt));
        ProjectileBody mid = projectile;
        mid.velocity = vMid;
        const Vec3 aMid = ComputeProjectileAcceleration(mid, environment, settings);

        projectile.velocity = Add(projectile.velocity, Multiply(aMid, dt));
        const float speed = Length(projectile.velocity);
        if (speed > std::max(1.0f, settings.maxSpeedMetersPerSecond))
        {
            projectile.velocity = Multiply(projectile.velocity, settings.maxSpeedMetersPerSecond / speed);
        }

        projectile.position = Add(projectile.position, Multiply(projectile.velocity, dt));

        if (settings.enableSpinStabilization)
        {
            UpdateProjectileAngular(projectile, dt);
        }
        if (settings.enableThermal)
        {
            UpdateProjectileThermal(projectile, environment, Length(projectile.velocity), dt);
        }
        if (settings.enableMassErosion)
        {
            result.massLostKilograms = UpdateProjectileMassErosion(projectile, environment, dt);
        }

        if (settings.destroyOnStop && (Length(projectile.velocity) < settings.stopSpeedMetersPerSecond || projectile.ageSeconds > settings.maxSimulationSeconds || projectile.massKilograms <= 0.0001f))
        {
            SetProjectileState(projectile, ProjectileState::Destroyed);
        }
        if (std::abs(projectile.position.x) > settings.maxAbsPositionMeters
            || std::abs(projectile.position.y) > settings.maxAbsPositionMeters
            || std::abs(projectile.position.z) > settings.maxAbsPositionMeters)
        {
            SetProjectileState(projectile, ProjectileState::Destroyed);
            result.warnings.push_back("projectile_out_of_bounds");
        }

        result.ok = true;
        result.state = projectile.state;
        result.newPosition = projectile.position;
        result.accelerationMetersPerSecondSquared = aMid;
        result.windMetersPerSecond = environment.windMetersPerSecond;
        result.speedMetersPerSecond = Length(projectile.velocity);
        result.kineticEnergyJoules = ComputeProjectileKineticEnergyJoules(projectile);
        result.massKilograms = projectile.massKilograms;
        result.temperatureKelvin = projectile.temperatureKelvin;
        result.mach = environment.speedOfSoundMetersPerSecond > 1.0e-3f ? result.speedMetersPerSecond / environment.speedOfSoundMetersPerSecond : 0.0f;
        result.dragCoefficient = ProjectileCdFromMach(result.mach);
        result.finite = IsFinite(projectile.position) && IsFinite(projectile.velocity) && IsFinite(projectile.massKilograms) && IsFinite(projectile.temperatureKelvin);
        if (!result.finite)
        {
            SetProjectileState(projectile, ProjectileState::Destroyed);
            result.warnings.push_back("projectile_became_non_finite");
        }
        return result;
    }

    GravitySample SampleProjectileGravity(const ProjectileWorldState& world, const ProjectileEnvironment& environment, const std::vector<GravityFieldDesc>& gravityFields, const GravitySolverConfig& config)
    {
        const DVec3 absolute = WorldPositionToAbsoluteMeters(world.position, world.cellSizeMeters);
        GravitySample sample{};
        if (!gravityFields.empty())
        {
            sample = SampleGravityFields(gravityFields, absolute, config);
            return sample;
        }

        sample.accelerationMetersPerSecondSquared = ToDVec3Checked(environment.gravityMetersPerSecondSquared);
        sample.magnitudeMetersPerSecondSquared = Length(sample.accelerationMetersPerSecondSquared);
        sample.up = sample.magnitudeMetersPerSecondSquared > 1.0e-9
            ? Normalize(Multiply(sample.accelerationMetersPerSecondSquared, -1.0), {0.0, 1.0, 0.0})
            : DVec3{0.0, 1.0, 0.0};
        sample.sourceKind = sample.magnitudeMetersPerSecondSquared > 1.0e-9 ? GravityFieldKind::Uniform : GravityFieldKind::Zero;
        sample.finite = IsFinite(sample.accelerationMetersPerSecondSquared);
        sample.insideField = true;
        sample.sourceName = "projectile_environment";
        return sample;
    }

    ProjectileNativeStepResult StepProjectileNative(ProjectileBody& projectile, ProjectileWorldState& world, ProjectileEnvironment& environment, const std::vector<GravityFieldDesc>& gravityFields, const ProjectileNativeStepSettings& settings)
    {
        ProjectileNativeStepResult result{};
        result.previousWorldPosition = world.position;

        if (!IsFiniteWorldState(world))
        {
            result.finite = false;
            result.warnings.push_back("large_world_state_not_finite");
            SetProjectileState(projectile, ProjectileState::Destroyed);
            return result;
        }

        if (settings.rebasePhysicsOrigin)
        {
            const LargeWorldConfig lw = settings.largeWorld;
            if (ShouldRebaseOrigin(world.position, WorldOrigin{world.physicsIslandOrigin}, lw))
            {
                world.physicsIslandOrigin = BuildOriginFromCamera(world.position, world.cellSizeMeters).position;
                world.originRebasedThisStep = true;
                result.originRebased = true;
            }
            else
            {
                world.originRebasedThisStep = false;
            }
        }

        result.gravity = SampleProjectileGravity(world, environment, settings.sampleGravityFields ? gravityFields : std::vector<GravityFieldDesc>{}, settings.gravity);
        if (result.gravity.finite)
        {
            environment.gravityMetersPerSecondSquared = ToVec3Checked(result.gravity.accelerationMetersPerSecondSquared);
        }
        else
        {
            result.warnings.push_back("gravity_sample_not_finite");
        }

        if (settings.syncBodyFromLargeWorld)
        {
            SyncProjectileBodyFromWorldState(projectile, world);
        }

        const WorldPosition cameraOrigin = world.physicsIslandOrigin;
        result.physicsLocal = ToCameraRelativeFloat(world.position, cameraOrigin, settings.largeWorld);
        result.step = StepProjectileAdvanced(projectile, environment, settings.step);

        if (settings.syncLargeWorldFromBody)
        {
            SyncProjectileWorldStateFromBody(projectile, world);
        }

        result.newWorldPosition = world.position;
        result.finite = world.finite && result.step.finite && result.gravity.finite && result.physicsLocal.finite;
        if (result.physicsLocal.precisionRisk)
        {
            result.warnings.push_back("physics_local_precision_risk");
        }
        result.ok = result.step.ok && result.finite;
        return result;
    }

    ProjectileImpactMaterial MakeProjectileImpactMaterial(const SurfaceMaterialDesc& material)
    {
        ProjectileImpactMaterial impact{};
        impact.name = material.name;
        impact.densityKgPerCubicMeter = material.physics.densityKgPerCubicMeter;
        impact.destructionResistance = material.physics.destructionResistance;
        impact.energyDissipation = std::clamp(material.physics.destructionResistance * 0.08f, 0.02f, 0.65f);
        impact.ricochetEnergyLoss = std::clamp(0.08f + material.physics.dynamicFriction * 0.24f, 0.02f, 0.85f);
        impact.maxDeformationMeters = std::clamp(0.002f + (1.0f / std::max(0.1f, material.physics.hardness + 0.5f)) * 0.018f, 0.001f, 0.08f);
        impact.yieldStrengthPascals = std::clamp((material.physics.hardness * 18.0e6f + material.physics.destructionResistance * 42.0e6f), 0.5e6f, 900.0e6f);
        impact.youngModulusPascals = std::clamp(impact.yieldStrengthPascals * 80.0f, 10.0e6f, 220.0e9f);
        impact.speedOfSoundMetersPerSecond = std::clamp(std::sqrt(std::max(1.0f, impact.youngModulusPascals) / std::max(1.0f, impact.densityKgPerCubicMeter)), 300.0f, 6500.0f);
        impact.softNoRicochet = impact.yieldStrengthPascals <= 1.0e6f;
        return SanitizeProjectileImpactMaterial(impact);
    }

    ProjectileImpactMaterial SanitizeProjectileImpactMaterial(ProjectileImpactMaterial material)
    {
        if (material.name.empty())
        {
            material.name = "default";
        }
        material.densityKgPerCubicMeter = ClampPositive(material.densityKgPerCubicMeter, 1000.0f);
        material.speedOfSoundMetersPerSecond = ClampPositive(material.speedOfSoundMetersPerSecond, 1200.0f);
        material.yieldStrengthPascals = ClampPositive(material.yieldStrengthPascals, 25.0e6f);
        material.youngModulusPascals = ClampPositive(material.youngModulusPascals, 2.0e9f);
        material.ricochetEnergyLoss = std::clamp(ClampNonNegative(material.ricochetEnergyLoss), 0.0f, 0.95f);
        material.maxDeformationMeters = std::clamp(ClampNonNegative(material.maxDeformationMeters), 0.0f, 1.0f);
        material.energyDissipation = std::clamp(ClampNonNegative(material.energyDissipation), 0.0f, 0.95f);
        material.destructionResistance = std::clamp(ClampNonNegative(material.destructionResistance), 0.0f, 100.0f);
        material.thicknessOverrideMeters = ClampNonNegative(material.thicknessOverrideMeters);
        material.softNoRicochet = material.softNoRicochet || material.yieldStrengthPascals <= 1.0e6f;
        material.valid = IsFinite(material.densityKgPerCubicMeter)
            && IsFinite(material.speedOfSoundMetersPerSecond)
            && IsFinite(material.yieldStrengthPascals)
            && IsFinite(material.youngModulusPascals);
        return material;
    }

    ProjectileMaterialDamageState MakeProjectileMaterialDamageState(const ProjectileImpactMaterial& material)
    {
        const ProjectileImpactMaterial sanitized = SanitizeProjectileImpactMaterial(material);
        ProjectileMaterialDamageState state{};
        state.name = sanitized.name;
        state.originalYieldStrengthPascals = sanitized.yieldStrengthPascals;
        state.currentYieldStrengthPascals = sanitized.yieldStrengthPascals;
        state.originalDensityKgPerCubicMeter = sanitized.densityKgPerCubicMeter;
        state.currentDensityKgPerCubicMeter = sanitized.densityKgPerCubicMeter;
        state.valid = sanitized.valid;
        return state;
    }

    float ComputePenetrationEnergyJoules(const ProjectileBody& projectile, const ProjectileImpactMaterial& material, float obliquityDegrees, float thicknessMeters)
    {
        const float obliquity = std::clamp(obliquityDegrees, 0.0f, 89.0f) * DegToRad32;
        const float incidence = std::max(0.12f, std::cos(obliquity));
        const float effectiveThickness = std::max(0.001f, thicknessMeters) / incidence;
        const float area = ComputeProjectileAreaSquareMeters(projectile);
        const float resistance = std::max(0.1f, 0.5f + material.destructionResistance * 0.5f);
        return area * material.yieldStrengthPascals * effectiveThickness * resistance;
    }

    float ComputeRicochetThresholdDegrees(const ProjectileBody& projectile, const ProjectileImpactMaterial& material)
    {
        if (material.softNoRicochet || material.yieldStrengthPascals <= 1.0e6f)
        {
            return 0.0f;
        }
        const float speed = std::max(1.0f, Length(projectile.velocity));
        const float velocityRatio = material.speedOfSoundMetersPerSecond / speed;
        const float materialCoefficient = std::sqrt(std::clamp(material.yieldStrengthPascals / std::max(1.0f, material.youngModulusPascals), 0.0f, 1.0f));
        return std::acos(std::clamp(velocityRatio * materialCoefficient, -1.0f, 1.0f)) * RadToDeg32;
    }

    float ComputeProjectileImpactForceNewtons(const ProjectileBody& projectile, const ProjectileImpactMaterial& material, float thicknessMeters, float impactAngleFromPlaneDegrees)
    {
        const float angleRad = impactAngleFromPlaneDegrees * DegToRad32;
        const float contactTime = std::clamp(std::max(0.001f, thicknessMeters) / (std::max(material.speedOfSoundMetersPerSecond, 500.0f) * std::max(std::cos(angleRad), 0.2f)), 0.00005f, 0.003f);
        const float normalSpeed = Length(projectile.velocity) * std::max(std::cos(angleRad), 0.0f);
        const float deltaMomentum = projectile.massKilograms * normalSpeed;
        const float force = deltaMomentum / contactTime;
        const float dissipation = std::clamp(material.energyDissipation, 0.0f, 1.0f) * (1.0f - std::cos(angleRad));
        return force * (1.0f - dissipation);
    }

    ProjectileMaterialDamageState ApplyProjectileMaterialDeformation(ProjectileMaterialDamageState state, const ProjectileImpactMaterial& material, const ProjectileBody& projectile, float impactForceNewtons, float thicknessMeters)
    {
        if (!state.valid)
        {
            state = MakeProjectileMaterialDamageState(material);
        }
        const float area = std::max(ComputeProjectileAreaSquareMeters(projectile), MinimumArea);
        const float stress = impactForceNewtons / area;
        const float strain = stress / std::max(1.0f, material.youngModulusPascals);
        const float deformation = strain * std::max(0.001f, thicknessMeters);
        const float damageIncrement = material.maxDeformationMeters > 0.0f ? (deformation / material.maxDeformationMeters) * 100.0f : 0.0f;
        state.accumulatedDamagePercent = std::clamp(state.accumulatedDamagePercent + std::max(0.0f, damageIncrement), 0.0f, 100.0f);
        const float alive = 1.0f - state.accumulatedDamagePercent / 100.0f;
        state.currentYieldStrengthPascals = state.originalYieldStrengthPascals * alive;
        state.currentDensityKgPerCubicMeter = state.originalDensityKgPerCubicMeter * alive;
        return state;
    }

    ProjectileThicknessResult ComputeProjectileThickness(const ProjectileThicknessInput& input)
    {
        ProjectileThicknessResult result{};
        const float minT = std::max(0.0001f, input.minThicknessMeters);
        const Vec3 n = Normalize(input.surfaceNormal, {0.0f, 1.0f, 0.0f});

        if (input.shape.kind == ProjectileThicknessShapeKind::Override || input.shape.overrideThicknessMeters > 0.0f)
        {
            result.valid = true;
            result.thicknessMeters = std::max(minT, input.shape.overrideThicknessMeters > 0.0f ? input.shape.overrideThicknessMeters : input.fallbackThicknessMeters);
            result.exitPoint = Subtract(input.contactPoint, Multiply(n, result.thicknessMeters));
            result.method = "override";
            return result;
        }

        if (input.shape.kind == ProjectileThicknessShapeKind::OrientedBox)
        {
            const Vec3 ax = Normalize(input.shape.boxAxisX, {1.0f, 0.0f, 0.0f});
            const Vec3 ay = Normalize(input.shape.boxAxisY, {0.0f, 1.0f, 0.0f});
            const Vec3 az = Normalize(input.shape.boxAxisZ, {0.0f, 0.0f, 1.0f});
            const float proj = std::abs(Dot(n, ax)) * std::abs(input.shape.boxSizeMeters.x)
                + std::abs(Dot(n, ay)) * std::abs(input.shape.boxSizeMeters.y)
                + std::abs(Dot(n, az)) * std::abs(input.shape.boxSizeMeters.z);
            result.valid = proj > minT;
            result.thicknessMeters = std::clamp(proj, minT, 1000.0f);
            result.exitPoint = Subtract(input.contactPoint, Multiply(n, result.thicknessMeters));
            result.method = "oriented_box_projection";
            return result;
        }

        if (input.shape.kind == ProjectileThicknessShapeKind::Sphere)
        {
            const Vec3 oc = Subtract(input.contactPoint, input.shape.sphere.center);
            const Vec3 dir = Negate(n);
            const float b = 2.0f * Dot(oc, dir);
            const float c = LengthSquared(oc) - input.shape.sphere.radius * input.shape.sphere.radius;
            const float disc = b * b - 4.0f * c;
            if (disc >= 0.0f)
            {
                const float t = std::max((-b + std::sqrt(disc)) * 0.5f, 0.0f);
                result.valid = t > minT;
                result.thicknessMeters = std::clamp(t, minT, 1000.0f);
                result.exitPoint = Add(input.contactPoint, Multiply(dir, result.thicknessMeters));
                result.method = "sphere_chord";
                return result;
            }
        }

        if (input.shape.kind == ProjectileThicknessShapeKind::VoxelGrid && input.shape.voxelGrid && IsGridUsable(*input.shape.voxelGrid))
        {
            const CsgVoxelGrid& grid = *input.shape.voxelGrid;
            const Vec3 dir = Negate(n);
            const Vec3 cell = CsgCellSize(grid);
            const float step = std::max(0.0005f, std::min({cell.x, cell.y, cell.z}) * 0.5f);
            float thickness = 0.0f;
            bool insideAnySolid = false;
            Vec3 p = input.contactPoint;
            for (u32 i = 0; i < 4096; ++i)
            {
                const i32 ix = CellIndexAlongAxis(p.x, grid.bounds.min.x, cell.x, grid.resolutionX);
                const i32 iy = CellIndexAlongAxis(p.y, grid.bounds.min.y, cell.y, grid.resolutionY);
                const i32 iz = CellIndexAlongAxis(p.z, grid.bounds.min.z, cell.z, grid.resolutionZ);
                if (!IsInsideGrid(grid, ix, iy, iz))
                {
                    break;
                }
                if (CsgIsSolid(grid, static_cast<u32>(ix), static_cast<u32>(iy), static_cast<u32>(iz)))
                {
                    insideAnySolid = true;
                    thickness += step;
                }
                else if (insideAnySolid)
                {
                    break;
                }
                p = Add(p, Multiply(dir, step));
            }
            if (insideAnySolid)
            {
                result.valid = true;
                result.thicknessMeters = std::clamp(thickness, minT, 1000.0f);
                result.exitPoint = Add(input.contactPoint, Multiply(dir, result.thicknessMeters));
                result.method = "voxel_solid_scan";
                return result;
            }
        }

        AABB3 bounds = input.shape.bounds;
        if (input.shape.kind == ProjectileThicknessShapeKind::PhysicsCollider)
        {
            bounds = ComputeColliderLocalBounds(input.shape.physicsCollider);
        }

        if (input.shape.kind == ProjectileThicknessShapeKind::AABB || input.shape.kind == ProjectileThicknessShapeKind::PhysicsCollider)
        {
            const float proj = 2.0f * Dot(AbsVec(Extents(bounds)), AbsVec(n));
            if (proj > minT)
            {
                result.valid = true;
                result.thicknessMeters = std::clamp(proj, minT, 1000.0f);
                result.exitPoint = Subtract(input.contactPoint, Multiply(n, result.thicknessMeters));
                result.method = "aabb_projection";
                return result;
            }
        }

        result.valid = false;
        result.thicknessMeters = std::max(minT, input.fallbackThicknessMeters);
        result.exitPoint = Subtract(input.contactPoint, Multiply(n, result.thicknessMeters));
        result.method = "fallback";
        return result;
    }

    ProjectileImpactResult ResolveProjectileImpact(const ProjectileImpactInput& input)
    {
        ProjectileImpactResult result{};
        result.outPosition = input.projectile.position;
        result.outVelocity = input.projectile.velocity;
        result.outAngularVelocity = input.projectile.angularVelocity;

        ProjectileImpactMaterial material = SanitizeProjectileImpactMaterial(input.material);
        ProjectileMaterialDamageState damageState = input.materialDamage.valid ? input.materialDamage : MakeProjectileMaterialDamageState(material);
        if (!material.valid || !IsFinite(input.surfaceNormal) || !IsFinite(input.projectile.velocity))
        {
            result.outVelocity = {};
            result.nextState = ProjectileState::Stuck;
            result.outcome = ProjectileImpactOutcome::Stopped;
            result.eventType = ProjectileTrajectoryEventType::Stuck;
            return result;
        }

        if (damageState.valid && damageState.currentYieldStrengthPascals > 0.0f)
        {
            material.yieldStrengthPascals = damageState.currentYieldStrengthPascals;
            material.densityKgPerCubicMeter = std::max(1.0f, damageState.currentDensityKgPerCubicMeter);
        }

        const Vec3 direction = Normalize(input.projectile.velocity, {0.0f, 0.0f, 1.0f});
        const Vec3 normal = Normalize(input.surfaceNormal, {0.0f, 1.0f, 0.0f});
        const float incomingNormal = std::abs(Dot(Negate(direction), normal));
        result.obliquityDegrees = std::acos(std::clamp(incomingNormal, 0.0f, 1.0f)) * RadToDeg32;
        // Original Unity code tried to express “0° frontal, 90° grazing”. Use the physically stable obliquity computed from the incoming normal.
        result.impactAngleFromPlaneDegrees = result.obliquityDegrees;
        result.kineticEnergyJoules = ComputeProjectileKineticEnergyJoules(input.projectile);
        result.penetrationEnergyJoules = ComputePenetrationEnergyJoules(input.projectile, material, result.obliquityDegrees, input.thicknessMeters);
        result.impactForceNewtons = ComputeProjectileImpactForceNewtons(input.projectile, material, std::max(input.thicknessMeters, 0.01f), result.impactAngleFromPlaneDegrees);

        const float ricochetThreshold = ComputeRicochetThresholdDegrees(input.projectile, material);
        const float shellEpsilon = std::max(0.0f, input.shellEpsilonMeters);
        const float speed = Length(input.projectile.velocity);

        const bool wantsRicochet = result.impactAngleFromPlaneDegrees >= ricochetThreshold
            && result.kineticEnergyJoules < result.penetrationEnergyJoules * (1.0f + material.energyDissipation);
        if (wantsRicochet)
        {
            const Vec3 reflected = Normalize(Reflect(direction, normal), normal);
            const float speedAfter = speed * (1.0f - material.ricochetEnergyLoss);
            result.outVelocity = Multiply(reflected, speedAfter);
            result.outAngularVelocity = Multiply(input.projectile.angularVelocity, 1.0f - material.ricochetEnergyLoss);
            result.outPosition = Add(input.hitPoint, Multiply(reflected, shellEpsilon));
            result.residualEnergyJoules = 0.5f * input.projectile.massKilograms * speedAfter * speedAfter;
            result.materialDamage = ApplyProjectileMaterialDeformation(damageState, material, input.projectile, result.impactForceNewtons, std::max(input.thicknessMeters, 0.01f));
            result.damagePercent = result.materialDamage.accumulatedDamagePercent;
            result.outcome = ProjectileImpactOutcome::Ricochet;
            result.nextState = ProjectileState::Ricocheted;
            result.eventType = ProjectileTrajectoryEventType::Ricocheted;
            result.valid = true;
            return result;
        }

        if (result.kineticEnergyJoules >= result.penetrationEnergyJoules)
        {
            const float energyLoss = result.penetrationEnergyJoules * (1.0f + material.energyDissipation);
            result.residualEnergyJoules = std::max(0.0f, result.kineticEnergyJoules - energyLoss);
            const float speedAfter = std::sqrt(2.0f * result.residualEnergyJoules / std::max(MinimumMass, input.projectile.massKilograms));
            const Vec3 tangent = Normalize(Cross(direction, Cross(direction, {0.0f, 1.0f, 0.0f})), {0.0f, 0.0f, 0.0f});
            const Vec3 exitDirection = Normalize(Subtract(direction, Multiply(tangent, 0.02f)), direction);
            result.outVelocity = Multiply(exitDirection, speedAfter);
            result.outAngularVelocity = Multiply(input.projectile.angularVelocity, 1.0f - material.ricochetEnergyLoss);
            const Vec3 posAfter = LengthSquared(input.exitPoint) > 0.0f ? input.exitPoint : Add(input.hitPoint, Multiply(direction, std::max(input.thicknessMeters, 0.01f)));
            result.outPosition = Add(posAfter, Multiply(Normalize(result.outVelocity, exitDirection), shellEpsilon));
            result.massLossKilograms = std::max(0.0f, input.projectile.massKilograms * 0.02f);
            result.materialDamage = ApplyProjectileMaterialDeformation(damageState, material, input.projectile, result.impactForceNewtons, std::max(input.thicknessMeters, 0.01f));
            result.damagePercent = result.materialDamage.accumulatedDamagePercent;
            result.outcome = ProjectileImpactOutcome::Penetration;
            result.nextState = ProjectileState::Penetrated;
            result.eventType = ProjectileTrajectoryEventType::Penetrated;
            result.valid = true;
            return result;
        }

        const float energyFraction = std::clamp(result.kineticEnergyJoules / std::max(1.0e-6f, result.penetrationEnergyJoules), 0.0f, 1.0f);
        const float maxEmbed = material.maxDeformationMeters > 0.0f ? material.maxDeformationMeters : input.projectile.maxEmbedDepthFallbackMeters;
        result.embedDepthMeters = std::clamp(input.projectile.embedNoseOffsetMeters + energyFraction * maxEmbed, input.projectile.embedNoseOffsetMeters * 0.25f, std::max(input.projectile.embedNoseOffsetMeters, maxEmbed));
        result.outVelocity = {};
        result.outAngularVelocity = {};
        result.outPosition = Subtract(input.hitPoint, Multiply(normal, result.embedDepthMeters));
        result.materialDamage = ApplyProjectileMaterialDeformation(damageState, material, input.projectile, result.impactForceNewtons, std::max(input.thicknessMeters, 0.01f));
        result.damagePercent = result.materialDamage.accumulatedDamagePercent;
        result.outcome = energyFraction > 0.2f ? ProjectileImpactOutcome::Embedded : ProjectileImpactOutcome::Stopped;
        result.nextState = energyFraction > 0.2f ? ProjectileState::Embedded : ProjectileState::Stuck;
        result.eventType = EventFromOutcome(result.outcome);
        result.valid = true;
        return result;
    }

    void ApplyProjectileImpact(ProjectileBody& projectile, const ProjectileImpactResult& impact)
    {
        projectile.position = impact.outPosition;
        projectile.velocity = impact.outVelocity;
        projectile.angularVelocity = impact.outAngularVelocity;
        if (impact.massLossKilograms > 0.0f)
        {
            projectile.massKilograms = std::max(projectile.massKilograms - impact.massLossKilograms, 0.0001f);
            projectile.thermalEnergyJoules = std::max(projectile.thermalEnergyJoules - projectile.latentHeatJPerKg * impact.massLossKilograms, 0.0f);
        }
        SetProjectileState(projectile, impact.nextState);
    }

    ProjectilePhysicsHit SweepProjectileAgainstPhysicsScene(const PhysicsScene& scene, Vec3 from, Vec3 to, float shellEpsilonMeters)
    {
        ProjectilePhysicsHit best{};
        float bestT = std::numeric_limits<float>::max();
        const Vec3 segment = Subtract(to, from);
        const float segmentLength = Length(segment);
        if (segmentLength <= 1.0e-6f)
        {
            return best;
        }
        const Ray3 ray{from, Divide(segment, segmentLength)};

        for (std::size_t i = 0; i < scene.colliders.size(); ++i)
        {
            const PhysicsCollider& collider = scene.colliders[i];
            if (!collider.enabled || collider.trigger)
            {
                continue;
            }
            const PhysicsBody* body = FindBody(scene, collider.bodyId);
            if (!body || !body->enabled)
            {
                continue;
            }

            float tMeters = 0.0f;
            bool hit = false;
            Vec3 normal{0.0f, 1.0f, 0.0f};
            Vec3 point{};

            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                const Sphere3 sphere{Add(body->position, collider.localCenter), collider.radius + shellEpsilonMeters};
                float tNorm = 0.0f;
                if (SweepSegmentSphere(from, to, sphere, &tNorm))
                {
                    hit = true;
                    tMeters = tNorm * segmentLength;
                    point = Add(from, Multiply(segment, tNorm));
                    normal = Normalize(Subtract(point, sphere.center), {0.0f, 1.0f, 0.0f});
                }
            }
            else
            {
                const AABB3 bounds = InflateAABB(ComputeColliderWorldBounds(*body, collider), shellEpsilonMeters);
                float tMin = 0.0f;
                float tMax = 0.0f;
                if (RayIntersectsAABB(ray, bounds, &tMin, &tMax) && tMin >= 0.0f && tMin <= segmentLength)
                {
                    hit = true;
                    tMeters = tMin;
                    point = Add(from, Multiply(ray.direction, tMin));
                    normal = ComputeAabbNormalAtPoint(bounds, point);
                }
            }

            if (!hit || tMeters >= bestT)
            {
                continue;
            }

            bestT = tMeters;
            best.hit = true;
            best.colliderIndex = i;
            best.bodyId = collider.bodyId;
            best.colliderKind = collider.kind;
            best.point = point;
            best.normal = normal;
            best.distanceMeters = tMeters;

            ProjectileThicknessInput thicknessInput{};
            thicknessInput.contactPoint = point;
            thicknessInput.surfaceNormal = normal;
            thicknessInput.travelDirection = ray.direction;
            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                thicknessInput.shape.kind = ProjectileThicknessShapeKind::Sphere;
                thicknessInput.shape.sphere = {Add(body->position, collider.localCenter), collider.radius};
            }
            else
            {
                thicknessInput.shape.kind = ProjectileThicknessShapeKind::AABB;
                thicknessInput.shape.bounds = ComputeColliderWorldBounds(*body, collider);
            }
            best.thickness = ComputeProjectileThickness(thicknessInput);
        }
        return best;
    }

    ProjectileMultiHitResult SweepProjectileMultiHitAgainstPhysicsScene(const PhysicsScene& scene, Vec3 from, Vec3 to, const ProjectileSweepSettings& settings)
    {
        ProjectileMultiHitResult result{};
        const Vec3 segment = Subtract(to, from);
        const float distance = Length(segment);
        if (!IsFinite(from) || !IsFinite(to) || distance <= MinimumDirectionLength)
        {
            result.finite = false;
            return result;
        }

        const float maxDistance = settings.maxDistanceMeters > 0.0f ? settings.maxDistanceMeters : 10000.0f;
        if (distance > maxDistance)
        {
            to = Add(from, Multiply(Divide(segment, distance), maxDistance));
        }

        for (std::size_t i = 0; i < scene.colliders.size(); ++i)
        {
            ProjectilePhysicsHit hit{};
            if (SweepColliderForProjectile(scene, i, from, to, std::max(0.0f, settings.shellRadiusMeters), settings.includeTriggers, hit))
            {
                if (!settings.computeThickness)
                {
                    hit.thickness = {};
                }
                result.hits.push_back(hit);
            }
        }

        if (settings.sortByDistance)
        {
            std::sort(result.hits.begin(), result.hits.end(), [](const ProjectilePhysicsHit& a, const ProjectilePhysicsHit& b)
            {
                return a.distanceMeters < b.distanceMeters;
            });
        }

        const u32 maxHits = settings.maxHits > 0 ? settings.maxHits : 1;
        if (result.hits.size() > maxHits)
        {
            result.hits.resize(maxHits);
            result.clippedByMaxHits = true;
        }
        result.hit = !result.hits.empty();
        return result;
    }

    ProjectileIntegratedStepResult StepProjectileIntegrated(ProjectileBody& projectile, PhysicsScene& physicsScene, ProjectileEnvironment& environment, const ProjectileImpactMaterial& material, const ProjectileStepSettings& settings)
    {
        ProjectileIntegratedStepResult result{};
        const Vec3 from = projectile.position;
        ProjectileBody predicted = projectile;
        result.step = StepProjectileAdvanced(predicted, environment, settings);
        result.hit = SweepProjectileAgainstPhysicsScene(physicsScene, from, predicted.position, projectile.radiusMeters + 0.003f);
        if (!result.hit.hit)
        {
            projectile = predicted;
            result.ok = result.step.ok;
            return result;
        }

        ProjectileImpactInput impactInput{};
        impactInput.projectile = projectile;
        impactInput.material = material;
        impactInput.hitPoint = result.hit.point;
        impactInput.surfaceNormal = result.hit.normal;
        impactInput.thicknessMeters = result.hit.thickness.thicknessMeters;
        impactInput.exitPoint = result.hit.thickness.exitPoint;
        impactInput.shellEpsilonMeters = 0.003f;
        result.impact = ResolveProjectileImpact(impactInput);
        if (result.impact.valid)
        {
            ApplyProjectileImpact(projectile, result.impact);
            result.collisionResolved = true;
        }
        else
        {
            projectile.position = result.hit.point;
            SetProjectileState(projectile, ProjectileState::Stuck);
            result.warnings.push_back("impact_not_valid");
        }
        result.ok = result.step.ok && result.hit.hit && result.collisionResolved;
        return result;
    }

    VoxelDamageStats RemoveVoxelTunnelDda(CsgVoxelGrid& grid, Vec3 localStart, Vec3 localDirection, const VoxelTunnelSettings& settings)
    {
        VoxelDamageStats stats{};
        if (!IsGridUsable(grid))
        {
            stats.warnings.push_back("invalid_grid");
            return stats;
        }

        const Vec3 dir = Normalize(localDirection, {0.0f, 0.0f, 0.0f});
        if (LengthSquared(dir) <= MinimumDirectionLength)
        {
            stats.warnings.push_back("zero_direction");
            return stats;
        }

        const Vec3 cell = CsgCellSize(grid);
        i32 ix = CellIndexAlongAxis(localStart.x, grid.bounds.min.x, cell.x, grid.resolutionX);
        i32 iy = CellIndexAlongAxis(localStart.y, grid.bounds.min.y, cell.y, grid.resolutionY);
        i32 iz = CellIndexAlongAxis(localStart.z, grid.bounds.min.z, cell.z, grid.resolutionZ);
        if (!IsInsideGrid(grid, ix, iy, iz))
        {
            stats.clippedByBounds = true;
            stats.warnings.push_back("start_outside_grid");
            return stats;
        }

        const i32 stepX = dir.x >= 0.0f ? 1 : -1;
        const i32 stepY = dir.y >= 0.0f ? 1 : -1;
        const i32 stepZ = dir.z >= 0.0f ? 1 : -1;

        const auto nextBoundary = [](float minValue, float cellSize, i32 index, i32 step)
        {
            return minValue + (step > 0 ? static_cast<float>(index + 1) : static_cast<float>(index)) * cellSize;
        };

        float tx = dir.x != 0.0f ? (nextBoundary(grid.bounds.min.x, cell.x, ix, stepX) - localStart.x) / dir.x : std::numeric_limits<float>::infinity();
        float ty = dir.y != 0.0f ? (nextBoundary(grid.bounds.min.y, cell.y, iy, stepY) - localStart.y) / dir.y : std::numeric_limits<float>::infinity();
        float tz = dir.z != 0.0f ? (nextBoundary(grid.bounds.min.z, cell.z, iz, stepZ) - localStart.z) / dir.z : std::numeric_limits<float>::infinity();

        const float tStepX = dir.x != 0.0f ? cell.x / std::abs(dir.x) : std::numeric_limits<float>::infinity();
        const float tStepY = dir.y != 0.0f ? cell.y / std::abs(dir.y) : std::numeric_limits<float>::infinity();
        const float tStepZ = dir.z != 0.0f ? cell.z / std::abs(dir.z) : std::numeric_limits<float>::infinity();
        const float maxDepth = std::max(0.0f, settings.maxDepthMeters);
        const float radius = std::max(0.0f, settings.radiusMeters);

        float t = 0.0f;
        while (t <= maxDepth && IsInsideGrid(grid, ix, iy, iz))
        {
            const Vec3 center = CsgCellCenter(grid, static_cast<u32>(ix), static_cast<u32>(iy), static_cast<u32>(iz));
            ClearSphere(grid, center, radius, stats, Multiply(dir, 1.0f));
            ++stats.visitedTunnelCells;

            if (tx < ty)
            {
                if (tx < tz)
                {
                    ix += stepX;
                    t = tx;
                    tx += tStepX;
                }
                else
                {
                    iz += stepZ;
                    t = tz;
                    tz += tStepZ;
                }
            }
            else
            {
                if (ty < tz)
                {
                    iy += stepY;
                    t = ty;
                    ty += tStepY;
                }
                else
                {
                    iz += stepZ;
                    t = tz;
                    tz += tStepZ;
                }
            }
        }

        stats.clippedByBounds = !IsInsideGrid(grid, ix, iy, iz);
        if (settings.removeUnsupportedAfterCut)
        {
            std::vector<VoxelDebrisCandidate> unsupportedDebris;
            stats.unsupportedRemovedVoxels = RemoveUnsupportedVoxelsFloodFill(grid, settings.supportFromBottomY, settings.createDebrisCandidates ? &unsupportedDebris : nullptr);
            stats.debrisCandidateVoxels += stats.unsupportedRemovedVoxels;
            stats.debrisCandidates.insert(stats.debrisCandidates.end(), unsupportedDebris.begin(), unsupportedDebris.end());
        }
        stats.exposedFacesAfter = CountVoxelExposedFaces(grid);
        stats.ok = stats.clearedVoxels > 0;
        return stats;
    }

    u64 RemoveUnsupportedVoxelsFloodFill(CsgVoxelGrid& grid, bool supportFromBottomY, std::vector<VoxelDebrisCandidate>* outDebris)
    {
        if (!IsGridUsable(grid))
        {
            return 0;
        }

        std::vector<u8> visited(grid.solid.size(), 0);
        std::queue<u64> queue;

        if (supportFromBottomY)
        {
            for (u32 z = 0; z < grid.resolutionZ; ++z)
            {
                for (u32 x = 0; x < grid.resolutionX; ++x)
                {
                    const u64 index = CsgCellIndex(grid, x, 0, z);
                    if (grid.solid[static_cast<std::size_t>(index)] != 0)
                    {
                        visited[static_cast<std::size_t>(index)] = 1;
                        queue.push(index);
                    }
                }
            }
        }
        else
        {
            for (u32 z = 0; z < grid.resolutionZ; ++z)
            {
                for (u32 y = 0; y < grid.resolutionY; ++y)
                {
                    for (u32 x = 0; x < grid.resolutionX; ++x)
                    {
                        const bool boundary = x == 0 || y == 0 || z == 0 || x + 1 == grid.resolutionX || y + 1 == grid.resolutionY || z + 1 == grid.resolutionZ;
                        if (!boundary)
                        {
                            continue;
                        }
                        const u64 index = CsgCellIndex(grid, x, y, z);
                        if (grid.solid[static_cast<std::size_t>(index)] != 0)
                        {
                            visited[static_cast<std::size_t>(index)] = 1;
                            queue.push(index);
                        }
                    }
                }
            }
        }

        (void)FloodFillFromSeeds(grid, visited, queue);

        u64 removed = 0;
        for (u32 z = 0; z < grid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY; ++y)
            {
                for (u32 x = 0; x < grid.resolutionX; ++x)
                {
                    const u64 index = CsgCellIndex(grid, x, y, z);
                    if (grid.solid[static_cast<std::size_t>(index)] != 0 && visited[static_cast<std::size_t>(index)] == 0)
                    {
                        grid.solid[static_cast<std::size_t>(index)] = 0;
                        ++removed;
                        PushDebrisCandidate(grid, x, y, z, {0.0f, -1.0f, 0.0f}, outDebris);
                    }
                }
            }
        }
        return removed;
    }

    u64 CountVoxelExposedFaces(const CsgVoxelGrid& grid)
    {
        if (!IsGridUsable(grid))
        {
            return 0;
        }
        u64 faces = 0;
        constexpr std::array<std::array<i32, 3>, 6> offsets{{
            {{ 1,  0,  0}},
            {{-1,  0,  0}},
            {{ 0,  1,  0}},
            {{ 0, -1,  0}},
            {{ 0,  0,  1}},
            {{ 0,  0, -1}}
        }};
        for (u32 z = 0; z < grid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < grid.resolutionY; ++y)
            {
                for (u32 x = 0; x < grid.resolutionX; ++x)
                {
                    if (!CsgIsSolid(grid, x, y, z))
                    {
                        continue;
                    }
                    for (const auto& offset : offsets)
                    {
                        const i32 nx = static_cast<i32>(x) + offset[0];
                        const i32 ny = static_cast<i32>(y) + offset[1];
                        const i32 nz = static_cast<i32>(z) + offset[2];
                        if (!IsInsideGrid(grid, nx, ny, nz) || !CsgIsSolid(grid, static_cast<u32>(nx), static_cast<u32>(ny), static_cast<u32>(nz)))
                        {
                            ++faces;
                        }
                    }
                }
            }
        }
        return faces;
    }

    std::vector<VoxelRleRun> CompressVoxelGridRLE(const CsgVoxelGrid& grid)
    {
        std::vector<VoxelRleRun> runs;
        if (grid.solid.empty())
        {
            return runs;
        }

        u8 current = grid.solid.front();
        u32 count = 0;
        for (u8 value : grid.solid)
        {
            if (value == current && count < std::numeric_limits<u32>::max())
            {
                ++count;
                continue;
            }
            runs.push_back({current, count});
            current = value;
            count = 1;
        }
        runs.push_back({current, count});
        return runs;
    }

    bool DecompressVoxelGridRLE(CsgVoxelGrid& grid, const std::vector<VoxelRleRun>& runs)
    {
        std::vector<u8> values;
        values.reserve(grid.solid.size());
        for (const VoxelRleRun& run : runs)
        {
            if (run.count == 0)
            {
                return false;
            }
            if (values.size() + run.count > grid.solid.size())
            {
                return false;
            }
            values.insert(values.end(), run.count, run.value != 0 ? 1 : 0);
        }
        if (values.size() != grid.solid.size())
        {
            return false;
        }
        grid.solid = std::move(values);
        return true;
    }

    VoxelBitsetOccupancy BuildVoxelBitsetOccupancy(const CsgVoxelGrid& grid)
    {
        VoxelBitsetOccupancy occupancy{};
        occupancy.resolutionX = grid.resolutionX;
        occupancy.resolutionY = grid.resolutionY;
        occupancy.resolutionZ = grid.resolutionZ;
        if (!IsGridUsable(grid))
        {
            return occupancy;
        }

        const u64 cellCount = CsgCellCount(grid);
        occupancy.words.assign(static_cast<std::size_t>(VoxelBitWordCount(cellCount)), 0u);
        for (u64 i = 0; i < cellCount; ++i)
        {
            if (grid.solid[static_cast<std::size_t>(i)] != 0)
            {
                occupancy.words[static_cast<std::size_t>(i >> 6u)] |= (1ull << (i & 63u));
                ++occupancy.solidCount;
            }
        }
        occupancy.valid = true;
        return occupancy;
    }

    bool ApplyVoxelBitsetOccupancy(CsgVoxelGrid& grid, const VoxelBitsetOccupancy& occupancy)
    {
        if (!IsGridUsable(grid) || !IsVoxelBitsetShapeValid(occupancy)
            || grid.resolutionX != occupancy.resolutionX
            || grid.resolutionY != occupancy.resolutionY
            || grid.resolutionZ != occupancy.resolutionZ)
        {
            return false;
        }

        const u64 cellCount = CsgCellCount(grid);
        for (u64 i = 0; i < cellCount; ++i)
        {
            grid.solid[static_cast<std::size_t>(i)] = GetVoxelBit(occupancy, i) ? 1 : 0;
        }
        return true;
    }

    bool GetVoxelBit(const VoxelBitsetOccupancy& occupancy, u64 index)
    {
        if (!IsVoxelBitsetShapeValid(occupancy))
        {
            return false;
        }
        const u64 cellCount = static_cast<u64>(occupancy.resolutionX) * occupancy.resolutionY * occupancy.resolutionZ;
        if (index >= cellCount)
        {
            return false;
        }
        return (occupancy.words[static_cast<std::size_t>(index >> 6u)] & (1ull << (index & 63u))) != 0;
    }

    void SetVoxelBit(VoxelBitsetOccupancy& occupancy, u64 index, bool solid)
    {
        if (!IsVoxelBitsetShapeValid(occupancy))
        {
            return;
        }
        const u64 cellCount = static_cast<u64>(occupancy.resolutionX) * occupancy.resolutionY * occupancy.resolutionZ;
        if (index >= cellCount)
        {
            return;
        }
        u64& word = occupancy.words[static_cast<std::size_t>(index >> 6u)];
        const u64 mask = (1ull << (index & 63u));
        const bool wasSet = (word & mask) != 0;
        if (solid)
        {
            word |= mask;
            if (!wasSet)
            {
                ++occupancy.solidCount;
            }
        }
        else
        {
            word &= ~mask;
            if (wasSet && occupancy.solidCount > 0)
            {
                --occupancy.solidCount;
            }
        }
    }

    VoxelDamageWorkset BuildVoxelDamageWorkset(const CsgVoxelGrid& grid, const VoxelDamageStats& stats, u32 chunkSize)
    {
        VoxelDamageWorkset workset{};
        workset.chunkSize = std::max(1u, chunkSize);
        workset.occupancy = BuildVoxelBitsetOccupancy(grid);
        if (!workset.occupancy.valid)
        {
            return workset;
        }

        const u32 chunksX = (grid.resolutionX + workset.chunkSize - 1u) / workset.chunkSize;
        const u32 chunksY = (grid.resolutionY + workset.chunkSize - 1u) / workset.chunkSize;
        const u32 chunksZ = (grid.resolutionZ + workset.chunkSize - 1u) / workset.chunkSize;
        workset.chunkCount = static_cast<u64>(chunksX) * chunksY * chunksZ;
        workset.dirtyChunkCount = stats.clearedVoxels > 0 || stats.unsupportedRemovedVoxels > 0 ? std::min<u64>(workset.chunkCount, std::max<u64>(1, (stats.clearedVoxels + stats.unsupportedRemovedVoxels + 63u) / 64u)) : 0;
        workset.bytesUsed = static_cast<u64>(workset.occupancy.words.size() * sizeof(u64));
        if (stats.clearedVoxels > 0 || stats.unsupportedRemovedVoxels > 0)
        {
            workset.dirtyRange = {0, 0, 0, grid.resolutionX > 0 ? grid.resolutionX - 1u : 0u, grid.resolutionY > 0 ? grid.resolutionY - 1u : 0u, grid.resolutionZ > 0 ? grid.resolutionZ - 1u : 0u, true};
        }
        workset.valid = true;
        return workset;
    }

    void AddProjectileTrajectorySample(ProjectileTrajectory& trajectory, Vec3 position, float timeSeconds)
    {
        if (trajectory.maxSamples == 0)
        {
            return;
        }
        if (trajectory.samples.size() >= trajectory.maxSamples)
        {
            trajectory.samples.erase(trajectory.samples.begin());
        }
        trajectory.samples.push_back({position, timeSeconds});
    }

    void AddProjectileTrajectoryEvent(ProjectileTrajectory& trajectory, Vec3 position, ProjectileTrajectoryEventType type, float timeSeconds)
    {
        trajectory.events.push_back({position, type, timeSeconds});
    }

    std::vector<Vec3> BuildProjectileCatmullRomTrajectory(const ProjectileTrajectory& trajectory, u32 interpolationSteps)
    {
        std::vector<Vec3> points;
        if (trajectory.samples.empty())
        {
            return points;
        }
        if (trajectory.samples.size() < 2 || interpolationSteps == 0)
        {
            for (const ProjectileTrajectorySample& sample : trajectory.samples)
            {
                points.push_back(sample.position);
            }
            return points;
        }

        points.push_back(trajectory.samples.front().position);
        for (std::size_t i = 0; i + 1 < trajectory.samples.size(); ++i)
        {
            const Vec3 p0 = i == 0 ? trajectory.samples[i].position : trajectory.samples[i - 1].position;
            const Vec3 p1 = trajectory.samples[i].position;
            const Vec3 p2 = trajectory.samples[i + 1].position;
            const Vec3 p3 = (i + 2 < trajectory.samples.size()) ? trajectory.samples[i + 2].position : trajectory.samples.back().position;
            for (u32 j = 1; j <= interpolationSteps; ++j)
            {
                const float t = static_cast<float>(j) / static_cast<float>(interpolationSteps);
                const float t2 = t * t;
                const float t3 = t2 * t;
                const Vec3 a = Multiply(p1, 2.0f);
                const Vec3 b = Multiply(Subtract(p2, p0), t);
                const Vec3 c = Multiply(Add(Subtract(Multiply(p0, 2.0f), Multiply(p1, 5.0f)), Subtract(Multiply(p2, 4.0f), p3)), t2);
                const Vec3 d = Multiply(Add(Subtract(Multiply(p1, 3.0f), p0), Subtract(p3, Multiply(p2, 3.0f))), t3);
                points.push_back(Multiply(Add(Add(a, b), Add(c, d)), 0.5f));
            }
        }
        return points;
    }

    ProjectileSystemStats ProjectileSystemFixedUpdate(std::vector<ProjectileComponent>& projectiles, PhysicsScene& physicsScene, const std::vector<GravityFieldDesc>& gravityFields, const ProjectileNativeStepSettings& settings)
    {
        ProjectileSystemStats stats{};
        for (ProjectileComponent& projectile : projectiles)
        {
            if (!projectile.active)
            {
                continue;
            }

            ++stats.activeProjectiles;
            const Vec3 from = projectile.body.position;
            ProjectileNativeStepResult native = StepProjectileNative(projectile.body, projectile.world, projectile.environment, gravityFields, settings);
            ++stats.steppedProjectiles;
            if (native.originRebased)
            {
                ++stats.largeWorldRebases;
            }
            if (native.gravity.finite)
            {
                ++stats.gravitySamples;
            }
            stats.warnings += static_cast<u32>(native.warnings.size() + native.step.warnings.size());

            ProjectileSweepSettings sweepSettings{};
            sweepSettings.shellRadiusMeters = projectile.body.radiusMeters + 0.003f;
            sweepSettings.maxHits = 4;
            ProjectileMultiHitResult hits = SweepProjectileMultiHitAgainstPhysicsScene(physicsScene, from, projectile.body.position, sweepSettings);
            ++stats.collisionQueries;
            if (hits.hit)
            {
                const ProjectilePhysicsHit& hit = hits.hits.front();
                ProjectileImpactInput impactInput{};
                impactInput.projectile = projectile.body;
                impactInput.material = projectile.impactMaterial;
                impactInput.hitPoint = hit.point;
                impactInput.surfaceNormal = hit.normal;
                impactInput.thicknessMeters = hit.thickness.thicknessMeters;
                impactInput.exitPoint = hit.thickness.exitPoint;
                ProjectileImpactResult impact = ResolveProjectileImpact(impactInput);
                if (impact.valid)
                {
                    ApplyProjectileImpact(projectile.body, impact);
                    SyncProjectileWorldStateFromBody(projectile.body, projectile.world);
                    AddProjectileTrajectoryEvent(projectile.trajectory, impact.outPosition, impact.eventType, projectile.body.ageSeconds);
                    ++stats.impacts;
                    if (impact.outcome == ProjectileImpactOutcome::Penetration)
                    {
                        ++stats.penetrations;
                    }
                    else if (impact.outcome == ProjectileImpactOutcome::Ricochet)
                    {
                        ++stats.ricochets;
                    }
                    else if (impact.outcome == ProjectileImpactOutcome::Embedded || impact.outcome == ProjectileImpactOutcome::Stopped)
                    {
                        ++stats.stopped;
                    }
                }
            }

            AddProjectileTrajectorySample(projectile.trajectory, projectile.body.position, projectile.body.ageSeconds);
            projectile.active = IsFlying(projectile.body.state);
        }

        stats.ok = stats.activeProjectiles == 0 || stats.steppedProjectiles > 0;
        return stats;
    }

    std::string ToDebugString(const ProjectileStepResult& result)
    {
        std::ostringstream out;
        out << "projectile step state=" << ToString(result.state)
            << " speed=" << result.speedMetersPerSecond
            << " mach=" << result.mach
            << " cd=" << result.dragCoefficient
            << " energy=" << result.kineticEnergyJoules
            << " mass=" << result.massKilograms
            << " temp=" << result.temperatureKelvin
            << " wind=" << ToDebugString(result.windMetersPerSecond, 2)
            << " finite=" << (result.finite ? "true" : "false")
            << " warnings=" << result.warnings.size();
        return out.str();
    }

    std::string ToDebugString(const ProjectileImpactResult& result)
    {
        std::ostringstream out;
        out << "impact outcome=" << ToString(result.outcome)
            << " next=" << ToString(result.nextState)
            << " event=" << ToString(result.eventType)
            << " obliquity=" << result.obliquityDegrees
            << " plane_angle=" << result.impactAngleFromPlaneDegrees
            << " kinetic=" << result.kineticEnergyJoules
            << " pen_req=" << result.penetrationEnergyJoules
            << " residual=" << result.residualEnergyJoules
            << " force=" << result.impactForceNewtons
            << " mass_loss=" << result.massLossKilograms
            << " embed=" << result.embedDepthMeters
            << " damage=" << result.damagePercent
            << " valid=" << (result.valid ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const ProjectilePhysicsHit& hit)
    {
        std::ostringstream out;
        out << "physics hit=" << (hit.hit ? "true" : "false")
            << " collider=" << hit.colliderIndex
            << " body=" << hit.bodyId
            << " kind=" << ToString(hit.colliderKind)
            << " distance=" << hit.distanceMeters
            << " normal=" << ToDebugString(hit.normal, 2)
            << " thickness=" << hit.thickness.thicknessMeters
            << " method=" << hit.thickness.method;
        return out.str();
    }

    std::string ToDebugString(const VoxelDamageStats& stats)
    {
        std::ostringstream out;
        out << "voxel damage visited=" << stats.visitedTunnelCells
            << " cleared=" << stats.clearedVoxels
            << " unsupported=" << stats.unsupportedRemovedVoxels
            << " debris=" << stats.debrisCandidateVoxels
            << " debris_records=" << stats.debrisCandidates.size()
            << " exposed_faces=" << stats.exposedFacesAfter
            << " clipped=" << (stats.clippedByBounds ? "true" : "false")
            << " warnings=" << stats.warnings.size()
            << " ok=" << (stats.ok ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const ProjectileProbeResult& probe)
    {
        std::ostringstream out;
        out << probe.summary << '\n'
            << ToDebugString(probe.step) << '\n'
            << ToDebugString(probe.physicsHit) << '\n'
            << ToDebugString(probe.impact) << '\n'
            << ToDebugString(probe.damage) << '\n'
            << "native ok=" << (probe.nativeStep.ok ? "yes" : "no")
            << " gravity=" << ToString(probe.nativeStep.gravity.sourceKind)
            << " rebase=" << (probe.nativeStep.originRebased ? "yes" : "no")
            << " lw_cell=" << ToDebugString(probe.nativeStep.newWorldPosition.cell) << '\n'
            << "multi_hit count=" << probe.multiHit.hits.size()
            << " clipped=" << (probe.multiHit.clippedByMaxHits ? "yes" : "no") << '\n'
            << "voxel_bitset words=" << probe.voxelWorkset.occupancy.words.size()
            << " chunks=" << probe.voxelWorkset.chunkCount
            << " dirty_chunks=" << probe.voxelWorkset.dirtyChunkCount
            << " bytes=" << probe.voxelWorkset.bytesUsed << '\n'
            << "projectile_system stepped=" << probe.systemStats.steppedProjectiles
            << " impacts=" << probe.systemStats.impacts
            << " gravity_samples=" << probe.systemStats.gravitySamples
            << " warnings=" << probe.systemStats.warnings << '\n'
            << "trajectory samples=" << probe.trajectory.samples.size()
            << " events=" << probe.trajectory.events.size()
            << " rle runs=" << probe.compressedRuns.size()
            << " solids_after=" << CountSolidVoxels(probe.damagedGrid);
        return out.str();
    }

    ProjectileProbeResult BuildProjectileProbe()
    {
        ProjectileProbeResult probe{};
        const ProjectilePreset preset = MakeProjectilePreset(ProjectileDamageType::Physical, ProjectileWeaponType::SniperRifle);
        probe.environment = MakeProjectileEnvironment(ProjectileEnvironmentPreset::Storm);
        probe.environment.randomSeed = 0xC001B411u;

        probe.projectile = MakeProjectileBody(preset, {-1.2f, 0.02f, 0.0f}, {1.0f, 0.0f, 0.0f});
        ProjectileStepSettings stepSettings{};
        stepSettings.deltaSeconds = 1.0f / 240.0f;
        stepSettings.enableMagnus = true;
        stepSettings.enableCoriolis = true;
        stepSettings.enableTurbulence = true;
        stepSettings.enableThermal = true;
        stepSettings.enableMassErosion = true;

        AddProjectileTrajectorySample(probe.trajectory, probe.projectile.position, 0.0f);
        probe.step = StepProjectileAdvanced(probe.projectile, probe.environment, stepSettings);
        AddProjectileTrajectorySample(probe.trajectory, probe.projectile.position, probe.projectile.ageSeconds);

        ProjectileBody nativeProjectile = MakeProjectileBody(preset, {}, {1.0f, 0.0f, 0.0f});
        ProjectileWorldState nativeWorld = MakeProjectileWorldState(MakeWorldPosition(6371000.0 + 1200.0, 32.0, 0.0), nativeProjectile.velocity);
        ProjectileEnvironment nativeEnvironment = MakeProjectileEnvironment(ProjectileEnvironmentPreset::HighAltitude);
        ProjectileNativeStepSettings nativeSettings{};
        nativeSettings.step = stepSettings;
        nativeSettings.largeWorld.cellSizeMeters = DefaultWorldCellSizeMeters;
        nativeSettings.largeWorld.rebaseThresholdMeters = 16.0;
        nativeSettings.largeWorld.maxCameraRelativeMeters = 2048.0;
        nativeSettings.gravity.maxAccelerationMetersPerSecondSquared = 200.0;
        std::vector<GravityFieldDesc> gravityFields;
        gravityFields.push_back(MakePlanetGravityField({}, {0.0, 0.0, 0.0}));
        probe.nativeStep = StepProjectileNative(nativeProjectile, nativeWorld, nativeEnvironment, gravityFields, nativeSettings);

        PhysicsScene physicsScene{};
        physicsScene.bodies.push_back(MakeStaticBody(1, {0.0f, 0.0f, 0.0f}));
        physicsScene.colliders.push_back(MakeBoxCollider(1, {0.18f, 0.45f, 0.35f}, {0.0f, 0.0f, 0.0f}));
        physicsScene.bodies.push_back(MakeStaticBody(2, {0.42f, 0.0f, 0.0f}));
        physicsScene.colliders.push_back(MakeSphereCollider(2, 0.12f, {0.0f, 0.0f, 0.0f}));
        ProjectileSweepSettings multiSweepSettings{};
        multiSweepSettings.shellRadiusMeters = probe.projectile.radiusMeters + 0.003f;
        multiSweepSettings.maxHits = 4;
        probe.multiHit = SweepProjectileMultiHitAgainstPhysicsScene(physicsScene, {-0.55f, 0.02f, 0.0f}, {0.65f, 0.02f, 0.0f}, multiSweepSettings);

        SurfaceMaterialRegistry registry = BuildDefaultSurfaceMaterialRegistry();
        const SurfaceMaterialDesc* concrete = registry.FindByKind(SurfaceMaterialKind::Concrete);
        ProjectileImpactMaterial impactMaterial = concrete ? MakeProjectileImpactMaterial(*concrete) : ProjectileImpactMaterial{};
        impactMaterial.yieldStrengthPascals = std::max(impactMaterial.yieldStrengthPascals, 70.0e6f);
        impactMaterial.youngModulusPascals = std::max(impactMaterial.youngModulusPascals, 70.0e9f);
        impactMaterial.speedOfSoundMetersPerSecond = 5000.0f;
        impactMaterial.ricochetEnergyLoss = 0.15f;
        impactMaterial.maxDeformationMeters = 0.005f;
        impactMaterial.energyDissipation = 0.1f;

        ProjectileBody impactProjectile = probe.projectile;
        impactProjectile.position = {-0.24f, 0.02f, 0.0f};
        impactProjectile.velocity = {Length(impactProjectile.velocity), 0.0f, 0.0f};
        probe.integrated = StepProjectileIntegrated(impactProjectile, physicsScene, probe.environment, impactMaterial, stepSettings);
        probe.physicsHit = probe.integrated.hit;
        probe.impact = probe.integrated.impact;
        if (probe.impact.valid)
        {
            AddProjectileTrajectoryEvent(probe.trajectory, probe.impact.outPosition, probe.impact.eventType, impactProjectile.ageSeconds);
        }

        probe.damagedGrid = MakeCsgVoxelGrid(MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.6f, 0.45f, 0.35f}), 18, 14, 12);
        for (u32 z = 0; z < probe.damagedGrid.resolutionZ; ++z)
        {
            for (u32 y = 0; y < probe.damagedGrid.resolutionY; ++y)
            {
                for (u32 x = 0; x < probe.damagedGrid.resolutionX; ++x)
                {
                    CsgSetSolid(probe.damagedGrid, x, y, z, true);
                }
            }
        }

        VoxelTunnelSettings damageSettings{};
        damageSettings.radiusMeters = 0.09f;
        damageSettings.maxDepthMeters = 1.3f;
        damageSettings.removeUnsupportedAfterCut = true;
        damageSettings.createDebrisCandidates = true;
        probe.damage = RemoveVoxelTunnelDda(probe.damagedGrid, {-0.55f, 0.05f, 0.0f}, {1.0f, 0.0f, 0.0f}, damageSettings);
        probe.voxelWorkset = BuildVoxelDamageWorkset(probe.damagedGrid, probe.damage, 8);

        ProjectileThicknessInput voxelThicknessInput{};
        voxelThicknessInput.shape.kind = ProjectileThicknessShapeKind::VoxelGrid;
        voxelThicknessInput.shape.voxelGrid = &probe.damagedGrid;
        voxelThicknessInput.contactPoint = {-0.55f, 0.35f, 0.22f};
        voxelThicknessInput.surfaceNormal = {-1.0f, 0.0f, 0.0f};
        const ProjectileThicknessResult voxelThickness = ComputeProjectileThickness(voxelThicknessInput);

        probe.compressedRuns = CompressVoxelGridRLE(probe.damagedGrid);
        CsgVoxelGrid roundtrip = MakeCsgVoxelGrid(probe.damagedGrid.bounds, probe.damagedGrid.resolutionX, probe.damagedGrid.resolutionY, probe.damagedGrid.resolutionZ);
        const bool rleOk = DecompressVoxelGridRLE(roundtrip, probe.compressedRuns) && roundtrip.solid == probe.damagedGrid.solid;
        CsgVoxelGrid bitsetRoundtrip = MakeCsgVoxelGrid(probe.damagedGrid.bounds, probe.damagedGrid.resolutionX, probe.damagedGrid.resolutionY, probe.damagedGrid.resolutionZ);
        const bool bitsetOk = ApplyVoxelBitsetOccupancy(bitsetRoundtrip, probe.voxelWorkset.occupancy) && bitsetRoundtrip.solid == probe.damagedGrid.solid;
        const std::vector<Vec3> smoothTrajectory = BuildProjectileCatmullRomTrajectory(probe.trajectory, 20);

        ProjectileComponent component{};
        component.body = MakeProjectileBody(preset, {-0.35f, 0.02f, 0.0f}, {1.0f, 0.0f, 0.0f});
        component.world = MakeProjectileWorldState(MakeWorldPosition(-0.35, 0.02, 0.0), component.body.velocity);
        SyncProjectileBodyFromWorldState(component.body, component.world);
        component.environment = MakeProjectileEnvironment(ProjectileEnvironmentPreset::OpenField);
        component.impactMaterial = impactMaterial;
        std::vector<ProjectileComponent> components;
        components.push_back(component);
        probe.systemStats = ProjectileSystemFixedUpdate(components, physicsScene, gravityFields, nativeSettings);

        probe.ok = probe.step.ok
            && probe.step.finite
            && probe.physicsHit.hit
            && probe.physicsHit.thickness.valid
            && probe.impact.valid
            && probe.integrated.collisionResolved
            && probe.nativeStep.ok
            && probe.nativeStep.gravity.finite
            && probe.multiHit.hit
            && probe.multiHit.hits.size() >= 2
            && probe.damage.ok
            && probe.damage.clearedVoxels > 0
            && probe.damage.exposedFacesAfter > 0
            && !probe.damage.debrisCandidates.empty()
            && voxelThickness.valid
            && probe.voxelWorkset.valid
            && probe.voxelWorkset.occupancy.valid
            && bitsetOk
            && rleOk
            && probe.systemStats.ok
            && probe.systemStats.steppedProjectiles > 0
            && smoothTrajectory.size() >= probe.trajectory.samples.size();

        std::ostringstream out;
        out << (probe.ok ? "[ ok ]" : "[fail]")
            << " projectile engine-native optimization"
            << " preset=" << ToString(preset.damageType) << "/" << ToString(preset.weaponType)
            << " env=" << ToString(probe.environment.preset)
            << " speed=" << std::fixed << std::setprecision(2) << probe.step.speedMetersPerSecond
            << " mach=" << probe.step.mach
            << " wind=" << ToDebugString(probe.step.windMetersPerSecond, 2)
            << " hit=" << (probe.physicsHit.hit ? "yes" : "no")
            << " impact=" << ToString(probe.impact.outcome)
            << " thickness=" << probe.physicsHit.thickness.thicknessMeters
            << " voxel_thickness=" << voxelThickness.thicknessMeters
            << " mass_loss=" << probe.impact.massLossKilograms
            << " damage=" << probe.impact.damagePercent
            << " debris=" << probe.damage.debrisCandidates.size()
            << " native_gravity=" << ToString(probe.nativeStep.gravity.sourceKind)
            << " multi_hits=" << probe.multiHit.hits.size()
            << " bitset_words=" << probe.voxelWorkset.occupancy.words.size()
            << " system_steps=" << probe.systemStats.steppedProjectiles;
        probe.summary = out.str();
        return probe;
    }

    std::string BuildProjectileProbeSummary()
    {
        return BuildProjectileProbe().summary;
    }
}
