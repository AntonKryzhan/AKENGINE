#include <AK/Math/Geometry.hpp>

#include <iomanip>
#include <iostream>

int main()
{
    const AK::AABB3 bounds = AK::MakeAABB3FromCenterExtents({0.0f, 1.0f, 0.0f}, {1.0f, 2.0f, 1.0f});
    const AK::Ray3 ray{{0.0f, 1.0f, -5.0f}, AK::Normalize(AK::MakeVec3(0.0f, 0.0f, 1.0f))};
    float tMin = 0.0f;
    float tMax = 0.0f;
    const bool hit = AK::RayIntersectsAABB(ray, bounds, &tMin, &tMax);

    const AK::Quat yaw = AK::QuatFromAxisAngle({0.0f, 1.0f, 0.0f}, 90.0f * AK::DegToRad32);
    const AK::Vec3 rotatedForward = AK::Rotate(yaw, {0.0f, 0.0f, 1.0f});
    const AK::Mat4 transform = AK::Mat4Multiply(AK::Mat4Translation({2.0f, 0.0f, 0.0f}), AK::Mat4FromQuat(yaw));
    const AK::Vec3 transformedPoint = AK::TransformPoint(transform, {0.0f, 0.0f, 1.0f});

    std::cout << AK::BuildGeometryProbeSummary() << '\n';
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "ray_aabb_hit=" << (hit ? "true" : "false") << " t_min=" << tMin << " t_max=" << tMax << '\n';
    std::cout << "rotated_forward=" << AK::ToDebugString(rotatedForward, 3) << '\n';
    std::cout << "transformed_point=" << AK::ToDebugString(transformedPoint, 3) << '\n';
    std::cout << "surface_area=" << AK::SurfaceArea(bounds) << '\n';

    return hit ? 0 : 1;
}
