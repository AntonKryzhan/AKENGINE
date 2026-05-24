#include <AK/Ragdoll/Ragdoll.hpp>

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
        constexpr float MinimumBoneMass = 0.05f;
        constexpr float MinimumRadius = 0.02f;
        constexpr float MinimumHalfHeight = 0.01f;
        constexpr float MinimumDeltaSeconds = 1.0e-6f;
        constexpr float MaximumCorrectionVelocity = 20.0f;

        Vec3 ClampLength(Vec3 value, float maxLength)
        {
            const float length = Length(value);
            if (length <= maxLength || length <= FloatEpsilon)
            {
                return value;
            }
            return Multiply(value, maxLength / length);
        }

        Quat SafeRotation(Quat value)
        {
            if (!IsFinite(Vec3{value.x, value.y, value.z}) || !IsFinite(value.w))
            {
                return QuatIdentity();
            }
            return Normalize(value);
        }

        Vec3 TransformLocal(Vec3 rootPosition, Quat rootOrientation, Vec3 localPosition)
        {
            return Add(rootPosition, Rotate(rootOrientation, localPosition));
        }

        float SafeMassFraction(float value)
        {
            if (!IsFinite(value) || value <= 0.0f)
            {
                return 0.01f;
            }
            return value;
        }

        void AddWarning(std::vector<std::string>& warnings, const std::string& text)
        {
            if (warnings.size() < 16)
            {
                warnings.push_back(text);
            }
        }

        RagdollBoneDesc MakeBone(RagdollBoneRole role, const char* name, int parent, Vec3 localPosition, float radius, float halfHeight, float massFraction, PhysicsColliderKind colliderKind = PhysicsColliderKind::Capsule, Quat localRotation = {})
        {
            RagdollBoneDesc bone{};
            bone.role = role;
            bone.name = name;
            bone.parentBone = parent;
            bone.localPosition = localPosition;
            bone.localRotation = localRotation.w == 0.0f && localRotation.x == 0.0f && localRotation.y == 0.0f && localRotation.z == 0.0f ? QuatIdentity() : localRotation;
            bone.radius = radius;
            bone.halfHeight = halfHeight;
            bone.massFraction = massFraction;
            bone.colliderKind = colliderKind;
            bone.material = MakePhysicsMaterial(0.70f, 0.58f, 0.02f, 985.0f);
            return bone;
        }

        RagdollJointDesc MakeJoint(RagdollJointKind kind, std::size_t parentBone, std::size_t childBone, float swing, float twist, float stiffness)
        {
            RagdollJointDesc joint{};
            joint.kind = kind;
            joint.parentBone = parentBone;
            joint.childBone = childBone;
            joint.swingLimitDegrees = swing;
            joint.twistLimitDegrees = twist;
            joint.stiffness = stiffness;
            joint.damping = 0.05f;
            joint.enabled = true;
            return joint;
        }

        PhysicsCollider MakeBoneCollider(u32 bodyId, const RagdollBoneDesc& bone, const RagdollConfig& config)
        {
            PhysicsCollider collider{};
            if (bone.colliderKind == PhysicsColliderKind::Box)
            {
                collider = MakeBoxCollider(bodyId, {bone.radius * 1.75f, bone.radius * 0.70f, bone.halfHeight}, {});
            }
            else
            {
                collider = MakeCapsuleCollider(bodyId, bone.radius, bone.halfHeight, {});
            }
            collider.localRotation = SafeRotation(bone.localRotation);
            collider.filter = config.filter;
            collider.material = SanitizePhysicsMaterial(bone.material.valid ? bone.material : config.material);
            collider.debugName = bone.name;
            return collider;
        }

        const RagdollPoseSample* FindPoseSample(const std::vector<RagdollPoseSample>& pose, RagdollBoneRole role)
        {
            for (const RagdollPoseSample& sample : pose)
            {
                if (sample.role == role && sample.valid)
                {
                    return &sample;
                }
            }
            return nullptr;
        }

        RagdollStepStats BuildStats(const PhysicsScene& scene, const RagdollInstance& instance)
        {
            RagdollStepStats stats{};
            stats.boneCount = instance.bones.size();
            stats.constraintCount = instance.constraintIndices.size();
            for (const RagdollBoneRuntime& bone : instance.bones)
            {
                const PhysicsBody* body = FindBody(scene, bone.bodyId);
                if (body == nullptr)
                {
                    stats.finite = false;
                    AddWarning(stats.warnings, "ragdoll bone body missing");
                    continue;
                }
                stats.totalMassKilograms += body->massKilograms;
                if (body->sleeping)
                {
                    ++stats.sleepingBoneCount;
                }
                if (!IsFinite(*body))
                {
                    stats.finite = false;
                    AddWarning(stats.warnings, "ragdoll bone body is not finite");
                }
                if (bone.colliderIndex < scene.colliders.size())
                {
                    ++stats.colliderCount;
                    if (!IsFinite(scene.colliders[bone.colliderIndex]))
                    {
                        stats.finite = false;
                        AddWarning(stats.warnings, "ragdoll bone collider is not finite");
                    }
                }
            }
            return stats;
        }
    }

    const char* ToString(RagdollBoneRole role)
    {
        switch (role)
        {
        case RagdollBoneRole::Root: return "Root";
        case RagdollBoneRole::Pelvis: return "Pelvis";
        case RagdollBoneRole::Spine: return "Spine";
        case RagdollBoneRole::Chest: return "Chest";
        case RagdollBoneRole::Neck: return "Neck";
        case RagdollBoneRole::Head: return "Head";
        case RagdollBoneRole::UpperArmLeft: return "UpperArmLeft";
        case RagdollBoneRole::LowerArmLeft: return "LowerArmLeft";
        case RagdollBoneRole::HandLeft: return "HandLeft";
        case RagdollBoneRole::UpperArmRight: return "UpperArmRight";
        case RagdollBoneRole::LowerArmRight: return "LowerArmRight";
        case RagdollBoneRole::HandRight: return "HandRight";
        case RagdollBoneRole::UpperLegLeft: return "UpperLegLeft";
        case RagdollBoneRole::LowerLegLeft: return "LowerLegLeft";
        case RagdollBoneRole::FootLeft: return "FootLeft";
        case RagdollBoneRole::UpperLegRight: return "UpperLegRight";
        case RagdollBoneRole::LowerLegRight: return "LowerLegRight";
        case RagdollBoneRole::FootRight: return "FootRight";
        default: return "Unknown";
        }
    }

    const char* ToString(RagdollJointKind kind)
    {
        switch (kind)
        {
        case RagdollJointKind::Fixed: return "Fixed";
        case RagdollJointKind::ConeTwist: return "ConeTwist";
        case RagdollJointKind::Hinge: return "Hinge";
        default: return "Unknown";
        }
    }

    RagdollConfig MakeDefaultRagdollConfig()
    {
        RagdollConfig config{};
        config.totalMassKilograms = 78.0f;
        config.driveBlend = 0.0f;
        config.driveStrength = 0.35f;
        config.maxDriveCorrectionMeters = 0.18f;
        config.startSleeping = false;
        config.enableSelfCollision = false;
        config.filter.layerMask = PhysicsLayer_Character;
        config.filter.collidesWithMask = PhysicsLayer_Static | PhysicsLayer_Dynamic | PhysicsLayer_Projectile | PhysicsLayer_Destructible;
        config.material = MakePhysicsMaterial(0.70f, 0.58f, 0.02f, 985.0f);
        return config;
    }

    RagdollConfig SanitizeRagdollConfig(RagdollConfig config)
    {
        config.totalMassKilograms = std::clamp(config.totalMassKilograms, 1.0f, 500.0f);
        config.driveBlend = std::clamp(config.driveBlend, 0.0f, 1.0f);
        config.driveStrength = std::clamp(config.driveStrength, 0.0f, 10.0f);
        config.maxDriveCorrectionMeters = std::clamp(config.maxDriveCorrectionMeters, 0.0f, 10.0f);
        config.hitImpulseWakeThreshold = std::max(0.0f, config.hitImpulseWakeThreshold);
        config.material = SanitizePhysicsMaterial(config.material);
        if (config.enableSelfCollision)
        {
            config.filter.collidesWithMask |= PhysicsLayer_Character;
        }
        else
        {
            config.filter.collidesWithMask &= ~PhysicsLayer_Character;
        }
        return config;
    }

    RagdollDescription MakeHumanoidRagdollDescription()
    {
        RagdollDescription description{};
        const Quat horizontalLeft = QuatFromAxisAngle({0.0f, 0.0f, 1.0f}, -HalfPi32);
        const Quat horizontalRight = QuatFromAxisAngle({0.0f, 0.0f, 1.0f}, HalfPi32);
        const Quat footRotation = QuatFromAxisAngle({1.0f, 0.0f, 0.0f}, HalfPi32);

        description.bones = {
            MakeBone(RagdollBoneRole::Pelvis, "pelvis", -1, {0.0f, 1.02f, 0.0f}, 0.18f, 0.18f, 0.14f),
            MakeBone(RagdollBoneRole::Spine, "spine", 0, {0.0f, 1.25f, 0.0f}, 0.16f, 0.20f, 0.11f),
            MakeBone(RagdollBoneRole::Chest, "chest", 1, {0.0f, 1.50f, 0.0f}, 0.20f, 0.22f, 0.18f),
            MakeBone(RagdollBoneRole::Neck, "neck", 2, {0.0f, 1.72f, 0.0f}, 0.08f, 0.08f, 0.03f),
            MakeBone(RagdollBoneRole::Head, "head", 3, {0.0f, 1.88f, 0.0f}, 0.15f, 0.10f, 0.08f),
            MakeBone(RagdollBoneRole::UpperArmLeft, "upper_arm_l", 2, {-0.34f, 1.54f, 0.0f}, 0.075f, 0.20f, 0.035f, PhysicsColliderKind::Capsule, horizontalLeft),
            MakeBone(RagdollBoneRole::LowerArmLeft, "lower_arm_l", 5, {-0.72f, 1.50f, 0.0f}, 0.065f, 0.20f, 0.025f, PhysicsColliderKind::Capsule, horizontalLeft),
            MakeBone(RagdollBoneRole::HandLeft, "hand_l", 6, {-1.02f, 1.47f, 0.04f}, 0.060f, 0.09f, 0.010f, PhysicsColliderKind::Box, horizontalLeft),
            MakeBone(RagdollBoneRole::UpperArmRight, "upper_arm_r", 2, {0.34f, 1.54f, 0.0f}, 0.075f, 0.20f, 0.035f, PhysicsColliderKind::Capsule, horizontalRight),
            MakeBone(RagdollBoneRole::LowerArmRight, "lower_arm_r", 8, {0.72f, 1.50f, 0.0f}, 0.065f, 0.20f, 0.025f, PhysicsColliderKind::Capsule, horizontalRight),
            MakeBone(RagdollBoneRole::HandRight, "hand_r", 9, {1.02f, 1.47f, 0.04f}, 0.060f, 0.09f, 0.010f, PhysicsColliderKind::Box, horizontalRight),
            MakeBone(RagdollBoneRole::UpperLegLeft, "upper_leg_l", 0, {-0.12f, 0.66f, 0.0f}, 0.095f, 0.28f, 0.105f),
            MakeBone(RagdollBoneRole::LowerLegLeft, "lower_leg_l", 11, {-0.12f, 0.27f, 0.0f}, 0.080f, 0.26f, 0.055f),
            MakeBone(RagdollBoneRole::FootLeft, "foot_l", 12, {-0.12f, 0.06f, 0.12f}, 0.075f, 0.13f, 0.018f, PhysicsColliderKind::Box, footRotation),
            MakeBone(RagdollBoneRole::UpperLegRight, "upper_leg_r", 0, {0.12f, 0.66f, 0.0f}, 0.095f, 0.28f, 0.105f),
            MakeBone(RagdollBoneRole::LowerLegRight, "lower_leg_r", 14, {0.12f, 0.27f, 0.0f}, 0.080f, 0.26f, 0.055f),
            MakeBone(RagdollBoneRole::FootRight, "foot_r", 15, {0.12f, 0.06f, 0.12f}, 0.075f, 0.13f, 0.018f, PhysicsColliderKind::Box, footRotation)
        };

        description.joints = {
            MakeJoint(RagdollJointKind::ConeTwist, 0, 1, 35.0f, 25.0f, 0.90f),
            MakeJoint(RagdollJointKind::ConeTwist, 1, 2, 35.0f, 25.0f, 0.90f),
            MakeJoint(RagdollJointKind::ConeTwist, 2, 3, 25.0f, 35.0f, 0.80f),
            MakeJoint(RagdollJointKind::ConeTwist, 3, 4, 30.0f, 45.0f, 0.75f),
            MakeJoint(RagdollJointKind::ConeTwist, 2, 5, 65.0f, 45.0f, 0.85f),
            MakeJoint(RagdollJointKind::Hinge, 5, 6, 10.0f, 5.0f, 0.95f),
            MakeJoint(RagdollJointKind::Fixed, 6, 7, 5.0f, 5.0f, 1.00f),
            MakeJoint(RagdollJointKind::ConeTwist, 2, 8, 65.0f, 45.0f, 0.85f),
            MakeJoint(RagdollJointKind::Hinge, 8, 9, 10.0f, 5.0f, 0.95f),
            MakeJoint(RagdollJointKind::Fixed, 9, 10, 5.0f, 5.0f, 1.00f),
            MakeJoint(RagdollJointKind::ConeTwist, 0, 11, 55.0f, 25.0f, 0.95f),
            MakeJoint(RagdollJointKind::Hinge, 11, 12, 5.0f, 5.0f, 0.98f),
            MakeJoint(RagdollJointKind::Fixed, 12, 13, 5.0f, 5.0f, 0.95f),
            MakeJoint(RagdollJointKind::ConeTwist, 0, 14, 55.0f, 25.0f, 0.95f),
            MakeJoint(RagdollJointKind::Hinge, 14, 15, 5.0f, 5.0f, 0.98f),
            MakeJoint(RagdollJointKind::Fixed, 15, 16, 5.0f, 5.0f, 0.95f)
        };

        return description;
    }

    RagdollDescription SanitizeRagdollDescription(RagdollDescription description)
    {
        for (std::size_t i = 0; i < description.bones.size(); ++i)
        {
            RagdollBoneDesc& bone = description.bones[i];
            if (bone.name.empty())
            {
                bone.name = ToString(bone.role);
            }
            if (bone.parentBone >= static_cast<int>(description.bones.size()) || bone.parentBone == static_cast<int>(i))
            {
                bone.parentBone = -1;
            }
            bone.localRotation = SafeRotation(bone.localRotation);
            bone.radius = std::max(MinimumRadius, bone.radius);
            bone.halfHeight = std::max(MinimumHalfHeight, bone.halfHeight);
            bone.massFraction = SafeMassFraction(bone.massFraction);
            bone.material = SanitizePhysicsMaterial(bone.material);
            if (bone.colliderKind != PhysicsColliderKind::Box && bone.colliderKind != PhysicsColliderKind::Capsule)
            {
                bone.colliderKind = PhysicsColliderKind::Capsule;
            }
        }

        for (RagdollJointDesc& joint : description.joints)
        {
            if (joint.parentBone >= description.bones.size() || joint.childBone >= description.bones.size() || joint.parentBone == joint.childBone)
            {
                joint.enabled = false;
            }
            joint.swingLimitDegrees = std::clamp(joint.swingLimitDegrees, 0.0f, 180.0f);
            joint.twistLimitDegrees = std::clamp(joint.twistLimitDegrees, 0.0f, 180.0f);
            joint.restDistance = std::max(0.0f, joint.restDistance);
            joint.stiffness = std::clamp(joint.stiffness, 0.0f, 1.0f);
            joint.damping = std::clamp(joint.damping, 0.0f, 1.0f);
            joint.breakForceNewton = std::max(0.0f, joint.breakForceNewton);
        }

        return description;
    }

    RagdollInstance InstantiateRagdoll(PhysicsScene& scene, const RagdollDescription& sourceDescription, const RagdollConfig& sourceConfig, Vec3 rootPosition, Quat rootOrientation, u32 firstBodyId)
    {
        const RagdollDescription description = SanitizeRagdollDescription(sourceDescription);
        const RagdollConfig config = SanitizeRagdollConfig(sourceConfig);
        const Quat safeRootRotation = SafeRotation(rootOrientation);
        RagdollInstance instance{};
        instance.bones.reserve(description.bones.size());

        float totalFraction = 0.0f;
        for (const RagdollBoneDesc& bone : description.bones)
        {
            totalFraction += SafeMassFraction(bone.massFraction);
        }
        totalFraction = std::max(totalFraction, FloatEpsilon);

        for (std::size_t index = 0; index < description.bones.size(); ++index)
        {
            const RagdollBoneDesc& bone = description.bones[index];
            const u32 bodyId = firstBodyId + static_cast<u32>(index);
            const float mass = std::max(MinimumBoneMass, config.totalMassKilograms * bone.massFraction / totalFraction);
            PhysicsBody body = MakeDynamicBody(bodyId, TransformLocal(rootPosition, safeRootRotation, bone.localPosition), mass);
            body.orientation = Multiply(safeRootRotation, bone.localRotation);
            body.canSleep = true;
            body.sleeping = config.startSleeping;
            body.continuousCollision = true;
            body.linearDamping = 0.04f;
            scene.bodies.push_back(body);

            PhysicsCollider collider = MakeBoneCollider(bodyId, bone, config);
            const std::size_t colliderIndex = scene.colliders.size();
            scene.colliders.push_back(collider);

            RagdollBoneRuntime runtime{};
            runtime.role = bone.role;
            runtime.name = bone.name;
            runtime.bodyId = bodyId;
            runtime.colliderIndex = colliderIndex;
            runtime.descriptionIndex = index;
            runtime.parentBone = bone.parentBone;
            runtime.bindLocalPosition = bone.localPosition;
            runtime.bindLocalRotation = bone.localRotation;
            instance.bones.push_back(runtime);
        }

        for (RagdollJointDesc joint : description.joints)
        {
            if (!joint.enabled || joint.parentBone >= instance.bones.size() || joint.childBone >= instance.bones.size())
            {
                continue;
            }
            const PhysicsBody* parentBody = FindBody(scene, instance.bones[joint.parentBone].bodyId);
            const PhysicsBody* childBody = FindBody(scene, instance.bones[joint.childBone].bodyId);
            if (parentBody == nullptr || childBody == nullptr)
            {
                continue;
            }

            const float restDistance = joint.restDistance > 0.0f ? joint.restDistance : Length(Subtract(childBody->position, parentBody->position));
            PhysicsConstraint constraint = MakeDistanceConstraint(parentBody->id, childBody->id, restDistance, joint.parentAnchor, joint.childAnchor);
            constraint.distance.minDistance = restDistance * std::clamp(1.0f - joint.stiffness * 0.12f, 0.70f, 1.0f);
            constraint.distance.maxDistance = restDistance * std::clamp(1.0f + joint.stiffness * 0.12f, 1.0f, 1.30f);
            constraint.distance.stiffness = joint.stiffness;
            constraint.distance.damping = joint.damping;
            constraint.distance.enabled = true;
            instance.constraintIndices.push_back(scene.constraints.size());
            scene.constraints.push_back(constraint);
        }

        return instance;
    }

    std::vector<RagdollPoseSample> ExtractRagdollPhysicsPose(const PhysicsScene& scene, const RagdollInstance& instance)
    {
        std::vector<RagdollPoseSample> pose;
        pose.reserve(instance.bones.size());
        for (const RagdollBoneRuntime& bone : instance.bones)
        {
            const PhysicsBody* body = FindBody(scene, bone.bodyId);
            RagdollPoseSample sample{};
            sample.role = bone.role;
            if (body != nullptr)
            {
                sample.position = body->position;
                sample.orientation = body->orientation;
                sample.valid = IsFinite(*body);
            }
            pose.push_back(sample);
        }
        return pose;
    }

    RagdollStepStats DriveRagdollTowardPose(PhysicsScene& scene, const RagdollInstance& instance, const std::vector<RagdollPoseSample>& targetPose, const RagdollSyncSettings& sourceSettings, float deltaSeconds)
    {
        RagdollSyncSettings settings = sourceSettings;
        settings.blendToAnimation = std::clamp(settings.blendToAnimation, 0.0f, 1.0f);
        settings.maxCorrectionMeters = std::max(0.0f, settings.maxCorrectionMeters);
        const float safeDelta = std::max(deltaSeconds, MinimumDeltaSeconds);
        RagdollStepStats stats = BuildStats(scene, instance);

        if (settings.blendToAnimation <= 0.0f || targetPose.empty())
        {
            return stats;
        }

        for (const RagdollBoneRuntime& bone : instance.bones)
        {
            PhysicsBody* body = FindBody(scene, bone.bodyId);
            const RagdollPoseSample* target = FindPoseSample(targetPose, bone.role);
            if (body == nullptr || target == nullptr || !target->valid || !IsDynamic(*body))
            {
                continue;
            }

            Vec3 correction = Subtract(target->position, body->position);
            correction = ClampLength(correction, settings.maxCorrectionMeters);
            const Vec3 desiredVelocity = ClampLength(Multiply(correction, settings.blendToAnimation / safeDelta), MaximumCorrectionVelocity);
            body->velocity = Add(Multiply(body->velocity, 1.0f - settings.blendToAnimation), Multiply(desiredVelocity, settings.blendToAnimation));
            body->sleeping = false;
            body->orientation = Normalize(Multiply(body->orientation, target->orientation));
            ++stats.drivenBoneCount;
        }
        stats.sleepingBoneCount = 0;
        return stats;
    }

    RagdollStepStats ApplyRagdollHitReaction(PhysicsScene& scene, const RagdollInstance& instance, const RagdollHitReaction& sourceReaction)
    {
        RagdollHitReaction reaction = sourceReaction;
        reaction.radiusMeters = std::max(0.001f, reaction.radiusMeters);
        reaction.falloff = std::max(0.0f, reaction.falloff);
        RagdollStepStats stats = BuildStats(scene, instance);

        for (const RagdollBoneRuntime& bone : instance.bones)
        {
            PhysicsBody* body = FindBody(scene, bone.bodyId);
            if (body == nullptr || !IsDynamic(*body))
            {
                continue;
            }

            const float distance = Length(Subtract(body->position, reaction.point));
            if (distance > reaction.radiusMeters)
            {
                continue;
            }

            const float normalizedDistance = Saturate(distance / reaction.radiusMeters);
            const float weight = std::pow(1.0f - normalizedDistance, reaction.falloff);
            const Vec3 deltaVelocity = Multiply(reaction.impulseNewtonSeconds, body->inverseMass * weight);
            body->velocity = Add(body->velocity, deltaVelocity);
            if (reaction.wakeBodies && Length(reaction.impulseNewtonSeconds) >= 0.5f)
            {
                body->sleeping = false;
                body->sleepTimerSeconds = 0.0f;
            }
            ++stats.impulseBoneCount;
        }

        return stats;
    }

    RagdollStepStats StepRagdollBridge(PhysicsScene& scene, const RagdollInstance& instance, const std::vector<RagdollPoseSample>& animationPose, const RagdollSyncSettings& settings, float deltaSeconds)
    {
        RagdollStepStats stats = DriveRagdollTowardPose(scene, instance, animationPose, settings, deltaSeconds);
        const RagdollStepStats after = BuildStats(scene, instance);
        stats.boneCount = after.boneCount;
        stats.colliderCount = after.colliderCount;
        stats.constraintCount = after.constraintCount;
        stats.sleepingBoneCount = after.sleepingBoneCount;
        stats.totalMassKilograms = after.totalMassKilograms;
        stats.finite = stats.finite && after.finite;
        for (const std::string& warning : after.warnings)
        {
            AddWarning(stats.warnings, warning);
        }
        return stats;
    }

    std::string ToDebugString(const RagdollConfig& config, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision);
        stream << "ragdoll_config mass=" << config.totalMassKilograms
               << " drive_blend=" << config.driveBlend
               << " drive_strength=" << config.driveStrength
               << " max_correction=" << config.maxDriveCorrectionMeters
               << " self_collision=" << (config.enableSelfCollision ? "true" : "false")
               << " start_sleeping=" << (config.startSleeping ? "true" : "false");
        return stream.str();
    }

    std::string ToDebugString(const RagdollStepStats& stats)
    {
        std::ostringstream stream;
        stream << "ragdoll_stats bones=" << stats.boneCount
               << " colliders=" << stats.colliderCount
               << " constraints=" << stats.constraintCount
               << " driven=" << stats.drivenBoneCount
               << " sleeping=" << stats.sleepingBoneCount
               << " impulse_bones=" << stats.impulseBoneCount
               << " mass=" << std::fixed << std::setprecision(2) << stats.totalMassKilograms
               << " finite=" << (stats.finite ? "true" : "false")
               << " warnings=" << stats.warnings.size();
        return stream.str();
    }

    std::string ToDebugString(const RagdollPoseSample& sample, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision);
        stream << "ragdoll_pose role=" << ToString(sample.role)
               << " valid=" << (sample.valid ? "true" : "false")
               << " pos=" << sample.position.x << ", " << sample.position.y << ", " << sample.position.z
               << " rot=" << sample.orientation.x << ", " << sample.orientation.y << ", " << sample.orientation.z << ", " << sample.orientation.w;
        return stream.str();
    }

    std::string BuildRagdollProbeSummary()
    {
        return BuildRagdollProbe().summary;
    }

    RagdollProbeResult BuildRagdollProbe()
    {
        RagdollProbeResult probe{};
        probe.description = MakeHumanoidRagdollDescription();
        probe.config = SanitizeRagdollConfig(MakeDefaultRagdollConfig());
        probe.config.driveBlend = 0.18f;
        probe.config.enableSelfCollision = false;

        probe.scene.config.fixedDeltaSeconds = 1.0f / 120.0f;
        probe.scene.config.enableSpatialBroadphase = true;
        probe.scene.config.enableContinuousCollision = true;
        probe.scene.config.enableConstraints = true;
        probe.scene.config.solverIterations = 6;
        probe.scene.config.constraintIterations = 8;
        probe.scene.config.broadphaseGridCellSize = 0.75f;

        PhysicsBody floor = MakeStaticBody(1, {0.0f, -0.08f, 0.0f});
        PhysicsCollider floorCollider = MakeBoxCollider(floor.id, {3.0f, 0.08f, 3.0f}, {});
        floorCollider.filter.layerMask = PhysicsLayer_Static;
        floorCollider.filter.collidesWithMask = PhysicsLayer_All;
        floorCollider.material = MakePhysicsMaterial(0.85f, 0.72f, 0.01f, 2200.0f);
        floorCollider.debugName = "ragdoll_floor";
        probe.scene.bodies.push_back(floor);
        probe.scene.colliders.push_back(floorCollider);

        probe.instance = InstantiateRagdoll(probe.scene, probe.description, probe.config, {0.0f, 0.0f, 0.0f}, QuatIdentity(), 100);
        std::vector<RagdollPoseSample> animationPose = ExtractRagdollPhysicsPose(probe.scene, probe.instance);
        for (RagdollPoseSample& sample : animationPose)
        {
            if (sample.role == RagdollBoneRole::UpperArmLeft || sample.role == RagdollBoneRole::LowerArmLeft || sample.role == RagdollBoneRole::HandLeft)
            {
                sample.position = Add(sample.position, {-0.04f, 0.05f, 0.0f});
            }
            if (sample.role == RagdollBoneRole::UpperArmRight || sample.role == RagdollBoneRole::LowerArmRight || sample.role == RagdollBoneRole::HandRight)
            {
                sample.position = Add(sample.position, {0.04f, 0.05f, 0.0f});
            }
        }

        RagdollSyncSettings sync{};
        sync.blendToAnimation = 0.18f;
        sync.maxCorrectionMeters = 0.12f;
        RagdollStepStats driveStats = StepRagdollBridge(probe.scene, probe.instance, animationPose, sync, probe.scene.config.fixedDeltaSeconds);

        RagdollHitReaction reaction{};
        reaction.point = {0.02f, 1.50f, -0.18f};
        reaction.impulseNewtonSeconds = {0.0f, 3.0f, 24.0f};
        reaction.radiusMeters = 0.55f;
        reaction.falloff = 1.35f;
        const RagdollStepStats hitStats = ApplyRagdollHitReaction(probe.scene, probe.instance, reaction);
        driveStats.impulseBoneCount = hitStats.impulseBoneCount;

        probe.physicsStats = StepPhysics(probe.scene, probe.scene.config.fixedDeltaSeconds);
        probe.pose = ExtractRagdollPhysicsPose(probe.scene, probe.instance);
        probe.stats = BuildStats(probe.scene, probe.instance);
        probe.stats.drivenBoneCount = driveStats.drivenBoneCount;
        probe.stats.impulseBoneCount = driveStats.impulseBoneCount;
        probe.stats.finite = probe.stats.finite && driveStats.finite && hitStats.finite && probe.physicsStats.finite;

        const bool hasPose = probe.pose.size() == probe.instance.bones.size();
        const bool hasConstraints = !probe.instance.constraintIndices.empty();
        probe.ok = hasPose && hasConstraints && probe.stats.finite && probe.physicsStats.contactCount > 0 && probe.stats.impulseBoneCount > 0;

        std::ostringstream stream;
        stream << "[ ok ] ragdoll physics bridge bones=" << probe.stats.boneCount
               << " colliders=" << probe.stats.colliderCount
               << " constraints=" << probe.stats.constraintCount
               << " driven=" << probe.stats.drivenBoneCount
               << " impulse_bones=" << probe.stats.impulseBoneCount
               << " contacts=" << probe.physicsStats.contactCount
               << " manifolds=" << probe.physicsStats.manifoldCount
               << " islands=" << probe.physicsStats.islandCount
               << " ccd=" << probe.physicsStats.ccdHitCount << "/" << probe.physicsStats.ccdSweepCount
               << " events=" << probe.scene.events.size()
               << " mass=" << std::fixed << std::setprecision(2) << probe.stats.totalMassKilograms
               << " finite=" << (probe.stats.finite ? "true" : "false")
               << " warnings=" << probe.stats.warnings.size();
        if (!probe.ok)
        {
            stream.str({});
            stream.clear();
            stream << "[ fail ] ragdoll physics bridge bones=" << probe.stats.boneCount
                   << " constraints=" << probe.stats.constraintCount
                   << " contacts=" << probe.physicsStats.contactCount
                   << " finite=" << (probe.stats.finite ? "true" : "false");
        }
        probe.summary = stream.str();
        return probe;
    }
}
