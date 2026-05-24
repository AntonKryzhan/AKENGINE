#pragma once

#include <AK/Math/Geometry.hpp>

#include <string>

namespace AK
{
    struct EulerTransform
    {
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    struct TransformSanitizePolicy
    {
        float minAbsScale = 0.0001f;
        float maxAbsScale = 1000000.0f;
        float maxAbsLocalPosition = 1000000000.0f;
        bool wrapEulerDegrees = true;
        bool clampScale = true;
        bool clampLocalPosition = true;
    };

    struct TransformSanitizeResult
    {
        bool changed = false;
        bool positionFixed = false;
        bool rotationFixed = false;
        bool scaleFixed = false;
        bool hadNonFinite = false;
        std::string reason;
    };

    bool IsFinite(const Quat& value);
    bool IsFinite(const Mat4& value);
    bool IsFinite(const EulerTransform& value);

    Vec3 SanitizeScale(Vec3 scale, const TransformSanitizePolicy& policy, TransformSanitizeResult* result = nullptr);
    EulerTransform MakeEulerTransform(Vec3 position, Vec3 rotationDegrees, Vec3 scale);
    TransformSanitizeResult SanitizeEulerTransform(EulerTransform& transform, const TransformSanitizePolicy& policy = {});

    Quat QuatFromEulerXYZDegrees(Vec3 degrees);
    Mat4 Mat4FromTRS(Vec3 position, Quat rotation, Vec3 scale);
    Mat4 Mat4FromEulerTransform(const EulerTransform& transform);
    AABB3 TransformAABB(const Mat4& matrix, AABB3 localBounds);

    bool IsTransformUsable(const EulerTransform& transform, const TransformSanitizePolicy& policy = {});
    std::string ToDebugString(const EulerTransform& transform, int precision = 2);
    std::string ToDebugString(const TransformSanitizeResult& result);
    std::string BuildTransformProbeSummary();
}
