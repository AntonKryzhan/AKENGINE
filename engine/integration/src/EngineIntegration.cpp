#include <AK/Integration/EngineIntegration.hpp>

#include <algorithm>
#include <sstream>

namespace AK
{
    bool EngineFrameIntegrationPlan::AddStage(EngineFrameStageDesc stage)
    {
        if (stage.name.empty() || ContainsStage(stage.kind))
        {
            return false;
        }
        mStages.push_back(std::move(stage));
        return true;
    }

    EngineFrameIntegrationStats EngineFrameIntegrationPlan::Validate() const
    {
        EngineFrameIntegrationStats stats{};
        stats.stages = static_cast<u32>(mStages.size());

        for (std::size_t i = 0; i < mStages.size(); ++i)
        {
            const EngineFrameStageDesc& stage = mStages[i];
            switch (stage.domain)
            {
                case EngineFrameThreadDomain::MainThread:
                    ++stats.mainThreadStages;
                    break;
                case EngineFrameThreadDomain::JobSystem:
                    ++stats.jobStages;
                    break;
                case EngineFrameThreadDomain::RenderThread:
                    ++stats.renderThreadStages;
                    break;
                case EngineFrameThreadDomain::GpuQueue:
                    ++stats.gpuStages;
                    break;
            }

            if (stage.canRunAsync)
            {
                ++stats.asyncCandidates;
            }
            if (stage.deterministicOrderRequired)
            {
                ++stats.deterministicStages;
            }
            if (stage.writesSceneState)
            {
                ++stats.sceneWriteStages;
            }
            if (stage.writesRenderState)
            {
                ++stats.renderWriteStages;
            }
            if (stage.fixedTickOnly)
            {
                ++stats.fixedTickStages;
            }
            if (stage.gpuWork && stage.domain != EngineFrameThreadDomain::GpuQueue)
            {
                ++stats.orderingWarnings;
            }

            for (EngineFrameStageKind dependency : stage.dependsOn)
            {
                const int dependencyIndex = StageIndex(dependency);
                if (dependencyIndex < 0 || dependencyIndex >= static_cast<int>(i))
                {
                    ++stats.dependencyWarnings;
                }
            }
        }

        const int physics = StageIndex(EngineFrameStageKind::Physics);
        const int visibility = StageIndex(EngineFrameStageKind::Visibility);
        const int extraction = StageIndex(EngineFrameStageKind::RenderExtraction);
        const int graph = StageIndex(EngineFrameStageKind::RenderGraphBuild);
        const int rhi = StageIndex(EngineFrameStageKind::RhiSubmit);
        const int present = StageIndex(EngineFrameStageKind::Present);
        const int diagnostics = StageIndex(EngineFrameStageKind::Diagnostics);

        if (physics >= 0 && visibility >= 0 && visibility < physics)
        {
            ++stats.orderingWarnings;
        }
        if (extraction >= 0 && visibility >= 0 && extraction < visibility)
        {
            ++stats.orderingWarnings;
        }
        if (rhi >= 0 && graph >= 0 && rhi < graph)
        {
            ++stats.orderingWarnings;
        }
        if (present >= 0 && rhi >= 0 && present < rhi)
        {
            ++stats.orderingWarnings;
        }
        if (diagnostics >= 0 && diagnostics != static_cast<int>(mStages.size()) - 1)
        {
            ++stats.orderingWarnings;
        }

        stats.valid = stats.stages > 0 && stats.dependencyWarnings == 0 && stats.orderingWarnings == 0;
        return stats;
    }

    const std::vector<EngineFrameStageDesc>& EngineFrameIntegrationPlan::Stages() const
    {
        return mStages;
    }

    bool EngineFrameIntegrationPlan::ContainsStage(EngineFrameStageKind kind) const
    {
        return StageIndex(kind) >= 0;
    }

