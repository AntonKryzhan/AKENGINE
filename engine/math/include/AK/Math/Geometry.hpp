#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Math/Numerics.hpp>

#include <array>
#include <string>

namespace AK
{
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Vec4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };

    struct Quat
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;
    };

    struct Mat4
    {
        std::array<float, 16> m{};
    };

    struct Ray3
    {
        Vec3 origin{};
        Vec3 direction{0.0f, 0.0f, 1.0f};
    };

    struct Plane3
    {
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float distance = 0.0f;
    };

    struct Sphere3
    {
        Vec3 center{};
        float radius = 1.0f;
    };

    struct AABB3
    {
        Vec3 min{};
        Vec3 max{};
    };

    struct Frustum3
    {
        std::array<Plane3, 6> planes{};
    };

    Vec3 MakeVec3(float x, float y, float z);
    Vec3 Add(Vec3 a, Vec3 b);
    Vec3 Subtract(Vec3 a, Vec3 b);
    Vec3 Multiply(Vec3 value, float scalar);
    Vec3 Divide(Vec3 value, float scalar);
    Vec3 Min(Vec3 a, Vec3 b);
    Vec3 Max(Vec3 a, Vec3 b);
    float Dot(Vec3 a, Vec3 b);
    Vec3 Cross(Vec3 a, Vec3 b);
    float LengthSquared(Vec3 value);
    float Length(Vec3 value);
    Vec3 Normalize(Vec3 value, Vec3 fallback = {0.0f, 0.0f, 1.0f});
    bool IsFinite(Vec3 value);
    bool NearlyEqual(Vec3 a, Vec3 b, float absoluteTolerance = FloatEpsilon, float relativeTolerance = FloatEpsilon);
    std::string ToDebugString(Vec3 value, int precision = 2);

    Quat QuatIdentity();
    Quat QuatFromAxisAngle(Vec3 axis, float radians);
    Quat Normalize(Quat value);
    Quat Multiply(Quat a, Quat b);
    Vec3 Rotate(Quat rotation, Vec3 vector);

    Mat4 Mat4Identity();
    Mat4 Mat4Translation(Vec3 translation);
    Mat4 Mat4Scale(Vec3 scale);
    Mat4 Mat4FromQuat(Quat rotation);
    Mat4 Mat4Multiply(const Mat4& a, const Mat4& b);
    Vec3 TransformPoint(const Mat4& matrix, Vec3 point);
    Vec3 TransformVector(const Mat4& matrix, Vec3 vector);

    AABB3 MakeEmptyAABB3();
    AABB3 MakeAABB3(Vec3 minValue, Vec3 maxValue);
    AABB3 MakeAABB3FromCenterExtents(Vec3 center, Vec3 extents);
    AABB3 Expand(AABB3 bounds, Vec3 point);
    AABB3 Union(AABB3 a, AABB3 b);
    Vec3 Center(AABB3 bounds);
    Vec3 Extents(AABB3 bounds);
    Vec3 Size(AABB3 bounds);
    float SurfaceArea(AABB3 bounds);
    bool Contains(AABB3 bounds, Vec3 point);
    bool Intersects(AABB3 a, AABB3 b);
    bool IsValid(AABB3 bounds);

    Plane3 MakePlane(Vec3 normal, float distance);
    Plane3 MakePlaneFromPointNormal(Vec3 point, Vec3 normal);
    float SignedDistance(Plane3 plane, Vec3 point);

    bool RayIntersectsAABB(Ray3 ray, AABB3 bounds, float* outTMin = nullptr, float* outTMax = nullptr);
    bool RayIntersectsSphere(Ray3 ray, Sphere3 sphere, float* outT = nullptr);
    bool SphereIntersectsAABB(Sphere3 sphere, AABB3 bounds);
    bool FrustumContainsAABB(const Frustum3& frustum, AABB3 bounds);

    std::string BuildGeometryProbeSummary();
}
