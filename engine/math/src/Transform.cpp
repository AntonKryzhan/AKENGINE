#include <AK/Math/Transform.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace AK
{
    namespace
    {
        float SanitizeScalar(float value, float fallback, float maxAbsValue, bool* changed, bool* nonFinite)
        {
            if (!IsFinite(value))
            {
                if (changed)
                {
                    *changed = true;
                }
                if (nonFinite)
                {
                    *nonFinite = true;
                }
                return fallback;
            }

            if (std::fabs(value) > maxAbsValue)
            {
                if (changed)
                {
                    *changed = true;
                }
                return std::clamp(value, -maxAbsValue, maxAbsValue);
            }

            return value;
        }

        float SanitizeScaleScalar(float value, const TransformSanitizePolicy& policy, TransformSanitizeResult* result)
        {
            if (!IsFinite(value))
            {
                if (result)
                {
                    result->changed = true;
                    result->scaleFixed = true;
                    result->hadNonFinite = true;
                }
                return 1.0f;
            }

            if (!policy.clampScale)
            {
                return value;
            }

            const float absValue = std::fabs(value);
            if (absValue < policy.minAbsScale)
            {
                if (result)
                {
                    result->changed = true;
                    result->scaleFixed = true;
                }
                return value < 0.0f ? -policy.minAbsScale : policy.minAbsScale;
            }

            if (absValue > policy.maxAbsScale)
            {
                if (result)
                {
                    result->changed = true;
                    result->scaleFixed = true;
                }
                return value < 0.0f ? -policy.maxAbsScale : policy.maxAbsScale;
            }

            return value;
        }

        void AppendReason(std::string& reason, const char* text)
        {
            if (!reason.empty())
            {
                reason += ", ";
            }
            reason += text;
        }
    }

    bool IsFinite(const Quat& value)
    {
        return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z) && IsFinite(value.w);
    }

    bool IsFinite(const Mat4& value)
    {
        for (float element : value.m)
        {
            if (!IsFinite(element))
            {
                return false;
            }
        }
        return true;
    }

    bool IsFinite(const EulerTransform& value)
    {
        return IsFinite(value.position) && IsFinite(value.rotationDegrees) && IsFinite(value.scale);
    }

    Vec3 SanitizeScale(Vec3 scale, const TransformSanitizePolicy& policy, TransformSanitizeResult* result)
    {
        return {
            SanitizeScaleScalar(scale.x, policy, result),
            SanitizeScaleScalar(scale.y, policy, result),
            SanitizeScaleScalar(scale.z, policy, result)
        };
    }

    EulerTransform MakeEulerTransform(Vec3 position, Vec3 rotationDegrees, Vec3 scale)
    {
        return {position, rotationDegrees, scale};
    }

    TransformSanitizeResult SanitizeEulerTransform(EulerTransform& transform, const TransformSanitizePolicy& policy)
    {
        TransformSanitizeResult result{};

        Vec3 fixedPosition = transform.position;
        if (policy.clampLocalPosition)
        {
            fixedPosition.x = SanitizeScalar(fixedPosition.x, 0.0f, policy.maxAbsLocalPosition, &result.changed, &result.hadNonFinite);
            fixedPosition.y = SanitizeScalar(fixedPosition.y, 0.0f, policy.maxAbsLocalPosition, &result.changed, &result.hadNonFinite);
            fixedPosition.z = SanitizeScalar(fixedPosition.z, 0.0f, policy.maxAbsLocalPosition, &result.changed, &result.hadNonFinite);
        }
        else if (!IsFinite(fixedPosition))
        {
            fixedPosition = {0.0f, 0.0f, 0.0f};
            result.changed = true;
            result.hadNonFinite = true;
        }

        if (!NearlyEqual(fixedPosition, transform.position))
        {
            result.positionFixed = true;
            AppendReason(result.reason, "position");
        }
        transform.position = fixedPosition;

        Vec3 fixedRotation = transform.rotationDegrees;
        bool rotationNonFinite = false;
        fixedRotation.x = SanitizeScalar(fixedRotation.x, 0.0f, 1000000000.0f, &result.changed, &rotationNonFinite);
        fixedRotation.y = SanitizeScalar(fixedRotation.y, 0.0f, 1000000000.0f, &result.changed, &rotationNonFinite);
        fixedRotation.z = SanitizeScalar(fixedRotation.z, 0.0f, 1000000000.0f, &result.changed, &rotationNonFinite);
        if (policy.wrapEulerDegrees)
        {
            fixedRotation.x = WrapAngleDegrees(fixedRotation.x);
            fixedRotation.y = WrapAngleDegrees(fixedRotation.y);
            fixedRotation.z = WrapAngleDegrees(fixedRotation.z);
        }
        if (!NearlyEqual(fixedRotation, transform.rotationDegrees) || rotationNonFinite)
        {
            result.rotationFixed = true;
            result.hadNonFinite = result.hadNonFinite || rotationNonFinite;
            result.changed = true;
            AppendReason(result.reason, "rotation");
        }
        transform.rotationDegrees = fixedRotation;

        const Vec3 originalScale = transform.scale;
        transform.scale = SanitizeScale(transform.scale, policy, &result);
        if (!NearlyEqual(transform.scale, originalScale))
        {
            result.scaleFixed = true;
            AppendReason(result.reason, "scale");
        }

        if (!result.changed)
        {
            result.reason = "ok";
        }
        return result;
    }

    Quat QuatFromEulerXYZDegrees(Vec3 degrees)
    {
        const Quat qx = QuatFromAxisAngle({1.0f, 0.0f, 0.0f}, degrees.x * DegToRad32);
        const Quat qy = QuatFromAxisAngle({0.0f, 1.0f, 0.0f}, degrees.y * DegToRad32);
        const Quat qz = QuatFromAxisAngle({0.0f, 0.0f, 1.0f}, degrees.z * DegToRad32);
        return Normalize(Multiply(qz, Multiply(qy, qx)));
    }

    Mat4 Mat4FromTRS(Vec3 position, Quat rotation, Vec3 scale)
    {
        const Mat4 translation = Mat4Translation(position);
        const Mat4 rotationMatrix = Mat4FromQuat(rotation);
        const Mat4 scaleMatrix = Mat4Scale(scale);
        return Mat4Multiply(translation, Mat4Multiply(rotationMatrix, scaleMatrix));
    }

    Mat4 Mat4FromEulerTransform(const EulerTransform& transform)
    {
        return Mat4FromTRS(transform.position, QuatFromEulerXYZDegrees(transform.rotationDegrees), transform.scale);
    }

    AABB3 TransformAABB(const Mat4& matrix, AABB3 localBounds)
    {
        if (!IsValid(localBounds))
        {
            return MakeEmptyAABB3();
        }

        AABB3 result = MakeEmptyAABB3();
        for (int x = 0; x < 2; ++x)
        {
            for (int y = 0; y < 2; ++y)
            {
                for (int z = 0; z < 2; ++z)
                {
                    const Vec3 corner{
                        x == 0 ? localBounds.min.x : localBounds.max.x,
                        y == 0 ? localBounds.min.y : localBounds.max.y,
                        z == 0 ? localBounds.min.z : localBounds.max.z
                    };
                    result = Expand(result, TransformPoint(matrix, corner));
                }
            }
        }
        return result;
    }

    bool IsTransformUsable(const EulerTransform& transform, const TransformSanitizePolicy& policy)
    {
        EulerTransform copy = transform;
        const TransformSanitizeResult result = SanitizeEulerTransform(copy, policy);
        return !result.hadNonFinite && IsFinite(copy) && IsFinite(Mat4FromEulerTransform(copy));
    }

    std::string ToDebugString(const EulerTransform& transform, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << "P(" << ToDebugString(transform.position, precision) << ") "
            << "R(" << ToDebugString(transform.rotationDegrees, precision) << ") "
            << "S(" << ToDebugString(transform.scale, precision) << ")";
        return out.str();
    }

    std::string ToDebugString(const TransformSanitizeResult& result)
    {
        std::ostringstream out;
        out << "changed=" << (result.changed ? "yes" : "no")
            << " nonfinite=" << (result.hadNonFinite ? "yes" : "no")
            << " fixed=" << result.reason;
        return out.str();
    }

    std::string BuildTransformProbeSummary()
    {
        EulerTransform transform{{2.0f, 0.0f, -3.0f}, {725.0f, -540.0f, 0.0f}, {0.0f, 2.0f, 1.0f}};
        const TransformSanitizeResult sanitize = SanitizeEulerTransform(transform);
        const AABB3 localBounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
        const AABB3 worldBounds = TransformAABB(Mat4FromEulerTransform(transform), localBounds);

        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "Transform probe: " << ToDebugString(sanitize)
            << " rotationY=" << transform.rotationDegrees.y
            << " scaleX=" << transform.scale.x
            << " bounds_size=(" << ToDebugString(Size(worldBounds), 2) << ")";
        return out.str();
    }
}
