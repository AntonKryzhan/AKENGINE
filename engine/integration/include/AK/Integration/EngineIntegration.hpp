#pragma once

#include <AK/Core/Types.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class EngineFrameStageKind : u32
    {
        Input = 0,
        Commands = 1,
        FixedClock = 2,
        Physics = 3,
        Gameplay = 4,
        Streaming = 5,
        AssetResidency = 6,
        Visibility = 7,
        RenderExtraction = 8,
        RenderGraphBuild = 9,
        RhiSubmit = 10,
        Present = 11,
        Diagnostics = 12
    };

    enum class EngineFrameThreadDomain : u32
    {
        MainThread = 0,
        JobSystem = 1,
        RenderThread = 2,
        GpuQueue = 3
    };

    struct EngineFrameStageDesc
    {
        EngineFrameStageKind kind = EngineFrameStageKind::Input;
        EngineFrameThreadDomain domain = EngineFrameThreadDomain::MainThread;
        std::string name;
        std::vector<EngineFrameStageKind> dependsOn;
        bool fixedTickOnly = false;
        bool deterministicOrderRequired = true;
        bool canRunAsync = false;
        bool writesSceneState = false;
        bool writesRenderState = false;
        bool gpuWork = false;
    };

    struct EngineIntegrationConfig
    {
        u32 fixedTicksThisFrame = 1;
        u32 maxFixedTicksPerFrame = 4;
        bool enablePhysics = true;
        bool enableStreaming = true;
        bool enableRenderExtraction = true;
        bool enableVulkanRhi = true;
        bool enableDiagnostics = true;
        bool deterministicFrameMerge = true;
    };

    struct EngineFrameIntegrationStats
    {
        u32 stages = 0;
        u32 mainThreadStages = 0;
        u32 jobStages = 0;
        u32 renderThreadStages = 0;
        u32 gpuStages = 0;
        u32 asyncCandidates = 0;
        u32 deterministicStages = 0;
        u32 sceneWriteStages = 0;
        u32 renderWriteStages = 0;
        u32 fixedTickStages = 0;
        u32 dependencyWarnings = 0;
        u32 orderingWarnings = 0;
        bool valid = true;
    };

    class EngineFrameIntegrationPlan final
    {
    public:
        bool AddStage(EngineFrameStageDesc stage);
        EngineFrameIntegrationStats Validate() const;
        const std::vector<EngineFrameStageDesc>& Stages() const;

    private:
        bool ContainsStage(EngineFrameStageKind kind) const;
        int StageIndex(EngineFrameStageKind kind) const;

        std::vector<EngineFrameStageDesc> mStages;
    };

    struct EngineIntegrationProbe
    {
        EngineIntegrationConfig config{};
        EngineFrameIntegrationPlan plan{};
        EngineFrameIntegrationStats stats{};
        bool renderAfterVisibility = false;
        bool rhiAfterRenderGraph = false;
        bool diagnosticsLast = false;
        bool ok = false;
        std::string summary;
    };

    EngineFrameIntegrationPlan BuildDefaultEngineFrameIntegrationPlan(const EngineIntegrationConfig& config = {});
    EngineIntegrationProbe BuildEngineIntegrationProbe(const EngineIntegrationConfig& config = {});

    const char* ToString(EngineFrameStageKind kind);
    const char* ToString(EngineFrameThreadDomain domain);
    std::string ToDebugString(const EngineFrameIntegrationStats& stats);
    std::string ToDebugString(const EngineIntegrationProbe& probe);
}
