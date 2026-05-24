#include <AK/Render/VulkanUploadPlan.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace AK
{
    namespace
    {
        u64 AlignUp(u64 value, u64 alignment)
        {
            if (alignment == 0)
            {
                return value;
            }
            const u64 mask = alignment - 1u;
            return (value + mask) & ~mask;
        }

        bool IsPowerOfTwo(u64 value)
        {
            return value != 0 && (value & (value - 1u)) == 0;
        }

        u64 CountMaterials(const OptimizedRenderGeometry& geometry)
        {
            std::set<u32> materials;
            for (const RenderDrawCommand& draw : geometry.drawCommands)
            {
                if (draw.valid)
                {
                    materials.insert(draw.materialId);
                }
            }
            return std::max<u64>(1u, static_cast<u64>(materials.size()));
        }

        VulkanUploadCopyPlan MakeCopy(const std::string& name, u32 src, u32 dst, u64 srcOffset, u64 dstOffset, u64 bytes)
        {
            VulkanUploadCopyPlan copy{};
            copy.name = name;
            copy.sourceBuffer = src;
            copy.destinationBuffer = dst;
            copy.sourceOffset = srcOffset;
            copy.destinationOffset = dstOffset;
            copy.bytes = bytes;
            copy.aligned = (srcOffset % 4u) == 0 && (dstOffset % 4u) == 0 && (bytes % 4u) == 0;
            copy.valid = bytes > 0 && copy.aligned;
            return copy;
        }

        VulkanDrawIndexedRecord MakeDrawRecord(const RenderDrawCommand& draw, u32 drawOrdinal, const OptimizedRenderGeometry& geometry)
        {
            VulkanDrawIndexedRecord record{};
            record.name = draw.name.empty() ? ("draw_" + std::to_string(drawOrdinal)) : draw.name;
            record.sectionIndex = draw.sectionIndex;
            record.meshletIndex = 0xFFFFFFFFu;
            record.lodLevel = draw.lodLevel;
            record.indexCount = draw.indexCount;
            record.instanceCount = draw.instanceCount;
            record.firstIndex = draw.firstIndex;
            record.vertexOffset = static_cast<i32>(draw.vertexOffset);
            record.firstInstance = drawOrdinal;
            record.materialId = draw.materialId;
            record.cameraDistance = draw.cameraDistance;
            record.valid = draw.valid && draw.indexed && draw.indexCount > 0;

            for (u32 i = 0; i < geometry.meshlets.size(); ++i)
            {
                const RenderMeshlet& meshlet = geometry.meshlets[i];
                if (meshlet.valid && meshlet.sectionIndex == draw.sectionIndex && meshlet.firstIndex == draw.firstIndex)
                {
                    record.meshletIndex = i;
                    break;
                }
            }

            return record;
        }

        void MarkSortFlags(std::vector<VulkanDrawIndexedRecord>& draws)
        {
            bool frontToBack = true;
            bool materialSorted = true;
            for (u32 i = 1; i < draws.size(); ++i)
            {
                if (draws[i].cameraDistance + 1.0e-5f < draws[i - 1u].cameraDistance)
                {
                    frontToBack = false;
                }
                if (draws[i].materialId < draws[i - 1u].materialId)
                {
                    materialSorted = false;
                }
            }
            for (VulkanDrawIndexedRecord& draw : draws)
            {
                draw.frontToBackSorted = frontToBack;
                draw.materialSorted = materialSorted;
            }
        }
    }

    VulkanGpuBufferPlan MakeGpuBufferPlan(const std::string& name, VulkanBufferPlanKind kind, VulkanMemoryPlanDomain memory, u64 bytes, u32 strideBytes, u32 elementCount)
    {
        VulkanGpuBufferPlan plan{};
        plan.name = name;
        plan.kind = kind;
        plan.memory = memory;
        plan.bytes = bytes;
        plan.strideBytes = strideBytes;
        plan.elementCount = elementCount;
        plan.hostVisible = memory == VulkanMemoryPlanDomain::CpuUpload || memory == VulkanMemoryPlanDomain::CpuToGpuPersistent || memory == VulkanMemoryPlanDomain::GpuToCpuReadback;
        plan.deviceLocal = memory == VulkanMemoryPlanDomain::GpuOnly || memory == VulkanMemoryPlanDomain::CpuToGpuPersistent;
        plan.persistentlyMapped = memory == VulkanMemoryPlanDomain::CpuToGpuPersistent;
        plan.transferSrc = kind == VulkanBufferPlanKind::StagingUpload;
        plan.transferDst = kind == VulkanBufferPlanKind::DeviceVertex || kind == VulkanBufferPlanKind::DeviceIndex || kind == VulkanBufferPlanKind::DeviceIndirect || kind == VulkanBufferPlanKind::DeviceMeshlet;
        plan.vertexBuffer = kind == VulkanBufferPlanKind::DeviceVertex;
        plan.indexBuffer = kind == VulkanBufferPlanKind::DeviceIndex;
        plan.indirectBuffer = kind == VulkanBufferPlanKind::DeviceIndirect;
        plan.uniformBuffer = kind == VulkanBufferPlanKind::UniformRing;
        plan.storageBuffer = kind == VulkanBufferPlanKind::MaterialTable || kind == VulkanBufferPlanKind::ObjectTable || kind == VulkanBufferPlanKind::DeviceMeshlet;
        plan.valid = bytes > 0 && (strideBytes == 0 || elementCount > 0) && (!plan.persistentlyMapped || plan.hostVisible);
        return plan;
    }

    VulkanMeshUploadPlan BuildVulkanMeshUploadPlan(const OptimizedRenderGeometry& geometry, const VulkanRhiConfig& config, VulkanDrawSubmissionMode mode)
    {
        VulkanMeshUploadPlan plan{};
        plan.submissionMode = mode;
        plan.usesCombinedGeometryBuffers = true;
        plan.usesPackedVertices = true;
        plan.usesCameraRelativePositions = true;

        const u64 vertexBytes = static_cast<u64>(geometry.vertices.size()) * sizeof(PackedRenderVertex32);
        const u64 indexBytes = static_cast<u64>(geometry.indices.size()) * sizeof(u32);
        const u64 meshletBytes = static_cast<u64>(geometry.meshlets.size()) * 64u;
        const u64 drawIndirectBytes = static_cast<u64>(std::max<usize>(geometry.drawCommands.size(), 1u)) * 20u;
        const u64 materialCount = CountMaterials(geometry);
        const u64 materialBytes = AlignUp(materialCount * 64u, 256u);
        const u64 objectBytes = AlignUp(static_cast<u64>(std::max<usize>(geometry.drawCommands.size(), 1u)) * 128u, 256u);
        const u32 frames = std::max<u32>(2u, std::min<u32>(config.framesInFlight == 0 ? 3u : config.framesInFlight, 4u));

        const u64 alignedVertexBytes = AlignUp(vertexBytes, 16u);
        const u64 alignedIndexBytes = AlignUp(indexBytes, 16u);
        const u64 stagingBytes = AlignUp(alignedVertexBytes + alignedIndexBytes + meshletBytes + drawIndirectBytes, 256u);

        const u32 stagingIndex = static_cast<u32>(plan.buffers.size());
        plan.buffers.push_back(MakeGpuBufferPlan("frame_staging_upload_ring", VulkanBufferPlanKind::StagingUpload, VulkanMemoryPlanDomain::CpuUpload, stagingBytes, 0, 0));
        plan.vertexBufferIndex = static_cast<u32>(plan.buffers.size());
        plan.buffers.push_back(MakeGpuBufferPlan("combined_device_vertex_buffer_packed32", VulkanBufferPlanKind::DeviceVertex, VulkanMemoryPlanDomain::GpuOnly, alignedVertexBytes, sizeof(PackedRenderVertex32), static_cast<u32>(geometry.vertices.size())));
        plan.indexBufferIndex = static_cast<u32>(plan.buffers.size());
        plan.buffers.push_back(MakeGpuBufferPlan("combined_device_index_buffer_u32", VulkanBufferPlanKind::DeviceIndex, VulkanMemoryPlanDomain::GpuOnly, alignedIndexBytes, sizeof(u32), static_cast<u32>(geometry.indices.size())));
        const u32 meshletBufferIndex = static_cast<u32>(plan.buffers.size());
        plan.buffers.push_back(MakeGpuBufferPlan("meshlet_metadata_storage_buffer", VulkanBufferPlanKind::DeviceMeshlet, VulkanMemoryPlanDomain::GpuOnly, std::max<u64>(meshletBytes, 64u), 64u, static_cast<u32>(std::max<usize>(geometry.meshlets.size(), 1u))));
        plan.indirectBufferIndex = static_cast<u32>(plan.buffers.size());
        plan.buffers.push_back(MakeGpuBufferPlan("draw_indirect_command_buffer", VulkanBufferPlanKind::DeviceIndirect, VulkanMemoryPlanDomain::GpuOnly, std::max<u64>(drawIndirectBytes, 20u), 20u, static_cast<u32>(std::max<usize>(geometry.drawCommands.size(), 1u))));
        const u32 uniformBufferIndex = static_cast<u32>(plan.buffers.size());
        (void)uniformBufferIndex;
        plan.buffers.push_back(MakeGpuBufferPlan("per_frame_uniform_ring", VulkanBufferPlanKind::UniformRing, VulkanMemoryPlanDomain::CpuToGpuPersistent, AlignUp((256u + objectBytes + materialBytes) * frames, 256u), 256u, frames));
        plan.buffers.push_back(MakeGpuBufferPlan("material_table_storage", VulkanBufferPlanKind::MaterialTable, VulkanMemoryPlanDomain::GpuOnly, materialBytes, 64u, static_cast<u32>(materialCount)));
        plan.buffers.push_back(MakeGpuBufferPlan("object_table_storage", VulkanBufferPlanKind::ObjectTable, VulkanMemoryPlanDomain::GpuOnly, objectBytes, 128u, static_cast<u32>(std::max<usize>(geometry.drawCommands.size(), 1u))));

        plan.copies.push_back(MakeCopy("upload_vertices", stagingIndex, plan.vertexBufferIndex, 0u, 0u, alignedVertexBytes));
        plan.copies.push_back(MakeCopy("upload_indices", stagingIndex, plan.indexBufferIndex, alignedVertexBytes, 0u, alignedIndexBytes));
        plan.copies.push_back(MakeCopy("upload_meshlets", stagingIndex, meshletBufferIndex, alignedVertexBytes + alignedIndexBytes, 0u, std::max<u64>(meshletBytes, 64u)));
        plan.copies.push_back(MakeCopy("upload_indirect_draws", stagingIndex, plan.indirectBufferIndex, alignedVertexBytes + alignedIndexBytes + std::max<u64>(meshletBytes, 64u), 0u, std::max<u64>(drawIndirectBytes, 20u)));

        plan.draws.reserve(geometry.drawCommands.size());
        for (u32 i = 0; i < geometry.drawCommands.size(); ++i)
        {
            VulkanDrawIndexedRecord record = MakeDrawRecord(geometry.drawCommands[i], i, geometry);
            if (record.valid)
            {
                plan.draws.push_back(record);
            }
        }
        MarkSortFlags(plan.draws);

        plan.uniforms.framesInFlight = frames;
        plan.uniforms.cameraBytesPerFrame = 256u;
        plan.uniforms.objectBytesPerFrame = objectBytes;
        plan.uniforms.materialBytesPerFrame = materialBytes;
        plan.uniforms.totalBytes = AlignUp((plan.uniforms.cameraBytesPerFrame + objectBytes + materialBytes) * frames, 256u);
        plan.uniforms.objectCount = static_cast<u32>(std::max<usize>(geometry.drawCommands.size(), 1u));
        plan.uniforms.materialCount = static_cast<u32>(materialCount);
        plan.uniforms.dynamicOffsets = true;
        plan.uniforms.ringBuffered = true;
        plan.uniforms.valid = plan.uniforms.totalBytes > 0 && frames >= 2;

        plan.descriptors.descriptorSets = 2;
        plan.descriptors.uniformBindings = 1;
        plan.descriptors.storageBindings = 3;
        plan.descriptors.sampledImageBindings = 1;
        plan.descriptors.samplerBindings = 1;
        plan.descriptors.bindlessReady = config.requireDescriptorIndexing;
        plan.descriptors.materialTableAsStorage = true;
        plan.descriptors.objectTableAsStorage = true;
        plan.descriptors.valid = plan.descriptors.descriptorSets > 0 && plan.descriptors.uniformBindings > 0 && plan.descriptors.storageBindings >= 2;

        plan.shaders.shaderSourcePath = "assets/shaders/ak_primitive_mesh.slang";
        plan.shaders.vertexSpvPath = ".akcache/shaders/ak_primitive_mesh_vs.spv";
        plan.shaders.fragmentSpvPath = ".akcache/shaders/ak_primitive_mesh_fs.spv";
        plan.shaders.compiler = "slangc preferred, glslangValidator fallback";
        plan.shaders.embeddedFallbackShaders = true;
        plan.shaders.offlineCacheRequired = true;
        plan.shaders.hotReloadFriendly = true;
        plan.shaders.valid = !plan.shaders.vertexSpvPath.empty() && !plan.shaders.fragmentSpvPath.empty();

        plan.totalUploadBytes = stagingBytes;
        plan.stagingBytes = stagingBytes;
        for (const VulkanGpuBufferPlan& buffer : plan.buffers)
        {
            if (buffer.deviceLocal)
            {
                plan.deviceLocalBytes += buffer.bytes;
            }
            if (!buffer.valid)
            {
                ++plan.warnings;
            }
        }
        for (const VulkanUploadCopyPlan& copy : plan.copies)
        {
            if (!copy.valid)
            {
                ++plan.warnings;
            }
        }

        plan.drawCount = static_cast<u32>(plan.draws.size());
        plan.meshletDrawCount = static_cast<u32>(geometry.meshlets.size());
        plan.lodDrawCount = static_cast<u32>(geometry.lodRanges.size());
        plan.readyForVkBufferCreation = geometry.valid && vertexBytes > 0 && indexBytes > 0 && plan.vertexBufferIndex != 0xFFFFFFFFu && plan.indexBufferIndex != 0xFFFFFFFFu;
        plan.readyForDrawIndexedRecording = plan.readyForVkBufferCreation && !plan.draws.empty() && plan.uniforms.valid && plan.descriptors.valid && plan.shaders.valid;
        plan.valid = plan.readyForDrawIndexedRecording && plan.warnings == 0 && IsPowerOfTwo(256u);
        return plan;
    }

    VulkanMeshUploadProbe BuildVulkanMeshUploadProbe()
    {
        VulkanMeshUploadProbe probe{};
        probe.scene = BuildDefaultPrimitiveTestScene();
        const RenderMeshOptimizationConfig optConfig = MakeDefaultRenderMeshOptimizationConfig();
        probe.optimized = OptimizePrimitiveSceneForVulkan(probe.scene, optConfig);
        probe.optimizationStats = AnalyzeOptimizedRenderGeometry(probe.scene, probe.optimized, optConfig);

        VulkanRhiConfig rhi = MakeDefaultVulkanRhiConfig("AK Vulkan Upload Probe");
        rhi.framesInFlight = 3;
        rhi.window.platform = VulkanWindowPlatform::Win32;
        rhi.window.width = 1280;
        rhi.window.height = 720;
        rhi.window.valid = true;
        rhi.window.nativeHandle = reinterpret_cast<void*>(0x1);
        rhi.requireSwapchain = true;
        rhi.requireDescriptorIndexing = true;
        rhi.requireDynamicRendering = true;
        rhi.requireSynchronization2 = true;
        rhi.requireTimelineSemaphore = true;

        probe.pipeline = BuildVulkanPrimitiveGraphicsPipelinePlan(rhi);
        probe.pipeline.vertexLayout = BuildPackedVulkanMeshVertexLayout32();
        probe.pipelineValidation = ValidateVulkanGraphicsPipelinePlan(probe.pipeline);
        probe.upload = BuildVulkanMeshUploadPlan(probe.optimized, rhi, VulkanDrawSubmissionMode::DirectDrawIndexed);
        probe.ok = probe.scene.valid && probe.optimized.valid && probe.optimizationStats.valid && probe.pipelineValidation.ok && probe.upload.valid;
        probe.summary = probe.ok ? "ok" : "invalid";
        return probe;
    }

    const char* ToString(VulkanBufferPlanKind kind)
    {
        switch (kind)
        {
        case VulkanBufferPlanKind::StagingUpload: return "StagingUpload";
        case VulkanBufferPlanKind::DeviceVertex: return "DeviceVertex";
        case VulkanBufferPlanKind::DeviceIndex: return "DeviceIndex";
        case VulkanBufferPlanKind::DeviceIndirect: return "DeviceIndirect";
        case VulkanBufferPlanKind::DeviceMeshlet: return "DeviceMeshlet";
        case VulkanBufferPlanKind::UniformRing: return "UniformRing";
        case VulkanBufferPlanKind::MaterialTable: return "MaterialTable";
        case VulkanBufferPlanKind::ObjectTable: return "ObjectTable";
        default: return "Unknown";
        }
    }

    const char* ToString(VulkanMemoryPlanDomain domain)
    {
        switch (domain)
        {
        case VulkanMemoryPlanDomain::CpuUpload: return "CpuUpload";
        case VulkanMemoryPlanDomain::GpuOnly: return "GpuOnly";
        case VulkanMemoryPlanDomain::CpuToGpuPersistent: return "CpuToGpuPersistent";
        case VulkanMemoryPlanDomain::GpuToCpuReadback: return "GpuToCpuReadback";
        default: return "Unknown";
        }
    }

    const char* ToString(VulkanDrawSubmissionMode mode)
    {
        switch (mode)
        {
        case VulkanDrawSubmissionMode::DirectDrawIndexed: return "DirectDrawIndexed";
        case VulkanDrawSubmissionMode::MultiDrawIndirectPlanned: return "MultiDrawIndirectPlanned";
        case VulkanDrawSubmissionMode::MeshShaderPlanned: return "MeshShaderPlanned";
        default: return "Unknown";
        }
    }

    std::string ToDebugString(const VulkanGpuBufferPlan& plan)
    {
        std::ostringstream ss;
        ss << "buffer name=" << plan.name
           << " kind=" << ToString(plan.kind)
           << " memory=" << ToString(plan.memory)
           << " bytes=" << plan.bytes
           << " stride=" << plan.strideBytes
           << " elements=" << plan.elementCount
           << " host=" << (plan.hostVisible ? "true" : "false")
           << " device=" << (plan.deviceLocal ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanFrameUniformPlan& plan)
    {
        std::ostringstream ss;
        ss << "uniforms frames=" << plan.framesInFlight
           << " camera=" << plan.cameraBytesPerFrame
           << " object=" << plan.objectBytesPerFrame
           << " material=" << plan.materialBytesPerFrame
           << " total=" << plan.totalBytes
           << " objects=" << plan.objectCount
           << " materials=" << plan.materialCount
           << " ring=" << (plan.ringBuffered ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanDescriptorPlan& plan)
    {
        std::ostringstream ss;
        ss << "descriptors sets=" << plan.descriptorSets
           << " uniform=" << plan.uniformBindings
           << " storage=" << plan.storageBindings
           << " textures=" << plan.sampledImageBindings
           << " samplers=" << plan.samplerBindings
           << " bindless=" << (plan.bindlessReady ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanShaderBuildPlan& plan)
    {
        std::ostringstream ss;
        ss << "shaders source=" << plan.shaderSourcePath
           << " vs=" << plan.vertexSpvPath
           << " fs=" << plan.fragmentSpvPath
           << " compiler=" << plan.compiler
           << " fallback=" << (plan.embeddedFallbackShaders ? "true" : "false")
           << " hot_reload=" << (plan.hotReloadFriendly ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanMeshUploadPlan& plan)
    {
        std::ostringstream ss;
        ss << "vulkan_upload mode=" << ToString(plan.submissionMode)
           << " buffers=" << plan.buffers.size()
           << " copies=" << plan.copies.size()
           << " draws=" << plan.drawCount
           << " meshlets=" << plan.meshletDrawCount
           << " lods=" << plan.lodDrawCount
           << " staging=" << plan.stagingBytes
           << " device=" << plan.deviceLocalBytes
           << " packed=" << (plan.usesPackedVertices ? "true" : "false")
           << " combined=" << (plan.usesCombinedGeometryBuffers ? "true" : "false")
           << " camera_relative=" << (plan.usesCameraRelativePositions ? "true" : "false")
           << " ready_buffers=" << (plan.readyForVkBufferCreation ? "true" : "false")
           << " ready_draw=" << (plan.readyForDrawIndexedRecording ? "true" : "false")
           << " warnings=" << plan.warnings
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanMeshUploadProbe& probe)
    {
        std::ostringstream ss;
        ss << "vulkan_upload_probe ok=" << (probe.ok ? "true" : "false")
           << " source_vertices=" << probe.optimizationStats.sourceVertices
           << " packed_vertices=" << probe.optimizationStats.outputVertices
           << " draws=" << probe.upload.drawCount
           << " upload=" << probe.upload.totalUploadBytes
           << " device=" << probe.upload.deviceLocalBytes
           << " pipeline=" << (probe.pipelineValidation.ok ? "ok" : "bad")
           << " warnings=" << (probe.optimizationStats.validationWarnings + probe.upload.warnings + probe.pipelineValidation.warnings);
        return ss.str();
    }
}
