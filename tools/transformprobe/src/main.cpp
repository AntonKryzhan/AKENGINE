#include <AK/Math/Transform.hpp>

#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    AK::EulerTransform transform{
        {std::numeric_limits<float>::quiet_NaN(), 2.0f, -3.0f},
        {725.0f, -540.0f, 1080.0f},
        {0.0f, -0.000001f, 2.0f}
    };

    const AK::TransformSanitizeResult sanitize = AK::SanitizeEulerTransform(transform);
    const AK::Mat4 matrix = AK::Mat4FromEulerTransform(transform);
    const AK::AABB3 localBounds = AK::MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});
    const AK::AABB3 worldBounds = AK::TransformAABB(matrix, localBounds);

    std::cout << AK::BuildTransformProbeSummary() << '\n';
    std::cout << "sanitize=" << AK::ToDebugString(sanitize) << '\n';
    std::cout << "transform=" << AK::ToDebugString(transform, 4) << '\n';
    std::cout << "matrix_finite=" << (AK::IsFinite(matrix) ? "true" : "false") << '\n';
    std::cout << "bounds_valid=" << (AK::IsValid(worldBounds) ? "true" : "false") << '\n';
    std::cout << "bounds_size=" << AK::ToDebugString(AK::Size(worldBounds), 4) << '\n';

    if (!sanitize.changed || !sanitize.hadNonFinite)
    {
        return 1;
    }
    if (!AK::IsFinite(matrix) || !AK::IsValid(worldBounds))
    {
        return 2;
    }
    if (std::fabs(transform.scale.x) < 0.0001f || std::fabs(transform.scale.y) < 0.0001f)
    {
        return 3;
    }
    return 0;
}
