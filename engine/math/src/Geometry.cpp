#include <AK/Math/Geometry.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AK
{
    namespace
    {
        float MatrixAt(const Mat4& matrix, int row, int column)
        {
            return matrix.m[static_cast<std::size_t>(column * 4 + row)];
        }

        void MatrixSet(Mat4& matrix, int row, int column, float value)
        {
            matrix.m[static_cast<std::size_t>(column * 4 + row)] = value;
        }
    }

    Vec3 MakeVec3(float x, float y, float z)
    {
        return {x, y, z};
    }

    Vec3 Add(Vec3 a, Vec3 b)
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    Vec3 Subtract(Vec3 a, Vec3 b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    Vec3 Multiply(Vec3 value, float scalar)
    {
        return {value.x * scalar, value.y * scalar, value.z * scalar};
    }

    Vec3 Divide(Vec3 value, float scalar)
    {
        return {SafeDivide(value.x, scalar), SafeDivide(value.y, scalar), SafeDivide(value.z, scalar)};
    }

    Vec3 Min(Vec3 a, Vec3 b)
    {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }

    Vec3 Max(Vec3 a, Vec3 b)
    {
        return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }

    float Dot(Vec3 a, Vec3 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    Vec3 Cross(Vec3 a, Vec3 b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float LengthSquared(Vec3 value)
    {
        return Dot(value, value);
    }

    float Length(Vec3 value)
    {
        return std::sqrt(LengthSquared(value));
    }

    Vec3 Normalize(Vec3 value, Vec3 fallback)
    {
        const float lengthSq = LengthSquared(value);
        if (lengthSq <= FloatEpsilon * FloatEpsilon)
        {
            return fallback;
        }

        return Multiply(value, 1.0f / std::sqrt(lengthSq));
    }

    bool IsFinite(Vec3 value)
    {
        return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
    }

    bool NearlyEqual(Vec3 a, Vec3 b, float absoluteTolerance, float relativeTolerance)
    {
        return NearlyEqual(a.x, b.x, absoluteTolerance, relativeTolerance)
            && NearlyEqual(a.y, b.y, absoluteTolerance, relativeTolerance)
            && NearlyEqual(a.z, b.z, absoluteTolerance, relativeTolerance);
    }

    std::string ToDebugString(Vec3 value, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision)
            << value.x << ", " << value.y << ", " << value.z;
        return out.str();
    }

    Quat QuatIdentity()
    {
        return {};
    }

    Quat QuatFromAxisAngle(Vec3 axis, float radians)
    {
        const Vec3 unitAxis = Normalize(axis, {0.0f, 1.0f, 0.0f});
        const float halfAngle = radians * 0.5f;
        const float s = std::sin(halfAngle);
        return Normalize({unitAxis.x * s, unitAxis.y * s, unitAxis.z * s, std::cos(halfAngle)});
    }

    Quat Normalize(Quat value)
    {
        const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;
        if (lengthSq <= FloatEpsilon * FloatEpsilon)
        {
            return QuatIdentity();
        }

        const float invLength = 1.0f / std::sqrt(lengthSq);
        return {value.x * invLength, value.y * invLength, value.z * invLength, value.w * invLength};
    }

    Quat Multiply(Quat a, Quat b)
    {
        return {
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        };
    }

    Vec3 Rotate(Quat rotation, Vec3 vector)
    {
        const Quat q = Normalize(rotation);
        const Vec3 u{q.x, q.y, q.z};
        const float s = q.w;
        return Add(
            Add(Multiply(u, 2.0f * Dot(u, vector)), Multiply(vector, s * s - Dot(u, u))),
            Multiply(Cross(u, vector), 2.0f * s));
    }

    Mat4 Mat4Identity()
    {
        Mat4 result{};
        MatrixSet(result, 0, 0, 1.0f);
        MatrixSet(result, 1, 1, 1.0f);
        MatrixSet(result, 2, 2, 1.0f);
        MatrixSet(result, 3, 3, 1.0f);
        return result;
    }

    Mat4 Mat4Translation(Vec3 translation)
    {
        Mat4 result = Mat4Identity();
        MatrixSet(result, 0, 3, translation.x);
        MatrixSet(result, 1, 3, translation.y);
        MatrixSet(result, 2, 3, translation.z);
        return result;
    }

    Mat4 Mat4Scale(Vec3 scale)
    {
        Mat4 result{};
        MatrixSet(result, 0, 0, scale.x);
        MatrixSet(result, 1, 1, scale.y);
        MatrixSet(result, 2, 2, scale.z);
        MatrixSet(result, 3, 3, 1.0f);
        return result;
    }

    Mat4 Mat4FromQuat(Quat rotation)
    {
        const Quat q = Normalize(rotation);
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        Mat4 result = Mat4Identity();
        MatrixSet(result, 0, 0, 1.0f - 2.0f * (yy + zz));
        MatrixSet(result, 0, 1, 2.0f * (xy - wz));
        MatrixSet(result, 0, 2, 2.0f * (xz + wy));
        MatrixSet(result, 1, 0, 2.0f * (xy + wz));
        MatrixSet(result, 1, 1, 1.0f - 2.0f * (xx + zz));
        MatrixSet(result, 1, 2, 2.0f * (yz - wx));
        MatrixSet(result, 2, 0, 2.0f * (xz - wy));
        MatrixSet(result, 2, 1, 2.0f * (yz + wx));
        MatrixSet(result, 2, 2, 1.0f - 2.0f * (xx + yy));
        return result;
    }

    Mat4 Mat4Multiply(const Mat4& a, const Mat4& b)
    {
        Mat4 result{};
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                float value = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    value += MatrixAt(a, row, k) * MatrixAt(b, k, column);
                }
                MatrixSet(result, row, column, value);
            }
        }
        return result;
    }

    Vec3 TransformPoint(const Mat4& matrix, Vec3 point)
    {
        const float x = MatrixAt(matrix, 0, 0) * point.x + MatrixAt(matrix, 0, 1) * point.y + MatrixAt(matrix, 0, 2) * point.z + MatrixAt(matrix, 0, 3);
        const float y = MatrixAt(matrix, 1, 0) * point.x + MatrixAt(matrix, 1, 1) * point.y + MatrixAt(matrix, 1, 2) * point.z + MatrixAt(matrix, 1, 3);
        const float z = MatrixAt(matrix, 2, 0) * point.x + MatrixAt(matrix, 2, 1) * point.y + MatrixAt(matrix, 2, 2) * point.z + MatrixAt(matrix, 2, 3);
        const float w = MatrixAt(matrix, 3, 0) * point.x + MatrixAt(matrix, 3, 1) * point.y + MatrixAt(matrix, 3, 2) * point.z + MatrixAt(matrix, 3, 3);
        return {SafeDivide(x, w, x), SafeDivide(y, w, y), SafeDivide(z, w, z)};
    }

    Vec3 TransformVector(const Mat4& matrix, Vec3 vector)
    {
        return {
            MatrixAt(matrix, 0, 0) * vector.x + MatrixAt(matrix, 0, 1) * vector.y + MatrixAt(matrix, 0, 2) * vector.z,
            MatrixAt(matrix, 1, 0) * vector.x + MatrixAt(matrix, 1, 1) * vector.y + MatrixAt(matrix, 1, 2) * vector.z,
            MatrixAt(matrix, 2, 0) * vector.x + MatrixAt(matrix, 2, 1) * vector.y + MatrixAt(matrix, 2, 2) * vector.z
        };
    }

    AABB3 MakeEmptyAABB3()
    {
        const float inf = std::numeric_limits<float>::infinity();
        return {{inf, inf, inf}, {-inf, -inf, -inf}};
    }

    AABB3 MakeAABB3(Vec3 minValue, Vec3 maxValue)
    {
        return {Min(minValue, maxValue), Max(minValue, maxValue)};
    }

    AABB3 MakeAABB3FromCenterExtents(Vec3 center, Vec3 extents)
    {
        const Vec3 safeExtents{std::fabs(extents.x), std::fabs(extents.y), std::fabs(extents.z)};
        return {Subtract(center, safeExtents), Add(center, safeExtents)};
    }

    AABB3 Expand(AABB3 bounds, Vec3 point)
    {
        return {Min(bounds.min, point), Max(bounds.max, point)};
    }

    AABB3 Union(AABB3 a, AABB3 b)
    {
        return {Min(a.min, b.min), Max(a.max, b.max)};
    }

    Vec3 Center(AABB3 bounds)
    {
        return Multiply(Add(bounds.min, bounds.max), 0.5f);
    }

    Vec3 Extents(AABB3 bounds)
    {
        return Multiply(Size(bounds), 0.5f);
    }

    Vec3 Size(AABB3 bounds)
    {
        return Max(Subtract(bounds.max, bounds.min), {0.0f, 0.0f, 0.0f});
    }

    float SurfaceArea(AABB3 bounds)
    {
        const Vec3 size = Size(bounds);
        return 2.0f * (size.x * size.y + size.x * size.z + size.y * size.z);
    }

    bool Contains(AABB3 bounds, Vec3 point)
    {
        return point.x >= bounds.min.x && point.x <= bounds.max.x
            && point.y >= bounds.min.y && point.y <= bounds.max.y
            && point.z >= bounds.min.z && point.z <= bounds.max.z;
    }

    bool Intersects(AABB3 a, AABB3 b)
    {
        return a.min.x <= b.max.x && a.max.x >= b.min.x
            && a.min.y <= b.max.y && a.max.y >= b.min.y
            && a.min.z <= b.max.z && a.max.z >= b.min.z;
    }

    bool IsValid(AABB3 bounds)
    {
        return IsFinite(bounds.min) && IsFinite(bounds.max)
            && bounds.min.x <= bounds.max.x
            && bounds.min.y <= bounds.max.y
            && bounds.min.z <= bounds.max.z;
    }

    Plane3 MakePlane(Vec3 normal, float distance)
    {
        const float length = Length(normal);
        if (length <= FloatEpsilon)
        {
            return {};
        }

        return {Multiply(normal, 1.0f / length), distance / length};
    }

    Plane3 MakePlaneFromPointNormal(Vec3 point, Vec3 normal)
    {
        const Vec3 unitNormal = Normalize(normal, {0.0f, 1.0f, 0.0f});
        return {unitNormal, -Dot(unitNormal, point)};
    }

    float SignedDistance(Plane3 plane, Vec3 point)
    {
        return Dot(plane.normal, point) + plane.distance;
    }

    bool RayIntersectsAABB(Ray3 ray, AABB3 bounds, float* outTMin, float* outTMax)
    {
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        const float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
        const float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
        const float mins[3] = {bounds.min.x, bounds.min.y, bounds.min.z};
        const float maxs[3] = {bounds.max.x, bounds.max.y, bounds.max.z};

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(directions[axis]) <= FloatEpsilon)
            {
                if (origins[axis] < mins[axis] || origins[axis] > maxs[axis])
                {
                    return false;
                }
                continue;
            }

            const float invD = 1.0f / directions[axis];
            float t1 = (mins[axis] - origins[axis]) * invD;
            float t2 = (maxs[axis] - origins[axis]) * invD;
            if (t1 > t2)
            {
                std::swap(t1, t2);
            }

            tMin = std::max(tMin, t1);
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
        return true;
    }

    bool RayIntersectsSphere(Ray3 ray, Sphere3 sphere, float* outT)
    {
        const Vec3 m = Subtract(ray.origin, sphere.center);
        const float b = Dot(m, ray.direction);
        const float c = Dot(m, m) - sphere.radius * sphere.radius;

        if (c > 0.0f && b > 0.0f)
        {
            return false;
        }

        const float discriminant = b * b - c;
        if (discriminant < 0.0f)
        {
            return false;
        }

        if (outT)
        {
            *outT = std::max(0.0f, -b - std::sqrt(discriminant));
        }
        return true;
    }

    bool SphereIntersectsAABB(Sphere3 sphere, AABB3 bounds)
    {
        const Vec3 closest{
            ClampFloat(sphere.center.x, bounds.min.x, bounds.max.x),
            ClampFloat(sphere.center.y, bounds.min.y, bounds.max.y),
            ClampFloat(sphere.center.z, bounds.min.z, bounds.max.z)
        };
        return LengthSquared(Subtract(closest, sphere.center)) <= sphere.radius * sphere.radius;
    }

    bool FrustumContainsAABB(const Frustum3& frustum, AABB3 bounds)
    {
        for (const Plane3& plane : frustum.planes)
        {
            const Vec3 positiveVertex{
                plane.normal.x >= 0.0f ? bounds.max.x : bounds.min.x,
                plane.normal.y >= 0.0f ? bounds.max.y : bounds.min.y,
                plane.normal.z >= 0.0f ? bounds.max.z : bounds.min.z
            };

            if (SignedDistance(plane, positiveVertex) < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    std::string BuildGeometryProbeSummary()
    {
        const AABB3 bounds = MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f});
        const Ray3 ray{{0.0f, 0.0f, -8.0f}, Normalize(MakeVec3(0.0f, 0.0f, 1.0f))};
        float hitT = 0.0f;
        const bool hit = RayIntersectsAABB(ray, bounds, &hitT, nullptr);
        const Quat yaw = QuatFromAxisAngle({0.0f, 1.0f, 0.0f}, 90.0f * DegToRad32);
        const Vec3 forward = Rotate(yaw, {0.0f, 0.0f, 1.0f});

        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "Geometry probe: aabb_size=(" << ToDebugString(Size(bounds), 2) << ")"
            << " ray_hit=" << (hit ? "yes" : "no")
            << " t=" << hitT
            << " yaw90_forward=(" << ToDebugString(forward, 2) << ")";
        return out.str();
    }
}
