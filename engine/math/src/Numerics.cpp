#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>

namespace AK
{
    bool IsFinite(float value)
    {
        return std::isfinite(value);
    }

    bool IsFinite(double value)
    {
        return std::isfinite(value);
    }

    bool NearlyEqual(float a, float b, float absoluteTolerance, float relativeTolerance)
    {
        const float diff = std::fabs(a - b);
        if (diff <= absoluteTolerance)
        {
            return true;
        }

        return diff <= relativeTolerance * std::max(std::fabs(a), std::fabs(b));
    }

    float ClampFloat(float value, float minValue, float maxValue)
    {
        if (minValue > maxValue)
        {
            std::swap(minValue, maxValue);
        }
        return std::clamp(value, minValue, maxValue);
    }

    float Saturate(float value)
    {
        return ClampFloat(value, 0.0f, 1.0f);
    }

    float SafeDivide(float numerator, float denominator, float fallback, float epsilon)
    {
        if (std::fabs(denominator) <= epsilon)
        {
            return fallback;
        }

        return numerator / denominator;
    }

    float WrapAngleDegrees(float degrees)
    {
        float wrapped = std::fmod(degrees, 360.0f);
        if (wrapped < -180.0f)
        {
            wrapped += 360.0f;
        }
        else if (wrapped > 180.0f)
        {
            wrapped -= 360.0f;
        }
        return wrapped;
    }

    float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
}
