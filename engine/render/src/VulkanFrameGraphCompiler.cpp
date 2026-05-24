#include <AK/Render/VulkanFrameGraphCompiler.hpp>

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace AK
{
    namespace
    {
        constexpr u32 InvalidIndex = 0xFFFFFFFFu;

        u64 AlignUpU64(u64 value, u64 alignment)
        {
            if (alignment == 0)
            {
                return value;
            }
            const u64 mask = alignment - 1u;
            return (value + mask) & ~mask;
        }

        bool IsImageKind(VulkanGraphResourceKind kind)
        {
            return kind == VulkanGraphResourceKind::Image2D || kind == VulkanGraphResourceKind::SwapchainImage || kind == VulkanGraphResourceKind::DepthImage;
        }

        bool IsBufferKind(VulkanGraphResourceKind kind)
        {
            return kind == VulkanGraphResourceKind::Buffer;
        }

        bool IsWriteAccess(VulkanGraphAccess access)
        {
            switch (access)
            {
                case VulkanGraphAccess::TransferWrite:
                case VulkanGraphAccess::StorageWrite:
                case VulkanGraphAccess::ColorAttachmentWrite:
                case VulkanGraphAccess::DepthWrite:
                    return true;
                default:
                    return false;
            }
        }

        bool IsReadAccess(VulkanGraphAccess access)
        {
            return access != VulkanGraphAccess::None && !IsWriteAccess(access);
        }

        VulkanGraphQueueClass QueueForPassKind(VulkanGraphPassKind kind)
        {
            switch (kind)
            {
                case VulkanGraphPassKind::Upload:
                    return VulkanGraphQueueClass::Transfer;
                case VulkanGraphPassKind::Compute:
                    return VulkanGraphQueueClass::Compute;
                case VulkanGraphPassKind::Present:
                    return VulkanGraphQueueClass::Present;
                default:
                    return VulkanGraphQueueClass::Graphics;
            }
        }

        VulkanGraphLayout DefaultFinalLayout(VulkanGraphResourceKind kind)
        {
            switch (kind)
            {
                case VulkanGraphResourceKind::SwapchainImage:
                    return VulkanGraphLayout::PresentSrc;
                case VulkanGraphResourceKind::DepthImage:
                    return VulkanGraphLayout::DepthAttachment;
                case VulkanGraphResourceKind::Image2D:
                    return VulkanGraphLayout::ShaderReadOnly;
                case VulkanGraphResourceKind::Buffer:
                    return VulkanGraphLayout::Buffer;
                default:
                    return VulkanGraphLayout::General;
            }
        }

        u32 FindResourceIndex(const VulkanGraphDesc& graph, const std::string& name)
        {
            for (u32 i = 0; i < static_cast<u32>(graph.resources.size()); ++i)
            {
                if (graph.resources[i].name == name)
                {
                    return i;
                }
            }
            return InvalidIndex;
        }

        u64 ResourceBytes(const VulkanGraphResourceDesc& resource)
        {
            if (resource.byteSize > 0)
            {
                return AlignUpU64(resource.byteSize, 256u);
            }

            const u64 w = std::max<u32>(1u, resource.width);
            const u64 h = std::max<u32>(1u, resource.height);
            const u64 bytesPerPixel = resource.kind == VulkanGraphResourceKind::DepthImage ? 4u : 4u;
            return AlignUpU64(w * h * bytesPerPixel, 4096u);
        }

        bool UsesOverlap(const VulkanCompiledResourceLifetime& a, const VulkanCompiledResourceLifetime& b)
        {
            return !(a.lastPass < b.firstPass || b.lastPass < a.firstPass);
        }

        VulkanCompiledBarrier MakeBarrier(const VulkanGraphResourceDesc& resource,
                                          u32 resourceIndex,
                                          u32 beforePass,
                                          u32 afterPass,
                                          VulkanGraphStage srcStage,
                                          VulkanGraphStage dstStage,
                                          VulkanGraphAccess srcAccess,
                                          VulkanGraphAccess dstAccess,
                                          VulkanGraphLayout oldLayout,
                                          VulkanGraphLayout newLayout,
                                          VulkanGraphQueueClass srcQueue,
                                          VulkanGraphQueueClass dstQueue)
        {
            VulkanCompiledBarrier barrier{};
            barrier.resource = resource.name;
            barrier.resourceIndex = resourceIndex;
            barrier.beforePass = beforePass;
            barrier.afterPass = afterPass;
            barrier.srcStage = srcStage;
            barrier.dstStage = dstStage;
            barrier.srcAccess = srcAccess;
            barrier.dstAccess = dstAccess;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueue = srcQueue;
            barrier.dstQueue = dstQueue;
            barrier.ownershipTransfer = srcQueue != dstQueue && srcQueue != VulkanGraphQueueClass::Present && dstQueue != VulkanGraphQueueClass::Present;
            barrier.imageBarrier = IsImageKind(resource.kind);
            barrier.bufferBarrier = IsBufferKind(resource.kind);
            barrier.redundant = srcAccess == dstAccess && oldLayout == newLayout && srcQueue == dstQueue;
            barrier.valid = !barrier.resource.empty() && !barrier.redundant && (barrier.imageBarrier || barrier.bufferBarrier || resource.kind == VulkanGraphResourceKind::External);
            return barrier;
        }

        VulkanGraphResourceDesc MakeResource(const std::string& name,
                                             VulkanGraphResourceKind kind,
                                             u64 bytes,
                                             u32 width,
                                             u32 height,
                                             bool transient,
                                             bool imported,
                                             VulkanGraphLayout initialLayout,
                                             VulkanGraphLayout finalLayout)
        {
            VulkanGraphResourceDesc resource{};
            resource.name = name;
            resource.kind = kind;
            resource.byteSize = bytes;
            resource.width = width;
            resource.height = height;
            resource.formatId = kind == VulkanGraphResourceKind::DepthImage ? 126u : 50u;
            resource.transient = transient;
            resource.imported = imported;
            resource.hostVisible = kind == VulkanGraphResourceKind::Buffer && name.find("staging") != std::string::npos;
            resource.deviceLocal = !resource.hostVisible;
            resource.allowAliasing = transient && !imported;
            resource.initialLayout = initialLayout;
            resource.finalLayout = finalLayout == VulkanGraphLayout::Undefined ? DefaultFinalLayout(kind) : finalLayout;
            resource.valid = !resource.name.empty() && (kind == VulkanGraphResourceKind::Buffer ? resource.byteSize > 0 : resource.width > 0 && resource.height > 0);
            return resource;
        }

        VulkanGraphPassDesc MakePass(const std::string& name,
                                     VulkanGraphPassKind kind,
                                     std::vector<VulkanGraphResourceUse> uses,
                                     u32 draws,
                                     u32 dispatches,
                                     bool secondary,
                                     bool asyncCandidate)
        {
            VulkanGraphPassDesc pass{};
            pass.name = name;
            pass.kind = kind;
            pass.queue = QueueForPassKind(kind);
            pass.uses = std::move(uses);
            pass.estimatedDraws = draws;
            pass.estimatedDispatches = dispatches;
            pass.allowSecondaryCommandBuffers = secondary;
            pass.asyncCandidate = asyncCandidate;
            pass.present = kind == VulkanGraphPassKind::Present;
            pass.valid = !pass.name.empty();
            for (const VulkanGraphResourceUse& use : pass.uses)
            {
                pass.valid = pass.valid && use.valid;
            }
            return pass;
        }

        void CountPass(VulkanCompiledFrameGraph& compiled, const VulkanGraphPassDesc& pass)
        {
            switch (pass.queue)
            {
                case VulkanGraphQueueClass::Graphics:
                    ++compiled.graphicsPasses;
                    break;
                case VulkanGraphQueueClass::Transfer:
                    ++compiled.transferPasses;
                    break;
                case VulkanGraphQueueClass::Compute:
                    ++compiled.computePasses;
                    break;
                case VulkanGraphQueueClass::Present:
                    ++compiled.presentPasses;
                    break;
            }
        }
    }

    VulkanGraphResourceUse ReadResource(const std::string& name, VulkanGraphAccess access, VulkanGraphStage stage, VulkanGraphLayout layout)
    {
        VulkanGraphResourceUse use{};
        use.resource = name;
        use.access = access;
        use.stage = stage;
        use.layout = layout;
        use.read = true;
        use.write = false;
        use.discardBeforeWrite = false;
        use.preserveAfterPass = true;
        use.valid = !use.resource.empty() && IsReadAccess(access) && stage != VulkanGraphStage::None;
        return use;
    }

    VulkanGraphResourceUse WriteResource(const std::string& name, VulkanGraphAccess access, VulkanGraphStage stage, VulkanGraphLayout layout, bool discard)
    {
        VulkanGraphResourceUse use{};
        use.resource = name;
        use.access = access;
        use.stage = stage;
        use.layout = layout;
        use.read = false;
        use.write = true;
        use.discardBeforeWrite = discard;
        use.preserveAfterPass = true;
        use.valid = !use.resource.empty() && IsWriteAccess(access) && stage != VulkanGraphStage::None;
        return use;
    }

    VulkanGraphDesc BuildVulkanDefaultFrameGraphDesc(const VulkanDrawSubmissionPlan& submission, const VulkanRuntimeArchitectureHardeningPlan& architecture)
    {
        VulkanGraphDesc graph{};
        graph.framesInFlight = std::max(2u, architecture.limits.maxFramesInFlight);
        graph.useSynchronization2 = true;
        graph.useDynamicRendering = submission.usesDynamicRendering;
        graph.useTimelineSemaphores = true;
        graph.allowTransientAliasing = true;

        const u32 width = std::max(1u, submission.renderTarget.width);
        const u32 height = std::max(1u, submission.renderTarget.height);
        const u64 vertexBytes = (submission.upload.vertexBufferIndex < submission.upload.buffers.size()) ? std::max<u64>(1u, submission.upload.buffers[submission.upload.vertexBufferIndex].bytes) : 1u;
        const u64 indexBytes = (submission.upload.indexBufferIndex < submission.upload.buffers.size()) ? std::max<u64>(1u, submission.upload.buffers[submission.upload.indexBufferIndex].bytes) : 1u;
        const u64 stagingBytes = std::max<u64>(64u * 1024u, submission.upload.stagingBytes);
        const u64 uniformBytes = std::max<u64>(16u * 1024u, submission.upload.uniforms.totalBytes);

        graph.resources.reserve(10);
        graph.resources.push_back(MakeResource("swapchain_color", VulkanGraphResourceKind::SwapchainImage, 0, width, height, false, true, VulkanGraphLayout::Undefined, VulkanGraphLayout::PresentSrc));
        graph.resources.push_back(MakeResource("depth_d32", VulkanGraphResourceKind::DepthImage, 0, width, height, true, false, VulkanGraphLayout::Undefined, VulkanGraphLayout::DepthAttachment));
        graph.resources.push_back(MakeResource("frame_staging_upload_ring", VulkanGraphResourceKind::Buffer, stagingBytes, 0, 0, false, true, VulkanGraphLayout::Buffer, VulkanGraphLayout::Buffer));
        graph.resources.push_back(MakeResource("combined_device_vertex_buffer_packed32", VulkanGraphResourceKind::Buffer, vertexBytes, 0, 0, false, true, VulkanGraphLayout::Buffer, VulkanGraphLayout::Buffer));
        graph.resources.push_back(MakeResource("combined_device_index_buffer_u32", VulkanGraphResourceKind::Buffer, indexBytes, 0, 0, false, true, VulkanGraphLayout::Buffer, VulkanGraphLayout::Buffer));
        graph.resources.push_back(MakeResource("frame_uniform_ring", VulkanGraphResourceKind::Buffer, uniformBytes, 0, 0, false, true, VulkanGraphLayout::Buffer, VulkanGraphLayout::Buffer));
        graph.resources.push_back(MakeResource("object_material_tables", VulkanGraphResourceKind::Buffer, std::max<u64>(16u * 1024u, submission.upload.uniforms.objectBytesPerFrame + submission.upload.uniforms.materialBytesPerFrame), 0, 0, false, true, VulkanGraphLayout::Buffer, VulkanGraphLayout::Buffer));
        graph.resources.push_back(MakeResource("meshlet_metadata", VulkanGraphResourceKind::Buffer, std::max<u64>(4u * 1024u, static_cast<u64>(std::max(1u, submission.upload.meshletDrawCount)) * 64u), 0, 0, false, true, VulkanGraphLayout::Buffer, VulkanGraphLayout::Buffer));
        graph.resources.push_back(MakeResource("main_hdr_color", VulkanGraphResourceKind::Image2D, 0, width, height, true, false, VulkanGraphLayout::Undefined, VulkanGraphLayout::ShaderReadOnly));
        graph.resources.push_back(MakeResource("postprocess_color", VulkanGraphResourceKind::Image2D, 0, width, height, true, false, VulkanGraphLayout::Undefined, VulkanGraphLayout::ShaderReadOnly));

        graph.passes.reserve(7);
        graph.passes.push_back(MakePass("acquire_swapchain", VulkanGraphPassKind::Acquire,
            { WriteResource("swapchain_color", VulkanGraphAccess::ColorAttachmentWrite, VulkanGraphStage::ColorAttachmentOutput, VulkanGraphLayout::ColorAttachment, true) },
            0, 0, false, false));

        graph.passes.push_back(MakePass("upload_geometry_and_tables", VulkanGraphPassKind::Upload,
            {
                ReadResource("frame_staging_upload_ring", VulkanGraphAccess::TransferRead, VulkanGraphStage::Transfer, VulkanGraphLayout::Buffer),
                WriteResource("combined_device_vertex_buffer_packed32", VulkanGraphAccess::TransferWrite, VulkanGraphStage::Transfer, VulkanGraphLayout::Buffer, true),
                WriteResource("combined_device_index_buffer_u32", VulkanGraphAccess::TransferWrite, VulkanGraphStage::Transfer, VulkanGraphLayout::Buffer, true),
                WriteResource("frame_uniform_ring", VulkanGraphAccess::TransferWrite, VulkanGraphStage::Transfer, VulkanGraphLayout::Buffer, true),
                WriteResource("object_material_tables", VulkanGraphAccess::TransferWrite, VulkanGraphStage::Transfer, VulkanGraphLayout::Buffer, true),
                WriteResource("meshlet_metadata", VulkanGraphAccess::TransferWrite, VulkanGraphStage::Transfer, VulkanGraphLayout::Buffer, true)
            },
            0, 0, false, false));

        graph.passes.push_back(MakePass("depth_prepass", VulkanGraphPassKind::Graphics,
            {
                ReadResource("combined_device_vertex_buffer_packed32", VulkanGraphAccess::VertexBufferRead, VulkanGraphStage::VertexInput, VulkanGraphLayout::Buffer),
                ReadResource("combined_device_index_buffer_u32", VulkanGraphAccess::IndexBufferRead, VulkanGraphStage::VertexInput, VulkanGraphLayout::Buffer),
                ReadResource("frame_uniform_ring", VulkanGraphAccess::UniformRead, VulkanGraphStage::VertexShader, VulkanGraphLayout::Buffer),
                ReadResource("object_material_tables", VulkanGraphAccess::StorageRead, VulkanGraphStage::VertexShader, VulkanGraphLayout::Buffer),
                WriteResource("depth_d32", VulkanGraphAccess::DepthWrite, VulkanGraphStage::LateFragmentTests, VulkanGraphLayout::DepthAttachment, true)
            },
            submission.batchStats.drawCalls, 0, true, false));

        graph.passes.push_back(MakePass("forward_primitive_lit", VulkanGraphPassKind::Graphics,
            {
                ReadResource("combined_device_vertex_buffer_packed32", VulkanGraphAccess::VertexBufferRead, VulkanGraphStage::VertexInput, VulkanGraphLayout::Buffer),
                ReadResource("combined_device_index_buffer_u32", VulkanGraphAccess::IndexBufferRead, VulkanGraphStage::VertexInput, VulkanGraphLayout::Buffer),
                ReadResource("frame_uniform_ring", VulkanGraphAccess::UniformRead, VulkanGraphStage::VertexShader, VulkanGraphLayout::Buffer),
                ReadResource("object_material_tables", VulkanGraphAccess::StorageRead, VulkanGraphStage::FragmentShader, VulkanGraphLayout::Buffer),
                ReadResource("meshlet_metadata", VulkanGraphAccess::StorageRead, VulkanGraphStage::VertexShader, VulkanGraphLayout::Buffer),
                ReadResource("depth_d32", VulkanGraphAccess::DepthRead, VulkanGraphStage::EarlyFragmentTests, VulkanGraphLayout::DepthAttachment),
                WriteResource("main_hdr_color", VulkanGraphAccess::ColorAttachmentWrite, VulkanGraphStage::ColorAttachmentOutput, VulkanGraphLayout::ColorAttachment, true)
            },
            submission.batchStats.drawCalls, 0, true, false));

        graph.passes.push_back(MakePass("visibility_reduce_compute", VulkanGraphPassKind::Compute,
            {
                ReadResource("meshlet_metadata", VulkanGraphAccess::StorageRead, VulkanGraphStage::ComputeShader, VulkanGraphLayout::Buffer),
                ReadResource("frame_uniform_ring", VulkanGraphAccess::UniformRead, VulkanGraphStage::ComputeShader, VulkanGraphLayout::Buffer),
                WriteResource("object_material_tables", VulkanGraphAccess::StorageWrite, VulkanGraphStage::ComputeShader, VulkanGraphLayout::Buffer, false)
            },
            0, std::max(1u, (submission.upload.meshletDrawCount + 31u) / 32u), false, true));

        graph.passes.push_back(MakePass("tonemap_to_swapchain", VulkanGraphPassKind::Graphics,
            {
                ReadResource("main_hdr_color", VulkanGraphAccess::ShaderSampledRead, VulkanGraphStage::FragmentShader, VulkanGraphLayout::ShaderReadOnly),
                WriteResource("postprocess_color", VulkanGraphAccess::ColorAttachmentWrite, VulkanGraphStage::ColorAttachmentOutput, VulkanGraphLayout::ColorAttachment, true),
                WriteResource("swapchain_color", VulkanGraphAccess::ColorAttachmentWrite, VulkanGraphStage::ColorAttachmentOutput, VulkanGraphLayout::ColorAttachment, false)
            },
            1, 0, false, false));

        graph.passes.push_back(MakePass("present_swapchain", VulkanGraphPassKind::Present,
            { ReadResource("swapchain_color", VulkanGraphAccess::PresentRead, VulkanGraphStage::Present, VulkanGraphLayout::PresentSrc) },
            0, 0, false, false));

        graph.valid = !graph.resources.empty() && !graph.passes.empty();
        for (const VulkanGraphResourceDesc& resource : graph.resources)
        {
            graph.valid = graph.valid && resource.valid;
        }
        for (const VulkanGraphPassDesc& pass : graph.passes)
        {
            graph.valid = graph.valid && pass.valid;
        }
        return graph;
    }

    VulkanCompiledFrameGraph CompileVulkanFrameGraph(const VulkanGraphDesc& graph)
    {
        VulkanCompiledFrameGraph compiled{};
        compiled.source = graph;
        compiled.usesSynchronization2 = graph.useSynchronization2;
        compiled.usesDynamicRendering = graph.useDynamicRendering;
        compiled.usesTimelineSemaphores = graph.useTimelineSemaphores;

        const u32 resourceCount = static_cast<u32>(graph.resources.size());
        const u32 passCount = static_cast<u32>(graph.passes.size());
        std::vector<VulkanGraphAccess> lastAccess(resourceCount, VulkanGraphAccess::None);
        std::vector<VulkanGraphStage> lastStage(resourceCount, VulkanGraphStage::TopOfPipe);
        std::vector<VulkanGraphLayout> lastLayout(resourceCount, VulkanGraphLayout::Undefined);
        std::vector<VulkanGraphQueueClass> lastQueue(resourceCount, VulkanGraphQueueClass::Graphics);
        std::vector<u32> lastPass(resourceCount, InvalidIndex);
        std::vector<bool> everWritten(resourceCount, false);

        compiled.lifetimes.reserve(resourceCount);
        for (u32 resourceIndex = 0; resourceIndex < resourceCount; ++resourceIndex)
        {
            const VulkanGraphResourceDesc& resource = graph.resources[resourceIndex];
            VulkanCompiledResourceLifetime lifetime{};
            lifetime.name = resource.name;
            lifetime.resourceIndex = resourceIndex;
            lifetime.firstPass = InvalidIndex;
            lifetime.lastPass = 0;
            lifetime.aliasGroup = InvalidIndex;
            lifetime.byteSize = ResourceBytes(resource);
            lifetime.transient = resource.transient;
            lifetime.imported = resource.imported;
            lifetime.valid = resource.valid;
            compiled.lifetimes.push_back(lifetime);
            lastLayout[resourceIndex] = resource.initialLayout;
        }

        for (u32 passIndex = 0; passIndex < passCount; ++passIndex)
        {
            const VulkanGraphPassDesc& pass = graph.passes[passIndex];
            VulkanCompiledPass compiledPass{};
            compiledPass.name = pass.name;
            compiledPass.kind = pass.kind;
            compiledPass.queue = pass.queue;
            compiledPass.passIndex = passIndex;
            compiledPass.barrierBegin = static_cast<u32>(compiled.barriers.size());
            compiledPass.resourceUseCount = static_cast<u32>(pass.uses.size());
            compiledPass.estimatedDraws = pass.estimatedDraws;
            compiledPass.estimatedDispatches = pass.estimatedDispatches;
            compiledPass.secondaryCommandBuffers = pass.allowSecondaryCommandBuffers ? std::max(1u, (pass.estimatedDraws + 31u) / 32u) : 0;
            compiledPass.dynamicRendering = graph.useDynamicRendering && pass.kind == VulkanGraphPassKind::Graphics;
            compiledPass.asyncScheduled = pass.asyncCandidate && pass.queue != VulkanGraphQueueClass::Graphics;
            compiledPass.present = pass.present;
            compiledPass.valid = pass.valid;
            compiled.secondaryCommandBuffers += compiledPass.secondaryCommandBuffers;
            CountPass(compiled, pass);

            for (const VulkanGraphResourceUse& use : pass.uses)
            {
                const u32 resourceIndex = FindResourceIndex(graph, use.resource);
                if (resourceIndex == InvalidIndex)
                {
                    ++compiled.validation.missingResources;
                    compiledPass.valid = false;
                    continue;
                }

                VulkanCompiledResourceLifetime& lifetime = compiled.lifetimes[resourceIndex];
                lifetime.firstPass = lifetime.firstPass == InvalidIndex ? passIndex : std::min(lifetime.firstPass, passIndex);
                lifetime.lastPass = std::max(lifetime.lastPass, passIndex);

                const VulkanGraphResourceDesc& resource = graph.resources[resourceIndex];
                const bool readsWithoutWrite = use.read && !everWritten[resourceIndex] && !resource.imported && resource.initialLayout == VulkanGraphLayout::Undefined;
                if (readsWithoutWrite)
                {
                    ++compiled.validation.readBeforeWrite;
                }

                const bool shouldBarrier = lastPass[resourceIndex] != InvalidIndex &&
                    (lastAccess[resourceIndex] != use.access || lastLayout[resourceIndex] != use.layout || lastQueue[resourceIndex] != pass.queue || IsWriteAccess(lastAccess[resourceIndex]) || IsWriteAccess(use.access));

                if (shouldBarrier)
                {
                    VulkanCompiledBarrier barrier = MakeBarrier(resource,
                                                                resourceIndex,
                                                                lastPass[resourceIndex],
                                                                passIndex,
                                                                lastStage[resourceIndex],
                                                                use.stage,
                                                                lastAccess[resourceIndex],
                                                                use.access,
                                                                lastLayout[resourceIndex],
                                                                use.layout,
                                                                lastQueue[resourceIndex],
                                                                pass.queue);
                    if (barrier.redundant)
                    {
                        ++compiled.validation.redundantBarriers;
                    }
                    else
                    {
                        compiled.barriers.push_back(barrier);
                    }
                }
                else if (lastPass[resourceIndex] == InvalidIndex && IsImageKind(resource.kind) && resource.initialLayout != use.layout)
                {
                    VulkanCompiledBarrier barrier = MakeBarrier(resource,
                                                                resourceIndex,
                                                                passIndex,
                                                                passIndex,
                                                                VulkanGraphStage::TopOfPipe,
                                                                use.stage,
                                                                VulkanGraphAccess::None,
                                                                use.access,
                                                                resource.initialLayout,
                                                                use.layout,
                                                                pass.queue,
                                                                pass.queue);
                    if (!barrier.redundant)
                    {
                        compiled.barriers.push_back(barrier);
                    }
                }

                if (use.layout == VulkanGraphLayout::Undefined && resource.kind != VulkanGraphResourceKind::Buffer)
                {
                    ++compiled.validation.invalidLayouts;
                }

                everWritten[resourceIndex] = everWritten[resourceIndex] || use.write;
                lastAccess[resourceIndex] = use.access;
                lastStage[resourceIndex] = use.stage;
                lastLayout[resourceIndex] = use.layout;
                lastQueue[resourceIndex] = pass.queue;
                lastPass[resourceIndex] = passIndex;
            }

            compiledPass.barrierCount = static_cast<u32>(compiled.barriers.size()) - compiledPass.barrierBegin;
            compiled.passes.push_back(compiledPass);
        }

        for (u32 resourceIndex = 0; resourceIndex < resourceCount; ++resourceIndex)
        {
            const VulkanGraphResourceDesc& resource = graph.resources[resourceIndex];
            if (lastPass[resourceIndex] == InvalidIndex)
            {
                continue;
            }
            if (resource.finalLayout != VulkanGraphLayout::Undefined && lastLayout[resourceIndex] != resource.finalLayout)
            {
                VulkanCompiledBarrier barrier = MakeBarrier(resource,
                                                            resourceIndex,
                                                            lastPass[resourceIndex],
                                                            lastPass[resourceIndex],
                                                            lastStage[resourceIndex],
                                                            VulkanGraphStage::BottomOfPipe,
                                                            lastAccess[resourceIndex],
                                                            VulkanGraphAccess::None,
                                                            lastLayout[resourceIndex],
                                                            resource.finalLayout,
                                                            lastQueue[resourceIndex],
                                                            lastQueue[resourceIndex]);
                if (!barrier.redundant)
                {
                    compiled.barriers.push_back(barrier);
                }
            }
        }

        std::vector<u32> aliasOrder;
        aliasOrder.reserve(resourceCount);
        for (u32 i = 0; i < resourceCount; ++i)
        {
            VulkanCompiledResourceLifetime& lifetime = compiled.lifetimes[i];
            if (lifetime.firstPass == InvalidIndex)
            {
                lifetime.firstPass = 0;
                lifetime.lastPass = 0;
            }
            if (graph.resources[i].transient && graph.resources[i].allowAliasing && !graph.resources[i].imported)
            {
                aliasOrder.push_back(i);
                compiled.transientBytesUnaliased += lifetime.byteSize;
            }
        }

        std::sort(aliasOrder.begin(), aliasOrder.end(), [&](u32 a, u32 b) {
            const VulkanCompiledResourceLifetime& la = compiled.lifetimes[a];
            const VulkanCompiledResourceLifetime& lb = compiled.lifetimes[b];
            if (la.firstPass != lb.firstPass)
            {
                return la.firstPass < lb.firstPass;
            }
            return la.byteSize > lb.byteSize;
        });

        for (u32 resourceIndex : aliasOrder)
        {
            VulkanCompiledResourceLifetime& lifetime = compiled.lifetimes[resourceIndex];
            bool placed = false;
            if (graph.allowTransientAliasing)
            {
                for (VulkanTransientAliasGroup& group : compiled.aliasGroups)
                {
                    bool overlaps = false;
                    for (u32 existingIndex : group.resourceIndices)
                    {
                        if (UsesOverlap(compiled.lifetimes[existingIndex], lifetime))
                        {
                            overlaps = true;
                            break;
                        }
                    }
                    if (!overlaps)
                    {
                        group.resourceIndices.push_back(resourceIndex);
                        group.maxBytes = std::max(group.maxBytes, lifetime.byteSize);
                        group.totalUnaliasedBytes += lifetime.byteSize;
                        group.firstPass = std::min(group.firstPass, lifetime.firstPass);
                        group.lastPass = std::max(group.lastPass, lifetime.lastPass);
                        group.hasImage = group.hasImage || IsImageKind(graph.resources[resourceIndex].kind);
                        group.hasBuffer = group.hasBuffer || IsBufferKind(graph.resources[resourceIndex].kind);
                        lifetime.aliasGroup = group.groupIndex;
                        lifetime.aliased = group.resourceIndices.size() > 1;
                        placed = true;
                        break;
                    }
                }
            }
            if (!placed)
            {
                VulkanTransientAliasGroup group{};
                group.groupIndex = static_cast<u32>(compiled.aliasGroups.size());
                group.resourceIndices.push_back(resourceIndex);
                group.maxBytes = lifetime.byteSize;
                group.totalUnaliasedBytes = lifetime.byteSize;
                group.firstPass = lifetime.firstPass;
                group.lastPass = lifetime.lastPass;
                group.hasImage = IsImageKind(graph.resources[resourceIndex].kind);
                group.hasBuffer = IsBufferKind(graph.resources[resourceIndex].kind);
                group.valid = true;
                lifetime.aliasGroup = group.groupIndex;
                compiled.aliasGroups.push_back(group);
            }
        }

        for (VulkanTransientAliasGroup& group : compiled.aliasGroups)
        {
            group.valid = !group.resourceIndices.empty();
            compiled.transientBytesAliased += group.maxBytes;
            if (group.hasBuffer && group.hasImage && group.resourceIndices.size() > 1)
            {
                ++compiled.validation.aliasingHazards;
            }
        }
        compiled.transientBytesSaved = compiled.transientBytesUnaliased > compiled.transientBytesAliased ? compiled.transientBytesUnaliased - compiled.transientBytesAliased : 0;

        VulkanFrameGraphQueueBatch current{};
        current.valid = false;
        u64 timeline = 1;
        for (const VulkanCompiledPass& pass : compiled.passes)
        {
            const bool beginNew = !current.valid || current.queue != pass.queue || pass.present || current.present;
            if (beginNew)
            {
                if (current.valid)
                {
                    compiled.queueBatches.push_back(current);
                }
                current = {};
                current.queue = pass.queue;
                current.firstPass = pass.passIndex;
                current.passCount = 0;
                current.commandBuffers = 0;
                current.waitSemaphores = pass.passIndex == 0 ? 0 : 1;
                current.signalSemaphores = 1;
                current.timelineWait = timeline > 1 ? timeline - 1 : 0;
                current.timelineSignal = timeline++;
                current.asyncCompute = pass.queue == VulkanGraphQueueClass::Compute;
                current.transfer = pass.queue == VulkanGraphQueueClass::Transfer;
                current.present = pass.queue == VulkanGraphQueueClass::Present;
                current.valid = true;
            }
            current.passCount += 1;
            current.commandBuffers += std::max(1u, pass.secondaryCommandBuffers == 0 ? 1u : pass.secondaryCommandBuffers);
        }
        if (current.valid)
        {
            compiled.queueBatches.push_back(current);
        }

        compiled.validation.queueHazards = 0;
        for (const VulkanCompiledBarrier& barrier : compiled.barriers)
        {
            if (barrier.ownershipTransfer)
            {
                ++compiled.validation.queueHazards;
            }
        }
        compiled.validation.warnings = compiled.validation.missingResources + compiled.validation.readBeforeWrite + compiled.validation.invalidLayouts + compiled.validation.aliasingHazards;
        compiled.validation.valid = compiled.validation.warnings == 0;

        compiled.readyForVkCommandRecording = graph.valid && compiled.validation.missingResources == 0 && compiled.validation.invalidLayouts == 0 && !compiled.passes.empty();
        compiled.readyForRenderGraphExecution = compiled.readyForVkCommandRecording && compiled.usesSynchronization2 && compiled.usesDynamicRendering && compiled.usesTimelineSemaphores;
        compiled.valid = compiled.readyForRenderGraphExecution && compiled.validation.valid;

        std::ostringstream oss;
        oss << "vulkan_framegraph passes=" << compiled.passes.size()
            << " resources=" << graph.resources.size()
            << " barriers=" << compiled.barriers.size()
            << " alias_groups=" << compiled.aliasGroups.size()
            << " batches=" << compiled.queueBatches.size()
            << " saved=" << compiled.transientBytesSaved
            << " ready=" << (compiled.readyForRenderGraphExecution ? "true" : "false")
            << " warnings=" << compiled.validation.warnings;
        compiled.summary = oss.str();
        return compiled;
    }

    VulkanFrameGraphCompilerProbe BuildVulkanFrameGraphCompilerProbe()
    {
        VulkanFrameGraphCompilerProbe probe{};
        probe.architecture = BuildVulkanRuntimeArchitectureProbe();
        probe.draw = BuildVulkanDrawSubmissionProbe();
        probe.graph = BuildVulkanDefaultFrameGraphDesc(probe.draw.submission, probe.architecture.plan);
        probe.compiled = CompileVulkanFrameGraph(probe.graph);
        probe.ok = probe.architecture.ok && probe.draw.ok && probe.graph.valid && probe.compiled.valid;

        std::ostringstream oss;
        oss << "vulkan framegraph compiler probe ok=" << (probe.ok ? "true" : "false")
            << " passes=" << probe.compiled.passes.size()
            << " resources=" << probe.graph.resources.size()
            << " barriers=" << probe.compiled.barriers.size()
            << " alias_groups=" << probe.compiled.aliasGroups.size()
            << " saved=" << probe.compiled.transientBytesSaved;
        probe.summary = oss.str();
        return probe;
    }

    const char* ToString(VulkanGraphResourceKind kind)
    {
        switch (kind)
        {
            case VulkanGraphResourceKind::Buffer: return "Buffer";
            case VulkanGraphResourceKind::Image2D: return "Image2D";
            case VulkanGraphResourceKind::SwapchainImage: return "SwapchainImage";
            case VulkanGraphResourceKind::DepthImage: return "DepthImage";
            case VulkanGraphResourceKind::External: return "External";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanGraphAccess access)
    {
        switch (access)
        {
            case VulkanGraphAccess::None: return "None";
            case VulkanGraphAccess::TransferRead: return "TransferRead";
            case VulkanGraphAccess::TransferWrite: return "TransferWrite";
            case VulkanGraphAccess::VertexBufferRead: return "VertexBufferRead";
            case VulkanGraphAccess::IndexBufferRead: return "IndexBufferRead";
            case VulkanGraphAccess::UniformRead: return "UniformRead";
            case VulkanGraphAccess::StorageRead: return "StorageRead";
            case VulkanGraphAccess::StorageWrite: return "StorageWrite";
            case VulkanGraphAccess::ColorAttachmentRead: return "ColorAttachmentRead";
            case VulkanGraphAccess::ColorAttachmentWrite: return "ColorAttachmentWrite";
            case VulkanGraphAccess::DepthRead: return "DepthRead";
            case VulkanGraphAccess::DepthWrite: return "DepthWrite";
            case VulkanGraphAccess::ShaderSampledRead: return "ShaderSampledRead";
            case VulkanGraphAccess::PresentRead: return "PresentRead";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanGraphStage stage)
    {
        switch (stage)
        {
            case VulkanGraphStage::None: return "None";
            case VulkanGraphStage::TopOfPipe: return "TopOfPipe";
            case VulkanGraphStage::Transfer: return "Transfer";
            case VulkanGraphStage::VertexInput: return "VertexInput";
            case VulkanGraphStage::VertexShader: return "VertexShader";
            case VulkanGraphStage::FragmentShader: return "FragmentShader";
            case VulkanGraphStage::EarlyFragmentTests: return "EarlyFragmentTests";
            case VulkanGraphStage::LateFragmentTests: return "LateFragmentTests";
            case VulkanGraphStage::ColorAttachmentOutput: return "ColorAttachmentOutput";
            case VulkanGraphStage::ComputeShader: return "ComputeShader";
            case VulkanGraphStage::BottomOfPipe: return "BottomOfPipe";
            case VulkanGraphStage::Present: return "Present";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanGraphLayout layout)
    {
        switch (layout)
        {
            case VulkanGraphLayout::Undefined: return "Undefined";
            case VulkanGraphLayout::General: return "General";
            case VulkanGraphLayout::TransferSrc: return "TransferSrc";
            case VulkanGraphLayout::TransferDst: return "TransferDst";
            case VulkanGraphLayout::ColorAttachment: return "ColorAttachment";
            case VulkanGraphLayout::DepthAttachment: return "DepthAttachment";
            case VulkanGraphLayout::ShaderReadOnly: return "ShaderReadOnly";
            case VulkanGraphLayout::PresentSrc: return "PresentSrc";
            case VulkanGraphLayout::Buffer: return "Buffer";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanGraphQueueClass queue)
    {
        switch (queue)
        {
            case VulkanGraphQueueClass::Graphics: return "Graphics";
            case VulkanGraphQueueClass::Transfer: return "Transfer";
            case VulkanGraphQueueClass::Compute: return "Compute";
            case VulkanGraphQueueClass::Present: return "Present";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanGraphPassKind kind)
    {
        switch (kind)
        {
            case VulkanGraphPassKind::Acquire: return "Acquire";
            case VulkanGraphPassKind::Upload: return "Upload";
            case VulkanGraphPassKind::Graphics: return "Graphics";
            case VulkanGraphPassKind::Compute: return "Compute";
            case VulkanGraphPassKind::Present: return "Present";
            case VulkanGraphPassKind::Diagnostics: return "Diagnostics";
            default: return "Unknown";
        }
    }

    std::string ToDebugString(const VulkanGraphResourceDesc& resource)
    {
        std::ostringstream oss;
        oss << "graph_resource name=" << resource.name
            << " kind=" << ToString(resource.kind)
            << " bytes=" << ResourceBytes(resource)
            << " size=" << resource.width << "x" << resource.height
            << " transient=" << (resource.transient ? "true" : "false")
            << " imported=" << (resource.imported ? "true" : "false")
            << " initial=" << ToString(resource.initialLayout)
            << " final=" << ToString(resource.finalLayout)
            << " valid=" << (resource.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanGraphPassDesc& pass)
    {
        std::ostringstream oss;
        oss << "graph_pass name=" << pass.name
            << " kind=" << ToString(pass.kind)
            << " queue=" << ToString(pass.queue)
            << " uses=" << pass.uses.size()
            << " draws=" << pass.estimatedDraws
            << " dispatches=" << pass.estimatedDispatches
            << " secondary=" << (pass.allowSecondaryCommandBuffers ? "true" : "false")
            << " async=" << (pass.asyncCandidate ? "true" : "false")
            << " valid=" << (pass.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanCompiledBarrier& barrier)
    {
        std::ostringstream oss;
        oss << "barrier resource=" << barrier.resource
            << " pass=" << barrier.beforePass << "->" << barrier.afterPass
            << " stage=" << ToString(barrier.srcStage) << "->" << ToString(barrier.dstStage)
            << " access=" << ToString(barrier.srcAccess) << "->" << ToString(barrier.dstAccess)
            << " layout=" << ToString(barrier.oldLayout) << "->" << ToString(barrier.newLayout)
            << " queue=" << ToString(barrier.srcQueue) << "->" << ToString(barrier.dstQueue)
            << " ownership=" << (barrier.ownershipTransfer ? "true" : "false")
            << " image=" << (barrier.imageBarrier ? "true" : "false")
            << " buffer=" << (barrier.bufferBarrier ? "true" : "false")
            << " valid=" << (barrier.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanCompiledResourceLifetime& lifetime)
    {
        std::ostringstream oss;
        oss << "lifetime name=" << lifetime.name
            << " pass=" << lifetime.firstPass << ".." << lifetime.lastPass
            << " alias_group=" << lifetime.aliasGroup
            << " bytes=" << lifetime.byteSize
            << " transient=" << (lifetime.transient ? "true" : "false")
            << " imported=" << (lifetime.imported ? "true" : "false")
            << " aliased=" << (lifetime.aliased ? "true" : "false")
            << " valid=" << (lifetime.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanTransientAliasGroup& group)
    {
        std::ostringstream oss;
        oss << "alias_group index=" << group.groupIndex
            << " resources=" << group.resourceIndices.size()
            << " max=" << group.maxBytes
            << " unaliased=" << group.totalUnaliasedBytes
            << " saved=" << (group.totalUnaliasedBytes > group.maxBytes ? group.totalUnaliasedBytes - group.maxBytes : 0)
            << " pass=" << group.firstPass << ".." << group.lastPass
            << " image=" << (group.hasImage ? "true" : "false")
            << " buffer=" << (group.hasBuffer ? "true" : "false")
            << " valid=" << (group.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanFrameGraphQueueBatch& batch)
    {
        std::ostringstream oss;
        oss << "queue_batch queue=" << ToString(batch.queue)
            << " first_pass=" << batch.firstPass
            << " passes=" << batch.passCount
            << " cmdbufs=" << batch.commandBuffers
            << " wait=" << batch.waitSemaphores << "/" << batch.timelineWait
            << " signal=" << batch.signalSemaphores << "/" << batch.timelineSignal
            << " async_compute=" << (batch.asyncCompute ? "true" : "false")
            << " transfer=" << (batch.transfer ? "true" : "false")
            << " present=" << (batch.present ? "true" : "false")
            << " valid=" << (batch.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanFrameGraphValidationReport& validation)
    {
        std::ostringstream oss;
        oss << "framegraph_validation missing=" << validation.missingResources
            << " read_before_write=" << validation.readBeforeWrite
            << " queue_hazards=" << validation.queueHazards
            << " invalid_layouts=" << validation.invalidLayouts
            << " aliasing_hazards=" << validation.aliasingHazards
            << " redundant=" << validation.redundantBarriers
            << " warnings=" << validation.warnings
            << " valid=" << (validation.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanCompiledFrameGraph& graph)
    {
        std::ostringstream oss;
        oss << graph.summary
            << " graphics=" << graph.graphicsPasses
            << " transfer=" << graph.transferPasses
            << " compute=" << graph.computePasses
            << " present=" << graph.presentPasses
            << " secondary=" << graph.secondaryCommandBuffers
            << " transient=" << graph.transientBytesAliased << "/" << graph.transientBytesUnaliased
            << " sync2=" << (graph.usesSynchronization2 ? "true" : "false")
            << " dynamic_rendering=" << (graph.usesDynamicRendering ? "true" : "false")
            << " timeline=" << (graph.usesTimelineSemaphores ? "true" : "false")
            << " record_ready=" << (graph.readyForVkCommandRecording ? "true" : "false")
            << " execute_ready=" << (graph.readyForRenderGraphExecution ? "true" : "false")
            << " valid=" << (graph.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanFrameGraphCompilerProbe& probe)
    {
        std::ostringstream oss;
        oss << probe.summary
            << " arch=" << (probe.architecture.ok ? "ok" : "fail")
            << " draw=" << (probe.draw.ok ? "ok" : "fail")
            << " graph=" << (probe.graph.valid ? "ok" : "fail")
            << " compiled=" << (probe.compiled.valid ? "ok" : "fail");
        return oss.str();
    }
}