    int EngineFrameIntegrationPlan::StageIndex(EngineFrameStageKind kind) const
    {
        for (std::size_t i = 0; i < mStages.size(); ++i)
        {
            if (mStages[i].kind == kind)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    EngineFrameIntegrationPlan BuildDefaultEngineFrameIntegrationPlan(const EngineIntegrationConfig& config)
    {
        EngineFrameIntegrationPlan plan;
        plan.AddStage({EngineFrameStageKind::Input, EngineFrameThreadDomain::MainThread, "InputActions", {}, false, true, false, false, false, false});
        plan.AddStage({EngineFrameStageKind::Commands, EngineFrameThreadDomain::MainThread, "CommandRegistry", {EngineFrameStageKind::Input}, false, true, false, true, false, false});
        plan.AddStage({EngineFrameStageKind::FixedClock, EngineFrameThreadDomain::MainThread, "FixedTickClock", {EngineFrameStageKind::Commands}, false, true, false, false, false, false});

        if (config.enablePhysics)
        {
            plan.AddStage({EngineFrameStageKind::Physics, EngineFrameThreadDomain::JobSystem, "PhysicsAndSimulation", {EngineFrameStageKind::FixedClock}, true, true, true, true, false, false});
            plan.AddStage({EngineFrameStageKind::Gameplay, EngineFrameThreadDomain::MainThread, "GameplayEventsAndDamage", {EngineFrameStageKind::Physics}, true, true, false, true, false, false});
        }
        else
        {
            plan.AddStage({EngineFrameStageKind::Gameplay, EngineFrameThreadDomain::MainThread, "GameplayEventsAndDamage", {EngineFrameStageKind::FixedClock}, true, true, false, true, false, false});
        }

        if (config.enableStreaming)
        {
            plan.AddStage({EngineFrameStageKind::Streaming, EngineFrameThreadDomain::JobSystem, "WorldPartitionStreaming", {EngineFrameStageKind::Gameplay}, false, true, true, true, false, false});
            plan.AddStage({EngineFrameStageKind::AssetResidency, EngineFrameThreadDomain::JobSystem, "AssetResidency", {EngineFrameStageKind::Streaming}, false, true, true, false, true, false});
        }
        else
        {
            plan.AddStage({EngineFrameStageKind::AssetResidency, EngineFrameThreadDomain::MainThread, "AssetResidency", {EngineFrameStageKind::Gameplay}, false, true, false, false, true, false});
        }

        plan.AddStage({EngineFrameStageKind::Visibility, EngineFrameThreadDomain::JobSystem, "BoundsVisibility", {EngineFrameStageKind::AssetResidency}, false, true, true, false, true, false});

        if (config.enableRenderExtraction)
        {
            plan.AddStage({EngineFrameStageKind::RenderExtraction, EngineFrameThreadDomain::RenderThread, "RenderExtraction", {EngineFrameStageKind::Visibility}, false, true, false, false, true, false});
        }

        plan.AddStage({EngineFrameStageKind::RenderGraphBuild, EngineFrameThreadDomain::RenderThread, "RenderGraphBuild", {EngineFrameStageKind::RenderExtraction}, false, true, false, false, true, false});
        if (config.enableVulkanRhi)
        {
            plan.AddStage({EngineFrameStageKind::RhiSubmit, EngineFrameThreadDomain::GpuQueue, "VulkanRhiSubmit", {EngineFrameStageKind::RenderGraphBuild}, false, true, false, false, false, true});
        }
        plan.AddStage({EngineFrameStageKind::Present, EngineFrameThreadDomain::GpuQueue, "Present", {EngineFrameStageKind::RhiSubmit}, false, true, false, false, false, true});

        if (config.enableDiagnostics)
        {
            plan.AddStage({EngineFrameStageKind::Diagnostics, EngineFrameThreadDomain::MainThread, "DiagnosticsAndTelemetry", {EngineFrameStageKind::Present}, false, true, false, false, false, false});
        }
        return plan;
    }

    EngineIntegrationProbe BuildEngineIntegrationProbe(const EngineIntegrationConfig& config)
    {
        EngineIntegrationProbe probe{};
        probe.config = config;
        probe.plan = BuildDefaultEngineFrameIntegrationPlan(config);
        probe.stats = probe.plan.Validate();

        const auto& stages = probe.plan.Stages();
        auto findIndex = [&stages](EngineFrameStageKind kind) -> int
        {
            for (std::size_t i = 0; i < stages.size(); ++i)
            {
                if (stages[i].kind == kind)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };

        const int visibility = findIndex(EngineFrameStageKind::Visibility);
        const int extraction = findIndex(EngineFrameStageKind::RenderExtraction);
        const int graph = findIndex(EngineFrameStageKind::RenderGraphBuild);
        const int rhi = findIndex(EngineFrameStageKind::RhiSubmit);
        const int diagnostics = findIndex(EngineFrameStageKind::Diagnostics);

        probe.renderAfterVisibility = visibility >= 0 && extraction > visibility;
        probe.rhiAfterRenderGraph = graph >= 0 && rhi > graph;
        probe.diagnosticsLast = diagnostics >= 0 && diagnostics == static_cast<int>(stages.size()) - 1;
        probe.ok = probe.stats.valid && probe.renderAfterVisibility && probe.rhiAfterRenderGraph && probe.diagnosticsLast;

        std::ostringstream out;
        out << "engine integration ok=" << (probe.ok ? "true" : "false")
            << " stages=" << probe.stats.stages
            << " job=" << probe.stats.jobStages
            << " render=" << probe.stats.renderThreadStages
            << " gpu=" << probe.stats.gpuStages
            << " warnings=" << (probe.stats.dependencyWarnings + probe.stats.orderingWarnings);
        probe.summary = out.str();
        return probe;
    }

    const char* ToString(EngineFrameStageKind kind)
    {
        switch (kind)
        {
            case EngineFrameStageKind::Input: return "Input";
            case EngineFrameStageKind::Commands: return "Commands";
            case EngineFrameStageKind::FixedClock: return "FixedClock";
            case EngineFrameStageKind::Physics: return "Physics";
            case EngineFrameStageKind::Gameplay: return "Gameplay";
            case EngineFrameStageKind::Streaming: return "Streaming";
            case EngineFrameStageKind::AssetResidency: return "AssetResidency";
            case EngineFrameStageKind::Visibility: return "Visibility";
            case EngineFrameStageKind::RenderExtraction: return "RenderExtraction";
            case EngineFrameStageKind::RenderGraphBuild: return "RenderGraphBuild";
            case EngineFrameStageKind::RhiSubmit: return "RhiSubmit";
            case EngineFrameStageKind::Present: return "Present";
            case EngineFrameStageKind::Diagnostics: return "Diagnostics";
            default: return "Unknown";
        }
    }

    const char* ToString(EngineFrameThreadDomain domain)
    {
        switch (domain)
        {
            case EngineFrameThreadDomain::MainThread: return "main";
            case EngineFrameThreadDomain::JobSystem: return "jobs";
            case EngineFrameThreadDomain::RenderThread: return "render";
            case EngineFrameThreadDomain::GpuQueue: return "gpu";
            default: return "unknown";
        }
    }

    std::string ToDebugString(const EngineFrameIntegrationStats& stats)
    {
        std::ostringstream out;
        out << "stages=" << stats.stages
            << " main=" << stats.mainThreadStages
            << " jobs=" << stats.jobStages
            << " render=" << stats.renderThreadStages
            << " gpu=" << stats.gpuStages
            << " async=" << stats.asyncCandidates
            << " deterministic=" << stats.deterministicStages
            << " scene_writes=" << stats.sceneWriteStages
            << " render_writes=" << stats.renderWriteStages
            << " warnings=" << (stats.dependencyWarnings + stats.orderingWarnings)
            << " valid=" << (stats.valid ? "true" : "false");
        return out.str();
    }

    std::string ToDebugString(const EngineIntegrationProbe& probe)
    {
        std::ostringstream out;
        out << probe.summary
            << " render_after_visibility=" << (probe.renderAfterVisibility ? "true" : "false")
            << " rhi_after_graph=" << (probe.rhiAfterRenderGraph ? "true" : "false")
            << " diagnostics_last=" << (probe.diagnosticsLast ? "true" : "false");
        return out.str();
    }
}
