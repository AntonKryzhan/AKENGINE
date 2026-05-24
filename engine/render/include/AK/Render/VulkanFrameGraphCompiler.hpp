#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Render/RenderFoundation.hpp>
#include <AK/Render/VulkanRuntimeArchitecture.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class VulkanGraphResourceKind : u32
    {
        Buffer = 0,
        Image2D = 1,
        SwapchainImage = 2,
        DepthImage = 3,
        External = 4
    };

    enum class VulkanGraphAccess : u32
    {
        None = 0,
        TransferRead = 1,
        TransferWrite = 2,
        VertexBufferRead = 3,
        IndexBufferRead = 4,
        UniformRead = 5,
        StorageRead = 6,
        StorageWrite = 7,
        ColorAttachmentRead = 8,
        ColorAttachmentWrite = 9,
        DepthRead = 10,
        DepthWrite = 11,
        ShaderSampledRead = 12,
        PresentRead = 13
    };

    enum class VulkanGraphStage : u32
    {
        None = 0,
        TopOfPipe = 1,
        Transfer = 2,
        VertexInput = 3,
        VertexShader = 4,
        FragmentShader = 5,
        EarlyFragmentTests = 6,
        LateFragmentTests = 7,
        ColorAttachmentOutput = 8,
        ComputeShader = 9,
        BottomOfPipe = 10,
        Present = 11
    };

    enum class VulkanGraphLayout : u32
    {
        Undefined = 0,
        General = 1,
        TransferSrc = 2,
        TransferDst = 3,
        ColorAttachment = 4,
        DepthAttachment = 5,
        ShaderReadOnly = 6,
        PresentSrc = 7,
        Buffer = 8
    };

    enum class VulkanGraphQueueClass : u32
    {
        Graphics = 0,
        Transfer = 1,
        Compute = 2,
        Present = 3
    };

    enum class VulkanGraphPassKind : u32
    {
        Acquire = 0,
        Upload = 1,
        Graphics = 2,
        Compute = 3,
        Present = 4,
        Diagnostics = 5
    };

    struct VulkanGraphResourceDesc
    {
        std::string name;
        VulkanGraphResourceKind kind = VulkanGraphResourceKind::Buffer;
        u64 byteSize = 0;
        u32 width = 0;
        u32 height = 0;
        u32 formatId = 0;
        bool transient = true;
        bool imported = false;
        bool hostVisible = false;
        bool deviceLocal = true;
        bool allowAliasing = true;
        VulkanGraphLayout initialLayout = VulkanGraphLayout::Undefined;
        VulkanGraphLayout finalLayout = VulkanGraphLayout::Undefined;
        bool valid = false;
    };

    struct VulkanGraphResourceUse
    {
        std::string resource;
        VulkanGraphAccess access = VulkanGraphAccess::None;
        VulkanGraphStage stage = VulkanGraphStage::None;
        VulkanGraphLayout layout = VulkanGraphLayout::Undefined;
        bool read = false;
        bool write = false;
        bool discardBeforeWrite = false;
        bool preserveAfterPass = true;
        bool valid = false;
    };

    struct VulkanGraphPassDesc
    {
        std::string name;
        VulkanGraphPassKind kind = VulkanGraphPassKind::Graphics;
        VulkanGraphQueueClass queue = VulkanGraphQueueClass::Graphics;
        std::vector<VulkanGraphResourceUse> uses;
        u32 estimatedDraws = 0;
        u32 estimatedDispatches = 0;
        bool allowSecondaryCommandBuffers = false;
        bool asyncCandidate = false;
        bool present = false;
        bool valid = false;
    };

    struct VulkanGraphDesc
    {
        std::vector<VulkanGraphResourceDesc> resources;
        std::vector<VulkanGraphPassDesc> passes;
        u32 framesInFlight = 3;
        bool useSynchronization2 = true;
        bool useDynamicRendering = true;
        bool useTimelineSemaphores = true;
        bool allowTransientAliasing = true;
        bool valid = false;
    };

    struct VulkanCompiledResourceLifetime
    {
        std::string name;
        u32 resourceIndex = 0;
        u32 firstPass = 0;
        u32 lastPass = 0;
        u32 aliasGroup = 0;
        u64 byteSize = 0;
        bool transient = false;
        bool imported = false;
        bool aliased = false;
        bool valid = false;
    };

    struct VulkanCompiledBarrier
    {
        std::string resource;
        u32 resourceIndex = 0;
        u32 beforePass = 0;
        u32 afterPass = 0;
        VulkanGraphStage srcStage = VulkanGraphStage::None;
        VulkanGraphStage dstStage = VulkanGraphStage::None;
        VulkanGraphAccess srcAccess = VulkanGraphAccess::None;
        VulkanGraphAccess dstAccess = VulkanGraphAccess::None;
        VulkanGraphLayout oldLayout = VulkanGraphLayout::Undefined;
        VulkanGraphLayout newLayout = VulkanGraphLayout::Undefined;
        VulkanGraphQueueClass srcQueue = VulkanGraphQueueClass::Graphics;
        VulkanGraphQueueClass dstQueue = VulkanGraphQueueClass::Graphics;
        bool ownershipTransfer = false;
        bool imageBarrier = false;
        bool bufferBarrier = false;
        bool redundant = false;
        bool valid = false;
    };

    struct VulkanCompiledPass
    {
        std::string name;
        VulkanGraphPassKind kind = VulkanGraphPassKind::Graphics;
        VulkanGraphQueueClass queue = VulkanGraphQueueClass::Graphics;
        u32 passIndex = 0;
        u32 barrierBegin = 0;
        u32 barrierCount = 0;
        u32 resourceUseCount = 0;
        u32 estimatedDraws = 0;
        u32 estimatedDispatches = 0;
        u32 secondaryCommandBuffers = 0;
        bool dynamicRendering = false;
        bool asyncScheduled = false;
        bool present = false;
        bool valid = false;
    };

    struct VulkanTransientAliasGroup
    {
        u32 groupIndex = 0;
        std::vector<u32> resourceIndices;
        u64 maxBytes = 0;
        u64 totalUnaliasedBytes = 0;
        u32 firstPass = 0;
        u32 lastPass = 0;
        bool hasImage = false;
        bool hasBuffer = false;
        bool valid = false;
    };

    struct VulkanFrameGraphQueueBatch
    {
        VulkanGraphQueueClass queue = VulkanGraphQueueClass::Graphics;
        u32 firstPass = 0;
        u32 passCount = 0;
        u32 commandBuffers = 0;
        u32 waitSemaphores = 0;
        u32 signalSemaphores = 0;
        u64 timelineWait = 0;
        u64 timelineSignal = 0;
        bool asyncCompute = false;
        bool transfer = false;
        bool present = false;
        bool valid = false;
    };

    struct VulkanFrameGraphValidationReport
    {
        u32 missingResources = 0;
        u32 readBeforeWrite = 0;
        u32 queueHazards = 0;
        u32 invalidLayouts = 0;
        u32 aliasingHazards = 0;
        u32 redundantBarriers = 0;
        u32 warnings = 0;
        bool valid = false;
    };

    struct VulkanCompiledFrameGraph
    {
        VulkanGraphDesc source{};
        std::vector<VulkanCompiledPass> passes;
        std::vector<VulkanCompiledBarrier> barriers;
        std::vector<VulkanCompiledResourceLifetime> lifetimes;
        std::vector<VulkanTransientAliasGroup> aliasGroups;
        std::vector<VulkanFrameGraphQueueBatch> queueBatches;
        VulkanFrameGraphValidationReport validation{};
        u64 transientBytesAliased = 0;
        u64 transientBytesUnaliased = 0;
        u64 transientBytesSaved = 0;
        u32 graphicsPasses = 0;
        u32 transferPasses = 0;
        u32 computePasses = 0;
        u32 presentPasses = 0;
        u32 secondaryCommandBuffers = 0;
        bool usesSynchronization2 = true;
        bool usesDynamicRendering = true;
        bool usesTimelineSemaphores = true;
        bool readyForVkCommandRecording = false;
        bool readyForRenderGraphExecution = false;
        bool valid = false;
        std::string summary;
    };

    struct VulkanFrameGraphCompilerProbe
    {
        VulkanRuntimeArchitectureProbe architecture{};
        VulkanDrawSubmissionProbe draw{};
        VulkanGraphDesc graph{};
        VulkanCompiledFrameGraph compiled{};
        bool ok = false;
        std::string summary;
    };

    VulkanGraphResourceUse ReadResource(const std::string& name, VulkanGraphAccess access, VulkanGraphStage stage, VulkanGraphLayout layout);
    VulkanGraphResourceUse WriteResource(const std::string& name, VulkanGraphAccess access, VulkanGraphStage stage, VulkanGraphLayout layout, bool discard = false);

    VulkanGraphDesc BuildVulkanDefaultFrameGraphDesc(const VulkanDrawSubmissionPlan& submission, const VulkanRuntimeArchitectureHardeningPlan& architecture);
    VulkanCompiledFrameGraph CompileVulkanFrameGraph(const VulkanGraphDesc& graph);
    VulkanFrameGraphCompilerProbe BuildVulkanFrameGraphCompilerProbe();

    const char* ToString(VulkanGraphResourceKind kind);
    const char* ToString(VulkanGraphAccess access);
    const char* ToString(VulkanGraphStage stage);
    const char* ToString(VulkanGraphLayout layout);
    const char* ToString(VulkanGraphQueueClass queue);
    const char* ToString(VulkanGraphPassKind kind);

    std::string ToDebugString(const VulkanGraphResourceDesc& resource);
    std::string ToDebugString(const VulkanGraphPassDesc& pass);
    std::string ToDebugString(const VulkanCompiledBarrier& barrier);
    std::string ToDebugString(const VulkanCompiledResourceLifetime& lifetime);
    std::string ToDebugString(const VulkanTransientAliasGroup& group);
    std::string ToDebugString(const VulkanFrameGraphQueueBatch& batch);
    std::string ToDebugString(const VulkanFrameGraphValidationReport& validation);
    std::string ToDebugString(const VulkanCompiledFrameGraph& graph);
    std::string ToDebugString(const VulkanFrameGraphCompilerProbe& probe);
}
