#include <AK/Math/Chernoff.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        struct WeightedPoint
        {
            double x = 0.0;
            double weight = 1.0;
        };

        bool IsFinite(double value)
        {
            return std::isfinite(value);
        }

        double SafeCoefficient(const ScalarFunction1D& function, double x, double fallback)
        {
            const double value = function ? function(x) : fallback;
            return IsFinite(value) ? value : fallback;
        }

        double ClampWeight(double value, double maxAbsWeight)
        {
            if (!IsFinite(value))
            {
                return 0.0;
            }

            const double limit = std::max(1.0, maxAbsWeight);
            return std::clamp(value, -limit, limit);
        }
    }

    bool ValidateChernoffInput(const ChernoffCoefficients1D& coefficients, const ChernoffResolventDesc& desc, std::string* errorMessage)
    {
        auto fail = [&](const char* message)
        {
            if (errorMessage)
            {
                *errorMessage = message;
            }
            return false;
        };

        if (!coefficients.a || !coefficients.b || !coefficients.c || !coefficients.g)
        {
            return fail("Chernoff coefficients a, b, c and g must be provided");
        }

        if (!IsFinite(desc.lambda) || desc.lambda <= 0.0)
        {
            return fail("Chernoff lambda must be positive and finite");
        }

        if (!IsFinite(desc.timeHorizon) || desc.timeHorizon <= 0.0)
        {
            return fail("Chernoff time horizon must be positive and finite");
        }

        if (desc.timeSamples == 0)
        {
            return fail("Chernoff time sample count must be greater than zero");
        }

        if (desc.productSteps == 0)
        {
            return fail("Chernoff product step count must be greater than zero");
        }

        if (!IsFinite(desc.minDiffusion) || desc.minDiffusion <= 0.0)
        {
            return fail("Chernoff minimum diffusion must be positive and finite");
        }

        if (!IsFinite(desc.maxParticleWeight) || desc.maxParticleWeight < 1.0)
        {
            return fail("Chernoff maximum particle weight must be finite and not less than one");
        }

        return true;
    }

    double ApplyChernoffTranslationProduct(const ChernoffCoefficients1D& coefficients, double x, double t, u32 productSteps, double minDiffusion, double maxParticleWeight)
    {
        if (productSteps == 0 || t <= 0.0)
        {
            return SafeCoefficient(coefficients.g, x, 0.0);
        }

        std::vector<WeightedPoint> current;
        std::vector<WeightedPoint> next;
        current.reserve(static_cast<std::size_t>(1) << std::min<u32>(productSteps, 12));
        next.reserve(3u * current.capacity());
        current.push_back({x, 1.0});

        const double h = t / static_cast<double>(productSteps);
        for (u32 step = 0; step < productSteps; ++step)
        {
            next.clear();
            next.reserve(current.size() * 4u);

            for (const WeightedPoint& point : current)
            {
                const double a = std::max(minDiffusion, SafeCoefficient(coefficients.a, point.x, minDiffusion));
                const double b = SafeCoefficient(coefficients.b, point.x, 0.0);
                const double c = SafeCoefficient(coefficients.c, point.x, 0.0);
                const double diffusionShift = 2.0 * std::sqrt(a * h);
                const double driftShift = 2.0 * b * h;
                const double reactionWeight = h * c;

                next.push_back({point.x + diffusionShift, ClampWeight(point.weight * 0.25, maxParticleWeight)});
                next.push_back({point.x - diffusionShift, ClampWeight(point.weight * 0.25, maxParticleWeight)});
                next.push_back({point.x + driftShift, ClampWeight(point.weight * 0.50, maxParticleWeight)});

                if (reactionWeight != 0.0)
                {
                    next.push_back({point.x, ClampWeight(point.weight * reactionWeight, maxParticleWeight)});
                }
            }

            current.swap(next);
        }

        double value = 0.0;
        for (const WeightedPoint& point : current)
        {
            value += point.weight * SafeCoefficient(coefficients.g, point.x, 0.0);
        }

        return IsFinite(value) ? value : 0.0;
    }

    double EvaluateChernoffResolvent(const ChernoffCoefficients1D& coefficients, double x, const ChernoffResolventDesc& desc)
    {
        std::string error;
        if (!ValidateChernoffInput(coefficients, desc, &error))
        {
            (void)error;
            return 0.0;
        }

        const double dt = desc.timeHorizon / static_cast<double>(desc.timeSamples);
        double integral = 0.0;

        for (u32 i = 0; i < desc.timeSamples; ++i)
        {
            const double t = (static_cast<double>(i) + 0.5) * dt;
            const double product = ApplyChernoffTranslationProduct(
                coefficients,
                x,
                t,
                desc.productSteps,
                desc.minDiffusion,
                desc.maxParticleWeight);
            integral += std::exp(-desc.lambda * t) * product * dt;
        }

        return IsFinite(integral) ? integral : 0.0;
    }

    double EstimateChernoffTail(double lambda, double cNorm, double timeHorizon)
    {
        const double decay = lambda - std::abs(cNorm);
        if (!IsFinite(decay) || decay <= 0.0 || !IsFinite(timeHorizon) || timeHorizon <= 0.0)
        {
            return std::numeric_limits<double>::infinity();
        }

        return std::exp(-decay * timeHorizon) / decay;
    }

    std::vector<ChernoffSample1D> SampleChernoffResolvent(const ChernoffCoefficients1D& coefficients, const ChernoffResolventDesc& desc, double xMin, double xMax, u32 sampleCount)
    {
        std::vector<ChernoffSample1D> samples;
        if (sampleCount == 0)
        {
            return samples;
        }

        if (sampleCount == 1)
        {
            samples.push_back({xMin, EvaluateChernoffResolvent(coefficients, xMin, desc)});
            return samples;
        }

        samples.reserve(sampleCount);
        const double step = (xMax - xMin) / static_cast<double>(sampleCount - 1);
        for (u32 i = 0; i < sampleCount; ++i)
        {
            const double x = xMin + static_cast<double>(i) * step;
            samples.push_back({x, EvaluateChernoffResolvent(coefficients, x, desc)});
        }

        return samples;
    }

    ChernoffCoefficients1D BuildDefaultChernoffProbeCoefficients()
    {
        ChernoffCoefficients1D coefficients{};
        coefficients.a = [](double x)
        {
            return 0.22 + 0.06 * std::sin(0.75 * x);
        };
        coefficients.b = [](double x)
        {
            return 0.08 * std::sin(0.50 * x);
        };
        coefficients.c = [](double x)
        {
            (void)x;
            return -0.35;
        };
        coefficients.g = [](double x)
        {
            return std::exp(-0.35 * x * x);
        };
        return coefficients;
    }

    ChernoffProbeResult BuildDefaultChernoffProbe()
    {
        ChernoffResolventDesc desc{};
        desc.lambda = 2.0;
        desc.timeHorizon = 3.0;
        desc.timeSamples = 28;
        desc.productSteps = 6;
        desc.minDiffusion = 1.0e-5;
        desc.maxParticleWeight = 1.0e5;

        ChernoffProbeResult result{};
        const ChernoffCoefficients1D coefficients = BuildDefaultChernoffProbeCoefficients();
        result.samples = SampleChernoffResolvent(coefficients, desc, -2.0, 2.0, 9);
        result.tailEstimate = EstimateChernoffTail(desc.lambda, 0.35, desc.timeHorizon);

        std::ostringstream out;
        out.setf(std::ios::fixed);
        out.precision(5);
        out << "Chernoff resolvent probe: lambda=" << desc.lambda
            << " T=" << desc.timeHorizon
            << " n=" << desc.productSteps
            << " samples=" << result.samples.size()
            << " tail~" << result.tailEstimate;
        result.summary = out.str();
        return result;
    }
}
