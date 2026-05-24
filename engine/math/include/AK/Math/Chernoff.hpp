#pragma once

#include <AK/Core/Types.hpp>

#include <functional>
#include <string>
#include <vector>

namespace AK
{
    using ScalarFunction1D = std::function<double(double)>;

    struct ChernoffCoefficients1D
    {
        ScalarFunction1D a;
        ScalarFunction1D b;
        ScalarFunction1D c;
        ScalarFunction1D g;
    };

    struct ChernoffResolventDesc
    {
        double lambda = 2.0;
        double timeHorizon = 3.0;
        u32 timeSamples = 32;
        u32 productSteps = 6;
        double minDiffusion = 1.0e-6;
        double maxParticleWeight = 1.0e6;
    };

    struct ChernoffSample1D
    {
        double x = 0.0;
        double value = 0.0;
    };

    struct ChernoffProbeResult
    {
        std::vector<ChernoffSample1D> samples;
        double tailEstimate = 0.0;
        std::string summary;
    };

    bool ValidateChernoffInput(const ChernoffCoefficients1D& coefficients, const ChernoffResolventDesc& desc, std::string* errorMessage = nullptr);
    double ApplyChernoffTranslationProduct(const ChernoffCoefficients1D& coefficients, double x, double t, u32 productSteps, double minDiffusion, double maxParticleWeight);
    double EvaluateChernoffResolvent(const ChernoffCoefficients1D& coefficients, double x, const ChernoffResolventDesc& desc);
    double EstimateChernoffTail(double lambda, double cNorm, double timeHorizon);
    std::vector<ChernoffSample1D> SampleChernoffResolvent(const ChernoffCoefficients1D& coefficients, const ChernoffResolventDesc& desc, double xMin, double xMax, u32 sampleCount);

    ChernoffCoefficients1D BuildDefaultChernoffProbeCoefficients();
    ChernoffProbeResult BuildDefaultChernoffProbe();
}
