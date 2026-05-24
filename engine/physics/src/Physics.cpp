#include <AK/Physics/Physics.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr float MinimumPhysicsMass = 1.0e-6f;
        constexpr float MinimumContactNormalLength = 1.0e-8f;
        constexpr float MinimumRayLength = 1.0e-6f;
        constexpr float MinimumCcdRadius = 0.01f;
        constexpr float MinimumSatAxisLength = 1.0e-7f;
        constexpr float MaximumSleepTimer = 60.0f;

        struct BroadphaseCellKey
        {
            int x = 0;
            int y = 0;
            int z = 0;

            bool operator==(const BroadphaseCellKey& other) const
            {
                return x == other.x && y == other.y && z == other.z;
            }
        };

        struct BroadphaseCellKeyHash
        {
            std::size_t operator()(const BroadphaseCellKey& key) const
            {
                const std::uint64_t x = static_cast<std::uint64_t>(static_cast<std::int64_t>(key.x) + 0x80000000ll);
                const std::uint64_t y = static_cast<std::uint64_t>(static_cast<std::int64_t>(key.y) + 0x80000000ll);
                const std::uint64_t z = static_cast<std::uint64_t>(static_cast<std::int64_t>(key.z) + 0x80000000ll);
                std::uint64_t h = x * 73856093ull;
                h ^= y * 19349663ull;
                h ^= z * 83492791ull;
                return static_cast<std::size_t>(h);
            }
        };

        std::uint64_t MakePairKey(std::size_t a, std::size_t b)
        {
            const std::uint64_t minValue = static_cast<std::uint64_t>(std::min(a, b));
            const std::uint64_t maxValue = static_cast<std::uint64_t>(std::max(a, b));
            return (minValue << 32u) ^ maxValue;
        }

        void DecodePairKey(std::uint64_t key, std::size_t& a, std::size_t& b)
        {
            a = static_cast<std::size_t>(key >> 32u);
            b = static_cast<std::size_t>(key & 0xffffffffull);
        }

        bool HasAnyLayer(u32 mask, u32 layer)
        {
            return (mask & layer) != 0u;
        }

        float CombineScalar(float a, float b, PhysicsMaterialCombineMode mode)
        {
            switch (mode)
            {
            case PhysicsMaterialCombineMode::Minimum:
                return std::min(a, b);
            case PhysicsMaterialCombineMode::Maximum:
                return std::max(a, b);
            case PhysicsMaterialCombineMode::Multiply:
                return a * b;
            case PhysicsMaterialCombineMode::Average:
            default:
                return 0.5f * (a + b);
            }
        }

        bool ColliderFiltersAllowPair(const PhysicsCollider& a, const PhysicsCollider& b)
        {
            return PhysicsFiltersCanCollide(a.filter, b.filter);
        }

        PhysicsEventKind ContactEventKindFor(bool trigger, bool started)
        {
            if (trigger)
            {
                return started ? PhysicsEventKind::TriggerEntered : PhysicsEventKind::TriggerStayed;
            }
            return started ? PhysicsEventKind::ContactStarted : PhysicsEventKind::ContactStayed;
        }

        PhysicsEventKind EndedEventKindFor(bool trigger)
        {
            return trigger ? PhysicsEventKind::TriggerExited : PhysicsEventKind::ContactEnded;
        }

        void PushContactEvent(PhysicsScene& scene, const PhysicsContact& contact, PhysicsEventKind kind, u32 age)
        {
            if (contact.colliderA >= scene.colliders.size() || contact.colliderB >= scene.colliders.size())
            {
                return;
            }

            PhysicsEvent event{};
            event.kind = kind;
            event.colliderA = contact.colliderA;
            event.colliderB = contact.colliderB;
            event.bodyA = scene.colliders[contact.colliderA].bodyId;
            event.bodyB = scene.colliders[contact.colliderB].bodyId;
            event.point = contact.point;
            event.normal = Normalize(contact.normal, {0.0f, 1.0f, 0.0f});
            event.penetration = contact.penetration;
            event.age = age;
            scene.events.push_back(event);
        }

        void PushBodyEvent(PhysicsScene& scene, u32 bodyId, PhysicsEventKind kind)
        {
            PhysicsEvent event{};
            event.kind = kind;
            event.bodyA = bodyId;
            scene.events.push_back(event);
        }

        Vec3 Negate(Vec3 value)
        {
            return {-value.x, -value.y, -value.z};
        }

        Quat SafeRotation(Quat value)
        {
            if (!IsFinite(Vec3{value.x, value.y, value.z}) || !IsFinite(value.w))
            {
                return QuatIdentity();
            }
            return Normalize(value);
        }

        Vec3 RotateBodyLocalPoint(const PhysicsBody& body, Vec3 localPoint)
        {
            return Add(body.position, Rotate(SafeRotation(body.orientation), localPoint));
        }

        float Component(Vec3 value, int axis)
        {
            return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
        }

        Vec3 WithComponent(Vec3 value, int axis, float component)
        {
            if (axis == 0)
            {
                value.x = component;
            }
            else if (axis == 1)
            {
                value.y = component;
            }
            else
            {
                value.z = component;
            }
            return value;
        }

        Vec3 Clamp(Vec3 value, Vec3 minValue, Vec3 maxValue)
        {
            return {
                std::clamp(value.x, minValue.x, maxValue.x),
                std::clamp(value.y, minValue.y, maxValue.y),
                std::clamp(value.z, minValue.z, maxValue.z)
            };
        }

        Vec3 HalfExtentsVector(const PhysicsObbShape& obb)
        {
            return Max(obb.halfExtents, {0.0f, 0.0f, 0.0f});
        }

        float ProjectObbRadius(const PhysicsObbShape& obb, Vec3 axis)
        {
            const Vec3 e = HalfExtentsVector(obb);
            return std::fabs(Dot(obb.axes[0], axis)) * e.x
                + std::fabs(Dot(obb.axes[1], axis)) * e.y
                + std::fabs(Dot(obb.axes[2], axis)) * e.z;
        }

        Vec3 ClosestPointOnSegment(Vec3 point, Vec3 a, Vec3 b)
        {
            const Vec3 ab = Subtract(b, a);
            const float denom = LengthSquared(ab);
            if (denom <= MinimumContactNormalLength)
            {
                return a;
            }
            const float t = std::clamp(Dot(Subtract(point, a), ab) / denom, 0.0f, 1.0f);
            return Add(a, Multiply(ab, t));
        }

        Vec3 ClosestPointOnObb(const PhysicsObbShape& obb, Vec3 point)
        {
            Vec3 result = obb.center;
            const Vec3 d = Subtract(point, obb.center);
            const Vec3 e = HalfExtentsVector(obb);
            for (int axis = 0; axis < 3; ++axis)
            {
                const float distance = std::clamp(Dot(d, obb.axes[axis]), -Component(e, axis), Component(e, axis));
                result = Add(result, Multiply(obb.axes[axis], distance));
            }
            return result;
        }

        Vec3 ClosestPointOnSegmentToObb(const PhysicsObbShape& obb, Vec3 a, Vec3 b)
        {
            Vec3 best = a;
            float bestSq = std::numeric_limits<float>::max();

            // Small deterministic ternary refinement. It is enough for the current capsule-vs-box foundation
            // and avoids a brittle closed-form segment/OBB distance implementation at this stage.
            float lo = 0.0f;
            float hi = 1.0f;
            for (int iteration = 0; iteration < 10; ++iteration)
            {
                const float m1 = lo + (hi - lo) / 3.0f;
                const float m2 = hi - (hi - lo) / 3.0f;
                const Vec3 p1 = Add(a, Multiply(Subtract(b, a), m1));
                const Vec3 p2 = Add(a, Multiply(Subtract(b, a), m2));
                const float d1 = LengthSquared(Subtract(p1, ClosestPointOnObb(obb, p1)));
                const float d2 = LengthSquared(Subtract(p2, ClosestPointOnObb(obb, p2)));
                if (d1 < d2)
                {
                    hi = m2;
                }
                else
                {
                    lo = m1;
                }
            }

            for (float t : {0.0f, 0.5f * (lo + hi), 1.0f})
            {
                const Vec3 p = Add(a, Multiply(Subtract(b, a), t));
                const float d = LengthSquared(Subtract(p, ClosestPointOnObb(obb, p)));
                if (d < bestSq)
                {
                    bestSq = d;
                    best = p;
                }
            }
            return best;
        }

        void ClosestPointsSegmentSegment(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2, Vec3& outC1, Vec3& outC2)
        {
            const Vec3 d1 = Subtract(q1, p1);
            const Vec3 d2 = Subtract(q2, p2);
            const Vec3 r = Subtract(p1, p2);
            const float a = LengthSquared(d1);
            const float e = LengthSquared(d2);
            const float f = Dot(d2, r);

            float s = 0.0f;
            float t = 0.0f;

            if (a <= MinimumContactNormalLength && e <= MinimumContactNormalLength)
            {
                outC1 = p1;
                outC2 = p2;
                return;
            }

            if (a <= MinimumContactNormalLength)
            {
                s = 0.0f;
                t = std::clamp(f / e, 0.0f, 1.0f);
            }
            else
            {
                const float c = Dot(d1, r);
                if (e <= MinimumContactNormalLength)
                {
                    t = 0.0f;
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                }
                else
                {
                    const float b = Dot(d1, d2);
                    const float denom = a * e - b * b;
                    if (denom != 0.0f)
                    {
                        s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
                    }
                    else
                    {
                        s = 0.0f;
                    }

                    t = (b * s + f) / e;
                    if (t < 0.0f)
                    {
                        t = 0.0f;
                        s = std::clamp(-c / a, 0.0f, 1.0f);
                    }
                    else if (t > 1.0f)
                    {
                        t = 1.0f;
                        s = std::clamp((b - c) / a, 0.0f, 1.0f);
                    }
                }
            }

            outC1 = Add(p1, Multiply(d1, s));
            outC2 = Add(p2, Multiply(d2, t));
        }

        AABB3 BoundsFromObb(const PhysicsObbShape& obb)
        {
            AABB3 bounds = MakeEmptyAABB3();
            const Vec3 e = HalfExtentsVector(obb);
            for (int x = -1; x <= 1; x += 2)
            {
                for (int y = -1; y <= 1; y += 2)
                {
                    for (int z = -1; z <= 1; z += 2)
                    {
                        Vec3 corner = obb.center;
                        corner = Add(corner, Multiply(obb.axes[0], e.x * static_cast<float>(x)));
                        corner = Add(corner, Multiply(obb.axes[1], e.y * static_cast<float>(y)));
                        corner = Add(corner, Multiply(obb.axes[2], e.z * static_cast<float>(z)));
                        bounds = IsValid(bounds) ? Expand(bounds, corner) : MakeAABB3(corner, corner);
                    }
                }
            }
            return bounds;
        }

        AABB3 BoundsFromCapsule(const PhysicsCapsuleShape& capsule)
        {
            const float r = std::max(0.0f, capsule.radius);
            const Vec3 pad{r, r, r};
            return MakeAABB3(Subtract(Min(capsule.a, capsule.b), pad), Add(Max(capsule.a, capsule.b), pad));
        }


        bool RayIntersectsObb(Ray3 ray, const PhysicsObbShape& obb, float* outTMin, float* outTMax, Vec3* outNormal)
        {
            const Vec3 localOrigin{
                Dot(Subtract(ray.origin, obb.center), obb.axes[0]),
                Dot(Subtract(ray.origin, obb.center), obb.axes[1]),
                Dot(Subtract(ray.origin, obb.center), obb.axes[2])
            };
            const Vec3 localDirection{
                Dot(ray.direction, obb.axes[0]),
                Dot(ray.direction, obb.axes[1]),
                Dot(ray.direction, obb.axes[2])
            };
            float tMin = 0.0f;
            float tMax = outTMax ? *outTMax : std::numeric_limits<float>::max();
            int normalAxis = 1;
            float normalSign = 1.0f;
            const Vec3 e = HalfExtentsVector(obb);

            for (int axis = 0; axis < 3; ++axis)
            {
                const float origin = Component(localOrigin, axis);
                const float direction = Component(localDirection, axis);
                const float minValue = -Component(e, axis);
                const float maxValue = Component(e, axis);

                if (std::fabs(direction) < MinimumRayLength)
                {
                    if (origin < minValue || origin > maxValue)
                    {
                        return false;
                    }
                    continue;
                }

                float t1 = (minValue - origin) / direction;
                float t2 = (maxValue - origin) / direction;
                float sign = -1.0f;
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                    sign = 1.0f;
                }

                if (t1 > tMin)
                {
                    tMin = t1;
                    normalAxis = axis;
                    normalSign = sign;
                }
                tMax = std::min(tMax, t2);
                if (tMin > tMax)
                {
                    return false;
                }
            }

            if (outTMin)
            {
                *outTMin = tMin;
            }
            if (outTMax)
            {
                *outTMax = tMax;
            }
            if (outNormal)
            {
                *outNormal = Multiply(obb.axes[normalAxis], normalSign);
            }
            return true;
        }

        bool RayIntersectsCapsuleBounds(Ray3 ray, const PhysicsCapsuleShape& capsule, float maxDistance, float* outT, Vec3* outNormal)
        {
            const AABB3 bounds = BoundsFromCapsule(capsule);
            float tMin = 0.0f;
            float tMax = maxDistance;
            if (!RayIntersectsAABB(ray, bounds, &tMin, &tMax) || tMin < 0.0f || tMin > maxDistance)
            {
                return false;
            }
            const Vec3 point = Add(ray.origin, Multiply(ray.direction, tMin));
            const Vec3 closest = ClosestPointOnSegment(point, capsule.a, capsule.b);
            if (outT)
            {
                *outT = tMin;
            }
            if (outNormal)
            {
                *outNormal = Normalize(Subtract(point, closest), Normalize(Subtract(point, Center(bounds)), {0.0f, 1.0f, 0.0f}));
            }
            return true;
        }

        float ClampNonNegative(float value)
        {
            if (!IsFinite(value) || value < 0.0f)
            {
                return 0.0f;
            }
            return value;
        }

        float ColliderContactOffset(const PhysicsCollider& collider)
        {
            return std::max(0.0f, collider.contactOffset);
        }

        float ColliderRestOffset(const PhysicsCollider& collider)
        {
            return std::clamp(collider.restOffset, 0.0f, ColliderContactOffset(collider));
        }

        float PairContactOffset(const PhysicsCollider& a, const PhysicsCollider& b)
        {
            return ColliderContactOffset(a) + ColliderContactOffset(b);
        }

        float PairRestOffset(const PhysicsCollider& a, const PhysicsCollider& b)
        {
            return ColliderRestOffset(a) + ColliderRestOffset(b);
        }

        float ContactSkinPenetration(float rawPenetration, const PhysicsCollider& a, const PhysicsCollider& b)
        {
            const float predictiveSkin = std::max(0.0f, PairContactOffset(a, b) - PairRestOffset(a, b));
            return std::max(0.0f, rawPenetration + predictiveSkin);
        }

        bool ContactIsSpeculative(float rawPenetration, const PhysicsCollider& a, const PhysicsCollider& b)
        {
            const float skin = PairContactOffset(a, b);
            return rawPenetration <= skin + MinimumRayLength;
        }

        AABB3 ComputeMotionExpandedColliderWorldBounds(const PhysicsScene& scene, const PhysicsBody& body, const PhysicsCollider& collider)
        {
            const float contactMargin = scene.config.enableSpeculativeContacts ? ColliderContactOffset(collider) : 0.0f;
            const float margin = scene.config.broadphaseFatMargin + contactMargin;
            AABB3 bounds = InflateAABB(ComputeColliderWorldBounds(body, collider), margin);

            if (scene.config.enableSpeculativeContacts && IsDynamic(body) && IsFinite(body.previousPosition))
            {
                PhysicsBody previous = body;
                previous.position = body.previousPosition;
                const AABB3 previousBounds = InflateAABB(ComputeColliderWorldBounds(previous, collider), margin);
                if (IsValid(previousBounds))
                {
                    bounds = IsValid(bounds) ? Union(bounds, previousBounds) : previousBounds;
                }

                const Vec3 displacement = Subtract(body.position, body.previousPosition);
                const float speedMargin = Length(displacement) * std::max(0.0f, scene.config.speculativeVelocityMargin);
                if (speedMargin > 0.0f && IsValid(bounds))
                {
                    bounds = InflateAABB(bounds, speedMargin);
                }
            }

            return bounds;
        }

        Vec3 ClampVelocity(Vec3 velocity, float maxLinearVelocity)
        {
            const float maxVelocity = ClampNonNegative(maxLinearVelocity);
            if (maxVelocity <= 0.0f)
            {
                return velocity;
            }

            const float speedSq = LengthSquared(velocity);
            const float maxSq = maxVelocity * maxVelocity;
            if (speedSq <= maxSq || speedSq <= MinimumContactNormalLength)
            {
                return velocity;
            }

            return Multiply(velocity, maxVelocity / std::sqrt(speedSq));
        }

        float EstimateCcdRadiusForBody(const PhysicsScene& scene, u32 bodyId)
        {
            float radius = 0.0f;
            for (const PhysicsCollider& collider : scene.colliders)
            {
                if (!collider.enabled || collider.bodyId != bodyId)
                {
                    continue;
                }

                if (collider.kind == PhysicsColliderKind::Sphere || collider.kind == PhysicsColliderKind::Capsule)
                {
                    radius = std::max(radius, collider.radius);
                }
                else
                {
                    const Vec3 extents = Extents(ComputeColliderLocalBounds(collider));
                    const float minExtent = std::min(extents.x, std::min(extents.y, extents.z));
                    radius = std::max(radius, minExtent);
                }
            }
            return std::max(MinimumCcdRadius, radius);
        }

        bool IsBodyInContact(const PhysicsScene& scene, u32 bodyId, const std::vector<PhysicsContact>& contacts)
        {
            for (const PhysicsContact& contact : contacts)
            {
                if (!contact.valid || contact.colliderA >= scene.colliders.size() || contact.colliderB >= scene.colliders.size())
                {
                    continue;
                }
                if (scene.colliders[contact.colliderA].bodyId == bodyId || scene.colliders[contact.colliderB].bodyId == bodyId)
                {
                    return true;
                }
            }
            return false;
        }

        std::size_t WakeSleepingContactBodies(PhysicsScene& scene, const std::vector<PhysicsContact>& contacts)
        {
            std::size_t woken = 0;
            for (const PhysicsContact& contact : contacts)
            {
                if (!contact.valid || contact.colliderA >= scene.colliders.size() || contact.colliderB >= scene.colliders.size())
                {
                    continue;
                }

                PhysicsBody* bodyA = FindBody(scene, scene.colliders[contact.colliderA].bodyId);
                PhysicsBody* bodyB = FindBody(scene, scene.colliders[contact.colliderB].bodyId);
                if (!bodyA || !bodyB)
                {
                    continue;
                }

                const bool activeA = bodyA->kind == PhysicsBodyKind::Dynamic && !bodyA->sleeping && LengthSquared(bodyA->velocity) > 0.0f;
                const bool activeB = bodyB->kind == PhysicsBodyKind::Dynamic && !bodyB->sleeping && LengthSquared(bodyB->velocity) > 0.0f;
                if (bodyA->kind == PhysicsBodyKind::Dynamic && bodyA->sleeping && activeB)
                {
                    bodyA->sleeping = false;
                    bodyA->sleepTimerSeconds = 0.0f;
                    ++woken;
                }
                if (bodyB->kind == PhysicsBodyKind::Dynamic && bodyB->sleeping && activeA)
                {
                    bodyB->sleeping = false;
                    bodyB->sleepTimerSeconds = 0.0f;
                    ++woken;
                }
            }
            return woken;
        }

        std::size_t UpdateContactCache(PhysicsScene& scene, const std::vector<PhysicsContact>& contacts)
        {
            std::unordered_map<std::uint64_t, PhysicsContactCacheEntry> previous;
            previous.reserve(scene.contactCache.size());
            for (const PhysicsContactCacheEntry& entry : scene.contactCache)
            {
                previous[entry.pairKey] = entry;
            }

            std::unordered_set<std::uint64_t> currentKeys;
            scene.contactCache.clear();
            scene.contactCache.reserve(contacts.size());
            std::size_t persistentCount = 0;

            for (const PhysicsContact& contact : contacts)
            {
                if (!contact.valid)
                {
                    continue;
                }

                const std::uint64_t key = MakePairKey(contact.colliderA, contact.colliderB);
                currentKeys.insert(key);

                PhysicsContactCacheEntry entry{};
                entry.pairKey = key;
                entry.normal = Normalize(contact.normal, {0.0f, 1.0f, 0.0f});
                entry.point = contact.point;
                entry.penetration = contact.penetration;
                entry.touching = true;

                const auto it = previous.find(key);
                const bool persistent = it != previous.end();
                if (persistent)
                {
                    entry.age = it->second.age + 1u;
                    ++persistentCount;
                }
                else
                {
                    entry.age = 1u;
                }

                PushContactEvent(scene, contact, ContactEventKindFor(contact.trigger, !persistent), entry.age);
                scene.contactCache.push_back(entry);
            }

            for (const auto& entry : previous)
            {
                if (currentKeys.find(entry.first) != currentKeys.end())
                {
                    continue;
                }

                std::size_t a = 0;
                std::size_t b = 0;
                DecodePairKey(entry.first, a, b);
                if (a >= scene.colliders.size() || b >= scene.colliders.size())
                {
                    continue;
                }

                PhysicsContact ended{};
                ended.colliderA = a;
                ended.colliderB = b;
                ended.normal = entry.second.normal;
                ended.point = entry.second.point;
                ended.penetration = 0.0f;
                ended.trigger = scene.colliders[a].trigger || scene.colliders[b].trigger || scene.colliders[a].filter.queryOnly || scene.colliders[b].filter.queryOnly;
                ended.valid = true;
                PushContactEvent(scene, ended, EndedEventKindFor(ended.trigger), entry.second.age);
            }

            return persistentCount;
        }

        std::size_t UpdateSleepingPolicy(PhysicsScene& scene, const std::vector<PhysicsContact>& contacts, float deltaSeconds)
        {
            if (!scene.config.enableSleeping)
            {
                return 0;
            }

            std::size_t sleepingCount = 0;
            const float velocityThreshold = std::max(0.0f, scene.config.sleepLinearVelocityThreshold);
            const float velocityThresholdSq = velocityThreshold * velocityThreshold;
            const float timeThreshold = std::max(0.0f, scene.config.sleepTimeThreshold);

            for (PhysicsBody& body : scene.bodies)
            {
                if (body.kind != PhysicsBodyKind::Dynamic || !body.enabled || !body.canSleep)
                {
                    continue;
                }

                const bool wasSleeping = body.sleeping;
                const bool touching = IsBodyInContact(scene, body.id, contacts);
                const bool slow = LengthSquared(body.velocity) <= velocityThresholdSq;
                if (slow && touching)
                {
                    body.sleepTimerSeconds = std::min(MaximumSleepTimer, body.sleepTimerSeconds + std::max(0.0f, deltaSeconds));
                    if (body.sleepTimerSeconds >= timeThreshold)
                    {
                        body.sleeping = true;
                        body.velocity = {};
                    }
                }
                else
                {
                    body.sleeping = false;
                    body.sleepTimerSeconds = 0.0f;
                }

                if (body.sleeping)
                {
                    ++sleepingCount;
                }
                if (!wasSleeping && body.sleeping)
                {
                    PushBodyEvent(scene, body.id, PhysicsEventKind::BodySlept);
                }
                if (wasSleeping && !body.sleeping)
                {
                    PushBodyEvent(scene, body.id, PhysicsEventKind::BodyWoke);
                    body.sleepTimerSeconds = 0.0f;
                }
            }
            return sleepingCount;
        }

        bool IsAabbLike(PhysicsColliderKind kind)
        {
            return kind == PhysicsColliderKind::Box
                || kind == PhysicsColliderKind::ProxyAABB
                || kind == PhysicsColliderKind::Heightfield;
        }

        bool UsesBoundsFallback(PhysicsColliderKind kind)
        {
            return IsAabbLike(kind) || kind == PhysicsColliderKind::Capsule;
        }

        Sphere3 ComputeSphereWorldShape(const PhysicsBody& body, const PhysicsCollider& collider)
        {
            return {RotateBodyLocalPoint(body, collider.localCenter), std::max(0.0f, collider.radius)};
        }

        Vec3 ContactFallbackNormal(const PhysicsBody& bodyA, const PhysicsBody& bodyB)
        {
            return Normalize(Subtract(bodyB.position, bodyA.position), {0.0f, 1.0f, 0.0f});
        }

        PhysicsContact MakeInvalidContact(const PhysicsBroadphasePair& pair)
        {
            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            return contact;
        }

        bool BodyPassesQuery(const PhysicsBody& body, PhysicsQueryFlags flags)
        {
            if (!body.enabled)
            {
                return false;
            }

            if (body.kind == PhysicsBodyKind::Static)
            {
                return HasFlag(flags, PhysicsQueryFlags::IncludeStatic);
            }
            if (body.kind == PhysicsBodyKind::Kinematic)
            {
                return HasFlag(flags, PhysicsQueryFlags::IncludeKinematic);
            }
            return HasFlag(flags, PhysicsQueryFlags::IncludeDynamic);
        }

        bool ColliderPassesQuery(const PhysicsCollider& collider, PhysicsQueryFlags flags)
        {
            return collider.enabled && (!collider.trigger || HasFlag(flags, PhysicsQueryFlags::IncludeTriggers));
        }

        BroadphaseCellKey CellForPoint(Vec3 point, float cellSize)
        {
            const float safeCellSize = std::max(0.01f, cellSize);
            return {
                static_cast<int>(std::floor(point.x / safeCellSize)),
                static_cast<int>(std::floor(point.y / safeCellSize)),
                static_cast<int>(std::floor(point.z / safeCellSize))
            };
        }

        Vec3 EstimateAabbNormal(AABB3 bounds, Vec3 point)
        {
            const Vec3 center = Center(bounds);
            const Vec3 delta = Subtract(point, center);
            const Vec3 extents = Max(Extents(bounds), {MinimumRayLength, MinimumRayLength, MinimumRayLength});
            const Vec3 normalized{delta.x / extents.x, delta.y / extents.y, delta.z / extents.z};

            const float ax = std::fabs(normalized.x);
            const float ay = std::fabs(normalized.y);
            const float az = std::fabs(normalized.z);
            if (ax >= ay && ax >= az)
            {
                return {normalized.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
            }
            if (ay >= ax && ay >= az)
            {
                return {0.0f, normalized.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
            }
            return {0.0f, 0.0f, normalized.z >= 0.0f ? 1.0f : -1.0f};
        }

        PhysicsContact BuildSphereSphereContact(
            const PhysicsBody& bodyA,
            const PhysicsCollider& colliderA,
            const PhysicsBody& bodyB,
            const PhysicsCollider& colliderB,
            const PhysicsBroadphasePair& pair)
        {
            const Sphere3 sphereA = ComputeSphereWorldShape(bodyA, colliderA);
            const Sphere3 sphereB = ComputeSphereWorldShape(bodyB, colliderB);
            const Vec3 delta = Subtract(sphereB.center, sphereA.center);
            const float distanceSq = LengthSquared(delta);
            const float hardRadiusSum = sphereA.radius + sphereB.radius;
            const float radiusSum = hardRadiusSum + PairContactOffset(colliderA, colliderB);
            if (distanceSq > radiusSum * radiusSum)
            {
                return MakeInvalidContact(pair);
            }

            const float distance = std::sqrt(std::max(0.0f, distanceSq));
            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = distance > MinimumContactNormalLength ? Multiply(delta, 1.0f / distance) : ContactFallbackNormal(bodyA, bodyB);
            contact.penetration = ContactSkinPenetration(hardRadiusSum - distance, colliderA, colliderB);
            contact.speculative = ContactIsSpeculative(hardRadiusSum - distance, colliderA, colliderB);
            contact.point = Add(sphereA.center, Multiply(contact.normal, sphereA.radius - contact.penetration * 0.5f));
            contact.trigger = colliderA.trigger || colliderB.trigger;
            contact.valid = contact.penetration >= 0.0f;
            contact.feature = PhysicsContactFeature::SphereSphere;
            return contact;
        }

        PhysicsContact BuildAabbAabbContact(
            const PhysicsBody& bodyA,
            const PhysicsBody& bodyB,
            const PhysicsCollider& colliderA,
            const PhysicsCollider& colliderB,
            const PhysicsBroadphasePair& pair,
            AABB3 boundsA,
            AABB3 boundsB)
        {
            const AABB3 hardBoundsA = boundsA;
            const AABB3 hardBoundsB = boundsB;
            boundsA = InflateAABB(boundsA, ColliderContactOffset(colliderA));
            boundsB = InflateAABB(boundsB, ColliderContactOffset(colliderB));
            if (!Intersects(boundsA, boundsB))
            {
                return MakeInvalidContact(pair);
            }

            const Vec3 overlapMin = Max(boundsA.min, boundsB.min);
            const Vec3 overlapMax = Min(boundsA.max, boundsB.max);
            const Vec3 overlapSize = Subtract(overlapMax, overlapMin);
            const Vec3 centerDelta = Subtract(Center(boundsB), Center(boundsA));

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.penetration = overlapSize.x;
            contact.normal = {centerDelta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};

            if (overlapSize.y < contact.penetration)
            {
                contact.penetration = overlapSize.y;
                contact.normal = {0.0f, centerDelta.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
            }
            if (overlapSize.z < contact.penetration)
            {
                contact.penetration = overlapSize.z;
                contact.normal = {0.0f, 0.0f, centerDelta.z >= 0.0f ? 1.0f : -1.0f};
            }

            const Vec3 hardOverlapMin = Max(hardBoundsA.min, hardBoundsB.min);
            const Vec3 hardOverlapMax = Min(hardBoundsA.max, hardBoundsB.max);
            contact.point = IsValid(MakeAABB3(hardOverlapMin, hardOverlapMax)) ? Center(MakeAABB3(hardOverlapMin, hardOverlapMax)) : Center(MakeAABB3(overlapMin, overlapMax));
            contact.speculative = !Intersects(hardBoundsA, hardBoundsB);
            contact.trigger = colliderA.trigger || colliderB.trigger;
            contact.valid = contact.penetration >= 0.0f && IsFinite(contact.normal);
            contact.feature = PhysicsContactFeature::BoundsFallback;

            if (LengthSquared(contact.normal) <= MinimumContactNormalLength)
            {
                contact.normal = ContactFallbackNormal(bodyA, bodyB);
            }
            return contact;
        }

        PhysicsContact BuildSphereAabbContact(
            const PhysicsBody& sphereBody,
            const PhysicsCollider& sphereCollider,
            const PhysicsBody& boxBody,
            const PhysicsCollider& boxCollider,
            const PhysicsBroadphasePair& pair,
            bool sphereIsA)
        {
            const Sphere3 sphere = ComputeSphereWorldShape(sphereBody, sphereCollider);
            const AABB3 hardBox = ComputeColliderWorldBounds(boxBody, boxCollider);
            const AABB3 box = InflateAABB(hardBox, ColliderContactOffset(boxCollider));
            const Vec3 closest = Clamp(sphere.center, box.min, box.max);
            Vec3 sphereToBox = Subtract(closest, sphere.center);
            float distanceSq = LengthSquared(sphereToBox);
            float rawPenetration = sphere.radius + ColliderContactOffset(sphereCollider) - std::sqrt(std::max(0.0f, distanceSq));
            float penetration = rawPenetration;
            Vec3 normal = distanceSq > MinimumContactNormalLength ? Normalize(sphereToBox, {0.0f, -1.0f, 0.0f}) : Vec3{0.0f, -1.0f, 0.0f};
            Vec3 point = closest;

            if (Contains(box, sphere.center))
            {
                const float toMinX = std::abs(sphere.center.x - box.min.x);
                const float toMaxX = std::abs(box.max.x - sphere.center.x);
                const float toMinY = std::abs(sphere.center.y - box.min.y);
                const float toMaxY = std::abs(box.max.y - sphere.center.y);
                const float toMinZ = std::abs(sphere.center.z - box.min.z);
                const float toMaxZ = std::abs(box.max.z - sphere.center.z);

                float best = toMinX;
                normal = {1.0f, 0.0f, 0.0f};
                point = {box.min.x, sphere.center.y, sphere.center.z};

                if (toMaxX < best)
                {
                    best = toMaxX;
                    normal = {-1.0f, 0.0f, 0.0f};
                    point = {box.max.x, sphere.center.y, sphere.center.z};
                }
                if (toMinY < best)
                {
                    best = toMinY;
                    normal = {0.0f, 1.0f, 0.0f};
                    point = {sphere.center.x, box.min.y, sphere.center.z};
                }
                if (toMaxY < best)
                {
                    best = toMaxY;
                    normal = {0.0f, -1.0f, 0.0f};
                    point = {sphere.center.x, box.max.y, sphere.center.z};
                }
                if (toMinZ < best)
                {
                    best = toMinZ;
                    normal = {0.0f, 0.0f, 1.0f};
                    point = {sphere.center.x, sphere.center.y, box.min.z};
                }
                if (toMaxZ < best)
                {
                    best = toMaxZ;
                    normal = {0.0f, 0.0f, -1.0f};
                    point = {sphere.center.x, sphere.center.y, box.max.z};
                }
                rawPenetration = sphere.radius + best;
                penetration = rawPenetration + PairContactOffset(sphereCollider, boxCollider);
            }

            if (penetration < 0.0f)
            {
                return MakeInvalidContact(pair);
            }

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = sphereIsA ? normal : Negate(normal);
            contact.point = point;
            contact.penetration = std::max(0.0f, penetration);
            contact.speculative = ContactIsSpeculative(rawPenetration, sphereCollider, boxCollider);
            contact.trigger = sphereCollider.trigger || boxCollider.trigger;
            contact.valid = IsFinite(contact.normal) && IsFinite(contact.point);
            contact.feature = PhysicsContactFeature::BoundsFallback;
            return contact;
        }

        PhysicsContact BuildSphereBoxContact(
            const PhysicsBody& sphereBody,
            const PhysicsCollider& sphereCollider,
            const PhysicsBody& boxBody,
            const PhysicsCollider& boxCollider,
            const PhysicsBroadphasePair& pair,
            bool sphereIsA)
        {
            const Sphere3 sphere = ComputeSphereWorldShape(sphereBody, sphereCollider);
            PhysicsObbShape obb = ComputeBoxWorldShape(boxBody, boxCollider);
            obb.halfExtents = Add(obb.halfExtents, {ColliderContactOffset(boxCollider), ColliderContactOffset(boxCollider), ColliderContactOffset(boxCollider)});
            const Vec3 closest = ClosestPointOnObb(obb, sphere.center);
            const Vec3 boxToSphere = Subtract(sphere.center, closest);
            const float distanceSq = LengthSquared(boxToSphere);
            Vec3 normal = distanceSq > MinimumContactNormalLength ? Normalize(boxToSphere, {0.0f, 1.0f, 0.0f}) : ContactFallbackNormal(boxBody, sphereBody);
            float rawPenetration = sphere.radius + ColliderContactOffset(sphereCollider) - std::sqrt(std::max(0.0f, distanceSq));
            float penetration = rawPenetration;
            Vec3 point = closest;

            if (distanceSq <= MinimumContactNormalLength)
            {
                const Vec3 d = Subtract(sphere.center, obb.center);
                Vec3 local{Dot(d, obb.axes[0]), Dot(d, obb.axes[1]), Dot(d, obb.axes[2])};
                const Vec3 e = HalfExtentsVector(obb);
                float bestExit = e.x - std::fabs(local.x);
                int bestAxis = 0;
                if (e.y - std::fabs(local.y) < bestExit)
                {
                    bestExit = e.y - std::fabs(local.y);
                    bestAxis = 1;
                }
                if (e.z - std::fabs(local.z) < bestExit)
                {
                    bestExit = e.z - std::fabs(local.z);
                    bestAxis = 2;
                }
                normal = obb.axes[bestAxis];
                if (Component(local, bestAxis) < 0.0f)
                {
                    normal = Negate(normal);
                }
                penetration = sphere.radius + std::max(0.0f, bestExit);
                Vec3 faceLocal = local;
                faceLocal = WithComponent(faceLocal, bestAxis, Component(e, bestAxis) * (Component(local, bestAxis) < 0.0f ? -1.0f : 1.0f));
                point = obb.center;
                point = Add(point, Multiply(obb.axes[0], faceLocal.x));
                point = Add(point, Multiply(obb.axes[1], faceLocal.y));
                point = Add(point, Multiply(obb.axes[2], faceLocal.z));
            }

            if (penetration < 0.0f)
            {
                return MakeInvalidContact(pair);
            }

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = sphereIsA ? Negate(normal) : normal;
            contact.point = point;
            contact.penetration = std::max(0.0f, penetration);
            contact.speculative = ContactIsSpeculative(rawPenetration, sphereCollider, boxCollider);
            contact.trigger = sphereCollider.trigger || boxCollider.trigger;
            contact.valid = IsFinite(contact.normal) && IsFinite(contact.point);
            contact.feature = PhysicsContactFeature::SphereBox;
            return contact;
        }

        PhysicsContact BuildBoxBoxContact(
            const PhysicsBody& bodyA,
            const PhysicsBody& bodyB,
            const PhysicsCollider& colliderA,
            const PhysicsCollider& colliderB,
            const PhysicsBroadphasePair& pair)
        {
            const PhysicsObbShape a = ComputeBoxWorldShape(bodyA, colliderA);
            const PhysicsObbShape b = ComputeBoxWorldShape(bodyB, colliderB);
            const Vec3 centerDelta = Subtract(b.center, a.center);

            float bestOverlap = std::numeric_limits<float>::max();
            Vec3 bestAxis{0.0f, 1.0f, 0.0f};

            auto testAxis = [&](Vec3 rawAxis) -> bool
            {
                if (LengthSquared(rawAxis) <= MinimumSatAxisLength)
                {
                    return true;
                }
                Vec3 axis = Normalize(rawAxis, {0.0f, 1.0f, 0.0f});
                const float centerDistance = std::fabs(Dot(centerDelta, axis));
                const float radius = ProjectObbRadius(a, axis) + ProjectObbRadius(b, axis) + PairContactOffset(colliderA, colliderB);
                const float overlap = radius - centerDistance;
                if (overlap < 0.0f)
                {
                    return false;
                }
                if (overlap < bestOverlap)
                {
                    bestOverlap = overlap;
                    bestAxis = Dot(centerDelta, axis) >= 0.0f ? axis : Negate(axis);
                }
                return true;
            };

            for (int i = 0; i < 3; ++i)
            {
                if (!testAxis(a.axes[i]) || !testAxis(b.axes[i]))
                {
                    return MakeInvalidContact(pair);
                }
            }
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                {
                    if (!testAxis(Cross(a.axes[i], b.axes[j])))
                    {
                        return MakeInvalidContact(pair);
                    }
                }
            }

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = bestAxis;
            contact.penetration = std::max(0.0f, bestOverlap);
            contact.speculative = bestOverlap <= PairContactOffset(colliderA, colliderB);
            contact.point = Add(a.center, Multiply(bestAxis, ProjectObbRadius(a, bestAxis) - contact.penetration * 0.5f));
            contact.trigger = colliderA.trigger || colliderB.trigger;
            contact.valid = IsFinite(contact.normal) && IsFinite(contact.point);
            contact.feature = PhysicsContactFeature::BoxBox;
            return contact;
        }

        PhysicsContact BuildCapsuleSphereContact(
            const PhysicsBody& capsuleBody,
            const PhysicsCollider& capsuleCollider,
            const PhysicsBody& sphereBody,
            const PhysicsCollider& sphereCollider,
            const PhysicsBroadphasePair& pair,
            bool capsuleIsA)
        {
            const PhysicsCapsuleShape capsule = ComputeCapsuleWorldShape(capsuleBody, capsuleCollider);
            const Sphere3 sphere = ComputeSphereWorldShape(sphereBody, sphereCollider);
            const Vec3 closest = ClosestPointOnSegment(sphere.center, capsule.a, capsule.b);
            const Vec3 delta = Subtract(sphere.center, closest);
            const float distanceSq = LengthSquared(delta);
            const float hardRadiusSum = capsule.radius + sphere.radius;
            const float radiusSum = hardRadiusSum + PairContactOffset(capsuleCollider, sphereCollider);
            if (distanceSq > radiusSum * radiusSum)
            {
                return MakeInvalidContact(pair);
            }
            const float distance = std::sqrt(std::max(0.0f, distanceSq));
            Vec3 normal = distance > MinimumContactNormalLength ? Multiply(delta, 1.0f / distance) : ContactFallbackNormal(capsuleBody, sphereBody);

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = capsuleIsA ? normal : Negate(normal);
            contact.penetration = ContactSkinPenetration(hardRadiusSum - distance, capsuleCollider, sphereCollider);
            contact.speculative = ContactIsSpeculative(hardRadiusSum - distance, capsuleCollider, sphereCollider);
            contact.point = Add(closest, Multiply(normal, capsule.radius - contact.penetration * 0.5f));
            contact.trigger = capsuleCollider.trigger || sphereCollider.trigger;
            contact.valid = IsFinite(contact.normal) && IsFinite(contact.point);
            contact.feature = PhysicsContactFeature::CapsuleSphere;
            return contact;
        }

        PhysicsContact BuildCapsuleCapsuleContact(
            const PhysicsBody& bodyA,
            const PhysicsCollider& colliderA,
            const PhysicsBody& bodyB,
            const PhysicsCollider& colliderB,
            const PhysicsBroadphasePair& pair)
        {
            const PhysicsCapsuleShape a = ComputeCapsuleWorldShape(bodyA, colliderA);
            const PhysicsCapsuleShape b = ComputeCapsuleWorldShape(bodyB, colliderB);
            Vec3 closestA{};
            Vec3 closestB{};
            ClosestPointsSegmentSegment(a.a, a.b, b.a, b.b, closestA, closestB);
            const Vec3 delta = Subtract(closestB, closestA);
            const float distanceSq = LengthSquared(delta);
            const float hardRadiusSum = a.radius + b.radius;
            const float radiusSum = hardRadiusSum + PairContactOffset(colliderA, colliderB);
            if (distanceSq > radiusSum * radiusSum)
            {
                return MakeInvalidContact(pair);
            }
            const float distance = std::sqrt(std::max(0.0f, distanceSq));
            const Vec3 normal = distance > MinimumContactNormalLength ? Multiply(delta, 1.0f / distance) : ContactFallbackNormal(bodyA, bodyB);

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = normal;
            contact.penetration = ContactSkinPenetration(hardRadiusSum - distance, colliderA, colliderB);
            contact.speculative = ContactIsSpeculative(hardRadiusSum - distance, colliderA, colliderB);
            contact.point = Add(closestA, Multiply(normal, a.radius - contact.penetration * 0.5f));
            contact.trigger = colliderA.trigger || colliderB.trigger;
            contact.valid = IsFinite(contact.normal) && IsFinite(contact.point);
            contact.feature = PhysicsContactFeature::CapsuleCapsule;
            return contact;
        }

        PhysicsContact BuildCapsuleBoxContact(
            const PhysicsBody& capsuleBody,
            const PhysicsCollider& capsuleCollider,
            const PhysicsBody& boxBody,
            const PhysicsCollider& boxCollider,
            const PhysicsBroadphasePair& pair,
            bool capsuleIsA)
        {
            const PhysicsCapsuleShape capsule = ComputeCapsuleWorldShape(capsuleBody, capsuleCollider);
            PhysicsObbShape obb = ComputeBoxWorldShape(boxBody, boxCollider);
            obb.halfExtents = Add(obb.halfExtents, {ColliderContactOffset(boxCollider), ColliderContactOffset(boxCollider), ColliderContactOffset(boxCollider)});
            const Vec3 capsulePoint = ClosestPointOnSegmentToObb(obb, capsule.a, capsule.b);
            const Vec3 boxPoint = ClosestPointOnObb(obb, capsulePoint);
            const Vec3 delta = Subtract(capsulePoint, boxPoint);
            const float distanceSq = LengthSquared(delta);
            const float effectiveRadius = capsule.radius + ColliderContactOffset(capsuleCollider);
            if (distanceSq > effectiveRadius * effectiveRadius)
            {
                return MakeInvalidContact(pair);
            }
            const float distance = std::sqrt(std::max(0.0f, distanceSq));
            Vec3 normal = distance > MinimumContactNormalLength ? Normalize(delta, {0.0f, 1.0f, 0.0f}) : ContactFallbackNormal(boxBody, capsuleBody);

            PhysicsContact contact{};
            contact.colliderA = pair.colliderA;
            contact.colliderB = pair.colliderB;
            contact.normal = capsuleIsA ? Negate(normal) : normal;
            const float rawPenetration = capsule.radius - distance;
            contact.penetration = ContactSkinPenetration(rawPenetration, capsuleCollider, boxCollider);
            contact.speculative = ContactIsSpeculative(rawPenetration, capsuleCollider, boxCollider);
            contact.point = boxPoint;
            contact.trigger = capsuleCollider.trigger || boxCollider.trigger;
            contact.valid = IsFinite(contact.normal) && IsFinite(contact.point);
            contact.feature = PhysicsContactFeature::CapsuleBox;
            return contact;
        }

        void IntegrateBodyWithCcd(PhysicsScene& scene, PhysicsBody& body, const PhysicsWorldConfig& config, float deltaSeconds, PhysicsStepStats& stats)
        {
            if (!IsDynamic(body) || deltaSeconds <= 0.0f || !IsFinite(deltaSeconds))
            {
                body.accumulatedForce = {};
                return;
            }

            body.previousPosition = body.position;

            Vec3 acceleration = Multiply(body.accumulatedForce, body.inverseMass);
            if (config.enableGravity)
            {
                acceleration = Add(acceleration, Multiply(config.gravity, body.gravityScale));
            }

            body.velocity = Add(body.velocity, Multiply(acceleration, deltaSeconds));

            const float damping = std::clamp(1.0f - std::max(0.0f, body.linearDamping) * deltaSeconds, 0.0f, 1.0f);
            body.velocity = Multiply(body.velocity, damping);
            body.velocity = ClampVelocity(body.velocity, config.maxLinearVelocity);

            const Vec3 start = body.position;
            const Vec3 displacement = Multiply(body.velocity, deltaSeconds);
            const float distance = Length(displacement);
            bool consumedByCcd = false;

            if (config.enableContinuousCollision && body.continuousCollision && distance > std::max(0.0f, config.ccdMinVelocity) * deltaSeconds)
            {
                const float radius = EstimateCcdRadiusForBody(scene, body.id) * std::max(0.01f, config.ccdRadiusScale);
                ++stats.ccdSweepCount;
                const PhysicsSweepHit hit = SweepSpherePhysicsSceneExcludingBody(scene, start, Add(start, displacement), radius, body.id);
                if (hit.hit && hit.distance <= distance)
                {
                    ++stats.ccdHitCount;
                    const Vec3 direction = Normalize(displacement, {0.0f, 0.0f, 1.0f});
                    body.position = Add(start, Multiply(direction, std::max(0.0f, hit.distance - config.contactSlop)));
                    const float velocityIntoSurface = Dot(body.velocity, hit.normal);
                    if (velocityIntoSurface < 0.0f)
                    {
                        body.velocity = Subtract(body.velocity, Multiply(hit.normal, velocityIntoSurface));
                    }
                    consumedByCcd = true;
                }
            }

            if (!consumedByCcd)
            {
                body.position = Add(start, displacement);
            }

            body.accumulatedForce = {};

            if (!IsFinite(body))
            {
                body.position = {};
                body.velocity = {};
                body.accumulatedForce = {};
                body.sleeping = true;
            }
        }
    }

    PhysicsQueryFlags operator|(PhysicsQueryFlags a, PhysicsQueryFlags b)
    {
        return static_cast<PhysicsQueryFlags>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    bool HasFlag(PhysicsQueryFlags flags, PhysicsQueryFlags flag)
    {
        return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0u;
    }

    const char* ToString(PhysicsBodyKind kind)
    {
        switch (kind)
        {
        case PhysicsBodyKind::Static:
            return "Static";
        case PhysicsBodyKind::Kinematic:
            return "Kinematic";
        case PhysicsBodyKind::Dynamic:
            return "Dynamic";
        default:
            return "Unknown";
        }
    }

    const char* ToString(PhysicsColliderKind kind)
    {
        switch (kind)
        {
        case PhysicsColliderKind::Sphere:
            return "Sphere";
        case PhysicsColliderKind::Box:
            return "Box";
        case PhysicsColliderKind::ProxyAABB:
            return "ProxyAABB";
        case PhysicsColliderKind::Capsule:
            return "Capsule";
        case PhysicsColliderKind::Heightfield:
            return "Heightfield";
        default:
            return "Unknown";
        }
    }

    const char* ToString(PhysicsConstraintKind kind)
    {
        switch (kind)
        {
        case PhysicsConstraintKind::Distance:
            return "Distance";
        default:
            return "Unknown";
        }
    }


    const char* ToString(PhysicsContactFeature feature)
    {
        switch (feature)
        {
        case PhysicsContactFeature::None:
            return "None";
        case PhysicsContactFeature::SphereSphere:
            return "SphereSphere";
        case PhysicsContactFeature::SphereBox:
            return "SphereBox";
        case PhysicsContactFeature::BoxBox:
            return "BoxBox";
        case PhysicsContactFeature::CapsuleSphere:
            return "CapsuleSphere";
        case PhysicsContactFeature::CapsuleCapsule:
            return "CapsuleCapsule";
        case PhysicsContactFeature::CapsuleBox:
            return "CapsuleBox";
        case PhysicsContactFeature::BoundsFallback:
            return "BoundsFallback";
        default:
            return "Unknown";
        }
    }

    const char* ToString(PhysicsMaterialCombineMode mode)
    {
        switch (mode)
        {
        case PhysicsMaterialCombineMode::Average:
            return "Average";
        case PhysicsMaterialCombineMode::Minimum:
            return "Minimum";
        case PhysicsMaterialCombineMode::Maximum:
            return "Maximum";
        case PhysicsMaterialCombineMode::Multiply:
            return "Multiply";
        default:
            return "Unknown";
        }
    }

    const char* ToString(PhysicsEventKind kind)
    {
        switch (kind)
        {
        case PhysicsEventKind::ContactStarted:
            return "ContactStarted";
        case PhysicsEventKind::ContactStayed:
            return "ContactStayed";
        case PhysicsEventKind::ContactEnded:
            return "ContactEnded";
        case PhysicsEventKind::TriggerEntered:
            return "TriggerEntered";
        case PhysicsEventKind::TriggerStayed:
            return "TriggerStayed";
        case PhysicsEventKind::TriggerExited:
            return "TriggerExited";
        case PhysicsEventKind::BodySlept:
            return "BodySlept";
        case PhysicsEventKind::BodyWoke:
            return "BodyWoke";
        default:
            return "Unknown";
        }
    }

    PhysicsBody MakeStaticBody(u32 id, Vec3 position)
    {
        PhysicsBody body{};
        body.id = id;
        body.kind = PhysicsBodyKind::Static;
        body.position = position;
        body.previousPosition = position;
        body.inverseMass = 0.0f;
        body.massKilograms = 0.0f;
        return body;
    }

    PhysicsBody MakeKinematicBody(u32 id, Vec3 position)
    {
        PhysicsBody body{};
        body.id = id;
        body.kind = PhysicsBodyKind::Kinematic;
        body.position = position;
        body.previousPosition = position;
        body.inverseMass = 0.0f;
        body.massKilograms = 0.0f;
        return body;
    }

    PhysicsBody MakeDynamicBody(u32 id, Vec3 position, float massKilograms)
    {
        PhysicsBody body{};
        body.id = id;
        body.kind = PhysicsBodyKind::Dynamic;
        body.position = position;
        body.previousPosition = position;
        body.massKilograms = std::max(MinimumPhysicsMass, massKilograms);
        body.inverseMass = 1.0f / body.massKilograms;
        return body;
    }

    PhysicsBody MakePhysicsBody(const RigidBodyComponent& component, Vec3 position)
    {
        PhysicsBody body{};
        if (component.kind == PhysicsBodyKind::Dynamic)
        {
            body = MakeDynamicBody(component.bodyId, position, component.massKilograms);
        }
        else if (component.kind == PhysicsBodyKind::Kinematic)
        {
            body = MakeKinematicBody(component.bodyId, position);
        }
        else
        {
            body = MakeStaticBody(component.bodyId, position);
        }

        body.linearDamping = std::max(0.0f, component.linearDamping);
        body.gravityScale = component.gravityScale;
        body.enabled = component.enabled;
        body.canSleep = component.canSleep;
        body.continuousCollision = component.continuousCollision;
        return body;
    }

    PhysicsCollider MakeSphereCollider(u32 bodyId, float radius, Vec3 localCenter)
    {
        PhysicsCollider collider{};
        collider.bodyId = bodyId;
        collider.kind = PhysicsColliderKind::Sphere;
        collider.localCenter = localCenter;
        collider.radius = std::max(0.0f, radius);
        collider.localBounds = MakeAABB3FromCenterExtents(localCenter, {collider.radius, collider.radius, collider.radius});
        collider.debugName = "sphere";
        return collider;
    }

    PhysicsCollider MakeBoxCollider(u32 bodyId, Vec3 halfExtents, Vec3 localCenter)
    {
        PhysicsCollider collider{};
        collider.bodyId = bodyId;
        collider.kind = PhysicsColliderKind::Box;
        collider.localCenter = localCenter;
        collider.halfExtents = Max(halfExtents, {0.0f, 0.0f, 0.0f});
        collider.localBounds = MakeAABB3FromCenterExtents(localCenter, collider.halfExtents);
        collider.debugName = "box";
        return collider;
    }

    PhysicsCollider MakeCapsuleCollider(u32 bodyId, float radius, float halfHeight, Vec3 localCenter)
    {
        PhysicsCollider collider{};
        collider.bodyId = bodyId;
        collider.kind = PhysicsColliderKind::Capsule;
        collider.localCenter = localCenter;
        collider.radius = std::max(0.0f, radius);
        collider.halfHeight = std::max(0.0f, halfHeight);
        collider.halfExtents = {collider.radius, collider.halfHeight + collider.radius, collider.radius};
        collider.localBounds = MakeAABB3FromCenterExtents(localCenter, collider.halfExtents);
        collider.debugName = "capsule_y";
        return collider;
    }

    PhysicsCollider MakeHeightfieldCollider(u32 bodyId, AABB3 localBounds, Vec3 localCenter)
    {
        PhysicsCollider collider{};
        collider.bodyId = bodyId;
        collider.kind = PhysicsColliderKind::Heightfield;
        collider.localBounds = IsValid(localBounds) ? localBounds : MakeAABB3FromCenterExtents(localCenter, {0.5f, 0.5f, 0.5f});
        collider.localCenter = localCenter;
        collider.halfExtents = Extents(collider.localBounds);
        collider.debugName = "heightfield_proxy";
        return collider;
    }

    PhysicsCollider MakeProxyAabbCollider(u32 bodyId, AABB3 localBounds, std::string debugName)
    {
        PhysicsCollider collider{};
        collider.bodyId = bodyId;
        collider.kind = PhysicsColliderKind::ProxyAABB;
        collider.localBounds = IsValid(localBounds) ? localBounds : MakeAABB3FromCenterExtents({}, {0.5f, 0.5f, 0.5f});
        collider.localCenter = Center(collider.localBounds);
        collider.halfExtents = Extents(collider.localBounds);
        collider.debugName = std::move(debugName);
        if (collider.debugName.empty())
        {
            collider.debugName = "proxy_aabb";
        }
        return collider;
    }

    PhysicsCollider MakePhysicsCollider(const ColliderComponent& component)
    {
        PhysicsCollider collider{};
        if (component.kind == PhysicsColliderKind::Sphere)
        {
            collider = MakeSphereCollider(component.bodyId, component.radius, component.localCenter);
        }
        else if (component.kind == PhysicsColliderKind::Capsule)
        {
            collider = MakeCapsuleCollider(component.bodyId, component.radius, component.halfHeight, component.localCenter);
        }
        else if (component.kind == PhysicsColliderKind::Heightfield)
        {
            collider = MakeHeightfieldCollider(component.bodyId, MakeAABB3FromCenterExtents(component.localCenter, component.halfExtents), component.localCenter);
        }
        else
        {
            collider = MakeBoxCollider(component.bodyId, component.halfExtents, component.localCenter);
            collider.kind = component.kind == PhysicsColliderKind::ProxyAABB ? PhysicsColliderKind::ProxyAABB : PhysicsColliderKind::Box;
        }
        collider.enabled = component.enabled;
        collider.trigger = component.trigger;
        collider.localRotation = component.localRotation;
        collider.contactOffset = std::max(0.0f, component.contactOffset);
        collider.restOffset = std::clamp(component.restOffset, 0.0f, collider.contactOffset);
        collider.filter = component.filter;
        collider.material = SanitizePhysicsMaterial(component.material);
        return collider;
    }

    PhysicsConstraint MakeDistanceConstraint(u32 bodyA, u32 bodyB, float restDistance, Vec3 localAnchorA, Vec3 localAnchorB)
    {
        PhysicsConstraint constraint{};
        constraint.kind = PhysicsConstraintKind::Distance;
        constraint.distance.bodyA = bodyA;
        constraint.distance.bodyB = bodyB;
        constraint.distance.localAnchorA = localAnchorA;
        constraint.distance.localAnchorB = localAnchorB;
        constraint.distance.restDistance = std::max(0.0f, restDistance);
        constraint.distance.minDistance = constraint.distance.restDistance;
        constraint.distance.maxDistance = constraint.distance.restDistance;
        constraint.distance.stiffness = 1.0f;
        constraint.distance.damping = 0.0f;
        constraint.distance.enabled = true;
        return constraint;
    }

    PhysicsMaterialDesc MakePhysicsMaterial(float staticFriction, float dynamicFriction, float restitution, float densityKgPerCubicMeter)
    {
        PhysicsMaterialDesc material{};
        material.staticFriction = staticFriction;
        material.dynamicFriction = dynamicFriction;
        material.restitution = restitution;
        material.densityKgPerCubicMeter = densityKgPerCubicMeter;
        return SanitizePhysicsMaterial(material);
    }

    PhysicsMaterialDesc SanitizePhysicsMaterial(PhysicsMaterialDesc material)
    {
        material.staticFriction = std::clamp(material.staticFriction, 0.0f, 8.0f);
        material.dynamicFriction = std::clamp(material.dynamicFriction, 0.0f, 8.0f);
        material.restitution = std::clamp(material.restitution, 0.0f, 1.0f);
        material.rollingResistance = std::clamp(material.rollingResistance, 0.0f, 8.0f);
        material.densityKgPerCubicMeter = std::clamp(material.densityKgPerCubicMeter, 0.0f, 100000.0f);
        material.hardness = std::clamp(material.hardness, 0.0f, 1000.0f);
        material.valid = IsFinite(material.staticFriction)
            && IsFinite(material.dynamicFriction)
            && IsFinite(material.restitution)
            && IsFinite(material.rollingResistance)
            && IsFinite(material.densityKgPerCubicMeter)
            && IsFinite(material.hardness);
        if (!material.valid)
        {
            material = PhysicsMaterialDesc{};
        }
        return material;
    }

    PhysicsContactMaterial CombinePhysicsMaterials(const PhysicsMaterialDesc& materialA, const PhysicsMaterialDesc& materialB)
    {
        const PhysicsMaterialDesc a = SanitizePhysicsMaterial(materialA);
        const PhysicsMaterialDesc b = SanitizePhysicsMaterial(materialB);

        PhysicsContactMaterial combined{};
        combined.staticFriction = std::clamp(CombineScalar(a.staticFriction, b.staticFriction, a.frictionCombine), 0.0f, 8.0f);
        combined.dynamicFriction = std::clamp(CombineScalar(a.dynamicFriction, b.dynamicFriction, a.frictionCombine), 0.0f, combined.staticFriction);
        combined.restitution = std::clamp(CombineScalar(a.restitution, b.restitution, a.restitutionCombine), 0.0f, 1.0f);
        combined.rollingResistance = std::clamp(0.5f * (a.rollingResistance + b.rollingResistance), 0.0f, 8.0f);
        combined.hardness = std::max(0.0f, 0.5f * (a.hardness + b.hardness));
        combined.valid = a.valid && b.valid;
        return combined;
    }

    bool PhysicsFiltersCanCollide(const PhysicsCollisionFilter& a, const PhysicsCollisionFilter& b)
    {
        if (a.layerMask == 0u || b.layerMask == 0u)
        {
            return false;
        }
        return HasAnyLayer(a.collidesWithMask, b.layerMask) && HasAnyLayer(b.collidesWithMask, a.layerMask);
    }

    std::vector<PhysicsCollider> MakeProxyCollidersFromCsg(u32 bodyId, const std::vector<CsgCollisionProxy>& proxies, std::size_t maxProxyCount)
    {
        std::vector<PhysicsCollider> colliders;
        colliders.reserve(std::min(maxProxyCount, proxies.size()));

        for (std::size_t index = 0; index < proxies.size() && colliders.size() < maxProxyCount; ++index)
        {
            const CsgCollisionProxy& proxy = proxies[index];
            if (!IsValid(proxy.bounds) || proxy.voxelCount == 0)
            {
                continue;
            }
            colliders.push_back(MakeProxyAabbCollider(bodyId, proxy.bounds, "csg_proxy_" + std::to_string(index)));
        }

        return colliders;
    }

    PhysicsBody* FindBody(PhysicsScene& scene, u32 bodyId)
    {
        const auto it = std::find_if(scene.bodies.begin(), scene.bodies.end(), [bodyId](const PhysicsBody& body)
        {
            return body.id == bodyId;
        });
        return it == scene.bodies.end() ? nullptr : &(*it);
    }

    const PhysicsBody* FindBody(const PhysicsScene& scene, u32 bodyId)
    {
        const auto it = std::find_if(scene.bodies.begin(), scene.bodies.end(), [bodyId](const PhysicsBody& body)
        {
            return body.id == bodyId;
        });
        return it == scene.bodies.end() ? nullptr : &(*it);
    }

    bool IsDynamic(const PhysicsBody& body)
    {
        return body.enabled && !body.sleeping && body.kind == PhysicsBodyKind::Dynamic && body.inverseMass > 0.0f;
    }

    bool IsFinite(const PhysicsBody& body)
    {
        return IsFinite(body.position)
            && IsFinite(Vec3{body.orientation.x, body.orientation.y, body.orientation.z})
            && IsFinite(body.orientation.w)
            && IsFinite(body.velocity)
            && IsFinite(body.accumulatedForce)
            && IsFinite(body.massKilograms)
            && IsFinite(body.inverseMass)
            && IsFinite(body.linearDamping)
            && IsFinite(body.gravityScale)
            && IsFinite(body.restitution)
            && IsFinite(body.friction);
    }

    bool IsFinite(const PhysicsCollider& collider)
    {
        return IsFinite(collider.localCenter)
            && IsFinite(Vec3{collider.localRotation.x, collider.localRotation.y, collider.localRotation.z})
            && IsFinite(collider.localRotation.w)
            && IsFinite(collider.halfExtents)
            && IsFinite(collider.radius)
            && IsFinite(collider.halfHeight)
            && IsValid(collider.localBounds);
    }

    AABB3 ComputeColliderLocalBounds(const PhysicsCollider& collider)
    {
        if (collider.kind == PhysicsColliderKind::Sphere)
        {
            const float radius = std::max(0.0f, collider.radius);
            return MakeAABB3FromCenterExtents(collider.localCenter, {radius, radius, radius});
        }
        if (collider.kind == PhysicsColliderKind::Box)
        {
            return MakeAABB3FromCenterExtents(collider.localCenter, Max(collider.halfExtents, {0.0f, 0.0f, 0.0f}));
        }
        if (collider.kind == PhysicsColliderKind::Capsule)
        {
            const Vec3 extents{std::max(0.0f, collider.radius), std::max(0.0f, collider.halfHeight) + std::max(0.0f, collider.radius), std::max(0.0f, collider.radius)};
            return MakeAABB3FromCenterExtents(collider.localCenter, extents);
        }
        return IsValid(collider.localBounds) ? collider.localBounds : MakeEmptyAABB3();
    }

    PhysicsObbShape ComputeBoxWorldShape(const PhysicsBody& body, const PhysicsCollider& collider)
    {
        PhysicsObbShape obb{};
        const Quat bodyRotation = SafeRotation(body.orientation);
        const Quat colliderRotation = SafeRotation(collider.localRotation);
        const Quat worldRotation = SafeRotation(Multiply(bodyRotation, colliderRotation));
        obb.center = RotateBodyLocalPoint(body, collider.localCenter);
        obb.axes[0] = Normalize(Rotate(worldRotation, {1.0f, 0.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
        obb.axes[1] = Normalize(Rotate(worldRotation, {0.0f, 1.0f, 0.0f}), {0.0f, 1.0f, 0.0f});
        obb.axes[2] = Normalize(Rotate(worldRotation, {0.0f, 0.0f, 1.0f}), {0.0f, 0.0f, 1.0f});
        obb.halfExtents = Max(collider.halfExtents, {0.0f, 0.0f, 0.0f});
        return obb;
    }

    PhysicsCapsuleShape ComputeCapsuleWorldShape(const PhysicsBody& body, const PhysicsCollider& collider)
    {
        PhysicsCapsuleShape capsule{};
        const Quat worldRotation = SafeRotation(Multiply(SafeRotation(body.orientation), SafeRotation(collider.localRotation)));
        const Vec3 center = RotateBodyLocalPoint(body, collider.localCenter);
        const Vec3 axis = Normalize(Rotate(worldRotation, {0.0f, 1.0f, 0.0f}), {0.0f, 1.0f, 0.0f});
        const float halfHeight = std::max(0.0f, collider.halfHeight);
        capsule.a = Subtract(center, Multiply(axis, halfHeight));
        capsule.b = Add(center, Multiply(axis, halfHeight));
        capsule.radius = std::max(0.0f, collider.radius);
        return capsule;
    }

    AABB3 ComputeColliderWorldBounds(const PhysicsBody& body, const PhysicsCollider& collider)
    {
        if (collider.kind == PhysicsColliderKind::Sphere)
        {
            const Vec3 center = RotateBodyLocalPoint(body, collider.localCenter);
            const float radius = std::max(0.0f, collider.radius);
            return MakeAABB3FromCenterExtents(center, {radius, radius, radius});
        }
        if (collider.kind == PhysicsColliderKind::Box)
        {
            return BoundsFromObb(ComputeBoxWorldShape(body, collider));
        }
        if (collider.kind == PhysicsColliderKind::Capsule)
        {
            return BoundsFromCapsule(ComputeCapsuleWorldShape(body, collider));
        }

        const AABB3 local = ComputeColliderLocalBounds(collider);
        if (!IsValid(local))
        {
            return MakeEmptyAABB3();
        }

        const Vec3 minCorner = RotateBodyLocalPoint(body, local.min);
        const Vec3 maxCorner = RotateBodyLocalPoint(body, local.max);
        return MakeAABB3(Min(minCorner, maxCorner), Max(minCorner, maxCorner));
    }

    AABB3 InflateAABB(AABB3 bounds, float margin)
    {
        if (!IsValid(bounds))
        {
            return bounds;
        }

        const float safeMargin = std::max(0.0f, margin);
        const Vec3 pad{safeMargin, safeMargin, safeMargin};
        return MakeAABB3(Subtract(bounds.min, pad), Add(bounds.max, pad));
    }

    std::vector<PhysicsBroadphasePair> BuildBroadphasePairs(const PhysicsScene& scene)
    {
        std::vector<PhysicsBroadphasePair> pairs;

        for (std::size_t a = 0; a < scene.colliders.size(); ++a)
        {
            const PhysicsCollider& colliderA = scene.colliders[a];
            const PhysicsBody* bodyA = FindBody(scene, colliderA.bodyId);
            if (!bodyA || !bodyA->enabled || !colliderA.enabled)
            {
                continue;
            }

            const AABB3 boundsA = ComputeMotionExpandedColliderWorldBounds(scene, *bodyA, colliderA);
            if (!IsValid(boundsA))
            {
                continue;
            }

            for (std::size_t b = a + 1; b < scene.colliders.size(); ++b)
            {
                const PhysicsCollider& colliderB = scene.colliders[b];
                if (colliderA.bodyId == colliderB.bodyId)
                {
                    continue;
                }

                const PhysicsBody* bodyB = FindBody(scene, colliderB.bodyId);
                if (!bodyB || !bodyB->enabled || !colliderB.enabled)
                {
                    continue;
                }
                if (!IsDynamic(*bodyA) && !IsDynamic(*bodyB))
                {
                    continue;
                }
                if (!ColliderFiltersAllowPair(colliderA, colliderB))
                {
                    continue;
                }

                const AABB3 boundsB = ComputeMotionExpandedColliderWorldBounds(scene, *bodyB, colliderB);
                if (IsValid(boundsB) && Intersects(boundsA, boundsB))
                {
                    pairs.push_back({a, b, boundsA, boundsB});
                }
            }
        }

        return pairs;
    }

    std::vector<PhysicsBroadphasePair> BuildBroadphasePairsSpatialHash(const PhysicsScene& scene, PhysicsBroadphaseGridStats* outStats)
    {
        PhysicsBroadphaseGridStats stats{};
        stats.colliderCount = scene.colliders.size();
        stats.cellSize = std::max(0.01f, scene.config.broadphaseGridCellSize);

        if (!scene.config.enableSpatialBroadphase || scene.colliders.size() < 8)
        {
            std::vector<PhysicsBroadphasePair> fallback = BuildBroadphasePairs(scene);
            stats.usedFallback = true;
            stats.acceptedPairCount = fallback.size();
            if (outStats)
            {
                *outStats = stats;
            }
            return fallback;
        }

        std::vector<AABB3> bounds(scene.colliders.size(), MakeEmptyAABB3());
        std::unordered_map<BroadphaseCellKey, std::vector<std::size_t>, BroadphaseCellKeyHash> cells;

        for (std::size_t index = 0; index < scene.colliders.size(); ++index)
        {
            const PhysicsCollider& collider = scene.colliders[index];
            const PhysicsBody* body = FindBody(scene, collider.bodyId);
            if (!body || !body->enabled || !collider.enabled)
            {
                continue;
            }

            bounds[index] = ComputeMotionExpandedColliderWorldBounds(scene, *body, collider);
            if (!IsValid(bounds[index]))
            {
                continue;
            }

            const BroadphaseCellKey minCell = CellForPoint(bounds[index].min, stats.cellSize);
            const BroadphaseCellKey maxCell = CellForPoint(bounds[index].max, stats.cellSize);
            for (int z = minCell.z; z <= maxCell.z; ++z)
            {
                for (int y = minCell.y; y <= maxCell.y; ++y)
                {
                    for (int x = minCell.x; x <= maxCell.x; ++x)
                    {
                        cells[{x, y, z}].push_back(index);
                    }
                }
            }
        }

        stats.occupiedCellCount = cells.size();
        std::unordered_set<std::uint64_t> seenPairs;
        std::vector<PhysicsBroadphasePair> pairs;

        for (const auto& entry : cells)
        {
            const std::vector<std::size_t>& indices = entry.second;
            for (std::size_t i = 0; i < indices.size(); ++i)
            {
                for (std::size_t j = i + 1; j < indices.size(); ++j)
                {
                    ++stats.candidatePairCount;
                    const std::size_t a = std::min(indices[i], indices[j]);
                    const std::size_t b = std::max(indices[i], indices[j]);
                    const std::uint64_t pairKey = MakePairKey(a, b);
                    if (!seenPairs.insert(pairKey).second)
                    {
                        ++stats.duplicatePairCount;
                        continue;
                    }

                    const PhysicsCollider& colliderA = scene.colliders[a];
                    const PhysicsCollider& colliderB = scene.colliders[b];
                    if (colliderA.bodyId == colliderB.bodyId)
                    {
                        ++stats.skippedPairCount;
                        continue;
                    }

                    const PhysicsBody* bodyA = FindBody(scene, colliderA.bodyId);
                    const PhysicsBody* bodyB = FindBody(scene, colliderB.bodyId);
                    if (!bodyA || !bodyB || !bodyA->enabled || !bodyB->enabled || (!IsDynamic(*bodyA) && !IsDynamic(*bodyB)))
                    {
                        ++stats.skippedPairCount;
                        continue;
                    }
                    if (!ColliderFiltersAllowPair(colliderA, colliderB))
                    {
                        ++stats.filteredPairCount;
                        ++stats.skippedPairCount;
                        continue;
                    }

                    if (IsValid(bounds[a]) && IsValid(bounds[b]) && Intersects(bounds[a], bounds[b]))
                    {
                        pairs.push_back({a, b, bounds[a], bounds[b]});
                        ++stats.acceptedPairCount;
                    }
                }
            }
        }

        std::sort(pairs.begin(), pairs.end(), [](const PhysicsBroadphasePair& a, const PhysicsBroadphasePair& b)
        {
            return std::tie(a.colliderA, a.colliderB) < std::tie(b.colliderA, b.colliderB);
        });

        if (outStats)
        {
            *outStats = stats;
        }
        return pairs;
    }

    PhysicsContact BuildContact(const PhysicsScene& scene, const PhysicsBroadphasePair& pair)
    {
        if (pair.colliderA >= scene.colliders.size() || pair.colliderB >= scene.colliders.size())
        {
            return MakeInvalidContact(pair);
        }

        const PhysicsCollider& colliderA = scene.colliders[pair.colliderA];
        const PhysicsCollider& colliderB = scene.colliders[pair.colliderB];
        const PhysicsBody* bodyA = FindBody(scene, colliderA.bodyId);
        const PhysicsBody* bodyB = FindBody(scene, colliderB.bodyId);
        if (!bodyA || !bodyB)
        {
            return MakeInvalidContact(pair);
        }

        if (colliderA.kind == PhysicsColliderKind::Sphere && colliderB.kind == PhysicsColliderKind::Sphere)
        {
            return BuildSphereSphereContact(*bodyA, colliderA, *bodyB, colliderB, pair);
        }

        if (colliderA.kind == PhysicsColliderKind::Box && colliderB.kind == PhysicsColliderKind::Box)
        {
            return BuildBoxBoxContact(*bodyA, *bodyB, colliderA, colliderB, pair);
        }

        if (colliderA.kind == PhysicsColliderKind::Sphere && colliderB.kind == PhysicsColliderKind::Box)
        {
            return BuildSphereBoxContact(*bodyA, colliderA, *bodyB, colliderB, pair, true);
        }
        if (colliderA.kind == PhysicsColliderKind::Box && colliderB.kind == PhysicsColliderKind::Sphere)
        {
            return BuildSphereBoxContact(*bodyB, colliderB, *bodyA, colliderA, pair, false);
        }

        if (colliderA.kind == PhysicsColliderKind::Capsule && colliderB.kind == PhysicsColliderKind::Capsule)
        {
            return BuildCapsuleCapsuleContact(*bodyA, colliderA, *bodyB, colliderB, pair);
        }
        if (colliderA.kind == PhysicsColliderKind::Capsule && colliderB.kind == PhysicsColliderKind::Sphere)
        {
            return BuildCapsuleSphereContact(*bodyA, colliderA, *bodyB, colliderB, pair, true);
        }
        if (colliderA.kind == PhysicsColliderKind::Sphere && colliderB.kind == PhysicsColliderKind::Capsule)
        {
            return BuildCapsuleSphereContact(*bodyB, colliderB, *bodyA, colliderA, pair, false);
        }
        if (colliderA.kind == PhysicsColliderKind::Capsule && colliderB.kind == PhysicsColliderKind::Box)
        {
            return BuildCapsuleBoxContact(*bodyA, colliderA, *bodyB, colliderB, pair, true);
        }
        if (colliderA.kind == PhysicsColliderKind::Box && colliderB.kind == PhysicsColliderKind::Capsule)
        {
            return BuildCapsuleBoxContact(*bodyB, colliderB, *bodyA, colliderA, pair, false);
        }

        if (IsAabbLike(colliderA.kind) && IsAabbLike(colliderB.kind))
        {
            return BuildAabbAabbContact(*bodyA, *bodyB, colliderA, colliderB, pair, ComputeColliderWorldBounds(*bodyA, colliderA), ComputeColliderWorldBounds(*bodyB, colliderB));
        }

        if (colliderA.kind == PhysicsColliderKind::Sphere && IsAabbLike(colliderB.kind))
        {
            return BuildSphereAabbContact(*bodyA, colliderA, *bodyB, colliderB, pair, true);
        }

        if (IsAabbLike(colliderA.kind) && colliderB.kind == PhysicsColliderKind::Sphere)
        {
            return BuildSphereAabbContact(*bodyB, colliderB, *bodyA, colliderA, pair, false);
        }

        if (UsesBoundsFallback(colliderA.kind) && UsesBoundsFallback(colliderB.kind))
        {
            return BuildAabbAabbContact(*bodyA, *bodyB, colliderA, colliderB, pair, ComputeColliderWorldBounds(*bodyA, colliderA), ComputeColliderWorldBounds(*bodyB, colliderB));
        }

        if (colliderA.kind == PhysicsColliderKind::Sphere && UsesBoundsFallback(colliderB.kind))
        {
            return BuildSphereAabbContact(*bodyA, colliderA, *bodyB, colliderB, pair, true);
        }
        if (UsesBoundsFallback(colliderA.kind) && colliderB.kind == PhysicsColliderKind::Sphere)
        {
            return BuildSphereAabbContact(*bodyB, colliderB, *bodyA, colliderA, pair, false);
        }

        return MakeInvalidContact(pair);
    }

    std::vector<PhysicsContact> BuildContacts(const PhysicsScene& scene, const std::vector<PhysicsBroadphasePair>& pairs)
    {
        std::vector<PhysicsContact> contacts;
        contacts.reserve(pairs.size());

        for (const PhysicsBroadphasePair& pair : pairs)
        {
            PhysicsContact contact = BuildContact(scene, pair);
            if (contact.valid && contact.colliderA < scene.colliders.size() && contact.colliderB < scene.colliders.size())
            {
                const PhysicsCollider& colliderA = scene.colliders[contact.colliderA];
                const PhysicsCollider& colliderB = scene.colliders[contact.colliderB];
                contact.cacheKey = MakePairKey(contact.colliderA, contact.colliderB);
                contact.trigger = contact.trigger || colliderA.trigger || colliderB.trigger || colliderA.filter.queryOnly || colliderB.filter.queryOnly;
                contact.material = CombinePhysicsMaterials(colliderA.material, colliderB.material);
                contacts.push_back(contact);
            }
        }

        return contacts;
    }

    PhysicsContactManifold BuildContactManifold(const PhysicsContact& contact)
    {
        PhysicsContactManifold manifold{};
        manifold.colliderA = contact.colliderA;
        manifold.colliderB = contact.colliderB;
        manifold.normal = Normalize(contact.normal, {0.0f, 1.0f, 0.0f});
        manifold.points[0] = contact.point;
        manifold.penetrations[0] = contact.penetration;
        manifold.pointCount = contact.valid ? 1u : 0u;
        manifold.trigger = contact.trigger;
        manifold.valid = contact.valid;
        return manifold;
    }

    PhysicsContactManifold BuildContactManifold(const PhysicsScene& scene, const PhysicsContact& contact)
    {
        PhysicsContactManifold manifold = BuildContactManifold(contact);
        if (!manifold.valid || contact.colliderA >= scene.colliders.size() || contact.colliderB >= scene.colliders.size())
        {
            return manifold;
        }

        const PhysicsCollider& colliderA = scene.colliders[contact.colliderA];
        const PhysicsCollider& colliderB = scene.colliders[contact.colliderB];
        const PhysicsBody* bodyA = FindBody(scene, colliderA.bodyId);
        const PhysicsBody* bodyB = FindBody(scene, colliderB.bodyId);
        if (!bodyA || !bodyB)
        {
            return manifold;
        }

        if (contact.feature == PhysicsContactFeature::BoxBox || contact.feature == PhysicsContactFeature::SphereBox || contact.feature == PhysicsContactFeature::CapsuleBox)
        {
            const PhysicsCollider& boxCollider = colliderA.kind == PhysicsColliderKind::Box ? colliderA : colliderB;
            const PhysicsBody& boxBody = colliderA.kind == PhysicsColliderKind::Box ? *bodyA : *bodyB;
            const PhysicsObbShape obb = ComputeBoxWorldShape(boxBody, boxCollider);
            Vec3 tangentA = Cross(manifold.normal, obb.axes[0]);
            if (LengthSquared(tangentA) <= MinimumSatAxisLength)
            {
                tangentA = Cross(manifold.normal, obb.axes[1]);
            }
            tangentA = Normalize(tangentA, {1.0f, 0.0f, 0.0f});
            const Vec3 tangentB = Normalize(Cross(manifold.normal, tangentA), {0.0f, 0.0f, 1.0f});
            const float radiusA = std::min(0.25f, std::max(0.02f, ProjectObbRadius(obb, tangentA) * 0.25f));
            const float radiusB = std::min(0.25f, std::max(0.02f, ProjectObbRadius(obb, tangentB) * 0.25f));

            manifold.points[0] = Add(Add(contact.point, Multiply(tangentA, -radiusA)), Multiply(tangentB, -radiusB));
            manifold.points[1] = Add(Add(contact.point, Multiply(tangentA, radiusA)), Multiply(tangentB, -radiusB));
            manifold.points[2] = Add(Add(contact.point, Multiply(tangentA, radiusA)), Multiply(tangentB, radiusB));
            manifold.points[3] = Add(Add(contact.point, Multiply(tangentA, -radiusA)), Multiply(tangentB, radiusB));
            for (u32 i = 0; i < 4; ++i)
            {
                manifold.penetrations[i] = contact.penetration;
            }
            manifold.pointCount = 4;
        }
        else if (contact.feature == PhysicsContactFeature::CapsuleCapsule || contact.feature == PhysicsContactFeature::CapsuleSphere)
        {
            manifold.pointCount = 1;
        }

        return manifold;
    }

    std::vector<PhysicsContactManifold> BuildContactManifolds(const std::vector<PhysicsContact>& contacts)
    {
        std::vector<PhysicsContactManifold> manifolds;
        manifolds.reserve(contacts.size());
        for (const PhysicsContact& contact : contacts)
        {
            if (contact.valid)
            {
                manifolds.push_back(BuildContactManifold(contact));
            }
        }
        return manifolds;
    }

    std::vector<PhysicsContactManifold> BuildContactManifolds(const PhysicsScene& scene, const std::vector<PhysicsContact>& contacts)
    {
        std::vector<PhysicsContactManifold> manifolds;
        manifolds.reserve(contacts.size());
        for (const PhysicsContact& contact : contacts)
        {
            if (contact.valid)
            {
                manifolds.push_back(BuildContactManifold(scene, contact));
            }
        }
        return manifolds;
    }

    std::vector<PhysicsIsland> BuildPhysicsIslands(const PhysicsScene& scene, const std::vector<PhysicsBroadphasePair>& pairs)
    {
        std::unordered_map<u32, std::size_t> bodyToNode;
        std::vector<u32> nodes;
        for (const PhysicsBody& body : scene.bodies)
        {
            if (body.enabled)
            {
                bodyToNode[body.id] = nodes.size();
                nodes.push_back(body.id);
            }
        }

        std::vector<std::size_t> parent(nodes.size());
        for (std::size_t i = 0; i < parent.size(); ++i)
        {
            parent[i] = i;
        }

        auto findRoot = [&parent](std::size_t value)
        {
            while (parent[value] != value)
            {
                parent[value] = parent[parent[value]];
                value = parent[value];
            }
            return value;
        };

        auto unite = [&parent, &findRoot](std::size_t a, std::size_t b)
        {
            const std::size_t rootA = findRoot(a);
            const std::size_t rootB = findRoot(b);
            if (rootA != rootB)
            {
                parent[rootB] = rootA;
            }
        };

        for (const PhysicsBroadphasePair& pair : pairs)
        {
            if (pair.colliderA >= scene.colliders.size() || pair.colliderB >= scene.colliders.size())
            {
                continue;
            }
            const auto a = bodyToNode.find(scene.colliders[pair.colliderA].bodyId);
            const auto b = bodyToNode.find(scene.colliders[pair.colliderB].bodyId);
            if (a != bodyToNode.end() && b != bodyToNode.end())
            {
                unite(a->second, b->second);
            }
        }

        std::unordered_map<std::size_t, std::size_t> rootToIsland;
        std::vector<PhysicsIsland> islands;
        for (std::size_t node = 0; node < nodes.size(); ++node)
        {
            const std::size_t root = findRoot(node);
            auto [it, inserted] = rootToIsland.emplace(root, islands.size());
            if (inserted)
            {
                islands.push_back({});
            }
            PhysicsIsland& island = islands[it->second];
            island.bodyIds.push_back(nodes[node]);

            const PhysicsBody* body = FindBody(scene, nodes[node]);
            if (body)
            {
                island.sleeping = island.sleeping && (!IsDynamic(*body) || body->sleeping);
                for (const PhysicsCollider& collider : scene.colliders)
                {
                    if (collider.bodyId == body->id)
                    {
                        const AABB3 bounds = ComputeColliderWorldBounds(*body, collider);
                        if (IsValid(bounds))
                        {
                            island.bounds = IsValid(island.bounds) ? Union(island.bounds, bounds) : bounds;
                        }
                    }
                }
            }
        }

        return islands;
    }

    PhysicsRaycastHit RaycastPhysicsScene(const PhysicsScene& scene, Ray3 ray, float maxDistance, PhysicsQueryFlags flags)
    {
        PhysicsRaycastHit best{};
        best.distance = std::max(0.0f, maxDistance);

        const float safeMaxDistance = std::max(0.0f, maxDistance);
        if (safeMaxDistance <= 0.0f || !IsFinite(ray.origin) || !IsFinite(ray.direction))
        {
            return best;
        }
        ray.direction = Normalize(ray.direction, {0.0f, 0.0f, 1.0f});

        for (std::size_t index = 0; index < scene.colliders.size(); ++index)
        {
            const PhysicsCollider& collider = scene.colliders[index];
            const PhysicsBody* body = FindBody(scene, collider.bodyId);
            if (!body || !BodyPassesQuery(*body, flags) || !ColliderPassesQuery(collider, flags))
            {
                continue;
            }

            float tMin = 0.0f;
            float tMax = safeMaxDistance;
            bool hit = false;
            Vec3 normal{0.0f, 1.0f, 0.0f};

            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                float t = 0.0f;
                hit = RayIntersectsSphere(ray, ComputeSphereWorldShape(*body, collider), &t) && t <= safeMaxDistance;
                tMin = t;
                if (hit)
                {
                    const Vec3 point = Add(ray.origin, Multiply(ray.direction, tMin));
                    normal = Normalize(Subtract(point, ComputeSphereWorldShape(*body, collider).center), {0.0f, 1.0f, 0.0f});
                }
            }
            else if (collider.kind == PhysicsColliderKind::Box)
            {
                hit = RayIntersectsObb(ray, ComputeBoxWorldShape(*body, collider), &tMin, &tMax, &normal) && tMin <= safeMaxDistance;
            }
            else if (collider.kind == PhysicsColliderKind::Capsule)
            {
                hit = RayIntersectsCapsuleBounds(ray, ComputeCapsuleWorldShape(*body, collider), safeMaxDistance, &tMin, &normal);
            }
            else
            {
                const AABB3 bounds = ComputeColliderWorldBounds(*body, collider);
                hit = RayIntersectsAABB(ray, bounds, &tMin, &tMax) && tMin <= safeMaxDistance;
                if (hit)
                {
                    normal = EstimateAabbNormal(bounds, Add(ray.origin, Multiply(ray.direction, tMin)));
                }
            }

            if (hit && tMin >= 0.0f && (!best.hit || tMin < best.distance))
            {
                best.hit = true;
                best.collider = index;
                best.bodyId = collider.bodyId;
                best.distance = tMin;
                best.point = Add(ray.origin, Multiply(ray.direction, tMin));
                best.normal = normal;
            }
        }

        return best;
    }

    PhysicsSweepHit SweepSpherePhysicsScene(const PhysicsScene& scene, Vec3 start, Vec3 end, float radius, PhysicsQueryFlags flags)
    {
        PhysicsSweepHit best{};
        const Vec3 delta = Subtract(end, start);
        const float distance = Length(delta);
        if (distance <= MinimumRayLength || radius < 0.0f || !IsFinite(start) || !IsFinite(end))
        {
            return best;
        }

        const Ray3 ray{start, Normalize(delta, {0.0f, 0.0f, 1.0f})};
        const float safeRadius = std::max(0.0f, radius);
        best.distance = distance;

        for (std::size_t index = 0; index < scene.colliders.size(); ++index)
        {
            const PhysicsCollider& collider = scene.colliders[index];
            const PhysicsBody* body = FindBody(scene, collider.bodyId);
            if (!body || !BodyPassesQuery(*body, flags) || !ColliderPassesQuery(collider, flags))
            {
                continue;
            }

            float tMin = 0.0f;
            float tMax = distance;
            bool hit = false;
            Vec3 normal{0.0f, 1.0f, 0.0f};

            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                Sphere3 sphere = ComputeSphereWorldShape(*body, collider);
                sphere.radius += safeRadius;
                hit = RayIntersectsSphere(ray, sphere, &tMin) && tMin >= 0.0f && tMin <= distance;
                if (hit)
                {
                    normal = Normalize(Subtract(Add(start, Multiply(ray.direction, tMin)), sphere.center), {0.0f, 1.0f, 0.0f});
                }
            }
            else if (collider.kind == PhysicsColliderKind::Box)
            {
                PhysicsObbShape obb = ComputeBoxWorldShape(*body, collider);
                obb.halfExtents = Add(obb.halfExtents, {safeRadius, safeRadius, safeRadius});
                hit = RayIntersectsObb(ray, obb, &tMin, &tMax, &normal) && tMin >= 0.0f && tMin <= distance;
            }
            else if (collider.kind == PhysicsColliderKind::Capsule)
            {
                PhysicsCapsuleShape capsule = ComputeCapsuleWorldShape(*body, collider);
                capsule.radius += safeRadius;
                hit = RayIntersectsCapsuleBounds(ray, capsule, distance, &tMin, &normal);
            }
            else
            {
                const AABB3 expanded = InflateAABB(ComputeColliderWorldBounds(*body, collider), safeRadius);
                hit = IsValid(expanded) && RayIntersectsAABB(ray, expanded, &tMin, &tMax) && tMin >= 0.0f && tMin <= distance;
                if (hit)
                {
                    normal = EstimateAabbNormal(expanded, Add(start, Multiply(ray.direction, tMin)));
                }
            }

            if (hit && (!best.hit || tMin < best.distance))
            {
                best.hit = true;
                best.collider = index;
                best.bodyId = collider.bodyId;
                best.distance = tMin;
                best.timeOfImpact = SafeDivide(tMin, distance);
                best.point = Add(start, Multiply(ray.direction, tMin));
                best.normal = normal;
            }
        }

        return best;
    }

    PhysicsSweepHit SweepSpherePhysicsSceneExcludingBody(const PhysicsScene& scene, Vec3 start, Vec3 end, float radius, u32 excludedBodyId, PhysicsQueryFlags flags)
    {
        PhysicsSweepHit best{};
        const Vec3 delta = Subtract(end, start);
        const float distance = Length(delta);
        if (distance <= MinimumRayLength || radius < 0.0f || !IsFinite(start) || !IsFinite(end))
        {
            return best;
        }

        const Ray3 ray{start, Normalize(delta, {0.0f, 0.0f, 1.0f})};
        const float safeRadius = std::max(0.0f, radius);
        best.distance = distance;

        for (std::size_t index = 0; index < scene.colliders.size(); ++index)
        {
            const PhysicsCollider& collider = scene.colliders[index];
            if (collider.bodyId == excludedBodyId)
            {
                continue;
            }

            const PhysicsBody* body = FindBody(scene, collider.bodyId);
            if (!body || !BodyPassesQuery(*body, flags) || !ColliderPassesQuery(collider, flags))
            {
                continue;
            }

            float tMin = 0.0f;
            float tMax = distance;
            bool hit = false;
            Vec3 normal{0.0f, 1.0f, 0.0f};

            if (collider.kind == PhysicsColliderKind::Sphere)
            {
                Sphere3 sphere = ComputeSphereWorldShape(*body, collider);
                sphere.radius += safeRadius;
                hit = RayIntersectsSphere(ray, sphere, &tMin) && tMin >= 0.0f && tMin <= distance;
                if (hit)
                {
                    normal = Normalize(Subtract(Add(start, Multiply(ray.direction, tMin)), sphere.center), {0.0f, 1.0f, 0.0f});
                }
            }
            else if (collider.kind == PhysicsColliderKind::Box)
            {
                PhysicsObbShape obb = ComputeBoxWorldShape(*body, collider);
                obb.halfExtents = Add(obb.halfExtents, {safeRadius, safeRadius, safeRadius});
                hit = RayIntersectsObb(ray, obb, &tMin, &tMax, &normal) && tMin >= 0.0f && tMin <= distance;
            }
            else if (collider.kind == PhysicsColliderKind::Capsule)
            {
                PhysicsCapsuleShape capsule = ComputeCapsuleWorldShape(*body, collider);
                capsule.radius += safeRadius;
                hit = RayIntersectsCapsuleBounds(ray, capsule, distance, &tMin, &normal);
            }
            else
            {
                const AABB3 expanded = InflateAABB(ComputeColliderWorldBounds(*body, collider), safeRadius);
                hit = IsValid(expanded) && RayIntersectsAABB(ray, expanded, &tMin, &tMax) && tMin >= 0.0f && tMin <= distance;
                if (hit)
                {
                    normal = EstimateAabbNormal(expanded, Add(start, Multiply(ray.direction, tMin)));
                }
            }

            if (hit && (!best.hit || tMin < best.distance))
            {
                best.hit = true;
                best.collider = index;
                best.bodyId = collider.bodyId;
                best.distance = tMin;
                best.timeOfImpact = SafeDivide(tMin, distance);
                best.point = Add(start, Multiply(ray.direction, tMin));
                best.normal = normal;
            }
        }

        return best;
    }

    void ApplyForce(PhysicsBody& body, Vec3 forceNewton)
    {
        if (!IsDynamic(body) || !IsFinite(forceNewton))
        {
            return;
        }
        body.accumulatedForce = Add(body.accumulatedForce, forceNewton);
    }

    void IntegrateBodySemiImplicitEuler(PhysicsBody& body, const PhysicsWorldConfig& config, float deltaSeconds)
    {
        if (!IsDynamic(body) || deltaSeconds <= 0.0f || !IsFinite(deltaSeconds))
        {
            body.accumulatedForce = {};
            return;
        }

        body.previousPosition = body.position;

        Vec3 acceleration = Multiply(body.accumulatedForce, body.inverseMass);
        if (config.enableGravity)
        {
            acceleration = Add(acceleration, Multiply(config.gravity, body.gravityScale));
        }

        body.velocity = Add(body.velocity, Multiply(acceleration, deltaSeconds));

        const float damping = std::clamp(1.0f - std::max(0.0f, body.linearDamping) * deltaSeconds, 0.0f, 1.0f);
        body.velocity = Multiply(body.velocity, damping);
        body.velocity = ClampVelocity(body.velocity, config.maxLinearVelocity);
        body.position = Add(body.position, Multiply(body.velocity, deltaSeconds));
        body.accumulatedForce = {};

        if (!IsFinite(body))
        {
            body.position = {};
            body.velocity = {};
            body.accumulatedForce = {};
            body.sleeping = true;
        }
    }

    void ResolveContact(PhysicsScene& scene, const PhysicsContact& contact)
    {
        if (!contact.valid || contact.trigger || contact.penetration <= 0.0f)
        {
            return;
        }
        if (contact.colliderA >= scene.colliders.size() || contact.colliderB >= scene.colliders.size())
        {
            return;
        }

        const PhysicsCollider& colliderA = scene.colliders[contact.colliderA];
        const PhysicsCollider& colliderB = scene.colliders[contact.colliderB];
        PhysicsBody* bodyA = FindBody(scene, colliderA.bodyId);
        PhysicsBody* bodyB = FindBody(scene, colliderB.bodyId);
        if (!bodyA || !bodyB)
        {
            return;
        }

        const float invMassA = IsDynamic(*bodyA) ? bodyA->inverseMass : 0.0f;
        const float invMassB = IsDynamic(*bodyB) ? bodyB->inverseMass : 0.0f;
        const float totalInverseMass = invMassA + invMassB;
        if (totalInverseMass <= MinimumPhysicsMass)
        {
            return;
        }

        const Vec3 normal = Normalize(contact.normal, ContactFallbackNormal(*bodyA, *bodyB));
        const float penetration = std::max(0.0f, contact.penetration - scene.config.contactSlop);
        Vec3 correction = Multiply(normal, penetration / totalInverseMass);
        const float maxCorrection = std::max(0.0f, scene.config.maxDepenetrationVelocity) * std::max(1.0e-5f, scene.config.fixedDeltaSeconds);
        if (maxCorrection > 0.0f && Length(correction) > maxCorrection)
        {
            correction = Multiply(Normalize(correction, normal), maxCorrection);
        }

        if (invMassA > 0.0f)
        {
            bodyA->position = Subtract(bodyA->position, Multiply(correction, invMassA));
        }
        if (invMassB > 0.0f)
        {
            bodyB->position = Add(bodyB->position, Multiply(correction, invMassB));
        }

        const Vec3 relativeVelocity = Subtract(bodyB->velocity, bodyA->velocity);
        const float velocityAlongNormal = Dot(relativeVelocity, normal);
        if (velocityAlongNormal > 0.0f)
        {
            return;
        }

        const float bodyRestitution = std::min(bodyA->restitution, bodyB->restitution);
        const float restitution = std::clamp(std::max(bodyRestitution, contact.material.restitution), 0.0f, 1.0f);
        const float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / totalInverseMass;
        const Vec3 impulse = Multiply(normal, impulseMagnitude);

        if (invMassA > 0.0f)
        {
            bodyA->velocity = Subtract(bodyA->velocity, Multiply(impulse, invMassA));
        }
        if (invMassB > 0.0f)
        {
            bodyB->velocity = Add(bodyB->velocity, Multiply(impulse, invMassB));
        }

        const Vec3 updatedRelativeVelocity = Subtract(bodyB->velocity, bodyA->velocity);
        const Vec3 normalVelocity = Multiply(normal, Dot(updatedRelativeVelocity, normal));
        const Vec3 tangentVelocity = Subtract(updatedRelativeVelocity, normalVelocity);
        if (LengthSquared(tangentVelocity) > MinimumContactNormalLength)
        {
            const Vec3 tangent = Normalize(tangentVelocity, {1.0f, 0.0f, 0.0f});
            const float frictionA = std::clamp(bodyA->friction, 0.0f, 4.0f);
            const float frictionB = std::clamp(bodyB->friction, 0.0f, 4.0f);
            const float bodyFriction = std::sqrt(frictionA * frictionB);
            const float mixedFriction = std::clamp(std::max(bodyFriction, contact.material.dynamicFriction), 0.0f, 4.0f);
            const float tangentImpulseMagnitude = -Dot(updatedRelativeVelocity, tangent) / totalInverseMass;
            const float maxFrictionImpulse = std::fabs(impulseMagnitude) * mixedFriction;
            const float clampedTangentImpulse = std::clamp(tangentImpulseMagnitude, -maxFrictionImpulse, maxFrictionImpulse);
            const Vec3 frictionImpulse = Multiply(tangent, clampedTangentImpulse);

            if (invMassA > 0.0f)
            {
                bodyA->velocity = Subtract(bodyA->velocity, Multiply(frictionImpulse, invMassA));
            }
            if (invMassB > 0.0f)
            {
                bodyB->velocity = Add(bodyB->velocity, Multiply(frictionImpulse, invMassB));
            }
        }
    }

    bool SolveDistanceConstraint(PhysicsScene& scene, const PhysicsDistanceConstraint& constraint, float deltaSeconds)
    {
        if (!constraint.enabled)
        {
            return false;
        }

        PhysicsBody* bodyA = FindBody(scene, constraint.bodyA);
        PhysicsBody* bodyB = FindBody(scene, constraint.bodyB);
        if (!bodyA || !bodyB || !bodyA->enabled || !bodyB->enabled)
        {
            return false;
        }

        const float invMassA = IsDynamic(*bodyA) ? bodyA->inverseMass : 0.0f;
        const float invMassB = IsDynamic(*bodyB) ? bodyB->inverseMass : 0.0f;
        const float totalInverseMass = invMassA + invMassB;
        if (totalInverseMass <= MinimumPhysicsMass)
        {
            return false;
        }

        const Vec3 anchorA = Add(bodyA->position, constraint.localAnchorA);
        const Vec3 anchorB = Add(bodyB->position, constraint.localAnchorB);
        const Vec3 delta = Subtract(anchorB, anchorA);
        const float distance = Length(delta);
        if (distance <= MinimumContactNormalLength)
        {
            return false;
        }

        const float minDistance = std::max(0.0f, std::min(constraint.minDistance, constraint.maxDistance));
        const float maxDistance = std::max(minDistance, std::max(constraint.minDistance, constraint.maxDistance));
        const float target = std::clamp(std::max(0.0f, constraint.restDistance), minDistance, maxDistance);
        const float error = distance - target;
        if (std::fabs(error) <= scene.config.contactSlop)
        {
            return true;
        }

        const Vec3 normal = Multiply(delta, 1.0f / distance);
        const float stiffness = std::clamp(constraint.stiffness, 0.0f, 1.0f);
        const Vec3 correction = Multiply(normal, error * stiffness / totalInverseMass);
        if (invMassA > 0.0f)
        {
            bodyA->position = Add(bodyA->position, Multiply(correction, invMassA));
            bodyA->sleeping = false;
        }
        if (invMassB > 0.0f)
        {
            bodyB->position = Subtract(bodyB->position, Multiply(correction, invMassB));
            bodyB->sleeping = false;
        }

        const float damping = std::clamp(constraint.damping, 0.0f, 1.0f);
        if (damping > 0.0f && deltaSeconds > 0.0f)
        {
            const Vec3 relativeVelocity = Subtract(bodyB->velocity, bodyA->velocity);
            const float normalSpeed = Dot(relativeVelocity, normal);
            const Vec3 dampingImpulse = Multiply(normal, normalSpeed * damping / totalInverseMass);
            if (invMassA > 0.0f)
            {
                bodyA->velocity = Add(bodyA->velocity, Multiply(dampingImpulse, invMassA));
            }
            if (invMassB > 0.0f)
            {
                bodyB->velocity = Subtract(bodyB->velocity, Multiply(dampingImpulse, invMassB));
            }
        }

        return true;
    }


    std::size_t ApplyStaticLeakageGuard(PhysicsScene& scene, float deltaSeconds, PhysicsStepStats& stats)
    {
        if (!scene.config.enableStaticLeakageGuard || deltaSeconds <= 0.0f)
        {
            return 0;
        }

        std::size_t corrections = 0;
        const float skin = std::max(0.0f, scene.config.staticLeakageGuardSkin);
        const PhysicsQueryFlags staticQueryFlags = PhysicsQueryFlags::IncludeStatic | PhysicsQueryFlags::IncludeKinematic;

        for (PhysicsBody& body : scene.bodies)
        {
            if (!IsDynamic(body) || !body.enabled || body.sleeping)
            {
                continue;
            }

            const Vec3 displacement = Subtract(body.position, body.previousPosition);
            const float distance = Length(displacement);
            if (distance <= MinimumRayLength || !IsFinite(body.previousPosition) || !IsFinite(body.position))
            {
                continue;
            }

            const float radius = EstimateCcdRadiusForBody(scene, body.id)
                + skin
                + std::max(0.0f, scene.config.defaultContactOffset);
            ++stats.leakageGuardSweepCount;
            const PhysicsSweepHit hit = SweepSpherePhysicsSceneExcludingBody(scene, body.previousPosition, body.position, radius, body.id, staticQueryFlags);
            if (!hit.hit || hit.distance > distance + skin)
            {
                continue;
            }

            const Vec3 direction = Normalize(displacement, {0.0f, 0.0f, 1.0f});
            body.position = Add(body.previousPosition, Multiply(direction, std::max(0.0f, hit.distance - skin)));
            const float velocityIntoSurface = Dot(body.velocity, hit.normal);
            if (velocityIntoSurface < 0.0f)
            {
                body.velocity = Subtract(body.velocity, Multiply(hit.normal, velocityIntoSurface));
            }
            body.sleeping = false;
            body.sleepTimerSeconds = 0.0f;
            ++corrections;
        }

        const u32 iterations = std::max(1u, scene.config.leakageGuardIterations);
        for (u32 iteration = 0; iteration < iterations; ++iteration)
        {
            PhysicsBroadphaseGridStats ignored{};
            const std::vector<PhysicsBroadphasePair> pairs = BuildBroadphasePairsSpatialHash(scene, &ignored);
            const std::vector<PhysicsContact> contacts = BuildContacts(scene, pairs);
            bool changed = false;

            for (const PhysicsContact& contact : contacts)
            {
                if (!contact.valid || contact.trigger || contact.penetration <= scene.config.contactSlop)
                {
                    continue;
                }
                if (contact.colliderA >= scene.colliders.size() || contact.colliderB >= scene.colliders.size())
                {
                    continue;
                }

                const PhysicsBody* bodyA = FindBody(scene, scene.colliders[contact.colliderA].bodyId);
                const PhysicsBody* bodyB = FindBody(scene, scene.colliders[contact.colliderB].bodyId);
                if (!bodyA || !bodyB)
                {
                    continue;
                }

                const bool aDynamic = IsDynamic(*bodyA);
                const bool bDynamic = IsDynamic(*bodyB);
                const bool staticBarrierPair = (aDynamic && !bDynamic) || (!aDynamic && bDynamic);
                if (!staticBarrierPair)
                {
                    continue;
                }

                ResolveContact(scene, contact);
                stats.maxPenetrationAfterGuard = std::max(stats.maxPenetrationAfterGuard, contact.penetration);
                changed = true;
                ++corrections;
            }

            if (!changed)
            {
                break;
            }
        }

        return corrections;
    }


    std::size_t SolvePhysicsConstraints(PhysicsScene& scene, float deltaSeconds, u32 iterations)
    {
        if (!scene.config.enableConstraints || scene.constraints.empty())
        {
            return 0;
        }

        std::size_t solved = 0;
        const u32 safeIterations = std::max(1u, iterations);
        for (u32 iteration = 0; iteration < safeIterations; ++iteration)
        {
            for (const PhysicsConstraint& constraint : scene.constraints)
            {
                if (constraint.kind == PhysicsConstraintKind::Distance && SolveDistanceConstraint(scene, constraint.distance, deltaSeconds))
                {
                    ++solved;
                }
            }
        }
        return solved;
    }

    PhysicsStepStats StepPhysics(PhysicsScene& scene, float deltaSeconds)
    {
        PhysicsStepStats stats{};
        scene.events.clear();
        stats.bodyCount = scene.bodies.size();
        stats.colliderCount = scene.colliders.size();
        stats.constraintCount = scene.constraints.size();

        if (!IsFinite(deltaSeconds) || deltaSeconds <= 0.0f)
        {
            stats.warnings.push_back("delta_seconds_not_positive");
            stats.finite = false;
            return stats;
        }

        const float clampedDelta = std::min(deltaSeconds, std::max(scene.config.fixedDeltaSeconds, scene.config.maxDeltaSeconds));
        const u32 maxSubsteps = std::max(1u, scene.config.maxSubsteps);
        const float fixedDelta = std::max(1.0e-5f, scene.config.fixedDeltaSeconds);
        const u32 requestedSubsteps = static_cast<u32>(std::ceil(clampedDelta / fixedDelta));
        const u32 substeps = std::clamp(requestedSubsteps, 1u, maxSubsteps);
        const float substepDelta = clampedDelta / static_cast<float>(substeps);
        const u32 solverIterations = std::max(1u, scene.config.solverIterations);

        stats.simulatedSeconds = clampedDelta;
        stats.substeps = substeps;

        for (const PhysicsBody& body : scene.bodies)
        {
            if (body.kind == PhysicsBodyKind::Dynamic)
            {
                ++stats.dynamicBodyCount;
                if (body.sleeping)
                {
                    ++stats.sleepingBodyCount;
                }
            }
            if (!IsFinite(body))
            {
                stats.finite = false;
                stats.warnings.push_back("non_finite_body_" + std::to_string(body.id));
            }
        }

        for (const PhysicsCollider& collider : scene.colliders)
        {
            if (collider.kind == PhysicsColliderKind::ProxyAABB)
            {
                ++stats.proxyColliderCount;
            }
            if (!IsFinite(collider))
            {
                stats.finite = false;
                stats.warnings.push_back("non_finite_collider_" + std::to_string(collider.bodyId));
            }
        }

        for (u32 step = 0; step < substeps; ++step)
        {
            for (PhysicsBody& body : scene.bodies)
            {
                if (IsDynamic(body))
                {
                    IntegrateBodyWithCcd(scene, body, scene.config, substepDelta, stats);
                    ++stats.integratedBodyCount;
                }
            }

            PhysicsBroadphaseGridStats gridStats{};
            const std::vector<PhysicsBroadphasePair> pairs = BuildBroadphasePairsSpatialHash(scene, &gridStats);
            const std::vector<PhysicsContact> contacts = BuildContacts(scene, pairs);
            const std::vector<PhysicsContactManifold> manifolds = BuildContactManifolds(scene, contacts);
            stats.broadphasePairCount += pairs.size();
            stats.contactCount += contacts.size();
            stats.manifoldCount += manifolds.size();
            for (const PhysicsContact& contact : contacts)
            {
                stats.maxPenetrationBeforeSolve = std::max(stats.maxPenetrationBeforeSolve, contact.penetration);
                if (contact.speculative)
                {
                    ++stats.speculativeContactCount;
                }
                if (contact.trigger)
                {
                    ++stats.triggerContactCount;
                }
                if (contact.material.valid)
                {
                    ++stats.materialPairCount;
                }
                if (contact.feature == PhysicsContactFeature::SphereBox || contact.feature == PhysicsContactFeature::BoxBox)
                {
                    ++stats.obbContactCount;
                }
                else if (contact.feature == PhysicsContactFeature::CapsuleSphere || contact.feature == PhysicsContactFeature::CapsuleCapsule || contact.feature == PhysicsContactFeature::CapsuleBox)
                {
                    ++stats.capsuleContactCount;
                }
                else if (contact.feature == PhysicsContactFeature::BoundsFallback)
                {
                    ++stats.fallbackContactCount;
                }
            }
            for (const PhysicsContactManifold& manifold : manifolds)
            {
                stats.manifoldPointCount += manifold.pointCount;
                if (manifold.pointCount > 1)
                {
                    ++stats.clippedManifoldCount;
                }
            }
            stats.persistentContactCount += UpdateContactCache(scene, contacts);
            stats.wokenBodyCount += WakeSleepingContactBodies(scene, contacts);
            stats.filteredPairCount += gridStats.filteredPairCount;
            stats.broadphaseGrid = gridStats;

            if (scene.config.enableContactResolution)
            {
                for (u32 iteration = 0; iteration < solverIterations; ++iteration)
                {
                    for (const PhysicsContact& contact : contacts)
                    {
                        ResolveContact(scene, contact);
                    }
                }
                for (const PhysicsContact& contact : contacts)
                {
                    if (!contact.trigger)
                    {
                        ++stats.resolvedContactCount;
                    }
                }
            }

            stats.solvedConstraintCount += SolvePhysicsConstraints(scene, substepDelta, scene.config.constraintIterations);
            stats.leakageGuardCorrectionCount += ApplyStaticLeakageGuard(scene, substepDelta, stats);
            stats.sleepingBodyCount = UpdateSleepingPolicy(scene, contacts, substepDelta);
            stats.islandCount = BuildPhysicsIslands(scene, pairs).size();
        }

        for (const PhysicsEvent& event : scene.events)
        {
            switch (event.kind)
            {
            case PhysicsEventKind::ContactStarted:
                ++stats.contactStartedEventCount;
                break;
            case PhysicsEventKind::ContactStayed:
                ++stats.contactStayedEventCount;
                break;
            case PhysicsEventKind::ContactEnded:
                ++stats.contactEndedEventCount;
                break;
            case PhysicsEventKind::TriggerEntered:
            case PhysicsEventKind::TriggerStayed:
            case PhysicsEventKind::TriggerExited:
                ++stats.triggerEventCount;
                break;
            default:
                break;
            }
        }

        for (const PhysicsBody& body : scene.bodies)
        {
            stats.finite = stats.finite && IsFinite(body);
        }

        return stats;
    }

    PhysicsStepStats FixedUpdatePhysicsSystem(PhysicsScene& scene, PhysicsSystemState& state, double frameDeltaSeconds)
    {
        if (!IsFinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0)
        {
            PhysicsStepStats stats{};
            stats.finite = false;
            stats.warnings.push_back("frame_delta_seconds_not_positive");
            state.lastStats = stats;
            return state.lastStats;
        }

        const double fixedDelta = static_cast<double>(std::max(1.0e-5f, scene.config.fixedDeltaSeconds));
        const double maxAccumulated = static_cast<double>(std::max(scene.config.maxDeltaSeconds, scene.config.fixedDeltaSeconds));
        state.accumulatorSeconds = std::min(state.accumulatorSeconds + frameDeltaSeconds, maxAccumulated);

        PhysicsStepStats combined{};
        combined.bodyCount = scene.bodies.size();
        combined.colliderCount = scene.colliders.size();
        combined.finite = true;

        while (state.accumulatorSeconds + 1.0e-12 >= fixedDelta)
        {
            PhysicsStepStats stepStats = StepPhysics(scene, static_cast<float>(fixedDelta));
            combined.dynamicBodyCount = stepStats.dynamicBodyCount;
            combined.integratedBodyCount += stepStats.integratedBodyCount;
            combined.broadphasePairCount += stepStats.broadphasePairCount;
            combined.contactCount += stepStats.contactCount;
            combined.manifoldCount += stepStats.manifoldCount;
            combined.resolvedContactCount += stepStats.resolvedContactCount;
            combined.persistentContactCount += stepStats.persistentContactCount;
            combined.proxyColliderCount = stepStats.proxyColliderCount;
            combined.islandCount = stepStats.islandCount;
            combined.sleepingBodyCount = stepStats.sleepingBodyCount;
            combined.wokenBodyCount += stepStats.wokenBodyCount;
            combined.ccdSweepCount += stepStats.ccdSweepCount;
            combined.ccdHitCount += stepStats.ccdHitCount;
            combined.constraintCount = stepStats.constraintCount;
            combined.solvedConstraintCount += stepStats.solvedConstraintCount;
            combined.obbContactCount += stepStats.obbContactCount;
            combined.capsuleContactCount += stepStats.capsuleContactCount;
            combined.fallbackContactCount += stepStats.fallbackContactCount;
            combined.manifoldPointCount += stepStats.manifoldPointCount;
            combined.clippedManifoldCount += stepStats.clippedManifoldCount;
            combined.simulatedSeconds += stepStats.simulatedSeconds;
            combined.substeps += stepStats.substeps;
            combined.broadphaseGrid = stepStats.broadphaseGrid;
            combined.finite = combined.finite && stepStats.finite;
            combined.warnings.insert(combined.warnings.end(), stepStats.warnings.begin(), stepStats.warnings.end());
            state.accumulatorSeconds -= fixedDelta;
            ++state.fixedTick;
        }

        state.lastStats = combined;
        return state.lastStats;
    }

    std::string ToDebugString(const PhysicsBody& body, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "body id=" << body.id
            << " kind=" << ToString(body.kind)
            << " pos=(" << ToDebugString(body.position, precision) << ")"
            << " vel=(" << ToDebugString(body.velocity, precision) << ")"
            << " invMass=" << body.inverseMass;
        return out.str();
    }

    std::string ToDebugString(const PhysicsCollider& collider, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "collider body=" << collider.bodyId
            << " kind=" << ToString(collider.kind)
            << " center=(" << ToDebugString(collider.localCenter, precision) << ")"
            << " extents=(" << ToDebugString(collider.halfExtents, precision) << ")"
            << " radius=" << collider.radius
            << " halfHeight=" << collider.halfHeight
            << " trigger=" << (collider.trigger ? "true" : "false")
            << " name=" << collider.debugName;
        return out.str();
    }

    std::string ToDebugString(const PhysicsStepStats& stats)
    {
        std::ostringstream out;
        out << "physics bodies=" << stats.bodyCount
            << " colliders=" << stats.colliderCount
            << " dynamic=" << stats.dynamicBodyCount
            << " proxies=" << stats.proxyColliderCount
            << " pairs=" << stats.broadphasePairCount
            << " contacts=" << stats.contactCount
            << " manifolds=" << stats.manifoldCount
            << " persistent=" << stats.persistentContactCount
            << " resolved=" << stats.resolvedContactCount
            << " islands=" << stats.islandCount
            << " sleeping=" << stats.sleepingBodyCount
            << " ccd=" << stats.ccdHitCount << "/" << stats.ccdSweepCount
            << " constraints=" << stats.solvedConstraintCount << "/" << stats.constraintCount
            << " obb_contacts=" << stats.obbContactCount
            << " capsule_contacts=" << stats.capsuleContactCount
            << " fallback_contacts=" << stats.fallbackContactCount
            << " triggers=" << stats.triggerContactCount
            << " events=" << stats.contactStartedEventCount << "/" << stats.contactStayedEventCount << "/" << stats.contactEndedEventCount
            << " trigger_events=" << stats.triggerEventCount
            << " material_pairs=" << stats.materialPairCount
            << " filtered_pairs=" << stats.filteredPairCount
            << " speculative=" << stats.speculativeContactCount
            << " leak_guard=" << stats.leakageGuardCorrectionCount << "/" << stats.leakageGuardSweepCount
            << " max_pen_before=" << stats.maxPenetrationBeforeSolve
            << " max_pen_after=" << stats.maxPenetrationAfterGuard
            << " manifold_points=" << stats.manifoldPointCount
            << " clipped_manifolds=" << stats.clippedManifoldCount
            << " substeps=" << stats.substeps
            << " seconds=" << stats.simulatedSeconds
            << " finite=" << (stats.finite ? "true" : "false")
            << " grid_cells=" << stats.broadphaseGrid.occupiedCellCount
            << " warnings=" << stats.warnings.size();
        return out.str();
    }

    std::string ToDebugString(const PhysicsContact& contact, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "contact a=" << contact.colliderA
            << " b=" << contact.colliderB
            << " normal=(" << ToDebugString(contact.normal, precision) << ")"
            << " point=(" << ToDebugString(contact.point, precision) << ")"
            << " penetration=" << contact.penetration
            << " feature=" << ToString(contact.feature)
            << " friction=" << contact.material.dynamicFriction
            << " restitution=" << contact.material.restitution
            << " speculative=" << (contact.speculative ? "true" : "false")
            << " trigger=" << (contact.trigger ? "true" : "false")
            << " valid=" << (contact.valid ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PhysicsBroadphaseGridStats& stats)
    {
        std::ostringstream out;
        out << "broadphase_grid colliders=" << stats.colliderCount
            << " cells=" << stats.occupiedCellCount
            << " candidates=" << stats.candidatePairCount
            << " pairs=" << stats.acceptedPairCount
            << " duplicates=" << stats.duplicatePairCount
            << " skipped=" << stats.skippedPairCount
            << " filtered=" << stats.filteredPairCount
            << " cell_size=" << stats.cellSize
            << " fallback=" << (stats.usedFallback ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const PhysicsRaycastHit& hit, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "raycast hit=" << (hit.hit ? "true" : "false")
            << " collider=" << hit.collider
            << " body=" << hit.bodyId
            << " distance=" << hit.distance
            << " point=(" << ToDebugString(hit.point, precision) << ")"
            << " normal=(" << ToDebugString(hit.normal, precision) << ")";
        return out.str();
    }

    std::string ToDebugString(const PhysicsSweepHit& hit, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "sweep hit=" << (hit.hit ? "true" : "false")
            << " collider=" << hit.collider
            << " body=" << hit.bodyId
            << " distance=" << hit.distance
            << " toi=" << hit.timeOfImpact
            << " point=(" << ToDebugString(hit.point, precision) << ")"
            << " normal=(" << ToDebugString(hit.normal, precision) << ")";
        return out.str();
    }

    std::string ToDebugString(const PhysicsEvent& event, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "event kind=" << ToString(event.kind)
            << " bodyA=" << event.bodyA
            << " bodyB=" << event.bodyB
            << " colliderA=" << event.colliderA
            << " colliderB=" << event.colliderB
            << " point=(" << ToDebugString(event.point, precision) << ")"
            << " normal=(" << ToDebugString(event.normal, precision) << ")"
            << " penetration=" << event.penetration
            << " age=" << event.age;
        return out.str();
    }

    PhysicsProbeResult BuildPhysicsProbe()
    {
        PhysicsProbeResult probe{};
        probe.scene.config.fixedDeltaSeconds = 1.0f / 120.0f;
        probe.scene.config.maxSubsteps = 4;
        probe.scene.config.broadphaseFatMargin = 0.01f;
        probe.scene.config.broadphaseGridCellSize = 0.75f;
        probe.scene.config.contactSlop = 0.0005f;
        probe.scene.config.defaultContactOffset = 0.025f;
        probe.scene.config.staticLeakageGuardSkin = 0.006f;
        probe.scene.config.maxDepenetrationVelocity = 120.0f;
        probe.scene.config.leakageGuardIterations = 2;
        probe.scene.config.enableSpeculativeContacts = true;
        probe.scene.config.enableStaticLeakageGuard = true;
        probe.scene.config.solverIterations = 3;
        probe.scene.config.constraintIterations = 3;
        probe.scene.config.sleepLinearVelocityThreshold = 0.04f;
        probe.scene.config.sleepTimeThreshold = 0.0f;
        probe.scene.config.ccdMinVelocity = 8.0f;
        probe.scene.config.enableSpatialBroadphase = true;
        probe.scene.config.enableContinuousCollision = true;
        probe.scene.config.enableSleeping = true;
        probe.scene.config.enableConstraints = true;

        const PhysicsMaterialDesc concreteMaterial = MakePhysicsMaterial(0.9f, 0.7f, 0.02f, 2400.0f);
        PhysicsMaterialDesc rubberMaterial = MakePhysicsMaterial(1.3f, 1.0f, 0.35f, 1100.0f);
        rubberMaterial.frictionCombine = PhysicsMaterialCombineMode::Maximum;
        rubberMaterial.restitutionCombine = PhysicsMaterialCombineMode::Maximum;
        const PhysicsMaterialDesc iceMaterial = MakePhysicsMaterial(0.08f, 0.03f, 0.01f, 917.0f);

        PhysicsBody ground = MakeStaticBody(1, {0.0f, 0.0f, 0.0f});
        PhysicsBody falling = MakeDynamicBody(2, {0.0f, 0.46f, 0.0f}, 2.0f);
        falling.velocity = {0.0f, -0.5f, 0.0f};
        falling.restitution = 0.05f;

        PhysicsBody destructible = MakeStaticBody(3, {2.0f, 0.0f, 0.0f});
        PhysicsBody capsuleBody = MakeDynamicBody(4, {-1.25f, 0.92f, 0.0f}, 1.25f);
        capsuleBody.velocity = {0.15f, -0.25f, 0.0f};

        PhysicsBody ccdBody = MakeDynamicBody(5, {-2.25f, 0.78f, 0.0f}, 0.2f);
        ccdBody.velocity = {120.0f, 0.0f, 0.0f};
        ccdBody.linearDamping = 0.0f;
        ccdBody.gravityScale = 0.0f;
        ccdBody.canSleep = false;
        ccdBody.continuousCollision = true;

        PhysicsBody sleeper = MakeDynamicBody(6, {3.2f, 0.49f, 0.0f}, 1.0f);
        sleeper.velocity = {};
        sleeper.linearDamping = 0.2f;
        sleeper.gravityScale = 0.0f;

        PhysicsBody leakParticle = MakeDynamicBody(7, {-0.65f, 0.36f, -1.15f}, 0.05f);
        leakParticle.velocity = {95.0f, 0.0f, 0.0f};
        leakParticle.linearDamping = 0.0f;
        leakParticle.gravityScale = 0.0f;
        leakParticle.canSleep = false;
        leakParticle.continuousCollision = false;

        probe.scene.bodies.push_back(ground);
        probe.scene.bodies.push_back(falling);
        probe.scene.bodies.push_back(destructible);
        probe.scene.bodies.push_back(capsuleBody);
        probe.scene.bodies.push_back(ccdBody);
        probe.scene.bodies.push_back(sleeper);
        probe.scene.bodies.push_back(leakParticle);

        PhysicsCollider groundCollider = MakeBoxCollider(1, {4.0f, 0.25f, 4.0f}, {0.0f, -0.25f, 0.0f});
        groundCollider.filter.layerMask = PhysicsLayer_Static;
        groundCollider.filter.collidesWithMask = PhysicsLayer_Dynamic | PhysicsLayer_Character | PhysicsLayer_Projectile;
        groundCollider.material = concreteMaterial;

        PhysicsCollider tiltedQueryBox = MakeBoxCollider(1, {0.45f, 0.18f, 0.35f}, {-1.65f, 0.28f, 0.0f});
        tiltedQueryBox.localRotation = QuatFromAxisAngle({0.0f, 0.0f, 1.0f}, 0.22f);
        tiltedQueryBox.debugName = "tilted_query_box";
        tiltedQueryBox.filter.layerMask = PhysicsLayer_Static;
        tiltedQueryBox.filter.collidesWithMask = PhysicsLayer_Dynamic | PhysicsLayer_Character | PhysicsLayer_Projectile;
        tiltedQueryBox.material = iceMaterial;

        PhysicsCollider triggerZone = MakeBoxCollider(1, {0.65f, 0.18f, 0.65f}, {0.0f, 0.52f, 0.0f});
        triggerZone.trigger = true;
        triggerZone.debugName = "ground_trigger_zone";
        triggerZone.filter.layerMask = PhysicsLayer_Trigger;
        triggerZone.filter.collidesWithMask = PhysicsLayer_Dynamic;
        triggerZone.material = MakePhysicsMaterial(0.0f, 0.0f, 0.0f, 1.0f);

        PhysicsCollider fallingCollider = MakeSphereCollider(2, 0.5f);
        fallingCollider.filter.layerMask = PhysicsLayer_Dynamic;
        fallingCollider.filter.collidesWithMask = PhysicsLayer_Static | PhysicsLayer_Trigger | PhysicsLayer_Dynamic;
        fallingCollider.material = rubberMaterial;

        PhysicsCollider capsuleCollider = MakeCapsuleCollider(4, 0.28f, 0.55f);
        capsuleCollider.filter.layerMask = PhysicsLayer_Character;
        capsuleCollider.filter.collidesWithMask = PhysicsLayer_Static | PhysicsLayer_Dynamic;
        capsuleCollider.material = rubberMaterial;

        PhysicsCollider ccdCollider = MakeSphereCollider(5, 0.08f);
        ccdCollider.filter.layerMask = PhysicsLayer_Projectile;
        ccdCollider.filter.collidesWithMask = PhysicsLayer_Static | PhysicsLayer_Destructible;
        ccdCollider.material = MakePhysicsMaterial(0.3f, 0.25f, 0.1f, 7850.0f);

        PhysicsCollider filteredProjectileCollider = MakeSphereCollider(6, 0.5f);
        filteredProjectileCollider.filter.layerMask = PhysicsLayer_Projectile;
        filteredProjectileCollider.filter.collidesWithMask = PhysicsLayer_Destructible;
        filteredProjectileCollider.material = MakePhysicsMaterial(0.3f, 0.25f, 0.1f, 7850.0f);

        PhysicsCollider heightfieldCollider = MakeHeightfieldCollider(1, MakeAABB3FromCenterExtents({0.0f, -0.05f, 2.0f}, {4.0f, 0.08f, 0.75f}), {0.0f, -0.05f, 2.0f});
        heightfieldCollider.filter.layerMask = PhysicsLayer_Static;
        heightfieldCollider.filter.collidesWithMask = PhysicsLayer_Dynamic | PhysicsLayer_Character | PhysicsLayer_Projectile;
        heightfieldCollider.material = concreteMaterial;

        PhysicsCollider leakSealWall = MakeBoxCollider(1, {0.045f, 0.45f, 0.45f}, {0.20f, 0.36f, -1.15f});
        leakSealWall.debugName = "anti_leak_thin_wall";
        leakSealWall.contactOffset = 0.035f;
        leakSealWall.filter.layerMask = PhysicsLayer_Static;
        leakSealWall.filter.collidesWithMask = PhysicsLayer_Dynamic | PhysicsLayer_Projectile;
        leakSealWall.material = concreteMaterial;

        probe.scene.colliders.push_back(groundCollider);
        probe.scene.colliders.push_back(tiltedQueryBox);
        probe.scene.colliders.push_back(triggerZone);
        probe.scene.colliders.push_back(fallingCollider);
        probe.scene.colliders.push_back(capsuleCollider);
        probe.scene.colliders.push_back(ccdCollider);
        probe.scene.colliders.push_back(filteredProjectileCollider);
        probe.scene.colliders.push_back(heightfieldCollider);
        probe.scene.colliders.push_back(leakSealWall);

        PhysicsCollider leakParticleCollider = MakeSphereCollider(7, 0.035f);
        leakParticleCollider.debugName = "anti_leak_particle";
        leakParticleCollider.contactOffset = 0.04f;
        leakParticleCollider.filter.layerMask = PhysicsLayer_Dynamic;
        leakParticleCollider.filter.collidesWithMask = PhysicsLayer_Static;
        leakParticleCollider.material = rubberMaterial;
        probe.scene.colliders.push_back(leakParticleCollider);

        const CsgPrimitiveDesc wall = MakeCsgBox({0.0f, 0.75f, 0.0f}, {0.6f, 0.75f, 0.3f});
        const CsgPrimitiveDesc cutter = MakeCsgSphere({0.0f, 0.75f, 0.0f}, 0.42f);
        const CsgBooleanResult booleanResult = BuildPrimitiveBoolean(
            wall,
            cutter,
            CsgBooleanOperation::Difference,
            MakeAABB3FromCenterExtents({0.0f, 0.75f, 0.0f}, {0.75f, 0.9f, 0.45f}),
            8,
            8,
            6,
            CsgCollisionProxyMode::GreedyXAxisAABB);

        std::vector<PhysicsCollider> proxyColliders = MakeProxyCollidersFromCsg(3, booleanResult.collisionProxies, 24);
        for (PhysicsCollider& proxyCollider : proxyColliders)
        {
            proxyCollider.filter.layerMask = PhysicsLayer_Destructible;
            proxyCollider.filter.collidesWithMask = PhysicsLayer_Dynamic | PhysicsLayer_Projectile;
            proxyCollider.material = concreteMaterial;
        }
        probe.scene.colliders.insert(probe.scene.colliders.end(), proxyColliders.begin(), proxyColliders.end());

        PhysicsConstraint tether = MakeDistanceConstraint(2, 4, Length(Subtract(capsuleBody.position, falling.position)));
        tether.distance.stiffness = 0.35f;
        tether.distance.damping = 0.15f;
        probe.scene.constraints.push_back(tether);

        PhysicsBroadphaseGridStats preStepGrid{};
        probe.pairs = BuildBroadphasePairsSpatialHash(probe.scene, &preStepGrid);
        probe.contacts = BuildContacts(probe.scene, probe.pairs);
        probe.manifolds = BuildContactManifolds(probe.scene, probe.contacts);
        probe.islands = BuildPhysicsIslands(probe.scene, probe.pairs);

        probe.stats = StepPhysics(probe.scene, 1.0f / 30.0f);
        probe.events = probe.scene.events;
        if (probe.stats.broadphaseGrid.occupiedCellCount == 0)
        {
            probe.stats.broadphaseGrid = preStepGrid;
        }
        probe.raycast = RaycastPhysicsScene(probe.scene, {{0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}, 6.0f);
        probe.sweep = SweepSpherePhysicsScene(probe.scene, {-3.0f, 0.55f, 0.0f}, {3.0f, 0.55f, 0.0f}, 0.12f);

        PhysicsSystemState systemState{};
        const PhysicsStepStats fixedStats = FixedUpdatePhysicsSystem(probe.scene, systemState, 1.0 / 60.0);

        probe.ok = probe.stats.finite
            && fixedStats.finite
            && systemState.fixedTick >= 1
            && probe.stats.dynamicBodyCount == 5
            && probe.stats.speculativeContactCount > 0
            && probe.stats.leakageGuardCorrectionCount > 0
            && probe.stats.leakageGuardSweepCount > 0
            && probe.stats.obbContactCount > 0
            && probe.stats.capsuleContactCount > 0
            && probe.stats.triggerContactCount > 0
            && probe.stats.contactStartedEventCount > 0
            && probe.stats.triggerEventCount > 0
            && probe.stats.materialPairCount > 0
            && probe.stats.filteredPairCount > 0
            && !probe.events.empty()
            && probe.stats.manifoldPointCount >= probe.stats.manifoldCount
            && probe.stats.proxyColliderCount == proxyColliders.size()
            && !booleanResult.collisionProxies.empty()
            && !probe.scene.colliders.empty()
            && probe.stats.contactCount > 0
            && probe.stats.ccdHitCount > 0
            && probe.stats.constraintCount == 1
            && probe.stats.solvedConstraintCount > 0
            && !probe.manifolds.empty()
            && !probe.islands.empty()
            && probe.raycast.hit
            && probe.sweep.hit;

        std::ostringstream out;
        out << (probe.ok ? "[ ok ]" : "[fail]")
            << " physics anti-tunneling/leakage guard"
            << " bodies=" << probe.scene.bodies.size()
            << " colliders=" << probe.scene.colliders.size()
            << " csg_proxies=" << proxyColliders.size()
            << " pairs=" << probe.pairs.size()
            << " contacts=" << probe.contacts.size()
            << " manifolds=" << probe.manifolds.size()
            << " islands=" << probe.islands.size()
            << " grid_cells=" << probe.stats.broadphaseGrid.occupiedCellCount
            << " ccd_hits=" << probe.stats.ccdHitCount
            << " obb_contacts=" << probe.stats.obbContactCount
            << " capsule_contacts=" << probe.stats.capsuleContactCount
            << " triggers=" << probe.stats.triggerContactCount
            << " events=" << probe.events.size()
            << " filters=" << probe.stats.filteredPairCount
            << " speculative=" << probe.stats.speculativeContactCount
            << " leak_guard=" << probe.stats.leakageGuardCorrectionCount << "/" << probe.stats.leakageGuardSweepCount
            << " max_pen=" << probe.stats.maxPenetrationBeforeSolve
            << " materials=" << probe.stats.materialPairCount
            << " manifold_points=" << probe.stats.manifoldPointCount
            << " sleeping=" << probe.stats.sleepingBodyCount
            << " constraints=" << probe.stats.solvedConstraintCount << "/" << probe.stats.constraintCount
            << " raycast=" << (probe.raycast.hit ? "hit" : "miss")
            << " sweep=" << (probe.sweep.hit ? "hit" : "miss")
            << " fixed_ticks=" << systemState.fixedTick;
        probe.summary = out.str();
        return probe;
    }

    std::string BuildPhysicsProbeSummary()
    {
        return BuildPhysicsProbe().summary;
    }
}
