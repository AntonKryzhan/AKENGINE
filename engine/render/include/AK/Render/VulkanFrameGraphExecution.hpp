#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Render/VulkanFrameGraphCompiler.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class VulkanExecutionCommandKind : u32
    {
        BeginFrame = 0,
        AcquireSwapchainImage = 1,
        ResetCommandPool = 2,
        BeginPrimaryCommandBuffer = 3,
        BeginSecondaryCommandBuffer = 4,
        PipelineBarrier2 = 5,
        CopyBuffer = 6,
        BeginDynamicRendering = 7,
        BindGraphicsPipeline = 8,
        BindDescriptorSets = 9,
        BindVertexBuffer = 10,
        BindIndexBuffer = 11,
        SetViewport = 12,
        SetScissor = 13,
        PushConstants = 14,
        DrawIndexed = 15,
        Dispatch = 16,
        ExecuteSecondaryCommandBuffers = 17,
        EndDynamicRendering = 18,
        EndCommandBuffer = 19,
        QueueSubmit2 = 20,
        Present = 21,
        EndFrame = 22,
        DebugLabel = 23
    };

    enum class VulkanExecutionHazardPolicy : u32
    {
        Strict = 0,
        SkipRedundant = 1,
        DiagnosticOnly = 2
    };

    enum class VulkanExecutionMemoryPolicy : u32
    {
        PreallocatedFrameResources = 0,
        TransientAliasHeaps = 1,
        ImportedExternalResources = 2
    };

    struct VulkanExecutionConfig
    {
        VulkanExecutionHazardPolicy hazardPolicy = VulkanExecutionHazardPolicy::Strict;
        VulkanExecutionMemoryPolicy memoryPolicy = VulkanExecutionMemoryPolicy::TransientAliasHeaps;
        bool useSynchronization2 = true;
        bool useDynamicRendering = true;
        bool useTimelineSemaphores = true;
        bool useSecondaryCommandBuffers = true;
        bool resetCommandPoolsPerFrame = true;
        bool allowAsyncCompute = true;
        bool allowDedicatedTransfer = true;
        bool forbidGpuReadbackInFrame = true;
        bool forbidRuntimeAllocationInFrame = true;
        u32 maxCommandsPerFrame = 16384;
    };

    struct VulkanExecutionCommand
    {
        VulkanExecutionCommandKind kind = VulkanExecutionCommandKind::DebugLabel;
        VulkanGraphQueueClass queue = VulkanGraphQueueClass::Graphics;
        std::string label;
        std::string resource;
        u32 passIndex = 0xFFFFFFFFu;
        u32 batchIndex = 0xFFFFFFFFu;
        u32 commandBufferIndex = 0xFFFFFFFFu;
        u32 secondaryCommandBufferCount = 0;
        u32 barrierIndex = 0xFFFFFFFFu;
        u32 drawIndex = 0xFFFFFFFFu;
        u32 copyIndex = 0xFFFFFFFFu;
        u32 dispatchGroupsX = 0;
        u32 dispatchGroupsY = 0;
        u32 dispatchGroupsZ = 0;
        u32 firstIndex = 0;
        u32 indexCount = 0;
        i32 vertexOffset = 0;
        u32 instanceCount = 1;
        u64 bytes = 0;
        u64 timelineValue = 0;
        bool secondary = false;
        bool dynamicRendering = false;
        bool valid = false;
    };

    struct VulkanExecutionPassPlan
    {
        std::string name;
        VulkanGraphPassKind kind = VulkanGraphPassKind::Graphics;
        VulkanGraphQueueClass queue = VulkanGraphQueueClass::Graphics;
        u32 passIndex = 0;
        u32 primaryCommandBuffer = 0;
        u32 firstCommand = 0;
        u32 commandCount = 0;
        u32 barrierCommands = 0;
        u32 copyCommands = 0;
        u32 drawCommands = 0;
        u32 dispatchCommands = 0;
        u32 secondaryCommandBuffers = 0;
        bool opensDynamicRendering = false;
        bool closesDynamicRendering = false;
        bool present = false;
        bool valid = false;
    };

    struct VulkanExecutionSubmitPlan
    {
        VulkanGraphQueueClass queue = VulkanGraphQueueClass::Graphics;
        u32 batchIndex = 0;
        u32 firstPass = 0;
        u32 passCount = 0;
        u32 firstCommandBuffer = 0;
        u32 commandBufferCount = 0;
        u32 waitSemaphoreCount = 0;
        u32 signalSemaphoreCount = 0;
        u64 timelineWait = 0;
        u64 timelineSignal = 0;
        bool asyncCompute = false;
        bool transfer = false;
        bool present = false;
        bool valid = false;
    };

    struct VulkanExecutionFrameResourcePlan
    {
        u32 frameIndex = 0;
        u32 primaryCommandBuffers = 0;
        u32 secondaryCommandBuffers = 0;
        u32 commandPoolResets = 0;
        u32 descriptorArenaResets = 0;
        u32 transientHeapAliases = 0;
        u64 transientHeapBytes = 0;
        u64 uploadScratchBytes = 0;
        u64 timelineBaseValue = 0;
        bool deferredReleaseQueue = true;
        bool valid = false;
    };

    struct VulkanExecutionValidationReport
    {
        u32 missingCompiledGraph = 0;
        u32 missingDrawSubmission = 0;
        u32 missingFrameResources = 0;
        u32 missingSubmitBatches = 0;
        u32 missingCommandBuffers = 0;
        u32 missingDynamicRenderingScopes = 0;
        u32 missingDrawCommands = 0;
        u32 unexpectedReadbacks = 0;
        u32 runtimeAllocationRisks = 0;
        u32 commandBudgetExceeded = 0;
        u32 warnings = 0;
        bool valid = false;
    };

    struct VulkanExecutionStats
    {
        u32 framesInFlight = 0;
        u32 passes = 0;
        u32 commands = 0;
        u32 commandBuffers = 0;
        u32 primaryCommandBuffers = 0;
        u32 secondaryCommandBuffers = 0;
        u32 barrierCommands = 0;
        u32 copyCommands = 0;
        u32 drawCommands = 0;
        u32 dispatchCommands = 0;
        u32 dynamicRenderingScopes = 0;
        u32 submitBatches = 0;
        u32 presentCommands = 0;
        u64 copiedBytes = 0;
        u64 transientHeapBytes = 0;
        u64 savedTransientBytes = 0;
        bool usesSynchronization2 = true;
        bool usesDynamicRendering = true;
        bool usesTimelineSemaphores = true;
        bool usesAsyncCompute = false;
        bool usesDedicatedTransfer = false;
        bool valid = false;
    };

    struct VulkanFrameGraphExecutionPlan
    {
        VulkanExecutionConfig config{};
        VulkanCompiledFrameGraph compiled{};
        VulkanDrawSubmissionPlan submission{};
        std::vector<VulkanExecutionFrameResourcePlan> frameResources;
        std::vector<VulkanExecutionPassPlan> passes;
        std::vector<VulkanExecutionCommand> commands;
        std::vector<VulkanExecutionSubmitPlan> submits;
        VulkanExecutionStats stats{};
        VulkanExecutionValidationReport validation{};
        bool readyForVkCmdRecording = false;
        bool readyForQueueSubmit2 = false;
        bool readyForPersistentRenderer = false;
        bool valid = false;
        std::string summary;
    };

    struct VulkanFrameGraphExecutionProbe
    {
        VulkanFrameGraphCompilerProbe compiler{};
        VulkanFrameGraphExecutionPlan execution{};
        bool ok = false;
        std::string summary;
    };

    VulkanExecutionConfig MakeDefaultVulkanExecutionConfig();
    VulkanFrameGraphExecutionPlan BuildVulkanFrameGraphExecutionPlan(const VulkanCompiledFrameGraph& compiled,
                                                                    const VulkanDrawSubmissionPlan& submission,
                                                                    const VulkanRuntimeArchitectureHardeningPlan& architecture,
                                                                    const VulkanExecutionConfig& config = {});
    VulkanFrameGraphExecutionProbe BuildVulkanFrameGraphExecutionProbe();

    const char* ToString(VulkanExecutionCommandKind kind);
    const char* ToString(VulkanExecutionHazardPolicy policy);
    const char* ToString(VulkanExecutionMemoryPolicy policy);

    std::string ToDebugString(const VulkanExecutionConfig& config);
    std::string ToDebugString(const VulkanExecutionCommand& command);
    std::string ToDebugString(const VulkanExecutionPassPlan& pass);
    std::string ToDebugString(const VulkanExecutionSubmitPlan& submit);
    std::string ToDebugString(const VulkanExecutionFrameResourcePlan& frame);
    std::string ToDebugString(const VulkanExecutionValidationReport& validation);
    std::string ToDebugString(const VulkanExecutionStats& stats);
    std::string ToDebugString(const VulkanFrameGraphExecutionPlan& plan);
    std::string ToDebugString(const VulkanFrameGraphExecutionProbe& probe);
}
