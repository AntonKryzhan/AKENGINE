#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Render/VulkanUploadPlan.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class VulkanDrawCommandKind : u32
    {
        BeginFrame = 0,
        TransitionColorAttachment = 1,
        TransitionDepthAttachment = 2,
        BeginDynamicRendering = 3,
        BindGraphicsPipeline = 4,
        BindDescriptorSets = 5,
        BindVertexBuffer = 6,
        BindIndexBuffer = 7,
        SetViewport = 8,
        SetScissor = 9,
        PushConstants = 10,
        DrawIndexed = 11,
        EndDynamicRendering = 12,
        TransitionPresent = 13,
        EndFrame = 14
    };

    enum class VulkanDescriptorResourceKind : u32
    {
        UniformBuffer = 0,
        StorageBuffer = 1,
        SampledImage = 2,
        Sampler = 3
    };

    enum class VulkanShaderCompilerKind : u32
    {
        Slang = 0,
        GlslangValidator = 1,
        DxcSpirv = 2,
        EmbeddedFallback = 3
    };

    struct VulkanShaderCompilerInvocation
    {
        VulkanShaderCompilerKind compiler = VulkanShaderCompilerKind::Slang;
        std::string executable;
        std::vector<std::string> arguments;
        std::string inputPath;
        std::string outputPath;
        std::string entryPoint;
        std::string profile;
        bool debugInfo = true;
        bool optimization = false;
        bool deterministic = true;
        bool valid = false;
    };

    struct VulkanShaderCacheEntry
    {
        std::string sourcePath;
        std::string spvPath;
        std::string stage;
        std::string entryPoint;
        u64 sourceHash = 0;
        u32 estimatedSpvBytes = 0;
        bool sourcePresent = false;
        bool cachePathValid = false;
        bool needsCompile = true;
        bool hotReloadTracked = true;
        bool valid = false;
    };

    struct VulkanDescriptorBindingPlan
    {
        u32 set = 0;
        u32 binding = 0;
        VulkanDescriptorResourceKind resourceKind = VulkanDescriptorResourceKind::UniformBuffer;
        std::string name;
        u32 descriptorCount = 1;
        bool dynamicOffset = false;
        bool partiallyBound = false;
        bool updateAfterBind = false;
        bool valid = false;
    };

    struct VulkanPipelineLayoutPlan
    {
        std::vector<VulkanDescriptorBindingPlan> descriptorBindings;
        u32 descriptorSetCount = 0;
        u32 pushConstantBytes = 0;
        bool bindlessMaterialSet = false;
        bool objectTableStorage = true;
        bool materialTableStorage = true;
        bool valid = false;
        u32 warnings = 0;
    };

    struct VulkanRenderTargetPlan
    {
        std::string colorFormat = "VK_FORMAT_B8G8R8A8_SRGB";
        std::string depthFormat = "VK_FORMAT_D32_SFLOAT";
        u32 width = 1280;
        u32 height = 720;
        bool dynamicRendering = true;
        bool clearColor = true;
        bool clearDepth = true;
        bool reversedZ = true;
        bool valid = false;
    };

    struct VulkanRecordedDrawCommand
    {
        VulkanDrawCommandKind kind = VulkanDrawCommandKind::BeginFrame;
        std::string label;
        u32 drawIndex = 0xFFFFFFFFu;
        u32 materialId = 0;
        u32 firstIndex = 0;
        u32 indexCount = 0;
        i32 vertexOffset = 0;
        u32 instanceCount = 1;
        bool valid = false;
    };

    struct VulkanDrawBatchStats
    {
        u32 drawCalls = 0;
        u32 materialSwitches = 0;
        u32 pipelineBinds = 0;
        u32 descriptorBinds = 0;
        u32 vertexBufferBinds = 0;
        u32 indexBufferBinds = 0;
        u32 pushConstantWrites = 0;
        u32 dynamicStateWrites = 0;
        u32 recordedCommands = 0;
        u32 estimatedCpuBytes = 0;
        bool frontToBackSorted = false;
        bool materialCoherent = false;
        bool valid = false;
    };

    struct VulkanDrawSubmissionPlan
    {
        VulkanMeshUploadPlan upload{};
        VulkanGraphicsPipelinePlan pipeline{};
        VulkanPipelineLayoutPlan layout{};
        VulkanRenderTargetPlan renderTarget{};
        std::vector<VulkanShaderCacheEntry> shaderCache;
        std::vector<VulkanShaderCompilerInvocation> compilerInvocations;
        std::vector<VulkanRecordedDrawCommand> commands;
        VulkanDrawBatchStats batchStats{};
        bool readyForShaderCompilation = false;
        bool readyForPipelineCreation = false;
        bool readyForCommandRecording = false;
        bool readyForGpuSubmission = false;
        bool usesPackedVertex32 = true;
        bool usesDynamicRendering = true;
        bool usesReversedZ = true;
        bool usesBindlessMaterialSet = true;
        bool valid = false;
        u32 warnings = 0;
        std::string summary;
    };

    struct VulkanDrawSubmissionProbe
    {
        PrimitiveTestScene scene{};
        OptimizedRenderGeometry optimized{};
        RenderMeshOptimizationStats optimizationStats{};
        VulkanMeshUploadPlan upload{};
        VulkanDrawSubmissionPlan submission{};
        bool ok = false;
        std::string summary;
    };

    VulkanPipelineLayoutPlan BuildVulkanPrimitivePipelineLayoutPlan(const VulkanMeshUploadPlan& upload);
    VulkanRenderTargetPlan BuildVulkanDefaultRenderTargetPlan(const VulkanRhiConfig& config);
    std::vector<VulkanShaderCacheEntry> BuildVulkanPrimitiveShaderCacheEntries(const VulkanShaderBuildPlan& shaders);
    std::vector<VulkanShaderCompilerInvocation> BuildVulkanShaderCompilerInvocations(const VulkanShaderBuildPlan& shaders);
    std::vector<VulkanRecordedDrawCommand> RecordVulkanDrawCommandPlan(const VulkanMeshUploadPlan& upload, const VulkanGraphicsPipelinePlan& pipeline, const VulkanPipelineLayoutPlan& layout, const VulkanRenderTargetPlan& target);
    VulkanDrawBatchStats AnalyzeVulkanRecordedCommands(const std::vector<VulkanRecordedDrawCommand>& commands);
    VulkanDrawSubmissionPlan BuildVulkanDrawSubmissionPlan(const OptimizedRenderGeometry& geometry, const VulkanRhiConfig& config = {});
    VulkanDrawSubmissionProbe BuildVulkanDrawSubmissionProbe();

    const char* ToString(VulkanDrawCommandKind kind);
    const char* ToString(VulkanDescriptorResourceKind kind);
    const char* ToString(VulkanShaderCompilerKind kind);
    std::string ToDebugString(const VulkanShaderCompilerInvocation& invocation);
    std::string ToDebugString(const VulkanShaderCacheEntry& entry);
    std::string ToDebugString(const VulkanPipelineLayoutPlan& plan);
    std::string ToDebugString(const VulkanRenderTargetPlan& plan);
    std::string ToDebugString(const VulkanDrawBatchStats& stats);
    std::string ToDebugString(const VulkanDrawSubmissionPlan& plan);
    std::string ToDebugString(const VulkanDrawSubmissionProbe& probe);
}
