#include <AK/DebugDraw/DebugDraw.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;

        float Clamp(float value, float minValue, float maxValue)
        {
            return std::max(minValue, std::min(maxValue, value));
        }

        bool CategoryEnabled(const DebugDrawConfig& config, DebugDrawCategory category)
        {
            return HasDebugDrawCategory(config.enabledCategories, category);
        }

        std::string BodyLabel(u32 bodyId)
        {
            return "body " + std::to_string(bodyId);
        }


        float SafeRadius(float value)
        {
            return std::max(0.001f, value);
        }

        Sphere3 MakeSphere(Vec3 center, float radius)
        {
            Sphere3 sphere{};
            sphere.center = center;
            sphere.radius = SafeRadius(radius);
            return sphere;
        }

        void AddThreeAxisCross(DebugDrawList& list, Vec3 center, float radius, DebugDrawColor color, DebugDrawCategory category, const std::string& label)
        {
            AddDebugLine(list, Subtract(center, {radius, 0.0f, 0.0f}), Add(center, {radius, 0.0f, 0.0f}), color, category, label);
            AddDebugLine(list, Subtract(center, {0.0f, radius, 0.0f}), Add(center, {0.0f, radius, 0.0f}), color, category, {});
            AddDebugLine(list, Subtract(center, {0.0f, 0.0f, radius}), Add(center, {0.0f, 0.0f, radius}), color, category, {});
        }

        void AppendAabbEdges(DebugDrawList& list, AABB3 bounds, DebugDrawColor color, DebugDrawCategory category, const std::string& label)
        {
            const Vec3 mn = bounds.min;
            const Vec3 mx = bounds.max;
            const Vec3 p000{mn.x, mn.y, mn.z};
            const Vec3 p001{mn.x, mn.y, mx.z};
            const Vec3 p010{mn.x, mx.y, mn.z};
            const Vec3 p011{mn.x, mx.y, mx.z};
            const Vec3 p100{mx.x, mn.y, mn.z};
            const Vec3 p101{mx.x, mn.y, mx.z};
            const Vec3 p110{mx.x, mx.y, mn.z};
            const Vec3 p111{mx.x, mx.y, mx.z};
            AddDebugLine(list, p000, p001, color, category, label);
            AddDebugLine(list, p001, p011, color, category, {});
            AddDebugLine(list, p011, p010, color, category, {});
            AddDebugLine(list, p010, p000, color, category, {});
            AddDebugLine(list, p100, p101, color, category, {});
            AddDebugLine(list, p101, p111, color, category, {});
            AddDebugLine(list, p111, p110, color, category, {});
            AddDebugLine(list, p110, p100, color, category, {});
            AddDebugLine(list, p000, p100, color, category, {});
            AddDebugLine(list, p001, p101, color, category, {});
            AddDebugLine(list, p010, p110, color, category, {});
            AddDebugLine(list, p011, p111, color, category, {});
        }

        std::array<Vec3, 8> BuildObbCorners(Vec3 center, Vec3 halfExtents, Quat rotation)
        {
            const Vec3 ax = Rotate(rotation, {1.0f, 0.0f, 0.0f});
            const Vec3 ay = Rotate(rotation, {0.0f, 1.0f, 0.0f});
            const Vec3 az = Rotate(rotation, {0.0f, 0.0f, 1.0f});
            std::array<Vec3, 8> corners{};
            u32 index = 0;
            for (float sx : {-1.0f, 1.0f})
            {
                for (float sy : {-1.0f, 1.0f})
                {
                    for (float sz : {-1.0f, 1.0f})
                    {
                        Vec3 p = center;
                        p = Add(p, Multiply(ax, sx * halfExtents.x));
                        p = Add(p, Multiply(ay, sy * halfExtents.y));
                        p = Add(p, Multiply(az, sz * halfExtents.z));
                        corners[index++] = p;
                    }
                }
            }
            return corners;
        }

        void AppendObbEdges(DebugDrawList& list, Vec3 center, Vec3 halfExtents, Quat rotation, DebugDrawColor color, DebugDrawCategory category, const std::string& label)
        {
            const auto c = BuildObbCorners(center, halfExtents, rotation);
            const std::array<std::pair<u32, u32>, 12> edges{{
                {0, 1}, {0, 2}, {0, 4}, {3, 1}, {3, 2}, {3, 7},
                {5, 1}, {5, 4}, {5, 7}, {6, 2}, {6, 4}, {6, 7}
            }};
            bool labelled = false;
            for (const auto& edge : edges)
            {
                AddDebugLine(list, c[edge.first], c[edge.second], color, category, labelled ? std::string{} : label);
                labelled = true;
            }
        }

        void AppendCircleApprox(DebugDrawList& list, Vec3 center, Vec3 axisU, Vec3 axisV, float radius, DebugDrawColor color, DebugDrawCategory category)
        {
            constexpr u32 Segments = 16;
            Vec3 prev = Add(center, Multiply(axisU, radius));
            for (u32 i = 1; i <= Segments; ++i)
            {
                const float t = (static_cast<float>(i) / static_cast<float>(Segments)) * 2.0f * Pi;
                const Vec3 p = Add(center, Add(Multiply(axisU, std::cos(t) * radius), Multiply(axisV, std::sin(t) * radius)));
                AddDebugLine(list, prev, p, color, category, {});
                prev = p;
            }
        }

        void AppendSphereApprox(DebugDrawList& list, Sphere3 sphere, DebugDrawColor color, DebugDrawCategory category, const std::string& label)
        {
            AddDebugSphere(list, sphere, color, category, label);
            AppendCircleApprox(list, sphere.center, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, sphere.radius, color, category);
            AppendCircleApprox(list, sphere.center, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, sphere.radius, color, category);
            AppendCircleApprox(list, sphere.center, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, sphere.radius, color, category);
        }

        DebugDrawCategory CombineCategory(DebugDrawCategory a, DebugDrawCategory b)
        {
            return static_cast<DebugDrawCategory>(static_cast<u32>(a) | static_cast<u32>(b));
        }
    }

    DebugDrawCategory operator|(DebugDrawCategory a, DebugDrawCategory b)
    {
        return static_cast<DebugDrawCategory>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    DebugDrawCategory operator&(DebugDrawCategory a, DebugDrawCategory b)
    {
        return static_cast<DebugDrawCategory>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    bool HasDebugDrawCategory(DebugDrawCategory mask, DebugDrawCategory category)
    {
        return (static_cast<u32>(mask) & static_cast<u32>(category)) != 0u;
    }

    DebugDrawColor MakeDebugDrawColor(float r, float g, float b, float a)
    {
        return {Clamp(r, 0.0f, 1.0f), Clamp(g, 0.0f, 1.0f), Clamp(b, 0.0f, 1.0f), Clamp(a, 0.0f, 1.0f)};
    }

    DebugDrawColor DebugColorPhysicsBody() { return MakeDebugDrawColor(0.76f, 0.78f, 0.82f, 1.0f); }
    DebugDrawColor DebugColorCollider() { return MakeDebugDrawColor(0.18f, 0.72f, 1.0f, 1.0f); }
    DebugDrawColor DebugColorTrigger() { return MakeDebugDrawColor(0.75f, 0.35f, 1.0f, 1.0f); }
    DebugDrawColor DebugColorBroadphase() { return MakeDebugDrawColor(0.25f, 0.85f, 0.45f, 0.65f); }
    DebugDrawColor DebugColorContact() { return MakeDebugDrawColor(1.0f, 0.25f, 0.20f, 1.0f); }
    DebugDrawColor DebugColorQuery() { return MakeDebugDrawColor(1.0f, 0.84f, 0.20f, 1.0f); }
    DebugDrawColor DebugColorProjectile() { return MakeDebugDrawColor(1.0f, 0.56f, 0.16f, 1.0f); }
    DebugDrawColor DebugColorFluid() { return MakeDebugDrawColor(0.15f, 0.45f, 1.0f, 0.75f); }
    DebugDrawColor DebugColorDebris() { return MakeDebugDrawColor(0.72f, 0.54f, 0.35f, 1.0f); }
    DebugDrawColor DebugColorDamage() { return MakeDebugDrawColor(1.0f, 0.05f, 0.05f, 1.0f); }
    DebugDrawColor DebugColorExplosion() { return MakeDebugDrawColor(1.0f, 0.72f, 0.06f, 1.0f); }
    DebugDrawColor DebugColorForceField() { return MakeDebugDrawColor(0.40f, 1.0f, 0.82f, 1.0f); }

    const char* ToString(DebugDrawPrimitiveKind kind)
    {
        switch (kind)
        {
        case DebugDrawPrimitiveKind::Line: return "Line";
        case DebugDrawPrimitiveKind::Arrow: return "Arrow";
        case DebugDrawPrimitiveKind::Point: return "Point";
        case DebugDrawPrimitiveKind::Aabb: return "Aabb";
        case DebugDrawPrimitiveKind::Obb: return "Obb";
        case DebugDrawPrimitiveKind::Sphere: return "Sphere";
        case DebugDrawPrimitiveKind::Capsule: return "Capsule";
        case DebugDrawPrimitiveKind::Text: return "Text";
        }
        return "Unknown";
    }

    const char* ToString(DebugDrawCategory category)
    {
        switch (category)
        {
        case DebugDrawCategory::None: return "None";
        case DebugDrawCategory::PhysicsBody: return "PhysicsBody";
        case DebugDrawCategory::Collider: return "Collider";
        case DebugDrawCategory::Broadphase: return "Broadphase";
        case DebugDrawCategory::Contact: return "Contact";
        case DebugDrawCategory::Query: return "Query";
        case DebugDrawCategory::Projectile: return "Projectile";
        case DebugDrawCategory::Fluid: return "Fluid";
        case DebugDrawCategory::Debris: return "Debris";
        case DebugDrawCategory::Damage: return "Damage";
        case DebugDrawCategory::Explosion: return "Explosion";
        case DebugDrawCategory::ForceField: return "ForceField";
        case DebugDrawCategory::All: return "All";
        }
        return "Mixed";
    }

    const char* ToString(DebugDrawSpace space)
    {
        switch (space)
        {
        case DebugDrawSpace::World: return "World";
        case DebugDrawSpace::CameraRelative: return "CameraRelative";
        case DebugDrawSpace::PhysicsIslandLocal: return "PhysicsIslandLocal";
        }
        return "Unknown";
    }

    bool IsFinite(const DebugDrawCommand& command)
    {
        return AK::IsFinite(command.a) && AK::IsFinite(command.b) && AK::IsFinite(command.c) &&
               AK::IsFinite(command.halfExtents) && std::isfinite(command.radius) && std::isfinite(command.halfHeight) &&
               std::isfinite(command.thickness) && std::isfinite(command.lifetimeSeconds);
    }

    void ResetDebugDrawList(DebugDrawList& list, std::size_t maxCommands)
    {
        list.commands.clear();
        list.stats = {};
        list.maxCommands = maxCommands;
        list.clipped = false;
    }

    bool PushDebugDrawCommand(DebugDrawList& list, DebugDrawCommand command)
    {
        if (!IsFinite(command))
        {
            list.stats.finite = false;
            list.stats.warnings.push_back("rejected non-finite debug draw command");
            return false;
        }
        if (list.commands.size() >= list.maxCommands)
        {
            list.clipped = true;
            ++list.stats.clippedCommandCount;
            return false;
        }
        list.commands.push_back(std::move(command));
        return true;
    }

    bool AddDebugLine(DebugDrawList& list, Vec3 a, Vec3 b, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Line;
        command.category = category;
        command.color = color;
        command.a = a;
        command.b = b;
        command.label = std::move(label);
        return PushDebugDrawCommand(list, std::move(command));
    }

    bool AddDebugArrow(DebugDrawList& list, Vec3 a, Vec3 b, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Arrow;
        command.category = category;
        command.color = color;
        command.a = a;
        command.b = b;
        command.label = std::move(label);
        return PushDebugDrawCommand(list, std::move(command));
    }

    bool AddDebugPoint(DebugDrawList& list, Vec3 position, float radius, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Point;
        command.category = category;
        command.color = color;
        command.a = position;
        command.radius = SafeRadius(radius);
        command.label = std::move(label);
        return PushDebugDrawCommand(list, std::move(command));
    }

    bool AddDebugAabb(DebugDrawList& list, AABB3 bounds, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        if (!IsValid(bounds))
        {
            return false;
        }
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Aabb;
        command.category = category;
        command.color = color;
        command.a = bounds.min;
        command.b = bounds.max;
        command.halfExtents = Extents(bounds);
        command.label = label;
        const bool pushed = PushDebugDrawCommand(list, command);
        AppendAabbEdges(list, bounds, color, category, std::move(label));
        return pushed;
    }

    bool AddDebugObb(DebugDrawList& list, Vec3 center, Vec3 halfExtents, Quat rotation, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Obb;
        command.category = category;
        command.color = color;
        command.a = center;
        command.halfExtents = halfExtents;
        command.rotation = Normalize(rotation);
        command.label = label;
        const bool pushed = PushDebugDrawCommand(list, command);
        AppendObbEdges(list, center, halfExtents, rotation, color, category, std::move(label));
        return pushed;
    }

    bool AddDebugSphere(DebugDrawList& list, Sphere3 sphere, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Sphere;
        command.category = category;
        command.color = color;
        command.a = sphere.center;
        command.radius = SafeRadius(sphere.radius);
        command.label = std::move(label);
        return PushDebugDrawCommand(list, std::move(command));
    }

    bool AddDebugCapsule(DebugDrawList& list, Vec3 a, Vec3 b, float radius, DebugDrawColor color, DebugDrawCategory category, std::string label)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Capsule;
        command.category = category;
        command.color = color;
        command.a = a;
        command.b = b;
        command.radius = SafeRadius(radius);
        command.label = label;
        const bool pushed = PushDebugDrawCommand(list, command);
        AddDebugLine(list, a, b, color, category, std::move(label));
        AddDebugSphere(list, MakeSphere(a, radius), color, category, {});
        AddDebugSphere(list, MakeSphere(b, radius), color, category, {});
        return pushed;
    }

    bool AddDebugText(DebugDrawList& list, Vec3 position, std::string text, DebugDrawColor color, DebugDrawCategory category)
    {
        DebugDrawCommand command{};
        command.kind = DebugDrawPrimitiveKind::Text;
        command.category = category;
        command.color = color;
        command.a = position;
        command.label = std::move(text);
        return PushDebugDrawCommand(list, std::move(command));
    }

    void CollectPhysicsDebugDraw(DebugDrawList& list, const PhysicsScene& scene, const std::vector<PhysicsBroadphasePair>& pairs, const std::vector<PhysicsContact>& contacts, const std::vector<PhysicsContactManifold>& manifolds, const DebugDrawConfig& config)
    {
        if (config.drawPhysicsBodies && CategoryEnabled(config, DebugDrawCategory::PhysicsBody))
        {
            for (const PhysicsBody& body : scene.bodies)
            {
                if (!body.enabled)
                {
                    continue;
                }
                AddThreeAxisCross(list, body.position, 0.12f, body.sleeping ? MakeDebugDrawColor(0.35f, 0.35f, 0.35f, 1.0f) : DebugColorPhysicsBody(), DebugDrawCategory::PhysicsBody, BodyLabel(body.id));
                if (config.drawLabels)
                {
                    AddDebugText(list, Add(body.position, {0.0f, 0.18f, 0.0f}), BodyLabel(body.id), DebugColorPhysicsBody(), DebugDrawCategory::PhysicsBody);
                }
            }
        }

        if (config.drawColliders && CategoryEnabled(config, DebugDrawCategory::Collider))
        {
            for (const PhysicsCollider& collider : scene.colliders)
            {
                if (!collider.enabled)
                {
                    continue;
                }
                const PhysicsBody* body = FindBody(scene, collider.bodyId);
                if (!body)
                {
                    continue;
                }
                const DebugDrawColor color = collider.trigger ? DebugColorTrigger() : DebugColorCollider();
                const std::string label = collider.debugName.empty() ? ("collider " + std::to_string(collider.bodyId)) : collider.debugName;
                if (collider.kind == PhysicsColliderKind::Sphere)
                {
                    AppendSphereApprox(list, MakeSphere(Add(body->position, collider.localCenter), collider.radius), color, DebugDrawCategory::Collider, label);
                }
                else if (collider.kind == PhysicsColliderKind::Capsule)
                {
                    const PhysicsCapsuleShape capsule = ComputeCapsuleWorldShape(*body, collider);
                    AddDebugCapsule(list, capsule.a, capsule.b, capsule.radius, color, DebugDrawCategory::Collider, label);
                }
                else if (collider.kind == PhysicsColliderKind::Box)
                {
                    const Vec3 center = Add(body->position, Rotate(body->orientation, collider.localCenter));
                    AddDebugObb(list, center, collider.halfExtents, Multiply(body->orientation, collider.localRotation), color, DebugDrawCategory::Collider, label);
                }
                else
                {
                    AddDebugAabb(list, ComputeColliderWorldBounds(*body, collider), color, DebugDrawCategory::Collider, label);
                }
            }
        }

        if (config.drawBroadphasePairs && CategoryEnabled(config, DebugDrawCategory::Broadphase))
        {
            for (const PhysicsBroadphasePair& pair : pairs)
            {
                const Vec3 a = Center(pair.boundsA);
                const Vec3 b = Center(pair.boundsB);
                AddDebugAabb(list, pair.boundsA, DebugColorBroadphase(), DebugDrawCategory::Broadphase, "broadphase A");
                AddDebugAabb(list, pair.boundsB, DebugColorBroadphase(), DebugDrawCategory::Broadphase, "broadphase B");
                AddDebugLine(list, a, b, DebugColorBroadphase(), DebugDrawCategory::Broadphase, "pair");
            }
        }

        if (config.drawContacts && CategoryEnabled(config, DebugDrawCategory::Contact))
        {
            for (const PhysicsContact& contact : contacts)
            {
                if (!contact.valid)
                {
                    continue;
                }
                const Vec3 end = Add(contact.point, Multiply(contact.normal, 0.25f + std::max(0.0f, contact.penetration)));
                AddDebugPoint(list, contact.point, 0.035f, DebugColorContact(), DebugDrawCategory::Contact, "contact");
                AddDebugArrow(list, contact.point, end, DebugColorContact(), DebugDrawCategory::Contact, "normal");
            }
        }

        if (config.drawManifolds && CategoryEnabled(config, DebugDrawCategory::Contact))
        {
            for (const PhysicsContactManifold& manifold : manifolds)
            {
                if (!manifold.valid)
                {
                    continue;
                }
                for (u32 i = 0; i < manifold.pointCount && i < manifold.points.size(); ++i)
                {
                    AddDebugPoint(list, manifold.points[i], 0.025f, MakeDebugDrawColor(1.0f, 0.36f, 0.18f, 1.0f), DebugDrawCategory::Contact, "manifold");
                }
            }
        }
    }

    void CollectPhysicsQueryDebugDraw(DebugDrawList& list, const PhysicsRaycastHit& raycast, Ray3 ray, float rayLength, const PhysicsSweepHit& sweep, Vec3 sweepStart, Vec3 sweepEnd, float sweepRadius, const DebugDrawConfig& config)
    {
        if (!config.drawQueries || !CategoryEnabled(config, DebugDrawCategory::Query))
        {
            return;
        }
        const Vec3 rayEnd = Add(ray.origin, Multiply(Normalize(ray.direction), rayLength));
        AddDebugArrow(list, ray.origin, rayEnd, DebugColorQuery(), DebugDrawCategory::Query, "raycast");
        if (raycast.hit)
        {
            AddDebugPoint(list, raycast.point, 0.04f, DebugColorQuery(), DebugDrawCategory::Query, "ray hit");
            AddDebugArrow(list, raycast.point, Add(raycast.point, Multiply(raycast.normal, 0.25f)), DebugColorQuery(), DebugDrawCategory::Query, "ray normal");
        }
        AddDebugCapsule(list, sweepStart, sweepEnd, sweepRadius, DebugColorQuery(), DebugDrawCategory::Query, "sphere sweep");
        if (sweep.hit)
        {
            AddDebugPoint(list, sweep.point, 0.05f, DebugColorQuery(), DebugDrawCategory::Query, "sweep hit");
            AddDebugArrow(list, sweep.point, Add(sweep.point, Multiply(sweep.normal, 0.25f)), DebugColorQuery(), DebugDrawCategory::Query, "sweep normal");
        }
    }

    void CollectProjectileDebugDraw(DebugDrawList& list, const ProjectileProbeResult& projectile, const DebugDrawConfig& config)
    {
        if (!config.drawProjectileTrajectory || !CategoryEnabled(config, DebugDrawCategory::Projectile))
        {
            return;
        }
        for (std::size_t i = 1; i < projectile.trajectory.samples.size(); ++i)
        {
            AddDebugLine(list, projectile.trajectory.samples[i - 1].position, projectile.trajectory.samples[i].position, DebugColorProjectile(), DebugDrawCategory::Projectile, i == 1 ? "projectile trajectory" : std::string{});
        }
        if (projectile.trajectory.samples.empty())
        {
            AddDebugPoint(list, projectile.projectile.position, projectile.projectile.radiusMeters * 4.0f, DebugColorProjectile(), DebugDrawCategory::Projectile, "projectile");
        }
        else
        {
            AddDebugPoint(list, projectile.trajectory.samples.back().position, projectile.projectile.radiusMeters * 4.0f, DebugColorProjectile(), DebugDrawCategory::Projectile, "projectile head");
        }
        for (const ProjectileTrajectoryEvent& event : projectile.trajectory.events)
        {
            AddDebugPoint(list, event.position, 0.05f, DebugColorDamage(), DebugDrawCategory::Projectile, ToString(event.type));
        }
        if (projectile.physicsHit.hit)
        {
            AddDebugArrow(list, projectile.physicsHit.point, Add(projectile.physicsHit.point, Multiply(projectile.physicsHit.normal, 0.35f)), DebugColorProjectile(), DebugDrawCategory::Projectile, "projectile hit");
        }
    }

    void CollectFluidDebugDraw(DebugDrawList& list, const std::vector<FluidVolume>& volumes, const std::vector<FluidColliderInteraction>& interactions, const DebugDrawConfig& config)
    {
        if (!config.drawFluids || !CategoryEnabled(config, DebugDrawCategory::Fluid))
        {
            return;
        }
        for (const FluidVolume& volume : volumes)
        {
            if (!volume.enabled)
            {
                continue;
            }
            if (volume.kind == FluidVolumeKind::Sphere)
            {
                AppendSphereApprox(list, MakeSphere(volume.center, volume.radius), DebugColorFluid(), DebugDrawCategory::Fluid, volume.debugName.empty() ? "fluid sphere" : volume.debugName);
            }
            else if (volume.kind == FluidVolumeKind::Plane)
            {
                const Vec3 center{volume.center.x, volume.surfaceHeight, volume.center.z};
                AddDebugAabb(list, MakeAABB3FromCenterExtents(center, {8.0f, 0.01f, 8.0f}), DebugColorFluid(), DebugDrawCategory::Fluid, volume.debugName.empty() ? "fluid plane" : volume.debugName);
            }
            else
            {
                AddDebugAabb(list, MakeAABB3FromCenterExtents(volume.center, volume.halfExtents), DebugColorFluid(), DebugDrawCategory::Fluid, volume.debugName.empty() ? "fluid box" : volume.debugName);
            }
            if (LengthSquared(volume.flowVelocity) > 0.0001f)
            {
                AddDebugArrow(list, volume.center, Add(volume.center, Multiply(Normalize(volume.flowVelocity), 0.75f)), DebugColorFluid(), DebugDrawCategory::Fluid, "flow");
            }
        }
        for (const FluidColliderInteraction& interaction : interactions)
        {
            if (!interaction.active)
            {
                continue;
            }
            AddDebugArrow(list, interaction.applicationPoint, Add(interaction.applicationPoint, Multiply(Normalize(interaction.totalForce, {0.0f, 1.0f, 0.0f}), 0.35f)), DebugColorFluid(), DebugDrawCategory::Fluid, "fluid force");
        }
    }

    void CollectDebrisDebugDraw(DebugDrawList& list, const DebrisSystem& debris, const DebugDrawConfig& config)
    {
        if (!config.drawDebris || !CategoryEnabled(config, DebugDrawCategory::Debris))
        {
            return;
        }
        for (const DebrisParticle& particle : debris.particles)
        {
            if (particle.state == DebrisParticleState::Dead)
            {
                continue;
            }
            AddDebugAabb(list, ComputeDebrisParticleBounds(particle), particle.state == DebrisParticleState::Sleeping ? MakeDebugDrawColor(0.35f, 0.30f, 0.25f, 1.0f) : DebugColorDebris(), DebugDrawCategory::Debris, "debris");
            if (LengthSquared(particle.velocity) > 0.0001f)
            {
                AddDebugArrow(list, particle.position, Add(particle.position, Multiply(Normalize(particle.velocity), std::min(0.5f, Length(particle.velocity) * 0.05f))), DebugColorDebris(), DebugDrawCategory::Debris, "debris velocity");
            }
        }
    }

    void CollectExplosionDebugDraw(DebugDrawList& list, const ExplosionDesc& explosion, const ExplosionResult& result, const DebugDrawConfig& config)
    {
        if (!config.drawExplosions || !CategoryEnabled(config, DebugDrawCategory::Explosion))
        {
            return;
        }
        AppendSphereApprox(list, MakeSphere(explosion.center, explosion.radiusMeters), DebugColorExplosion(), DebugDrawCategory::Explosion, "explosion radius");
        for (const ExplosionHit& hit : result.hits)
        {
            if (!hit.affected)
            {
                continue;
            }
            AddDebugLine(list, explosion.center, hit.point, hit.occluded ? MakeDebugDrawColor(0.45f, 0.45f, 0.45f, 1.0f) : DebugColorExplosion(), DebugDrawCategory::Explosion, hit.occluded ? "blast occluded" : "blast ray");
            AddDebugArrow(list, hit.point, Add(hit.point, Multiply(hit.direction, std::min(0.75f, hit.impulseNewtonSeconds * 0.02f))), DebugColorExplosion(), DebugDrawCategory::Explosion, "blast impulse");
        }
    }

    void CollectForceFieldDebugDraw(DebugDrawList& list, const std::vector<ForceFieldDesc>& fields, const std::vector<ForceFieldBodySample>& samples, const DebugDrawConfig& config)
    {
        if (!config.drawForceFields || !CategoryEnabled(config, DebugDrawCategory::ForceField))
        {
            return;
        }
        for (const ForceFieldDesc& field : fields)
        {
            if (!field.enabled)
            {
                continue;
            }
            const std::string label = field.debugName.empty() ? "force field" : field.debugName;
            if (field.volumeKind == ForceFieldVolumeKind::Sphere)
            {
                AppendSphereApprox(list, MakeSphere(field.center, field.radiusMeters), DebugColorForceField(), DebugDrawCategory::ForceField, label);
            }
            else if (field.volumeKind == ForceFieldVolumeKind::Box)
            {
                AddDebugAabb(list, MakeAABB3FromCenterExtents(field.center, field.halfExtents), DebugColorForceField(), DebugDrawCategory::ForceField, label);
            }
            else if (field.volumeKind == ForceFieldVolumeKind::HalfSpace)
            {
                AddDebugArrow(list, field.center, Add(field.center, Multiply(Normalize(field.direction, {0.0f, 1.0f, 0.0f}), 1.0f)), DebugColorForceField(), DebugDrawCategory::ForceField, label);
            }
            else
            {
                AddDebugText(list, field.center, label, DebugColorForceField(), DebugDrawCategory::ForceField);
            }
        }
        for (const ForceFieldBodySample& sample : samples)
        {
            if (!sample.active)
            {
                continue;
            }
            const Vec3 vector = LengthSquared(sample.forceNewton) > 0.0001f ? Normalize(sample.forceNewton) : Normalize(sample.accelerationMetersPerSecondSquared, {0.0f, 1.0f, 0.0f});
            AddDebugArrow(list, sample.point, Add(sample.point, Multiply(vector, 0.45f * std::max(0.1f, sample.weight))), DebugColorForceField(), DebugDrawCategory::ForceField, "field sample");
            if (sample.queuedDamage)
            {
                AddDebugPoint(list, sample.point, 0.04f, DebugColorDamage(), CombineCategory(DebugDrawCategory::ForceField, DebugDrawCategory::Damage), "field damage");
            }
        }
    }

    DebugDrawStats RecomputeDebugDrawStats(const DebugDrawList& list)
    {
        DebugDrawStats stats{};
        stats.commandCount = list.commands.size();
        stats.clippedCommandCount = list.stats.clippedCommandCount;
        stats.finite = list.stats.finite;
        stats.warnings = list.stats.warnings;
        for (const DebugDrawCommand& command : list.commands)
        {
            switch (command.kind)
            {
            case DebugDrawPrimitiveKind::Line: ++stats.lineCount; break;
            case DebugDrawPrimitiveKind::Arrow: ++stats.arrowCount; break;
            case DebugDrawPrimitiveKind::Point: ++stats.pointCount; break;
            case DebugDrawPrimitiveKind::Aabb: ++stats.aabbCount; break;
            case DebugDrawPrimitiveKind::Obb: ++stats.obbCount; break;
            case DebugDrawPrimitiveKind::Sphere: ++stats.sphereCount; break;
            case DebugDrawPrimitiveKind::Capsule: ++stats.capsuleCount; break;
            case DebugDrawPrimitiveKind::Text: ++stats.textCount; break;
            }
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::PhysicsBody) || HasDebugDrawCategory(command.category, DebugDrawCategory::Collider)) ++stats.physicsCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Broadphase)) ++stats.broadphaseCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Contact)) ++stats.contactCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Query)) ++stats.queryCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Projectile)) ++stats.projectileCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Fluid)) ++stats.fluidCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Debris)) ++stats.debrisCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Damage)) ++stats.damageCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::Explosion)) ++stats.explosionCommandCount;
            if (HasDebugDrawCategory(command.category, DebugDrawCategory::ForceField)) ++stats.forceFieldCommandCount;
            if (!IsFinite(command))
            {
                stats.finite = false;
            }
        }
        return stats;
    }

    std::string ToDebugString(const DebugDrawColor& color)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << color.r << ',' << color.g << ',' << color.b << ',' << color.a;
        return out.str();
    }

    std::string ToDebugString(const DebugDrawCommand& command, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "debug_draw kind=" << ToString(command.kind)
            << " category=" << ToString(command.category)
            << " a=" << AK::ToDebugString(command.a, precision)
            << " b=" << AK::ToDebugString(command.b, precision)
            << " radius=" << command.radius
            << " color=" << ToDebugString(command.color);
        if (!command.label.empty())
        {
            out << " label=\"" << command.label << '"';
        }
        return out.str();
    }

    std::string ToDebugString(const DebugDrawStats& stats)
    {
        std::ostringstream out;
        out << "debug_draw_stats commands=" << stats.commandCount
            << " lines=" << stats.lineCount
            << " arrows=" << stats.arrowCount
            << " points=" << stats.pointCount
            << " aabbs=" << stats.aabbCount
            << " obbs=" << stats.obbCount
            << " spheres=" << stats.sphereCount
            << " capsules=" << stats.capsuleCount
            << " text=" << stats.textCount
            << " physics=" << stats.physicsCommandCount
            << " broadphase=" << stats.broadphaseCommandCount
            << " contacts=" << stats.contactCommandCount
            << " queries=" << stats.queryCommandCount
            << " projectile=" << stats.projectileCommandCount
            << " fluid=" << stats.fluidCommandCount
            << " debris=" << stats.debrisCommandCount
            << " explosion=" << stats.explosionCommandCount
            << " forcefield=" << stats.forceFieldCommandCount
            << " clipped=" << stats.clippedCommandCount
            << " finite=" << (stats.finite ? "true" : "false")
            << " warnings=" << stats.warnings.size();
        return out.str();
    }

    std::string BuildDebugDrawProbeSummary()
    {
        return BuildDebugDrawProbe().summary;
    }

    DebugDrawProbeResult BuildDebugDrawProbe()
    {
        DebugDrawProbeResult probe{};
        probe.physics = BuildPhysicsProbe();
        probe.projectile = BuildProjectileProbe();
        probe.fluid = BuildFluidProbe();
        probe.debris = BuildDebrisProbe();
        probe.explosion = BuildExplosionProbe();
        probe.forceField = BuildForceFieldProbe();

        DebugDrawConfig config{};
        config.maxCommands = 12000;
        ResetDebugDrawList(probe.list, config.maxCommands);

        CollectPhysicsDebugDraw(probe.list, probe.physics.scene, probe.physics.pairs, probe.physics.contacts, probe.physics.manifolds, config);
        CollectPhysicsQueryDebugDraw(probe.list,
                                     probe.physics.raycast,
                                     Ray3{{0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
                                     8.0f,
                                     probe.physics.sweep,
                                     {-3.0f, 0.55f, 0.0f},
                                     {3.0f, 0.55f, 0.0f},
                                     0.25f,
                                     config);
        CollectProjectileDebugDraw(probe.list, probe.projectile, config);
        CollectFluidDebugDraw(probe.list, probe.fluid.volumes, probe.fluid.interactions, config);
        CollectDebrisDebugDraw(probe.list, probe.debris.system, config);
        CollectExplosionDebugDraw(probe.list, probe.explosion.explosion, probe.explosion.explosionResult, config);
        CollectForceFieldDebugDraw(probe.list, probe.forceField.fields, probe.forceField.samples, config);

        probe.list.stats = RecomputeDebugDrawStats(probe.list);
        probe.ok = probe.physics.ok && probe.projectile.ok && probe.fluid.ok && probe.debris.ok && probe.explosion.ok && probe.forceField.ok && probe.list.stats.finite && probe.list.commands.size() > 64;

        std::ostringstream out;
        out << "[ ok ] debug draw/runtime visualization foundation"
            << " commands=" << probe.list.stats.commandCount
            << " physics=" << probe.list.stats.physicsCommandCount
            << " broadphase=" << probe.list.stats.broadphaseCommandCount
            << " contacts=" << probe.list.stats.contactCommandCount
            << " queries=" << probe.list.stats.queryCommandCount
            << " projectile=" << probe.list.stats.projectileCommandCount
            << " fluid=" << probe.list.stats.fluidCommandCount
            << " debris=" << probe.list.stats.debrisCommandCount
            << " explosion=" << probe.list.stats.explosionCommandCount
            << " forcefield=" << probe.list.stats.forceFieldCommandCount
            << " clipped=" << probe.list.clipped
            << " finite=" << (probe.list.stats.finite ? "true" : "false")
            << " warnings=" << probe.list.stats.warnings.size();
        if (!probe.ok)
        {
            out.str(std::string{});
            out << "[ fail ] debug draw/runtime visualization foundation commands=" << probe.list.stats.commandCount;
        }
        probe.summary = out.str();
        return probe;
    }
}
