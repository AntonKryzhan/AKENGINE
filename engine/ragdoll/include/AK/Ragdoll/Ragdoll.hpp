#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Geometry.hpp>
#include <AK/Physics/Physics.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace AK
{
    enum class RagdollBoneRole : u32
    {
        Root = 0,
        Pelvis = 1,
        Spine = 2,
        Chest = 3,
        Neck = 4,
        Head = 5,
        UpperArmLeft = 6,
        LowerArmLeft = 7,
        HandLeft = 8,
        UpperArmRight = 9,
        LowerArmRight = 10,
        HandRight = 11,
        UpperLegLeft = 12,
        LowerLegLeft = 13,
        FootLeft = 14,
        UpperLegRight = 15,
        LowerLegRight = 16,
        FootRight = 17
    };

    enum class RagdollJointKind : u32
    {
        Fixed = 0,
        ConeTwist = 1,
        Hinge = 2
    };

    struct RagdollBoneDesc
    {
        RagdollBoneRole role = RagdollBoneRole::Root;
        std::string name;
        int parentBone = -1;
        Vec3 localPosition{};
        Quat localRotation{};
        float radius = 0.12f;
        float halfHeight = 0.18f;
        float massFraction = 0.05f;
        PhysicsColliderKind colliderKind = PhysicsColliderKind::Capsule;
        PhysicsMaterialDesc material{};
    };

    struct RagdollJointDesc
    {
        RagdollJointKind kind = RagdollJointKind::ConeTwist;
        std::size_t parentBone = 0;
        std::size_t childBone = 0;
        Vec3 parentAnchor{};
        Vec3 childAnchor{};
        float swingLimitDegrees = 45.0f;
        float twistLimitDegrees = 30.0f;
        float restDistance = 0.0f;
        float stiffness = 0.85f;
        float damping = 0.05f;
        float breakForceNewton = 0.0f;
        bool enabled = true;
    };

    struct RagdollDescription
    {
        std::vector<RagdollBoneDesc> bones;
        std::vector<RagdollJointDesc> joints;
    };

    struct RagdollConfig
    {
        float totalMassKilograms = 78.0f;
        float driveBlend = 0.0f;
        float driveStrength = 0.35f;
        float maxDriveCorrectionMeters = 0.20f;
        float hitImpulseWakeThreshold = 0.5f;
        bool startSleeping = false;
        bool enableSelfCollision = false;
        PhysicsCollisionFilter filter{PhysicsLayer_Character, PhysicsLayer_Static | PhysicsLayer_Dynamic | PhysicsLayer_Projectile | PhysicsLayer_Destructible, PhysicsLayer_All, false};
        PhysicsMaterialDesc material{};
    };

    struct RagdollBoneRuntime
    {
        RagdollBoneRole role = RagdollBoneRole::Root;
        std::string name;
        u32 bodyId = 0;
        std::size_t colliderIndex = 0;
        std::size_t descriptionIndex = 0;
        int parentBone = -1;
        Vec3 bindLocalPosition{};
        Quat bindLocalRotation{};
    };

    struct RagdollInstance
    {
        std::vector<RagdollBoneRuntime> bones;
        std::vector<std::size_t> constraintIndices;
        bool active = true;
    };

    struct RagdollPoseSample
    {
        RagdollBoneRole role = RagdollBoneRole::Root;
        Vec3 position{};
        Quat orientation{};
        bool valid = false;
    };

    struct RagdollHitReaction
    {
        Vec3 point{};
        Vec3 impulseNewtonSeconds{};
        float radiusMeters = 0.35f;
        float falloff = 1.0f;
        bool wakeBodies = true;
    };

    struct RagdollSyncSettings
    {
        float blendToAnimation = 0.0f;
        float maxCorrectionMeters = 0.20f;
        bool writeBackPhysicsPose = true;
        bool driveKinematicBodies = false;
    };

    struct RagdollStepStats
    {
        std::size_t boneCount = 0;
        std::size_t colliderCount = 0;
        std::size_t constraintCount = 0;
        std::size_t drivenBoneCount = 0;
        std::size_t sleepingBoneCount = 0;
        std::size_t impulseBoneCount = 0;
        float totalMassKilograms = 0.0f;
        bool finite = true;
        std::vector<std::string> warnings;
    };

    struct RagdollProbeResult
    {
        bool ok = false;
        std::string summary;
        PhysicsScene scene{};
        RagdollDescription description{};
        RagdollConfig config{};
        RagdollInstance instance{};
        std::vector<RagdollPoseSample> pose{};
        RagdollStepStats stats{};
        PhysicsStepStats physicsStats{};
    };

    const char* ToString(RagdollBoneRole role);
    const char* ToString(RagdollJointKind kind);

    RagdollConfig MakeDefaultRagdollConfig();
    RagdollConfig SanitizeRagdollConfig(RagdollConfig config);
    RagdollDescription MakeHumanoidRagdollDescription();
    RagdollDescription SanitizeRagdollDescription(RagdollDescription description);

    RagdollInstance InstantiateRagdoll(PhysicsScene& scene, const RagdollDescription& description, const RagdollConfig& config, Vec3 rootPosition, Quat rootOrientation, u32 firstBodyId);
    std::vector<RagdollPoseSample> ExtractRagdollPhysicsPose(const PhysicsScene& scene, const RagdollInstance& instance);
    RagdollStepStats DriveRagdollTowardPose(PhysicsScene& scene, const RagdollInstance& instance, const std::vector<RagdollPoseSample>& targetPose, const RagdollSyncSettings& settings, float deltaSeconds);
    RagdollStepStats ApplyRagdollHitReaction(PhysicsScene& scene, const RagdollInstance& instance, const RagdollHitReaction& reaction);
    RagdollStepStats StepRagdollBridge(PhysicsScene& scene, const RagdollInstance& instance, const std::vector<RagdollPoseSample>& animationPose, const RagdollSyncSettings& settings, float deltaSeconds);

    std::string ToDebugString(const RagdollConfig& config, int precision = 2);
    std::string ToDebugString(const RagdollStepStats& stats);
    std::string ToDebugString(const RagdollPoseSample& sample, int precision = 3);
    std::string BuildRagdollProbeSummary();
    RagdollProbeResult BuildRagdollProbe();
}
