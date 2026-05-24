#pragma once

#include <AK/Core/Types.hpp>

#include <cmath>
#include <limits>

namespace AK
{
    constexpr float Pi32 = 3.14159265358979323846f;
    constexpr float TwoPi32 = 6.28318530717958647692f;
    constexpr float HalfPi32 = 1.57079632679489661923f;
    constexpr float DegToRad32 = Pi32 / 180.0f;
    constexpr float RadToDeg32 = 180.0f / Pi32;
    constexpr float FloatEpsilon = 1.0e-6f;

    bool IsFinite(float value);
    bool IsFinite(double value);
    bool NearlyEqual(float a, float b, float absoluteTolerance = FloatEpsilon, float relativeTolerance = FloatEpsilon);
    float ClampFloat(float value, float minValue, float maxValue);
    float Saturate(float value);
    float SafeDivide(float numerator, float denominator, float fallback = 0.0f, float epsilon = FloatEpsilon);
    float WrapAngleDegrees(float degrees);
    float Lerp(float a, float b, float t);
}
