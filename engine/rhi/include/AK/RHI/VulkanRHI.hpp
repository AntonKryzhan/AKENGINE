#pragma once

#include <AK/Core/Types.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class RhiBackend : u32
    {
        None = 0,
        Vulkan = 1
    };

    enum class VulkanWindowPlatform : u32
    {
        Headless = 0,
        Win32 = 1,
        Xlib = 2,
        Xcb = 3,
        Wayland = 4,
        Metal = 5,
        Unknown = 255
    };

    enum class VulkanPresentModePolicy : u32
    {
        MailboxPreferLowLatency = 0,
        FifoAlwaysAvailable = 1,
        ImmediateAllowTearing = 2
    };

    enum class VulkanFramePacingPolicy : u32
    {
        FixedFramesInFlight = 0,
        AdaptiveLatency = 1
    };

    struct NativeWindowSurfaceDesc
    {
        VulkanWindowPlatform platform = VulkanWindowPlatform::Headless;
        void* nativeHandle = nullptr;
        void* nativeInstance = nullptr;
        u32 width = 0;
        u32 height = 0;
        bool valid = false;
    };

    struct VulkanRhiConfig
    {
        std::string applicationName = "AK Engine";
        std::string engineName = "AK Engine";
        u32 applicationVersionMajor = 1;
        u32 applicationVersionMinor = 0;
        u32 applicationVersionPatch = 0;
        bool enableValidation = true;
        bool enableDebugUtils = true;
        bool requireSwapchain = true;
        bool requireDynamicRendering = true;
        bool requireTimelineSemaphore = true;
        bool requireDescriptorIndexing = true;
        bool requireBufferDeviceAddress = false;
        bool requireSynchronization2 = true;
        bool preferDedicatedTransferQueue = true;
        bool preferDedicatedComputeQueue = true;
        u32 framesInFlight = 3;
        VulkanPresentModePolicy presentModePolicy = VulkanPresentModePolicy::MailboxPreferLowLatency;
        VulkanFramePacingPolicy framePacingPolicy = VulkanFramePacingPolicy::FixedFramesInFlight;
        NativeWindowSurfaceDesc window{};
    };

    struct VulkanQueueFamilyPlan
    {
        bool graphics = false;
        bool compute = false;
        bool transfer = false;
        bool present = false;
        bool preferDedicated = false;
        std::string name;
    };

    struct VulkanFrameResourcePlan
    {
        u32 frameIndex = 0;
        bool commandPool = true;
        bool graphicsCommandBuffer = true;
        bool imageAvailableSemaphore = true;
        bool renderFinishedSemaphore = true;
        bool timelineFenceValue = true;
        bool deferredReleaseQueue = true;
        bool uploadScratchArena = true;
    };

    struct VulkanSwapchainPlan
    {
        u32 width = 0;
        u32 height = 0;
        u32 imageCount = 3;
        bool srgbColor = true;
        bool reversedZDepth = true;
        bool depthFloat = true;
        bool vsyncSafeFallback = true;
        VulkanPresentModePolicy presentModePolicy = VulkanPresentModePolicy::MailboxPreferLowLatency;
        std::string colorFormat = "VK_FORMAT_B8G8R8A8_SRGB";
        std::string depthFormat = "VK_FORMAT_D32_SFLOAT";
    };

    struct VulkanRhiPlan
    {
        RhiBackend backend = RhiBackend::Vulkan;
        VulkanRhiConfig config{};
        std::string loaderLibraryName;
        std::vector<std::string> instanceExtensions;
        std::vector<std::string> deviceExtensions;
        std::vector<std::string> validationLayers;
        std::vector<VulkanQueueFamilyPlan> queues;
        std::vector<VulkanFrameResourcePlan> frames;
        VulkanSwapchainPlan swapchain{};
        bool headless = false;
        bool valid = false;
        u32 warnings = 0;
        std::string summary;
    };

    struct VulkanLoaderProbe
    {
        bool attempted = false;
        bool loaded = false;
        bool vkGetInstanceProcAddrResolved = false;
        std::string libraryName;
        std::string message;
    };

    struct VulkanRhiProbe
    {
        VulkanRhiPlan plan{};
        VulkanLoaderProbe loader{};
        bool renderGraphCompatible = false;
        bool policyCompatible = false;
        bool swapchainCompatible = false;
        bool framesInFlightCompatible = false;
        bool ok = false;
        u32 warnings = 0;
        std::string summary;
    };

    enum class VulkanRuntimeStage : u32
    {
        NotStarted = 0,
        Loader,
        Instance,
        Surface,
        PhysicalDevice,
        LogicalDevice,
        Swapchain,
        ImageViews,
        CommandResources,
        SyncObjects,
        ClearFrame,
        Present,
        Completed,
        Failed
    };


    enum class VulkanPrimitiveTopology : u32
    {
        TriangleList = 0,
        LineList = 1,
        PointList = 2
    };

    enum class VulkanVertexFormat : u32
    {
        Float2 = 0,
        Float3 = 1,
        Float4 = 2,
        Snorm16x2 = 3,
        Unorm16x2 = 4,
        Uint32 = 5
    };

    enum class VulkanCullMode : u32
    {
        None = 0,
        Back = 1,
        Front = 2
    };

    enum class VulkanDepthCompare : u32
    {
        GreaterOrEqual = 0,
        LessOrEqual = 1,
        Always = 2
    };

    struct VulkanVertexAttributeDesc
    {
        std::string semantic;
        u32 location = 0;
        VulkanVertexFormat format = VulkanVertexFormat::Float3;
        u32 offsetBytes = 0;
    };

    struct VulkanVertexLayoutDesc
    {
        u32 strideBytes = 0;
        std::vector<VulkanVertexAttributeDesc> attributes;
        bool valid = false;
    };

    struct VulkanShaderModulePlan
    {
        std::string name;
        std::string entryPoint = "main";
        std::string stage;
        std::string sourceLanguage = "Slang/HLSL planned";
        std::string bytecodePath;
        bool embeddedFallback = false;
        bool valid = false;
    };

    struct VulkanGraphicsPipelinePlan
    {
        VulkanPrimitiveTopology topology = VulkanPrimitiveTopology::TriangleList;
        VulkanCullMode cullMode = VulkanCullMode::Back;
        VulkanDepthCompare depthCompare = VulkanDepthCompare::GreaterOrEqual;
        VulkanVertexLayoutDesc vertexLayout{};
        VulkanShaderModulePlan vertexShader{};
        VulkanShaderModulePlan fragmentShader{};
        bool colorAttachment = true;
        bool depthAttachment = true;
        bool reversedZ = true;
        bool dynamicViewport = true;
        bool dynamicScissor = true;
        bool alphaBlend = false;
        bool wireframeAllowed = false;
        bool valid = false;
        u32 warnings = 0;
        std::string summary;
    };

    struct VulkanDrawCallPlan
    {
        std::string name;
        u32 vertexCount = 0;
        u32 indexCount = 0;
        u32 instanceCount = 1;
        bool indexed = true;
        bool valid = false;
    };

    struct VulkanPipelineValidationReport
    {
        bool ok = false;
        bool vertexLayoutOk = false;
        bool shadersOk = false;
        bool attachmentsOk = false;
        bool depthPolicyOk = false;
        u32 warnings = 0;
        std::string summary;
    };

    struct VulkanPrimitiveRendererProbe
    {
        VulkanGraphicsPipelinePlan pipeline{};
        VulkanPipelineValidationReport validation{};
        std::vector<VulkanDrawCallPlan> drawCalls;
        u32 totalVertices = 0;
        u32 totalIndices = 0;
        u32 totalTriangles = 0;
        bool readyForGpuDraw = false;
        bool usesReversedZ = true;
        bool finite = true;
        u32 warnings = 0;
        std::string summary;
    };
    struct VulkanRuntimeResult
    {
        bool attempted = false;
        bool ok = false;
        bool headless = false;
        bool loaderLoaded = false;
        bool instanceCreated = false;
        bool surfaceCreated = false;
        bool physicalDeviceSelected = false;
        bool deviceCreated = false;
        bool swapchainCreated = false;
        bool imageViewsCreated = false;
        bool commandResourcesCreated = false;
        bool syncObjectsCreated = false;
        bool clearSubmitted = false;
        bool presented = false;
        bool validationEnabled = false;
        bool usedTransferClear = false;
        bool usedFifoFallback = false;
        bool usedMailboxPresent = false;
        bool swapchainSkipped = false;
        u32 physicalDeviceCount = 0;
        u32 graphicsQueueFamily = 0xFFFFFFFFu;
        u32 presentQueueFamily = 0xFFFFFFFFu;
        u32 swapchainImageCount = 0;
        u32 swapchainWidth = 0;
        u32 swapchainHeight = 0;
        u32 warnings = 0;
        i32 vulkanResult = 0;
        VulkanRuntimeStage lastStage = VulkanRuntimeStage::NotStarted;
        std::string adapterName;
        std::string colorFormat;
        std::string presentMode;
        std::string message;
        std::string summary;
    };

    NativeWindowSurfaceDesc MakeNativeWindowSurfaceDesc(void* nativeHandle, u32 width, u32 height);
    VulkanRhiConfig MakeDefaultVulkanRhiConfig(const std::string& applicationName = "AK Engine");
    VulkanRhiPlan BuildVulkanRhiPlan(const VulkanRhiConfig& config);
    VulkanRhiProbe BuildVulkanRhiProbe(const VulkanRhiConfig& config, bool tryLoadSystemVulkan = true);
    VulkanLoaderProbe ProbeVulkanLoader();
    VulkanRuntimeResult RunVulkanRuntimeBootstrap(const VulkanRhiConfig& config, bool submitClearFrame = true);

    VulkanVertexLayoutDesc BuildDefaultVulkanMeshVertexLayout();
    VulkanVertexLayoutDesc BuildPackedVulkanMeshVertexLayout32();
    VulkanGraphicsPipelinePlan BuildVulkanPrimitiveGraphicsPipelinePlan(const VulkanRhiConfig& config, const std::string& colorFormat = "VK_FORMAT_B8G8R8A8_SRGB", const std::string& depthFormat = "VK_FORMAT_D32_SFLOAT");
    VulkanPipelineValidationReport ValidateVulkanGraphicsPipelinePlan(const VulkanGraphicsPipelinePlan& plan);
    VulkanPrimitiveRendererProbe BuildVulkanPrimitiveRendererProbe(const VulkanRhiConfig& config, const std::vector<VulkanDrawCallPlan>& drawCalls);

    const char* ToString(RhiBackend backend);
    const char* ToString(VulkanWindowPlatform platform);
    const char* ToString(VulkanPresentModePolicy policy);
    const char* ToString(VulkanRuntimeStage stage);
    const char* ToString(VulkanPrimitiveTopology topology);
    const char* ToString(VulkanVertexFormat format);
    const char* ToString(VulkanCullMode cullMode);
    const char* ToString(VulkanDepthCompare compare);
    std::string ToDebugString(const VulkanRhiPlan& plan);
    std::string ToDebugString(const VulkanLoaderProbe& probe);
    std::string ToDebugString(const VulkanRhiProbe& probe);
    std::string ToDebugString(const VulkanRuntimeResult& result);
    std::string ToDebugString(const VulkanGraphicsPipelinePlan& plan);
    std::string ToDebugString(const VulkanPipelineValidationReport& report);
    std::string ToDebugString(const VulkanPrimitiveRendererProbe& probe);
}
