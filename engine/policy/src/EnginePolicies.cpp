#include <AK/Policy/EnginePolicies.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace AK
{
    namespace
    {
        constexpr u64 MaxPermutationCount = std::numeric_limits<u64>::max();

        void AddWarning(PolicyValidationReport& report, std::string message)
        {
            report.ok = false;
            ++report.warnings;
            report.messages.push_back(std::move(message));
        }

        void AddShaderMessage(ShaderPermutationReport& report, std::string message)
        {
            report.messages.push_back(std::move(message));
        }

        u64 SaturatingMultiply(u64 a, u64 b)
        {
            if (a == 0 || b == 0)
            {
                return 0;
            }
            if (a > MaxPermutationCount / b)
            {
                return MaxPermutationCount;
            }
            return a * b;
        }

        std::string JoinMessages(const std::vector<std::string>& messages)
        {
            std::ostringstream out;
            for (usize i = 0; i < messages.size(); ++i)
            {
                if (i != 0)
                {
                    out << "; ";
                }
                out << messages[i];
            }
            return out.str();
        }

        void RefreshSummary(PolicyValidationReport& report, std::string_view name)
        {
            std::ostringstream out;
            out << name << "=" << (report.ok ? "ok" : "risk") << " warnings=" << report.warnings;
            if (!report.messages.empty())
            {
                out << " [" << JoinMessages(report.messages) << "]";
            }
            report.summary = out.str();
        }

        std::string FeatureName(u32 index)
        {
            return "feature_" + std::to_string(index);
        }
    }

    bool ShaderPermutationPlan::RegisterFeature(ShaderFeatureDesc feature)
    {
        if (feature.name.empty() || feature.variants == 0)
        {
            return false;
        }

        const auto duplicate = std::find_if(mFeatures.begin(), mFeatures.end(), [&](const ShaderFeatureDesc& existing)
        {
            return existing.name == feature.name;
        });
        if (duplicate != mFeatures.end())
        {
            return false;
        }

        mFeatures.push_back(std::move(feature));
        return true;
    }

    void ShaderPermutationPlan::Clear()
    {
        mFeatures.clear();
    }

    ShaderPermutationReport ShaderPermutationPlan::Analyze(const ShaderPermutationBudget& budget, BuildMode mode) const
    {
        ShaderPermutationReport report{};
        report.features = static_cast<u32>(mFeatures.size());
        report.cacheBudget = budget.maxCachedPipelines;
        report.fallbackReady = budget.requireFallbackVariant;

        if (report.features > budget.maxKeywordsPerShader)
        {
            report.withinBudget = false;
            AddShaderMessage(report, "too many shader keywords for one shader");
        }

        for (const ShaderFeatureDesc& feature : mFeatures)
        {
            if (mode == BuildMode::Shipping && !feature.shippingAllowed)
            {
                ++report.shippingDisabledFeatures;
                continue;
            }
            if (mode == BuildMode::Shipping && feature.runtimeOnly)
            {
                report.runtimeCompilationRisk = true;
            }
            report.permutations = SaturatingMultiply(report.permutations, feature.variants);
        }

        if (report.permutations > budget.maxPermutationsPerShader)
        {
            report.withinBudget = false;
            AddShaderMessage(report, "per-shader permutation budget exceeded");
        }
        if (report.permutations > budget.maxCachedPipelines)
        {
            report.withinBudget = false;
            AddShaderMessage(report, "pipeline cache budget exceeded");
        }
        if (!budget.requireFallbackVariant)
        {
            report.fallbackReady = false;
            report.withinBudget = false;
            AddShaderMessage(report, "fallback shader variant is required");
        }
        if (mode == BuildMode::Shipping && budget.allowRuntimeCompilationInShipping)
        {
            report.runtimeCompilationRisk = true;
            report.withinBudget = false;
            AddShaderMessage(report, "runtime shader compilation is enabled in shipping");
        }
        if (report.runtimeCompilationRisk)
        {
            report.withinBudget = false;
            AddShaderMessage(report, "runtime-only shader features must be stripped before shipping");
        }

        std::ostringstream out;
        out << "shader permutations=" << report.permutations
            << " features=" << report.features
            << " stripped=" << report.shippingDisabledFeatures
            << " budget=" << budget.maxPermutationsPerShader
            << " status=" << (report.withinBudget ? "ok" : "risk");
        if (!report.messages.empty())
        {
            out << " [" << JoinMessages(report.messages) << "]";
        }
        report.summary = out.str();
        return report;
    }

    const std::vector<ShaderFeatureDesc>& ShaderPermutationPlan::Features() const
    {
        return mFeatures;
    }

    BuildMode DetectBuildMode()
    {
#if defined(AK_BUILD_MODE_SHIPPING)
        return BuildMode::Shipping;
#elif defined(AK_BUILD_MODE_RELEASE)
        return BuildMode::Release;
#elif defined(AK_BUILD_MODE_DEVELOPMENT)
        return BuildMode::Development;
#elif defined(AK_BUILD_MODE_DEBUG) || defined(AK_DEBUG) || !defined(NDEBUG)
        return BuildMode::Debug;
#else
        return BuildMode::Release;
#endif
    }

    BuildModePolicy MakeBuildModePolicy(BuildMode mode)
    {
        BuildModePolicy policy{};
        policy.mode = mode;

        switch (mode)
        {
            case BuildMode::Debug:
                policy.assertions = true;
                policy.validation = true;
                policy.profiling = true;
                policy.editorAllowed = true;
                policy.hotReloadAllowed = true;
                policy.verboseLogging = true;
                policy.crashDumps = true;
                break;
            case BuildMode::Development:
                policy.assertions = true;
                policy.validation = true;
                policy.profiling = true;
                policy.editorAllowed = true;
                policy.hotReloadAllowed = true;
                policy.verboseLogging = true;
                policy.crashDumps = true;
                break;
            case BuildMode::Release:
                policy.assertions = false;
                policy.validation = false;
                policy.profiling = true;
                policy.editorAllowed = false;
                policy.hotReloadAllowed = false;
                policy.verboseLogging = false;
                policy.crashDumps = true;
                break;
            case BuildMode::Shipping:
                policy.assertions = false;
                policy.validation = false;
                policy.profiling = false;
                policy.editorAllowed = false;
                policy.hotReloadAllowed = false;
                policy.verboseLogging = false;
                policy.crashDumps = true;
                break;
        }

        return policy;
    }

    EnginePolicySet MakeDefaultEnginePolicySet(BuildMode mode)
    {
        EnginePolicySet set{};
        set.build = MakeBuildModePolicy(mode);
        set.units = {};
        set.coordinates = {};
        set.gpuSync = {};
        set.shaderBudget = {};
        return set;
    }

    PolicyValidationReport ValidateBuildModePolicy(const BuildModePolicy& policy)
    {
        PolicyValidationReport report{};
        if ((policy.mode == BuildMode::Debug || policy.mode == BuildMode::Development) && !policy.assertions)
        {
            AddWarning(report, "debug/development builds must keep assertions enabled");
        }
        if ((policy.mode == BuildMode::Debug || policy.mode == BuildMode::Development) && !policy.validation)
        {
            AddWarning(report, "debug/development builds must keep validation enabled");
        }
        if (policy.mode == BuildMode::Shipping)
        {
            if (policy.editorAllowed)
            {
                AddWarning(report, "shipping build must not include editor-only systems");
            }
            if (policy.hotReloadAllowed)
            {
                AddWarning(report, "shipping build must not allow hot reload");
            }
            if (policy.verboseLogging)
            {
                AddWarning(report, "shipping build must not use verbose logging by default");
            }
            if (policy.validation)
            {
                AddWarning(report, "shipping build must not require validation layers");
            }
        }
        RefreshSummary(report, "build-policy");
        return report;
    }

    PolicyValidationReport ValidateUnitsPolicy(const UnitsPolicy& policy)
    {
        PolicyValidationReport report{};
        if (!std::isfinite(policy.metersPerUnit) || policy.metersPerUnit <= 0.0)
        {
            AddWarning(report, "meters-per-unit must be finite and positive");
        }
        if (std::fabs(policy.metersPerUnit - 1.0) > 0.000001)
        {
            AddWarning(report, "engine canonical unit should remain 1 unit = 1 meter");
        }
        if (!policy.secondsForTime)
        {
            AddWarning(report, "time unit must be seconds");
        }
        if (!policy.kilogramsForMass)
        {
            AddWarning(report, "mass unit must be kilograms");
        }
        if (policy.internalAngles != AngleUnit::Radians)
        {
            AddWarning(report, "math and physics must use radians internally");
        }
        if (!policy.degreesAllowedInUiOnly)
        {
            AddWarning(report, "degrees should be restricted to UI/import boundaries");
        }
        RefreshSummary(report, "units-policy");
        return report;
    }

    PolicyValidationReport ValidateCoordinatePolicy(const CoordinatePolicy& policy)
    {
        PolicyValidationReport report{};
        if (policy.handedness != Handedness::RightHanded)
        {
            AddWarning(report, "canonical world handedness should be right-handed");
        }
        if (policy.upAxis != UpAxis::Y)
        {
            AddWarning(report, "canonical world up-axis should be Y-up");
        }
        if (policy.matrixLayout != MatrixLayout::ColumnMajor)
        {
            AddWarning(report, "matrix layout must stay explicit; default policy is column-major");
        }
        if (policy.clipDepthRange != ClipDepthRange::ZeroToOne)
        {
            AddWarning(report, "Vulkan/DX style clip depth should be zero-to-one");
        }
        if (!policy.cameraRelativeGpuPositions)
        {
            AddWarning(report, "GPU positions must be camera-relative in large worlds");
        }
        if (!policy.reversedZDepth)
        {
            AddWarning(report, "renderer must use reversed-Z depth policy");
        }
        RefreshSummary(report, "coordinate-policy");
        return report;
    }

    PolicyValidationReport ValidateGpuSynchronizationPolicy(const GpuSynchronizationPolicy& policy)
    {
        PolicyValidationReport report{};
        if (policy.framesInFlight < 2 || policy.framesInFlight > 4)
        {
            AddWarning(report, "frames-in-flight should stay in the 2..4 range");
        }
        if (policy.deferredReleaseFrames < policy.framesInFlight)
        {
            AddWarning(report, "deferred resource release must be at least frames-in-flight");
        }
        if (!policy.duplicatePerFrameWritableResources)
        {
            AddWarning(report, "per-frame writable GPU resources must be duplicated");
        }
        if (policy.allowHotFrameReadback)
        {
            AddWarning(report, "GPU readback in the hot frame path is forbidden");
        }
        if (policy.allowDeviceWaitIdleInFrame)
        {
            AddWarning(report, "device/queue idle waits inside frame code are forbidden");
        }
        if (!policy.preferTimelineSemaphores)
        {
            AddWarning(report, "timeline semaphore path should remain the preferred backend contract");
        }
        RefreshSummary(report, "gpu-sync-policy");
        return report;
    }

    ShaderPermutationPlan BuildDefaultShaderPermutationPlan()
    {
        ShaderPermutationPlan plan;
        plan.RegisterFeature({"skinning", 2, true, false});
        plan.RegisterFeature({"gpu_instancing", 2, true, false});
        plan.RegisterFeature({"alpha_test", 2, true, false});
        plan.RegisterFeature({"normal_map", 2, true, false});
        plan.RegisterFeature({"clearcoat", 2, true, false});
        plan.RegisterFeature({"fog", 2, true, false});
        plan.RegisterFeature({"shadow_receiver", 2, true, false});
        plan.RegisterFeature({"decals", 2, true, false});
        return plan;
    }

    ShaderPermutationPlan BuildPermutationExplosionTestPlan(u32 binaryFeatureCount)
    {
        ShaderPermutationPlan plan;
        for (u32 i = 0; i < binaryFeatureCount; ++i)
        {
            plan.RegisterFeature({FeatureName(i), 2, true, false});
        }
        return plan;
    }

    FutureLimitProbeResult BuildFutureLimitProbe()
    {
        FutureLimitProbeResult result{};
        result.policies = MakeDefaultEnginePolicySet();
        result.buildReport = ValidateBuildModePolicy(result.policies.build);
        result.unitsReport = ValidateUnitsPolicy(result.policies.units);
        result.coordinateReport = ValidateCoordinatePolicy(result.policies.coordinates);
        result.gpuSyncReport = ValidateGpuSynchronizationPolicy(result.policies.gpuSync);
        result.safeShaderReport = BuildDefaultShaderPermutationPlan().Analyze(result.policies.shaderBudget, result.policies.build.mode);
        result.rejectedShaderReport = BuildPermutationExplosionTestPlan(20).Analyze(result.policies.shaderBudget, result.policies.build.mode);

        result.ok = result.buildReport.ok
            && result.unitsReport.ok
            && result.coordinateReport.ok
            && result.gpuSyncReport.ok
            && result.safeShaderReport.withinBudget
            && !result.rejectedShaderReport.withinBudget;

        std::ostringstream out;
        out << "Future limit guards: " << (result.ok ? "ok" : "risk")
            << " mode=" << ToString(result.policies.build.mode)
            << " gpu_frames=" << result.policies.gpuSync.framesInFlight
            << " shader_safe=" << result.safeShaderReport.permutations
            << " shader_rejected=" << result.rejectedShaderReport.permutations;
        result.summary = out.str();
        return result;
    }

    std::string BuildFutureLimitProbeSummary()
    {
        return BuildFutureLimitProbe().summary;
    }

    const char* ToString(BuildMode mode)
    {
        switch (mode)
        {
            case BuildMode::Debug: return "debug";
            case BuildMode::Development: return "development";
            case BuildMode::Release: return "release";
            case BuildMode::Shipping: return "shipping";
            default: return "unknown";
        }
    }

    const char* ToString(AngleUnit unit)
    {
        switch (unit)
        {
            case AngleUnit::Radians: return "radians";
            case AngleUnit::Degrees: return "degrees";
            default: return "unknown";
        }
    }

    const char* ToString(Handedness handedness)
    {
        switch (handedness)
        {
            case Handedness::RightHanded: return "right-handed";
            case Handedness::LeftHanded: return "left-handed";
            default: return "unknown";
        }
    }

    const char* ToString(UpAxis axis)
    {
        switch (axis)
        {
            case UpAxis::Y: return "Y-up";
            case UpAxis::Z: return "Z-up";
            default: return "unknown";
        }
    }

    const char* ToString(MatrixLayout layout)
    {
        switch (layout)
        {
            case MatrixLayout::ColumnMajor: return "column-major";
            case MatrixLayout::RowMajor: return "row-major";
            default: return "unknown";
        }
    }

    const char* ToString(ClipDepthRange range)
    {
        switch (range)
        {
            case ClipDepthRange::ZeroToOne: return "0..1";
            case ClipDepthRange::MinusOneToOne: return "-1..1";
            default: return "unknown";
        }
    }

    const char* ToString(FrontFaceWinding winding)
    {
        switch (winding)
        {
            case FrontFaceWinding::CounterClockwise: return "ccw";
            case FrontFaceWinding::Clockwise: return "cw";
            default: return "unknown";
        }
    }

    std::string ToDebugString(const PolicyValidationReport& report)
    {
        return report.summary;
    }

    std::string ToDebugString(const ShaderPermutationReport& report)
    {
        return report.summary;
    }
}
