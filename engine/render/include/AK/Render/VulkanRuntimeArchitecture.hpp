#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Render/VulkanDrawSubmission.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class VulkanFeatureProfileTier : u32
    {
        Required = 0,
        Preferred = 1,
        Optional = 2,
        Fallback = 3
    };

    enum class VulkanFeatureKind : u32
    {
        Vulkan13 = 0,
        DynamicRendering = 1,
        Synchronization2 = 2,
        TimelineSemaphore = 3,
        DescriptorIndexing = 4,
        BufferDeviceAddress = 5,
        ScalarBlockLayout = 6,
        Maintenance4 = 7,
        DynamicState = 8,
        ShaderObjects = 9,
        PipelineBinary = 10,
        PushDescriptor = 11,
        DeviceGeneratedCommands = 12
    };

    enum class VulkanDescriptorFrequency : u32
    {
        Frame = 0,
        View = 1,
        Material = 2,
        Object = 3,
        Draw = 4,
        BindlessTexture = 5
    };

    enum class VulkanDescriptorUpdatePolicy : u32
    {
        ImmutableAtLoad = 0,
        PerFrameRing = 1,
        StreamingBatched = 2,
        PushConstantIndex = 3,
        BindlessTable = 4
    };

    enum class VulkanMemoryBlockKind : u32
    {
        PersistentUpload = 0,
        TransientUpload = 1,
        DeviceStaticGeometry = 2,
        DeviceStreamingGeometry = 3,
        DeviceMaterialTables = 4,
        Readback = 5,
        TransientImage = 6,
        PipelineScratch = 7
    };

    enum class VulkanPipelineFallbackMode : u32
    {
        None = 0,
        UberShaderUntilVariantReady = 1,
        EmbeddedFallbackShader = 2,
        AsyncWarmupOnly = 3
    };

    enum class VulkanCommandPoolScope : u32
    {
        FrameGraphicsPrimary = 0,
        FrameGraphicsSecondary = 1,
        WorkerSecondary = 2,
        TransferUpload = 3,
        ComputeAsync = 4
    };

    enum class VulkanSubmitBatchKind : u32
    {
        Acquire = 0,
        UploadTransfer = 1,
        Graphics = 2,
        AsyncCompute = 3,
        Present = 4
    };

    struct VulkanFeatureRequirement
    {
        VulkanFeatureKind kind = VulkanFeatureKind::Vulkan13;
        VulkanFeatureProfileTier tier = VulkanFeatureProfileTier::Required;
        std::string name;
        std::string coreVersion;
        std::string extensionName;
        bool required = true;
        bool enabledByDefault = true;
        bool hasFallback = false;
        bool valid = false;
    };

    struct VulkanDeviceLimitsSnapshot
    {
        u32 apiMajor = 1;
        u32 apiMinor = 3;
        u32 maxFramesInFlight = 3;
        u32 maxBoundDescriptorSets = 4;
        u32 maxPushConstantBytes = 128;
        u32 minUniformBufferOffsetAlignment = 256;
        u32 minStorageBufferOffsetAlignment = 256;
        u32 maxPerStageDescriptorSampledImages = 1024;
        u32 maxDescriptorSetUpdateAfterBindSampledImages = 4096;
        u32 optimalBufferCopyOffsetAlignment = 256;
        u32 optimalBufferCopyRowPitchAlignment = 256;
        bool supportsNonCoherentAtomSize = true;
        bool supportsTimestampQueries = true;
        bool valid = false;
    };

    struct VulkanMemoryBlockAllocatorPlan
    {
        VulkanMemoryBlockKind kind = VulkanMemoryBlockKind::TransientUpload;
        std::string name;
        u64 blockBytes = 0;
        u32 blockCount = 0;
        u32 alignmentBytes = 256;
        u32 framesInFlight = 3;
        bool hostVisible = false;
        bool hostCoherent = false;
        bool deviceLocal = true;
        bool persistentlyMapped = false;
        bool ringBuffered = false;
        bool frameRetired = true;
        bool subAllocated = true;
        bool valid = false;
    };

    struct VulkanDescriptorSetFrequencyPlan
    {
        u32 setIndex = 0;
        VulkanDescriptorFrequency frequency = VulkanDescriptorFrequency::Frame;
        VulkanDescriptorUpdatePolicy updatePolicy = VulkanDescriptorUpdatePolicy::PerFrameRing;
        std::string name;
        u32 uniformBindings = 0;
        u32 storageBindings = 0;
        u32 sampledImageBindings = 0;
        u32 samplerBindings = 0;
        u32 maxDescriptors = 0;
        bool dynamicOffsets = false;
        bool updateAfterBind = false;
        bool cached = true;
        bool writeDuringHotFrame = false;
        bool valid = false;
    };

    struct VulkanPipelineCacheKeyPlan
    {
        u64 shaderHash = 0;
        u64 vertexLayoutHash = 0;
        u64 renderStateHash = 0;
        u64 pipelineLayoutHash = 0;
        u32 vendorId = 0;
        u32 deviceId = 0;
        u32 driverVersion = 0;
        std::string cachePath;
        bool includesDeviceIdentity = true;
        bool includesRenderTargetFormat = true;
        bool includesSpecializationConstants = true;
        bool valid = false;
    };

    struct VulkanShaderVariantKeyPlan
    {
        u64 materialFeatureMask = 0;
        u64 lightingFeatureMask = 0;
        u64 geometryFeatureMask = 0;
        u32 vertexLayoutId = 0;
        u32 renderPassClass = 0;
        bool staticSpecialization = true;
        bool dynamicFallbackUberShader = true;
        bool valid = false;
    };

    struct VulkanAsyncPipelineCompileQueuePlan
    {
        u32 workerCount = 2;
        u32 maxQueuedJobs = 256;
        u32 maxActiveJobs = 4;
        u32 warmedPipelines = 0;
        u32 fallbackPipelines = 1;
        VulkanPipelineFallbackMode fallbackMode = VulkanPipelineFallbackMode::UberShaderUntilVariantReady;
        bool diskCacheEnabled = true;
        bool compileOffRenderThread = true;
        bool stutterGuard = true;
        bool valid = false;
    };

    struct VulkanCommandPoolFrameThreadPlan
    {
        VulkanCommandPoolScope scope = VulkanCommandPoolScope::FrameGraphicsPrimary;
        std::string name;
        u32 framesInFlight = 3;
        u32 threadCount = 1;
        u32 poolsPerFrame = 1;
        u32 commandBuffersPerPool = 1;
        bool resetPoolPerFrame = true;
        bool oneTimeSubmit = true;
        bool secondaryCommandBuffers = false;
        bool externallySynchronized = true;
        bool valid = false;
    };

    struct VulkanRedundantBindFilterPlan
    {
        u32 inputCommands = 0;
        u32 outputCommands = 0;
        u32 removedPipelineBinds = 0;
        u32 removedDescriptorBinds = 0;
        u32 removedVertexBufferBinds = 0;
        u32 removedIndexBufferBinds = 0;
        u32 removedDynamicStateWrites = 0;
        u32 estimatedCpuBytesSaved = 0;
        bool stableCommandOrder = true;
        bool valid = false;
    };

    struct VulkanFrameSubmitBatchPlan
    {
        VulkanSubmitBatchKind kind = VulkanSubmitBatchKind::Graphics;
        std::string name;
        u32 queueFamily = 0;
        u32 waitSemaphores = 0;
        u32 signalSemaphores = 0;
        u32 commandBuffers = 0;
        u64 timelineWaitValue = 0;
        u64 timelineSignalValue = 0;
        bool usesTimelineSemaphore = true;
        bool canRunAsync = false;
        bool valid = false;
    };

    struct VulkanRuntimeArchitectureHardeningPlan
    {
        VulkanRhiPlan rhi{};
        VulkanDrawSubmissionPlan submission{};
        VulkanDeviceLimitsSnapshot limits{};
        std::vector<VulkanFeatureRequirement> features;
        std::vector<VulkanMemoryBlockAllocatorPlan> memoryAllocators;
        std::vector<VulkanDescriptorSetFrequencyPlan> descriptorSets;
        VulkanPipelineCacheKeyPlan pipelineCache{};
        VulkanShaderVariantKeyPlan shaderVariant{};
        VulkanAsyncPipelineCompileQueuePlan compileQueue{};
        std::vector<VulkanCommandPoolFrameThreadPlan> commandPools;
        VulkanRedundantBindFilterPlan bindFilter{};
        std::vector<VulkanFrameSubmitBatchPlan> submitBatches;
        bool avoidsPerDrawDescriptorUpdates = true;
        bool avoidsRuntimeVkAllocateMemory = true;
        bool usesDynamicRendering = true;
        bool usesSynchronization2 = true;
        bool usesTimelineSemaphores = true;
        bool supportsFallbackShaders = true;
        bool readyForRealVkHandles = false;
        bool valid = false;
        u32 warnings = 0;
        std::string summary;
    };

    struct VulkanRuntimeArchitectureProbe
    {
        VulkanRuntimeArchitectureHardeningPlan plan{};
        bool ok = false;
        std::string summary;
    };

    VulkanDeviceLimitsSnapshot BuildDefaultVulkanDeviceLimitsSnapshot(const VulkanRhiConfig& config = {});
    std::vector<VulkanFeatureRequirement> BuildVulkanFeatureProfile(const VulkanRhiConfig& config = {});
    std::vector<VulkanMemoryBlockAllocatorPlan> BuildVulkanMemoryAllocatorPlan(const VulkanMeshUploadPlan& upload, const VulkanDeviceLimitsSnapshot& limits);
    std::vector<VulkanDescriptorSetFrequencyPlan> BuildVulkanDescriptorFrequencyPlan(const VulkanDrawSubmissionPlan& submission, const VulkanDeviceLimitsSnapshot& limits);
    VulkanPipelineCacheKeyPlan BuildVulkanPipelineCacheKeyPlan(const VulkanDrawSubmissionPlan& submission);
    VulkanShaderVariantKeyPlan BuildVulkanShaderVariantKeyPlan(const VulkanDrawSubmissionPlan& submission);
    VulkanAsyncPipelineCompileQueuePlan BuildVulkanAsyncPipelineCompileQueuePlan(const VulkanDrawSubmissionPlan& submission);
    std::vector<VulkanCommandPoolFrameThreadPlan> BuildVulkanCommandPoolFrameThreadPlan(const VulkanRhiConfig& config, u32 workerThreads = 4);
    VulkanRedundantBindFilterPlan BuildVulkanRedundantBindFilterPlan(const VulkanDrawSubmissionPlan& submission);
    std::vector<VulkanFrameSubmitBatchPlan> BuildVulkanFrameSubmitBatchPlan(const VulkanRhiConfig& config, const VulkanDrawSubmissionPlan& submission);
    VulkanRuntimeArchitectureHardeningPlan BuildVulkanRuntimeArchitectureHardeningPlan(const VulkanRhiConfig& config = {});
    VulkanRuntimeArchitectureProbe BuildVulkanRuntimeArchitectureProbe();

    const char* ToString(VulkanFeatureProfileTier tier);
    const char* ToString(VulkanFeatureKind kind);
    const char* ToString(VulkanDescriptorFrequency frequency);
    const char* ToString(VulkanDescriptorUpdatePolicy policy);
    const char* ToString(VulkanMemoryBlockKind kind);
    const char* ToString(VulkanPipelineFallbackMode mode);
    const char* ToString(VulkanCommandPoolScope scope);
    const char* ToString(VulkanSubmitBatchKind kind);
    std::string ToDebugString(const VulkanFeatureRequirement& requirement);
    std::string ToDebugString(const VulkanDeviceLimitsSnapshot& limits);
    std::string ToDebugString(const VulkanMemoryBlockAllocatorPlan& plan);
    std::string ToDebugString(const VulkanDescriptorSetFrequencyPlan& plan);
    std::string ToDebugString(const VulkanPipelineCacheKeyPlan& plan);
    std::string ToDebugString(const VulkanShaderVariantKeyPlan& plan);
    std::string ToDebugString(const VulkanAsyncPipelineCompileQueuePlan& plan);
    std::string ToDebugString(const VulkanCommandPoolFrameThreadPlan& plan);
    std::string ToDebugString(const VulkanRedundantBindFilterPlan& plan);
    std::string ToDebugString(const VulkanFrameSubmitBatchPlan& plan);
    std::string ToDebugString(const VulkanRuntimeArchitectureHardeningPlan& plan);
    std::string ToDebugString(const VulkanRuntimeArchitectureProbe& probe);
}
