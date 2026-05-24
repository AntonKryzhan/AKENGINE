#include <AK/Render/VulkanRuntimeArchitecture.hpp>

#include <algorithm>
#include <cstdint>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr u32 InvalidIndex = 0xFFFFFFFFu;

        u64 MixHash(u64 h, u64 v)
        {
            h ^= v + 0x9E3779B97F4A7C15ull + (h << 6u) + (h >> 2u);
            return h;
        }

        u64 HashString(const std::string& text)
        {
            u64 h = 1469598103934665603ull;
            for (char c : text)
            {
                h ^= static_cast<u8>(c);
                h *= 1099511628211ull;
            }
            return h;
        }

        u64 AlignUpU64(u64 value, u64 alignment)
        {
            if (alignment == 0)
            {
                return value;
            }
            const u64 mask = alignment - 1u;
            return (value + mask) & ~mask;
        }

        u32 ClampU32(u32 value, u32 lo, u32 hi)
        {
            return std::max(lo, std::min(value, hi));
        }

        VulkanFeatureRequirement MakeFeature(VulkanFeatureKind kind,
                                             VulkanFeatureProfileTier tier,
                                             const std::string& name,
                                             const std::string& coreVersion,
                                             const std::string& extensionName,
                                             bool required,
                                             bool hasFallback)
        {
            VulkanFeatureRequirement feature{};
            feature.kind = kind;
            feature.tier = tier;
            feature.name = name;
            feature.coreVersion = coreVersion;
            feature.extensionName = extensionName;
            feature.required = required;
            feature.enabledByDefault = required || tier == VulkanFeatureProfileTier::Preferred;
            feature.hasFallback = hasFallback;
            feature.valid = !feature.name.empty() && (!feature.required || !feature.coreVersion.empty() || !feature.extensionName.empty());
            return feature;
        }

        VulkanMemoryBlockAllocatorPlan MakeMemoryAllocator(VulkanMemoryBlockKind kind,
                                                           const std::string& name,
                                                           u64 blockBytes,
                                                           u32 blockCount,
                                                           u32 alignment,
                                                           u32 framesInFlight,
                                                           bool hostVisible,
                                                           bool deviceLocal,
                                                           bool persistentlyMapped,
                                                           bool ringBuffered)
        {
            VulkanMemoryBlockAllocatorPlan plan{};
            plan.kind = kind;
            plan.name = name;
            plan.blockBytes = AlignUpU64(blockBytes, alignment);
            plan.blockCount = std::max(1u, blockCount);
            plan.alignmentBytes = std::max(1u, alignment);
            plan.framesInFlight = std::max(1u, framesInFlight);
            plan.hostVisible = hostVisible;
            plan.hostCoherent = hostVisible;
            plan.deviceLocal = deviceLocal;
            plan.persistentlyMapped = persistentlyMapped;
            plan.ringBuffered = ringBuffered;
            plan.frameRetired = true;
            plan.subAllocated = true;
            plan.valid = plan.blockBytes > 0 && plan.blockCount > 0 && !plan.name.empty();
            return plan;
        }

        VulkanDescriptorSetFrequencyPlan MakeDescriptorSet(u32 setIndex,
                                                           VulkanDescriptorFrequency frequency,
                                                           VulkanDescriptorUpdatePolicy policy,
                                                           const std::string& name,
                                                           u32 uniforms,
                                                           u32 storage,
                                                           u32 images,
                                                           u32 samplers,
                                                           u32 maxDescriptors,
                                                           bool dynamicOffsets,
                                                           bool updateAfterBind,
                                                           bool writeDuringHotFrame)
        {
            VulkanDescriptorSetFrequencyPlan plan{};
            plan.setIndex = setIndex;
            plan.frequency = frequency;
            plan.updatePolicy = policy;
            plan.name = name;
            plan.uniformBindings = uniforms;
            plan.storageBindings = storage;
            plan.sampledImageBindings = images;
            plan.samplerBindings = samplers;
            plan.maxDescriptors = maxDescriptors;
            plan.dynamicOffsets = dynamicOffsets;
            plan.updateAfterBind = updateAfterBind;
            plan.cached = !writeDuringHotFrame;
            plan.writeDuringHotFrame = writeDuringHotFrame;
            plan.valid = !plan.name.empty() && plan.maxDescriptors > 0 && !(plan.writeDuringHotFrame && plan.frequency == VulkanDescriptorFrequency::Draw);
            return plan;
        }

        VulkanCommandPoolFrameThreadPlan MakeCommandPool(VulkanCommandPoolScope scope,
                                                         const std::string& name,
                                                         u32 framesInFlight,
                                                         u32 threadCount,
                                                         u32 poolsPerFrame,
                                                         u32 commandBuffersPerPool,
                                                         bool secondary)
        {
            VulkanCommandPoolFrameThreadPlan plan{};
            plan.scope = scope;
            plan.name = name;
            plan.framesInFlight = std::max(1u, framesInFlight);
            plan.threadCount = std::max(1u, threadCount);
            plan.poolsPerFrame = std::max(1u, poolsPerFrame);
            plan.commandBuffersPerPool = std::max(1u, commandBuffersPerPool);
            plan.resetPoolPerFrame = true;
            plan.oneTimeSubmit = true;
            plan.secondaryCommandBuffers = secondary;
            plan.externallySynchronized = true;
            plan.valid = !plan.name.empty();
            return plan;
        }

        VulkanFrameSubmitBatchPlan MakeSubmitBatch(VulkanSubmitBatchKind kind,
                                                   const std::string& name,
                                                   u32 queueFamily,
                                                   u32 waits,
                                                   u32 signals,
                                                   u32 commandBuffers,
                                                   u64 waitValue,
                                                   u64 signalValue,
                                                   bool async)
        {
            VulkanFrameSubmitBatchPlan plan{};
            plan.kind = kind;
            plan.name = name;
            plan.queueFamily = queueFamily;
            plan.waitSemaphores = waits;
            plan.signalSemaphores = signals;
            plan.commandBuffers = commandBuffers;
            plan.timelineWaitValue = waitValue;
            plan.timelineSignalValue = signalValue;
            plan.usesTimelineSemaphore = true;
            plan.canRunAsync = async;
            plan.valid = !plan.name.empty() && signalValue >= waitValue;
            return plan;
        }
    }

    VulkanDeviceLimitsSnapshot BuildDefaultVulkanDeviceLimitsSnapshot(const VulkanRhiConfig& config)
    {
        VulkanDeviceLimitsSnapshot limits{};
        limits.apiMajor = 1;
        limits.apiMinor = 3;
        limits.maxFramesInFlight = ClampU32(config.framesInFlight == 0 ? 3 : config.framesInFlight, 2, 4);
        limits.maxBoundDescriptorSets = 4;
        limits.maxPushConstantBytes = 128;
        limits.minUniformBufferOffsetAlignment = 256;
        limits.minStorageBufferOffsetAlignment = 256;
        limits.maxPerStageDescriptorSampledImages = config.requireDescriptorIndexing ? 4096 : 128;
        limits.maxDescriptorSetUpdateAfterBindSampledImages = config.requireDescriptorIndexing ? 16384 : 1024;
        limits.optimalBufferCopyOffsetAlignment = 256;
        limits.optimalBufferCopyRowPitchAlignment = 256;
        limits.supportsNonCoherentAtomSize = true;
        limits.supportsTimestampQueries = true;
        limits.valid = limits.maxFramesInFlight >= 2 && limits.maxPushConstantBytes >= 128;
        return limits;
    }

    std::vector<VulkanFeatureRequirement> BuildVulkanFeatureProfile(const VulkanRhiConfig& config)
    {
        std::vector<VulkanFeatureRequirement> features;
        features.reserve(13);

        features.push_back(MakeFeature(VulkanFeatureKind::Vulkan13, VulkanFeatureProfileTier::Required, "Vulkan 1.3 desktop baseline", "1.3", "", true, false));
        features.push_back(MakeFeature(VulkanFeatureKind::DynamicRendering, VulkanFeatureProfileTier::Required, "dynamic rendering", "1.3", "VK_KHR_dynamic_rendering", config.requireDynamicRendering, true));
        features.push_back(MakeFeature(VulkanFeatureKind::Synchronization2, VulkanFeatureProfileTier::Required, "synchronization2", "1.3", "VK_KHR_synchronization2", config.requireSynchronization2, true));
        features.push_back(MakeFeature(VulkanFeatureKind::TimelineSemaphore, VulkanFeatureProfileTier::Required, "timeline semaphore", "1.2", "VK_KHR_timeline_semaphore", config.requireTimelineSemaphore, false));
        features.push_back(MakeFeature(VulkanFeatureKind::DescriptorIndexing, VulkanFeatureProfileTier::Preferred, "descriptor indexing", "1.2", "VK_EXT_descriptor_indexing", config.requireDescriptorIndexing, true));
        features.push_back(MakeFeature(VulkanFeatureKind::BufferDeviceAddress, VulkanFeatureProfileTier::Preferred, "buffer device address", "1.2", "VK_KHR_buffer_device_address", config.requireBufferDeviceAddress, true));
        features.push_back(MakeFeature(VulkanFeatureKind::ScalarBlockLayout, VulkanFeatureProfileTier::Preferred, "scalar block layout", "1.2", "VK_EXT_scalar_block_layout", false, true));
        features.push_back(MakeFeature(VulkanFeatureKind::Maintenance4, VulkanFeatureProfileTier::Preferred, "maintenance4", "1.3", "VK_KHR_maintenance4", false, true));
        features.push_back(MakeFeature(VulkanFeatureKind::DynamicState, VulkanFeatureProfileTier::Preferred, "extended dynamic state", "1.3", "VK_EXT_extended_dynamic_state", false, true));
        features.push_back(MakeFeature(VulkanFeatureKind::ShaderObjects, VulkanFeatureProfileTier::Optional, "shader objects fast path", "", "VK_EXT_shader_object", false, true));
        features.push_back(MakeFeature(VulkanFeatureKind::PipelineBinary, VulkanFeatureProfileTier::Optional, "pipeline binary fast path", "", "VK_KHR_pipeline_binary", false, true));
        features.push_back(MakeFeature(VulkanFeatureKind::PushDescriptor, VulkanFeatureProfileTier::Optional, "push descriptors fast path", "1.4", "VK_KHR_push_descriptor", false, true));
        features.push_back(MakeFeature(VulkanFeatureKind::DeviceGeneratedCommands, VulkanFeatureProfileTier::Optional, "device generated commands future path", "", "VK_EXT_device_generated_commands", false, true));

        return features;
    }

    std::vector<VulkanMemoryBlockAllocatorPlan> BuildVulkanMemoryAllocatorPlan(const VulkanMeshUploadPlan& upload, const VulkanDeviceLimitsSnapshot& limits)
    {
        const u32 frames = std::max(2u, limits.maxFramesInFlight);
        const u64 staging = std::max<u64>(64u * 1024u, AlignUpU64(upload.stagingBytes + upload.uniforms.totalBytes, limits.optimalBufferCopyOffsetAlignment));
        const u64 geometryStatic = std::max<u64>(256u * 1024u, AlignUpU64(upload.deviceLocalBytes, limits.optimalBufferCopyOffsetAlignment));
        const u64 streamingGeometry = std::max<u64>(128u * 1024u, AlignUpU64(upload.deviceLocalBytes / 2u, limits.optimalBufferCopyOffsetAlignment));
        const u64 tables = std::max<u64>(64u * 1024u, AlignUpU64(upload.uniforms.objectBytesPerFrame + upload.uniforms.materialBytesPerFrame, limits.minStorageBufferOffsetAlignment));

        std::vector<VulkanMemoryBlockAllocatorPlan> plans;
        plans.reserve(8);
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::PersistentUpload, "persistent_upload_ring", staging, frames, limits.optimalBufferCopyOffsetAlignment, frames, true, false, true, true));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::TransientUpload, "transient_upload_scratch", staging / 2u, frames, limits.optimalBufferCopyOffsetAlignment, frames, true, false, true, true));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::DeviceStaticGeometry, "device_static_geometry_blocks", geometryStatic, 2, limits.optimalBufferCopyOffsetAlignment, frames, false, true, false, false));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::DeviceStreamingGeometry, "device_streaming_geometry_blocks", streamingGeometry, frames, limits.optimalBufferCopyOffsetAlignment, frames, false, true, false, true));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::DeviceMaterialTables, "device_object_material_tables", tables, frames, limits.minStorageBufferOffsetAlignment, frames, false, true, false, true));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::Readback, "readback_diagnostics_ring", 64u * 1024u, frames, limits.optimalBufferCopyOffsetAlignment, frames, true, false, true, true));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::TransientImage, "rendergraph_transient_images", 16u * 1024u * 1024u, frames, 4096, frames, false, true, false, true));
        plans.push_back(MakeMemoryAllocator(VulkanMemoryBlockKind::PipelineScratch, "pipeline_compile_scratch", 4u * 1024u * 1024u, 2, 4096, frames, true, false, true, false));
        return plans;
    }

    std::vector<VulkanDescriptorSetFrequencyPlan> BuildVulkanDescriptorFrequencyPlan(const VulkanDrawSubmissionPlan& submission, const VulkanDeviceLimitsSnapshot& limits)
    {
        const u32 materials = std::max(1u, submission.upload.uniforms.materialCount);
        const u32 objects = std::max(1u, submission.upload.uniforms.objectCount);
        const u32 bindlessMax = std::min<u32>(limits.maxDescriptorSetUpdateAfterBindSampledImages, 8192u);

        std::vector<VulkanDescriptorSetFrequencyPlan> sets;
        sets.reserve(4);
        sets.push_back(MakeDescriptorSet(0, VulkanDescriptorFrequency::Frame, VulkanDescriptorUpdatePolicy::PerFrameRing, "set0_frame_camera", 1, 0, 0, 0, limits.maxFramesInFlight, true, false, false));
        sets.push_back(MakeDescriptorSet(1, VulkanDescriptorFrequency::View, VulkanDescriptorUpdatePolicy::PerFrameRing, "set1_view_lighting", 1, 1, 0, 0, limits.maxFramesInFlight, true, false, false));
        sets.push_back(MakeDescriptorSet(2, VulkanDescriptorFrequency::Material, VulkanDescriptorUpdatePolicy::BindlessTable, "set2_material_bindless", 0, 1, bindlessMax, 1, std::max(bindlessMax, materials), false, true, false));
        sets.push_back(MakeDescriptorSet(3, VulkanDescriptorFrequency::Object, VulkanDescriptorUpdatePolicy::PushConstantIndex, "set3_object_draw_tables", 0, 2, 0, 0, std::max(objects, submission.batchStats.drawCalls), false, false, false));
        return sets;
    }

    VulkanPipelineCacheKeyPlan BuildVulkanPipelineCacheKeyPlan(const VulkanDrawSubmissionPlan& submission)
    {
        VulkanPipelineCacheKeyPlan key{};
        u64 shaderHash = 1469598103934665603ull;
        for (const VulkanShaderCacheEntry& entry : submission.shaderCache)
        {
            shaderHash = MixHash(shaderHash, entry.sourceHash);
            shaderHash = MixHash(shaderHash, HashString(entry.entryPoint));
            shaderHash = MixHash(shaderHash, HashString(entry.stage));
        }

        u64 vertexLayoutHash = 1469598103934665603ull;
        vertexLayoutHash = MixHash(vertexLayoutHash, submission.pipeline.vertexLayout.strideBytes);
        for (const VulkanVertexAttributeDesc& attribute : submission.pipeline.vertexLayout.attributes)
        {
            vertexLayoutHash = MixHash(vertexLayoutHash, HashString(attribute.semantic));
            vertexLayoutHash = MixHash(vertexLayoutHash, attribute.location);
            vertexLayoutHash = MixHash(vertexLayoutHash, static_cast<u32>(attribute.format));
            vertexLayoutHash = MixHash(vertexLayoutHash, attribute.offsetBytes);
        }

        u64 renderStateHash = 1469598103934665603ull;
        renderStateHash = MixHash(renderStateHash, HashString(submission.renderTarget.colorFormat));
        renderStateHash = MixHash(renderStateHash, HashString(submission.renderTarget.depthFormat));
        renderStateHash = MixHash(renderStateHash, static_cast<u32>(submission.pipeline.cullMode));
        renderStateHash = MixHash(renderStateHash, static_cast<u32>(submission.pipeline.depthCompare));
        renderStateHash = MixHash(renderStateHash, submission.pipeline.alphaBlend ? 1u : 0u);

        u64 layoutHash = 1469598103934665603ull;
        for (const VulkanDescriptorBindingPlan& binding : submission.layout.descriptorBindings)
        {
            layoutHash = MixHash(layoutHash, binding.set);
            layoutHash = MixHash(layoutHash, binding.binding);
            layoutHash = MixHash(layoutHash, static_cast<u32>(binding.resourceKind));
            layoutHash = MixHash(layoutHash, binding.descriptorCount);
        }
        layoutHash = MixHash(layoutHash, submission.layout.pushConstantBytes);

        key.shaderHash = shaderHash;
        key.vertexLayoutHash = vertexLayoutHash;
        key.renderStateHash = renderStateHash;
        key.pipelineLayoutHash = layoutHash;
        key.vendorId = 0;
        key.deviceId = 0;
        key.driverVersion = 0;
        key.cachePath = ".akcache/pipelines/vulkan_pipeline_cache.akpcache";
        key.includesDeviceIdentity = true;
        key.includesRenderTargetFormat = true;
        key.includesSpecializationConstants = true;
        key.valid = key.shaderHash != 0 && key.vertexLayoutHash != 0 && key.renderStateHash != 0 && key.pipelineLayoutHash != 0;
        return key;
    }

    VulkanShaderVariantKeyPlan BuildVulkanShaderVariantKeyPlan(const VulkanDrawSubmissionPlan& submission)
    {
        VulkanShaderVariantKeyPlan key{};
        key.materialFeatureMask = 0x00000001ull; // base color / normal path.
        key.materialFeatureMask |= submission.usesBindlessMaterialSet ? 0x00000002ull : 0ull;
        key.lightingFeatureMask = 0x00000001ull; // unlit/debug primitive path now; direct lighting next.
        key.geometryFeatureMask = submission.usesPackedVertex32 ? 0x00000001ull : 0ull;
        key.geometryFeatureMask |= submission.upload.usesCameraRelativePositions ? 0x00000002ull : 0ull;
        key.vertexLayoutId = submission.pipeline.vertexLayout.strideBytes;
        key.renderPassClass = submission.usesDynamicRendering ? 1u : 0u;
        key.staticSpecialization = true;
        key.dynamicFallbackUberShader = true;
        key.valid = key.vertexLayoutId > 0 && key.renderPassClass > 0;
        return key;
    }

    VulkanAsyncPipelineCompileQueuePlan BuildVulkanAsyncPipelineCompileQueuePlan(const VulkanDrawSubmissionPlan& submission)
    {
        VulkanAsyncPipelineCompileQueuePlan plan{};
        plan.workerCount = 2;
        plan.maxQueuedJobs = 256;
        plan.maxActiveJobs = 4;
        plan.warmedPipelines = submission.readyForPipelineCreation ? 1u : 0u;
        plan.fallbackPipelines = 1;
        plan.fallbackMode = VulkanPipelineFallbackMode::UberShaderUntilVariantReady;
        plan.diskCacheEnabled = true;
        plan.compileOffRenderThread = true;
        plan.stutterGuard = true;
        plan.valid = plan.workerCount > 0 && plan.fallbackPipelines > 0 && plan.compileOffRenderThread;
        return plan;
    }

    std::vector<VulkanCommandPoolFrameThreadPlan> BuildVulkanCommandPoolFrameThreadPlan(const VulkanRhiConfig& config, u32 workerThreads)
    {
        const u32 frames = ClampU32(config.framesInFlight == 0 ? 3 : config.framesInFlight, 2, 4);
        const u32 workers = std::max(1u, workerThreads);

        std::vector<VulkanCommandPoolFrameThreadPlan> plans;
        plans.reserve(5);
        plans.push_back(MakeCommandPool(VulkanCommandPoolScope::FrameGraphicsPrimary, "graphics_primary_per_frame", frames, 1, 1, 1, false));
        plans.push_back(MakeCommandPool(VulkanCommandPoolScope::FrameGraphicsSecondary, "graphics_secondary_per_frame", frames, 1, 1, 8, true));
        plans.push_back(MakeCommandPool(VulkanCommandPoolScope::WorkerSecondary, "worker_secondary_per_frame_thread", frames, workers, 1, 16, true));
        plans.push_back(MakeCommandPool(VulkanCommandPoolScope::TransferUpload, "transfer_upload_per_frame", frames, 1, 1, 4, false));
        plans.push_back(MakeCommandPool(VulkanCommandPoolScope::ComputeAsync, "compute_async_per_frame", frames, std::max(1u, workers / 2u), 1, 4, false));
        return plans;
    }

    VulkanRedundantBindFilterPlan BuildVulkanRedundantBindFilterPlan(const VulkanDrawSubmissionPlan& submission)
    {
        VulkanRedundantBindFilterPlan plan{};
        plan.inputCommands = static_cast<u32>(submission.commands.size());
        plan.outputCommands = plan.inputCommands;

        bool pipelineBound = false;
        bool descriptorsBound = false;
        bool vertexBound = false;
        bool indexBound = false;
        bool viewportSet = false;
        bool scissorSet = false;

        for (const VulkanRecordedDrawCommand& command : submission.commands)
        {
            switch (command.kind)
            {
                case VulkanDrawCommandKind::BindGraphicsPipeline:
                    if (pipelineBound)
                    {
                        ++plan.removedPipelineBinds;
                    }
                    pipelineBound = true;
                    break;
                case VulkanDrawCommandKind::BindDescriptorSets:
                    if (descriptorsBound)
                    {
                        ++plan.removedDescriptorBinds;
                    }
                    descriptorsBound = true;
                    break;
                case VulkanDrawCommandKind::BindVertexBuffer:
                    if (vertexBound)
                    {
                        ++plan.removedVertexBufferBinds;
                    }
                    vertexBound = true;
                    break;
                case VulkanDrawCommandKind::BindIndexBuffer:
                    if (indexBound)
                    {
                        ++plan.removedIndexBufferBinds;
                    }
                    indexBound = true;
                    break;
                case VulkanDrawCommandKind::SetViewport:
                    if (viewportSet)
                    {
                        ++plan.removedDynamicStateWrites;
                    }
                    viewportSet = true;
                    break;
                case VulkanDrawCommandKind::SetScissor:
                    if (scissorSet)
                    {
                        ++plan.removedDynamicStateWrites;
                    }
                    scissorSet = true;
                    break;
                case VulkanDrawCommandKind::BeginDynamicRendering:
                case VulkanDrawCommandKind::EndDynamicRendering:
                    pipelineBound = false;
                    descriptorsBound = false;
                    vertexBound = false;
                    indexBound = false;
                    viewportSet = false;
                    scissorSet = false;
                    break;
                default:
                    break;
            }
        }

        const u32 removed = plan.removedPipelineBinds + plan.removedDescriptorBinds + plan.removedVertexBufferBinds + plan.removedIndexBufferBinds + plan.removedDynamicStateWrites;
        plan.outputCommands = plan.inputCommands >= removed ? plan.inputCommands - removed : 0;
        plan.estimatedCpuBytesSaved = removed * 64u;
        plan.stableCommandOrder = true;
        plan.valid = plan.outputCommands <= plan.inputCommands;
        return plan;
    }

    std::vector<VulkanFrameSubmitBatchPlan> BuildVulkanFrameSubmitBatchPlan(const VulkanRhiConfig& config, const VulkanDrawSubmissionPlan& submission)
    {
        const u32 graphicsQueue = 0;
        const u32 transferQueue = config.preferDedicatedTransferQueue ? 1u : 0u;
        const u32 computeQueue = config.preferDedicatedComputeQueue ? 2u : 0u;
        const u32 drawCommands = static_cast<u32>(std::count_if(submission.commands.begin(), submission.commands.end(), [](const VulkanRecordedDrawCommand& command)
        {
            return command.kind == VulkanDrawCommandKind::DrawIndexed;
        }));

        std::vector<VulkanFrameSubmitBatchPlan> batches;
        batches.reserve(5);
        batches.push_back(MakeSubmitBatch(VulkanSubmitBatchKind::Acquire, "acquire_swapchain_image", graphicsQueue, 0, 1, 0, 0, 1, false));
        batches.push_back(MakeSubmitBatch(VulkanSubmitBatchKind::UploadTransfer, "upload_geometry_and_tables", transferQueue, 1, 1, submission.upload.copies.empty() ? 0u : 1u, 1, 2, transferQueue != graphicsQueue));
        batches.push_back(MakeSubmitBatch(VulkanSubmitBatchKind::AsyncCompute, "async_culling_future_slot", computeQueue, 1, 1, 0, 2, 3, computeQueue != graphicsQueue));
        batches.push_back(MakeSubmitBatch(VulkanSubmitBatchKind::Graphics, "graphics_draw_indexed", graphicsQueue, 2, 1, drawCommands > 0 ? 1u : 0u, 3, 4, false));
        batches.push_back(MakeSubmitBatch(VulkanSubmitBatchKind::Present, "present_swapchain_image", graphicsQueue, 1, 1, 0, 4, 5, false));
        return batches;
    }

    VulkanRuntimeArchitectureHardeningPlan BuildVulkanRuntimeArchitectureHardeningPlan(const VulkanRhiConfig& config)
    {
        VulkanRuntimeArchitectureHardeningPlan plan{};
        plan.rhi = BuildVulkanRhiPlan(config);
        const VulkanDrawSubmissionProbe submissionProbe = BuildVulkanDrawSubmissionProbe();
        plan.submission = submissionProbe.submission;
        plan.limits = BuildDefaultVulkanDeviceLimitsSnapshot(config);
        plan.features = BuildVulkanFeatureProfile(config);
        plan.memoryAllocators = BuildVulkanMemoryAllocatorPlan(plan.submission.upload, plan.limits);
        plan.descriptorSets = BuildVulkanDescriptorFrequencyPlan(plan.submission, plan.limits);
        plan.pipelineCache = BuildVulkanPipelineCacheKeyPlan(plan.submission);
        plan.shaderVariant = BuildVulkanShaderVariantKeyPlan(plan.submission);
        plan.compileQueue = BuildVulkanAsyncPipelineCompileQueuePlan(plan.submission);
        plan.commandPools = BuildVulkanCommandPoolFrameThreadPlan(config, 4);
        plan.bindFilter = BuildVulkanRedundantBindFilterPlan(plan.submission);
        plan.submitBatches = BuildVulkanFrameSubmitBatchPlan(config, plan.submission);

        plan.avoidsPerDrawDescriptorUpdates = std::all_of(plan.descriptorSets.begin(), plan.descriptorSets.end(), [](const VulkanDescriptorSetFrequencyPlan& descriptor)
        {
            return !(descriptor.frequency == VulkanDescriptorFrequency::Draw && descriptor.writeDuringHotFrame);
        });
        plan.avoidsRuntimeVkAllocateMemory = std::all_of(plan.memoryAllocators.begin(), plan.memoryAllocators.end(), [](const VulkanMemoryBlockAllocatorPlan& allocator)
        {
            return allocator.subAllocated && allocator.blockBytes > 0;
        });
        plan.usesDynamicRendering = plan.submission.usesDynamicRendering && config.requireDynamicRendering;
        plan.usesSynchronization2 = config.requireSynchronization2;
        plan.usesTimelineSemaphores = config.requireTimelineSemaphore;
        plan.supportsFallbackShaders = plan.compileQueue.fallbackMode != VulkanPipelineFallbackMode::None;
        plan.readyForRealVkHandles = plan.rhi.valid && plan.submission.readyForGpuSubmission && plan.pipelineCache.valid && plan.shaderVariant.valid && plan.compileQueue.valid && plan.bindFilter.valid;

        u32 warnings = 0;
        for (const VulkanFeatureRequirement& feature : plan.features)
        {
            if (!feature.valid)
            {
                ++warnings;
            }
        }
        for (const VulkanMemoryBlockAllocatorPlan& allocator : plan.memoryAllocators)
        {
            if (!allocator.valid)
            {
                ++warnings;
            }
        }
        for (const VulkanDescriptorSetFrequencyPlan& descriptor : plan.descriptorSets)
        {
            if (!descriptor.valid)
            {
                ++warnings;
            }
        }
        for (const VulkanCommandPoolFrameThreadPlan& commandPool : plan.commandPools)
        {
            if (!commandPool.valid)
            {
                ++warnings;
            }
        }
        for (const VulkanFrameSubmitBatchPlan& batch : plan.submitBatches)
        {
            if (!batch.valid)
            {
                ++warnings;
            }
        }
        if (!plan.avoidsPerDrawDescriptorUpdates || !plan.avoidsRuntimeVkAllocateMemory || !plan.usesDynamicRendering || !plan.usesSynchronization2 || !plan.usesTimelineSemaphores)
        {
            ++warnings;
        }

        plan.warnings = warnings;
        plan.valid = plan.readyForRealVkHandles && warnings == 0;

        std::ostringstream ss;
        ss << "vulkan runtime architecture hardened features=" << plan.features.size()
           << " allocators=" << plan.memoryAllocators.size()
           << " descriptor_sets=" << plan.descriptorSets.size()
           << " command_pools=" << plan.commandPools.size()
           << " submit_batches=" << plan.submitBatches.size()
           << " async_compile=" << (plan.compileQueue.valid ? "ready" : "no")
           << " bind_filter=" << (plan.bindFilter.valid ? "ready" : "no")
           << " warnings=" << plan.warnings;
        plan.summary = ss.str();
        return plan;
    }

    VulkanRuntimeArchitectureProbe BuildVulkanRuntimeArchitectureProbe()
    {
        VulkanRhiConfig config = MakeDefaultVulkanRhiConfig("AK Vulkan Architecture Probe");
        config.framesInFlight = 3;
        config.requireBufferDeviceAddress = false;
        config.window.width = 1920;
        config.window.height = 1080;
        VulkanRuntimeArchitectureProbe probe{};
        probe.plan = BuildVulkanRuntimeArchitectureHardeningPlan(config);
        probe.ok = probe.plan.valid;

        std::ostringstream ss;
        ss << "vulkan runtime architecture hardening ok=" << (probe.ok ? "true" : "false")
           << " features=" << probe.plan.features.size()
           << " allocators=" << probe.plan.memoryAllocators.size()
           << " descriptors=" << probe.plan.descriptorSets.size()
           << " command_pools=" << probe.plan.commandPools.size()
           << " submit_batches=" << probe.plan.submitBatches.size()
           << " warnings=" << probe.plan.warnings;
        probe.summary = ss.str();
        return probe;
    }

    const char* ToString(VulkanFeatureProfileTier tier)
    {
        switch (tier)
        {
            case VulkanFeatureProfileTier::Required: return "Required";
            case VulkanFeatureProfileTier::Preferred: return "Preferred";
            case VulkanFeatureProfileTier::Optional: return "Optional";
            case VulkanFeatureProfileTier::Fallback: return "Fallback";
        }
        return "Unknown";
    }

    const char* ToString(VulkanFeatureKind kind)
    {
        switch (kind)
        {
            case VulkanFeatureKind::Vulkan13: return "Vulkan13";
            case VulkanFeatureKind::DynamicRendering: return "DynamicRendering";
            case VulkanFeatureKind::Synchronization2: return "Synchronization2";
            case VulkanFeatureKind::TimelineSemaphore: return "TimelineSemaphore";
            case VulkanFeatureKind::DescriptorIndexing: return "DescriptorIndexing";
            case VulkanFeatureKind::BufferDeviceAddress: return "BufferDeviceAddress";
            case VulkanFeatureKind::ScalarBlockLayout: return "ScalarBlockLayout";
            case VulkanFeatureKind::Maintenance4: return "Maintenance4";
            case VulkanFeatureKind::DynamicState: return "DynamicState";
            case VulkanFeatureKind::ShaderObjects: return "ShaderObjects";
            case VulkanFeatureKind::PipelineBinary: return "PipelineBinary";
            case VulkanFeatureKind::PushDescriptor: return "PushDescriptor";
            case VulkanFeatureKind::DeviceGeneratedCommands: return "DeviceGeneratedCommands";
        }
        return "Unknown";
    }

    const char* ToString(VulkanDescriptorFrequency frequency)
    {
        switch (frequency)
        {
            case VulkanDescriptorFrequency::Frame: return "Frame";
            case VulkanDescriptorFrequency::View: return "View";
            case VulkanDescriptorFrequency::Material: return "Material";
            case VulkanDescriptorFrequency::Object: return "Object";
            case VulkanDescriptorFrequency::Draw: return "Draw";
            case VulkanDescriptorFrequency::BindlessTexture: return "BindlessTexture";
        }
        return "Unknown";
    }

    const char* ToString(VulkanDescriptorUpdatePolicy policy)
    {
        switch (policy)
        {
            case VulkanDescriptorUpdatePolicy::ImmutableAtLoad: return "ImmutableAtLoad";
            case VulkanDescriptorUpdatePolicy::PerFrameRing: return "PerFrameRing";
            case VulkanDescriptorUpdatePolicy::StreamingBatched: return "StreamingBatched";
            case VulkanDescriptorUpdatePolicy::PushConstantIndex: return "PushConstantIndex";
            case VulkanDescriptorUpdatePolicy::BindlessTable: return "BindlessTable";
        }
        return "Unknown";
    }

    const char* ToString(VulkanMemoryBlockKind kind)
    {
        switch (kind)
        {
            case VulkanMemoryBlockKind::PersistentUpload: return "PersistentUpload";
            case VulkanMemoryBlockKind::TransientUpload: return "TransientUpload";
            case VulkanMemoryBlockKind::DeviceStaticGeometry: return "DeviceStaticGeometry";
            case VulkanMemoryBlockKind::DeviceStreamingGeometry: return "DeviceStreamingGeometry";
            case VulkanMemoryBlockKind::DeviceMaterialTables: return "DeviceMaterialTables";
            case VulkanMemoryBlockKind::Readback: return "Readback";
            case VulkanMemoryBlockKind::TransientImage: return "TransientImage";
            case VulkanMemoryBlockKind::PipelineScratch: return "PipelineScratch";
        }
        return "Unknown";
    }

    const char* ToString(VulkanPipelineFallbackMode mode)
    {
        switch (mode)
        {
            case VulkanPipelineFallbackMode::None: return "None";
            case VulkanPipelineFallbackMode::UberShaderUntilVariantReady: return "UberShaderUntilVariantReady";
            case VulkanPipelineFallbackMode::EmbeddedFallbackShader: return "EmbeddedFallbackShader";
            case VulkanPipelineFallbackMode::AsyncWarmupOnly: return "AsyncWarmupOnly";
        }
        return "Unknown";
    }

    const char* ToString(VulkanCommandPoolScope scope)
    {
        switch (scope)
        {
            case VulkanCommandPoolScope::FrameGraphicsPrimary: return "FrameGraphicsPrimary";
            case VulkanCommandPoolScope::FrameGraphicsSecondary: return "FrameGraphicsSecondary";
            case VulkanCommandPoolScope::WorkerSecondary: return "WorkerSecondary";
            case VulkanCommandPoolScope::TransferUpload: return "TransferUpload";
            case VulkanCommandPoolScope::ComputeAsync: return "ComputeAsync";
        }
        return "Unknown";
    }

    const char* ToString(VulkanSubmitBatchKind kind)
    {
        switch (kind)
        {
            case VulkanSubmitBatchKind::Acquire: return "Acquire";
            case VulkanSubmitBatchKind::UploadTransfer: return "UploadTransfer";
            case VulkanSubmitBatchKind::Graphics: return "Graphics";
            case VulkanSubmitBatchKind::AsyncCompute: return "AsyncCompute";
            case VulkanSubmitBatchKind::Present: return "Present";
        }
        return "Unknown";
    }

    std::string ToDebugString(const VulkanFeatureRequirement& requirement)
    {
        std::ostringstream ss;
        ss << "feature name=\"" << requirement.name << "\" kind=" << ToString(requirement.kind)
           << " tier=" << ToString(requirement.tier)
           << " core=" << requirement.coreVersion
           << " ext=" << (requirement.extensionName.empty() ? "none" : requirement.extensionName)
           << " required=" << (requirement.required ? "true" : "false")
           << " fallback=" << (requirement.hasFallback ? "true" : "false")
           << " valid=" << (requirement.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanDeviceLimitsSnapshot& limits)
    {
        std::ostringstream ss;
        ss << "limits api=" << limits.apiMajor << "." << limits.apiMinor
           << " frames=" << limits.maxFramesInFlight
           << " sets=" << limits.maxBoundDescriptorSets
           << " push=" << limits.maxPushConstantBytes
           << " ubo_align=" << limits.minUniformBufferOffsetAlignment
           << " ssbo_align=" << limits.minStorageBufferOffsetAlignment
           << " bindless_images=" << limits.maxDescriptorSetUpdateAfterBindSampledImages
           << " copy_align=" << limits.optimalBufferCopyOffsetAlignment
           << " timestamps=" << (limits.supportsTimestampQueries ? "true" : "false")
           << " valid=" << (limits.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanMemoryBlockAllocatorPlan& plan)
    {
        std::ostringstream ss;
        ss << "memory_allocator name=" << plan.name
           << " kind=" << ToString(plan.kind)
           << " block=" << plan.blockBytes
           << " count=" << plan.blockCount
           << " align=" << plan.alignmentBytes
           << " frames=" << plan.framesInFlight
           << " host=" << (plan.hostVisible ? "true" : "false")
           << " device=" << (plan.deviceLocal ? "true" : "false")
           << " mapped=" << (plan.persistentlyMapped ? "true" : "false")
           << " ring=" << (plan.ringBuffered ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanDescriptorSetFrequencyPlan& plan)
    {
        std::ostringstream ss;
        ss << "descriptor_set set=" << plan.setIndex
           << " name=" << plan.name
           << " frequency=" << ToString(plan.frequency)
           << " policy=" << ToString(plan.updatePolicy)
           << " uniform=" << plan.uniformBindings
           << " storage=" << plan.storageBindings
           << " images=" << plan.sampledImageBindings
           << " samplers=" << plan.samplerBindings
           << " max=" << plan.maxDescriptors
           << " dynamic=" << (plan.dynamicOffsets ? "true" : "false")
           << " update_after_bind=" << (plan.updateAfterBind ? "true" : "false")
           << " hot_write=" << (plan.writeDuringHotFrame ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanPipelineCacheKeyPlan& plan)
    {
        std::ostringstream ss;
        ss << "pipeline_cache shader=0x" << std::hex << plan.shaderHash
           << " vertex=0x" << plan.vertexLayoutHash
           << " state=0x" << plan.renderStateHash
           << " layout=0x" << plan.pipelineLayoutHash << std::dec
           << " path=" << plan.cachePath
           << " device_id=" << (plan.includesDeviceIdentity ? "true" : "false")
           << " rt_format=" << (plan.includesRenderTargetFormat ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanShaderVariantKeyPlan& plan)
    {
        std::ostringstream ss;
        ss << "shader_variant material=0x" << std::hex << plan.materialFeatureMask
           << " lighting=0x" << plan.lightingFeatureMask
           << " geometry=0x" << plan.geometryFeatureMask << std::dec
           << " vertex_layout=" << plan.vertexLayoutId
           << " render_class=" << plan.renderPassClass
           << " static=" << (plan.staticSpecialization ? "true" : "false")
           << " uber_fallback=" << (plan.dynamicFallbackUberShader ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanAsyncPipelineCompileQueuePlan& plan)
    {
        std::ostringstream ss;
        ss << "async_pipeline_compile workers=" << plan.workerCount
           << " queued=" << plan.maxQueuedJobs
           << " active=" << plan.maxActiveJobs
           << " warmed=" << plan.warmedPipelines
           << " fallback=" << plan.fallbackPipelines
           << " mode=" << ToString(plan.fallbackMode)
           << " disk_cache=" << (plan.diskCacheEnabled ? "true" : "false")
           << " off_render_thread=" << (plan.compileOffRenderThread ? "true" : "false")
           << " stutter_guard=" << (plan.stutterGuard ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanCommandPoolFrameThreadPlan& plan)
    {
        std::ostringstream ss;
        ss << "command_pool name=" << plan.name
           << " scope=" << ToString(plan.scope)
           << " frames=" << plan.framesInFlight
           << " threads=" << plan.threadCount
           << " pools_per_frame=" << plan.poolsPerFrame
           << " buffers_per_pool=" << plan.commandBuffersPerPool
           << " secondary=" << (plan.secondaryCommandBuffers ? "true" : "false")
           << " one_time=" << (plan.oneTimeSubmit ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanRedundantBindFilterPlan& plan)
    {
        std::ostringstream ss;
        ss << "bind_filter input=" << plan.inputCommands
           << " output=" << plan.outputCommands
           << " pipeline=" << plan.removedPipelineBinds
           << " descriptors=" << plan.removedDescriptorBinds
           << " vb=" << plan.removedVertexBufferBinds
           << " ib=" << plan.removedIndexBufferBinds
           << " dyn=" << plan.removedDynamicStateWrites
           << " saved_cpu_bytes=" << plan.estimatedCpuBytesSaved
           << " stable=" << (plan.stableCommandOrder ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanFrameSubmitBatchPlan& plan)
    {
        std::ostringstream ss;
        ss << "submit_batch name=" << plan.name
           << " kind=" << ToString(plan.kind)
           << " queue=" << plan.queueFamily
           << " waits=" << plan.waitSemaphores
           << " signals=" << plan.signalSemaphores
           << " command_buffers=" << plan.commandBuffers
           << " timeline=" << plan.timelineWaitValue << "->" << plan.timelineSignalValue
           << " async=" << (plan.canRunAsync ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanRuntimeArchitectureHardeningPlan& plan)
    {
        std::ostringstream ss;
        ss << "vulkan_arch features=" << plan.features.size()
           << " allocators=" << plan.memoryAllocators.size()
           << " descriptor_sets=" << plan.descriptorSets.size()
           << " command_pools=" << plan.commandPools.size()
           << " submit_batches=" << plan.submitBatches.size()
           << " no_per_draw_descriptors=" << (plan.avoidsPerDrawDescriptorUpdates ? "true" : "false")
           << " no_runtime_alloc=" << (plan.avoidsRuntimeVkAllocateMemory ? "true" : "false")
           << " dynamic_rendering=" << (plan.usesDynamicRendering ? "true" : "false")
           << " sync2=" << (plan.usesSynchronization2 ? "true" : "false")
           << " timeline=" << (plan.usesTimelineSemaphores ? "true" : "false")
           << " fallback=" << (plan.supportsFallbackShaders ? "true" : "false")
           << " ready=" << (plan.readyForRealVkHandles ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false")
           << " warnings=" << plan.warnings;
        return ss.str();
    }

    std::string ToDebugString(const VulkanRuntimeArchitectureProbe& probe)
    {
        std::ostringstream ss;
        ss << "vulkan_arch_probe ok=" << (probe.ok ? "true" : "false")
           << " summary=\"" << probe.summary << "\"";
        return ss.str();
    }
}
