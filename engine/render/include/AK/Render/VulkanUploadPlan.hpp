#pragma once

#include <AK/Core/Types.hpp>
#include <AK/Render/MeshOptimization.hpp>
#include <AK/RHI/VulkanRHI.hpp>

#include <string>
#include <vector>

namespace AK
{
    enum class VulkanBufferPlanKind : u32
    {
        Unknown = 0,
        StagingUpload = 1,
        DeviceVertex = 2,
        DeviceIndex = 3,
        DeviceIndirect = 4,
        DeviceMeshlet = 5,
        UniformRing = 6,
        MaterialTable = 7,
        ObjectTable = 8
    };

    enum class VulkanMemoryPlanDomain : u32
    {
        CpuUpload = 0,
        GpuOnly = 1,
        CpuToGpuPersistent = 2,
        GpuToCpuReadback = 3
    };

    enum class VulkanDrawSubmissionMode : u32
    {
        DirectDrawIndexed = 0,
        MultiDrawIndirectPlanned = 1,
        MeshShaderPlanned = 2
    };

    struct VulkanGpuBufferPlan
    {
        std::string name;
        VulkanBufferPlanKind kind = VulkanBufferPlanKind::Unknown;
        VulkanMemoryPlanDomain memory = VulkanMemoryPlanDomain::GpuOnly;
        u64 bytes = 0;
        u32 strideBytes = 0;
        u32 elementCount = 0;
        bool hostVisible = false;
        bool deviceLocal = true;
        bool persistentlyMapped = false;
        bool transferSrc = false;
        bool transferDst = false;
        bool vertexBuffer = false;
        bool indexBuffer = false;
        bool indirectBuffer = false;
        bool uniformBuffer = false;
        bool storageBuffer = false;
        bool valid = false;
    };

    struct VulkanUploadCopyPlan
    {
        std::string name;
        u32 sourceBuffer = 0;
        u32 destinationBuffer = 0;
        u64 sourceOffset = 0;
        u64 destinationOffset = 0;
        u64 bytes = 0;
        bool aligned = false;
        bool valid = false;
    };

    struct VulkanDrawIndexedRecord
    {
        std::string name;
        u32 sectionIndex = 0;
        u32 meshletIndex = 0;
        u32 lodLevel = 0;
        u32 indexCount = 0;
        u32 instanceCount = 1;
        u32 firstIndex = 0;
        i32 vertexOffset = 0;
        u32 firstInstance = 0;
        u32 materialId = 0;
        float cameraDistance = 0.0f;
        bool frontToBackSorted = false;
        bool materialSorted = false;
        bool valid = false;
    };

    struct VulkanFrameUniformPlan
    {
        u32 framesInFlight = 3;
        u64 cameraBytesPerFrame = 256;
        u64 objectBytesPerFrame = 0;
        u64 materialBytesPerFrame = 0;
        u64 totalBytes = 0;
        u32 objectCount = 0;
        u32 materialCount = 0;
        bool dynamicOffsets = true;
        bool ringBuffered = true;
        bool valid = false;
    };

    struct VulkanDescriptorPlan
    {
        u32 descriptorSets = 0;
        u32 uniformBindings = 0;
        u32 storageBindings = 0;
        u32 sampledImageBindings = 0;
        u32 samplerBindings = 0;
        bool bindlessReady = false;
        bool materialTableAsStorage = true;
        bool objectTableAsStorage = true;
        bool valid = false;
    };

    struct VulkanShaderBuildPlan
    {
        std::string shaderSourcePath;
        std::string vertexEntry = "vs_main";
        std::string fragmentEntry = "fs_main";
        std::string vertexSpvPath;
        std::string fragmentSpvPath;
        std::string compiler = "slangc/glslang planned";
        bool embeddedFallbackShaders = true;
        bool offlineCacheRequired = true;
        bool hotReloadFriendly = true;
        bool valid = false;
    };

    struct VulkanMeshUploadPlan
    {
        VulkanDrawSubmissionMode submissionMode = VulkanDrawSubmissionMode::DirectDrawIndexed;
        std::vector<VulkanGpuBufferPlan> buffers;
        std::vector<VulkanUploadCopyPlan> copies;
        std::vector<VulkanDrawIndexedRecord> draws;
        VulkanFrameUniformPlan uniforms{};
        VulkanDescriptorPlan descriptors{};
        VulkanShaderBuildPlan shaders{};
        u64 totalUploadBytes = 0;
        u64 deviceLocalBytes = 0;
        u64 stagingBytes = 0;
        u32 vertexBufferIndex = 0xFFFFFFFFu;
        u32 indexBufferIndex = 0xFFFFFFFFu;
        u32 indirectBufferIndex = 0xFFFFFFFFu;
        u32 drawCount = 0;
        u32 meshletDrawCount = 0;
        u32 lodDrawCount = 0;
        bool usesCombinedGeometryBuffers = true;
        bool usesPackedVertices = true;
        bool usesCameraRelativePositions = true;
        bool readyForVkBufferCreation = false;
        bool readyForDrawIndexedRecording = false;
        u32 warnings = 0;
        bool valid = false;
    };

    struct VulkanMeshUploadProbe
    {
        PrimitiveTestScene scene{};
        OptimizedRenderGeometry optimized{};
        RenderMeshOptimizationStats optimizationStats{};
        VulkanGraphicsPipelinePlan pipeline{};
        VulkanPipelineValidationReport pipelineValidation{};
        VulkanMeshUploadPlan upload{};
        bool ok = false;
        std::string summary;
    };

    VulkanGpuBufferPlan MakeGpuBufferPlan(const std::string& name, VulkanBufferPlanKind kind, VulkanMemoryPlanDomain memory, u64 bytes, u32 strideBytes, u32 elementCount);
    VulkanMeshUploadPlan BuildVulkanMeshUploadPlan(const OptimizedRenderGeometry& geometry, const VulkanRhiConfig& config = {}, VulkanDrawSubmissionMode mode = VulkanDrawSubmissionMode::DirectDrawIndexed);
    VulkanMeshUploadProbe BuildVulkanMeshUploadProbe();

    const char* ToString(VulkanBufferPlanKind kind);
    const char* ToString(VulkanMemoryPlanDomain domain);
    const char* ToString(VulkanDrawSubmissionMode mode);
    std::string ToDebugString(const VulkanGpuBufferPlan& plan);
    std::string ToDebugString(const VulkanFrameUniformPlan& plan);
    std::string ToDebugString(const VulkanDescriptorPlan& plan);
    std::string ToDebugString(const VulkanShaderBuildPlan& plan);
    std::string ToDebugString(const VulkanMeshUploadPlan& plan);
    std::string ToDebugString(const VulkanMeshUploadProbe& probe);
}
