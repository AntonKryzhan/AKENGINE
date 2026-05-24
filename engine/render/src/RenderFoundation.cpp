#include <AK/Render/RenderFoundation.hpp>

#include <AK/Math/Numerics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace AK
{
    DepthPolicy MakeDefaultDepthPolicy()
    {
        DepthPolicy policy{};
        policy.convention = DepthConvention::ReversedZ;
        policy.nearPlane = 0.05f;
        policy.farPlane = 100000.0f;
        policy.infiniteFarPlane = true;
        policy.floatingPointDepth = true;
        return policy;
    }

    DepthPrecisionReport AnalyzeDepthPolicy(const DepthPolicy& policy)
    {
        DepthPrecisionReport report{};
        report.nearPlane = policy.nearPlane;
        report.farPlane = policy.farPlane;
        report.reversedZ = policy.convention == DepthConvention::ReversedZ;
        report.infiniteFarPlane = policy.infiniteFarPlane;
        report.floatingPointDepth = policy.floatingPointDepth;
        report.valid = std::isfinite(policy.nearPlane) && std::isfinite(policy.farPlane) && policy.nearPlane > 0.0f && policy.farPlane > policy.nearPlane;

        if (!report.valid)
        {
            report.recommendation = "invalid near/far depth policy";
            report.farNearRatio = 0.0;
            report.nearPlaneTooSmall = true;
            report.farNearRatioRisk = true;
            report.standardDepthRisk = true;
            return report;
        }

        report.farNearRatio = static_cast<double>(policy.farPlane) / static_cast<double>(policy.nearPlane);
        report.nearPlaneTooSmall = policy.nearPlane < 0.01f;
        report.farNearRatioRisk = report.farNearRatio > 100000.0;
        report.standardDepthRisk = !report.reversedZ && report.farNearRatioRisk;

        if (report.reversedZ && report.floatingPointDepth)
        {
            report.recommendation = report.nearPlaneTooSmall
                ? "raise near plane if possible; reversed-Z is enabled"
                : "safe default: reversed-Z + floating depth + camera-relative positions";
        }
        else if (!report.reversedZ)
        {
            report.recommendation = "enable reversed-Z before Vulkan/DX12 renderer";
        }
        else
        {
            report.recommendation = "prefer floating point depth format with reversed-Z";
        }

        return report;
    }

    float ProjectViewDepthToDeviceZ(float viewDepthMeters, const DepthPolicy& policy)
    {
        if (!std::isfinite(viewDepthMeters) || viewDepthMeters <= 0.0f || policy.nearPlane <= 0.0f)
        {
            return 0.0f;
        }

        const float z = std::max(viewDepthMeters, policy.nearPlane);
        if (policy.convention == DepthConvention::ReversedZ)
        {
            if (policy.infiniteFarPlane)
            {
                return std::clamp(policy.nearPlane / z, 0.0f, 1.0f);
            }

            const float denominator = policy.farPlane - policy.nearPlane;
            if (denominator <= 0.0f)
            {
                return 0.0f;
            }
            return std::clamp((policy.farPlane - z) / denominator, 0.0f, 1.0f);
        }

        const float denominator = policy.farPlane - policy.nearPlane;
        if (denominator <= 0.0f)
        {
            return 0.0f;
        }
        return std::clamp((z - policy.nearPlane) / denominator, 0.0f, 1.0f);
    }

    CameraRelativeRenderPosition MakeCameraRelativeRenderPosition(WorldPosition position, WorldPosition camera, const LargeWorldConfig& config)
    {
        const CameraRelativePosition relative = ToCameraRelativeFloat(position, camera, config);
        CameraRelativeRenderPosition result{};
        result.value = relative.value;
        result.distanceMeters = relative.distanceMeters;
        result.estimatedFloatStepMeters = relative.estimatedFloatStepMeters;
        result.finite = relative.finite && IsFinite(relative.value);
        result.precisionRisk = relative.precisionRisk;
        return result;
    }

    bool RenderGraphPlan::AddResource(RenderGraphResourceDesc resource)
    {
        if (resource.name.empty() || FindResource(resource.name) != nullptr)
        {
            return false;
        }

        mResources.push_back(std::move(resource));
        return true;
    }

    bool RenderGraphPlan::AddPass(RenderGraphPassDesc pass)
    {
        if (pass.name.empty())
        {
            return false;
        }

        mPasses.push_back(std::move(pass));
        return true;
    }

    RenderGraphStats RenderGraphPlan::Build() const
    {
        RenderGraphStats stats{};
        stats.resources = static_cast<u32>(mResources.size());
        stats.passes = static_cast<u32>(mPasses.size());

        std::unordered_set<std::string> knownResources;
        for (const RenderGraphResourceDesc& resource : mResources)
        {
            knownResources.insert(resource.name);
            if (resource.transient)
            {
                ++stats.transientResources;
            }
            if (resource.imported)
            {
                ++stats.importedResources;
            }
        }

        for (const RenderGraphPassDesc& pass : mPasses)
        {
            if (pass.asyncCompute)
            {
                ++stats.asyncComputePasses;
            }
            if (pass.present)
            {
                ++stats.presentPasses;
            }

            for (const std::string& read : pass.reads)
            {
                if (knownResources.find(read) == knownResources.end())
                {
                    ++stats.validationWarnings;
                }
            }

            for (const std::string& write : pass.writes)
            {
                if (knownResources.find(write) == knownResources.end())
                {
                    ++stats.validationWarnings;
                }
            }

            if (!pass.reads.empty() && !pass.writes.empty())
            {
                ++stats.barriersEstimated;
            }
        }

        if (stats.presentPasses != 1)
        {
            ++stats.validationWarnings;
        }

        return stats;
    }

    const std::vector<RenderGraphResourceDesc>& RenderGraphPlan::Resources() const
    {
        return mResources;
    }

    const std::vector<RenderGraphPassDesc>& RenderGraphPlan::Passes() const
    {
        return mPasses;
    }

    const RenderGraphResourceDesc* RenderGraphPlan::FindResource(const std::string& name) const
    {
        for (const RenderGraphResourceDesc& resource : mResources)
        {
            if (resource.name == name)
            {
                return &resource;
            }
        }

        return nullptr;
    }

    RenderGraphPlan BuildDefaultRenderGraphPlan()
    {
        RenderGraphPlan graph;
        graph.AddResource({"swapchain", RenderResourceUsage::Present, RenderResourceUsage::Present, false, true});
        graph.AddResource({"scene_color", RenderResourceUsage::ColorAttachment, RenderResourceUsage::ShaderRead, true, false});
        graph.AddResource({"scene_depth", RenderResourceUsage::DepthAttachment, RenderResourceUsage::ShaderRead, true, false});
        graph.AddResource({"ui_color", RenderResourceUsage::ColorAttachment, RenderResourceUsage::Present, true, false});

        graph.AddPass({"depth_prepass", {}, {"scene_depth"}, false, false});
        graph.AddPass({"main_color", {"scene_depth"}, {"scene_color"}, false, false});
        graph.AddPass({"editor_ui", {"scene_color"}, {"ui_color"}, false, false});
        graph.AddPass({"present", {"ui_color"}, {"swapchain"}, false, true});
        return graph;
    }

    RenderFoundationProbe BuildRenderFoundationProbe()
    {
        RenderFoundationProbe probe{};
        probe.depthPolicy = MakeDefaultDepthPolicy();
        probe.depthReport = AnalyzeDepthPolicy(probe.depthPolicy);
        probe.graphStats = BuildDefaultRenderGraphPlan().Build();
        probe.cameraRelative = MakeCameraRelativeRenderPosition(MakeWorldPosition(1000000.25, 2.0, -1000000.5), MakeWorldPosition(1000000.0, 2.0, -1000000.0));

        std::ostringstream out;
        out << "Render foundation: " << ToString(probe.depthPolicy.convention)
            << ", graph passes=" << probe.graphStats.passes
            << ", resources=" << probe.graphStats.resources
            << ", camera-relative step=" << std::scientific << std::setprecision(2) << probe.cameraRelative.estimatedFloatStepMeters << "m";
        probe.summary = out.str();
        return probe;
    }

    const char* ToString(DepthConvention convention)
    {
        switch (convention)
        {
            case DepthConvention::ReversedZ:
                return "reversed-Z";
            case DepthConvention::StandardZ:
                return "standard-Z";
            default:
                return "unknown-Z";
        }
    }

    const char* ToString(RenderResourceUsage usage)
    {
        switch (usage)
        {
            case RenderResourceUsage::ColorAttachment:
                return "color attachment";
            case RenderResourceUsage::DepthAttachment:
                return "depth attachment";
            case RenderResourceUsage::ShaderRead:
                return "shader read";
            case RenderResourceUsage::TransferSrc:
                return "transfer src";
            case RenderResourceUsage::TransferDst:
                return "transfer dst";
            case RenderResourceUsage::Present:
                return "present";
            default:
                return "unknown";
        }
    }

    std::string ToDebugString(const DepthPrecisionReport& report)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << (report.reversedZ ? "reversed-Z" : "standard-Z")
            << " near=" << report.nearPlane
            << " far=" << report.farPlane
            << " ratio=" << std::scientific << std::setprecision(2) << report.farNearRatio
            << " risk=" << ((report.nearPlaneTooSmall || report.standardDepthRisk) ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const RenderGraphStats& stats)
    {
        std::ostringstream out;
        out << "passes=" << stats.passes
            << " resources=" << stats.resources
            << " transient=" << stats.transientResources
            << " imported=" << stats.importedResources
            << " barriers~" << stats.barriersEstimated
            << " warnings=" << stats.validationWarnings;
        return out.str();
    }
}
