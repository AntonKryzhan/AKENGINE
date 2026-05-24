#include <AK/Render/VulkanFrameGraphExecution.hpp>

#include <algorithm>
#include <sstream>
#include <string_view>

namespace AK
{
    namespace
    {
        constexpr u32 InvalidIndex = 0xFFFFFFFFu;

        bool Contains(std::string_view text, std::string_view needle)
        {
            return text.find(needle) != std::string_view::npos;
        }

        bool IsGraphicsDrawPass(const VulkanCompiledPass& pass)
        {
            return pass.queue == VulkanGraphQueueClass::Graphics && pass.estimatedDraws > 0;
        }

        bool IsForwardGeometryPass(const VulkanCompiledPass& pass)
        {
            return Contains(pass.name, "forward") || Contains(pass.name, "primitive") || Contains(pass.name, "depth");
        }

        bool IsTonemapPass(const VulkanCompiledPass& pass)
        {
            return Contains(pass.name, "tonemap") || Contains(pass.name, "post");
        }

        VulkanExecutionCommand MakeCommand(VulkanExecutionCommandKind kind,
                                           VulkanGraphQueueClass queue,
                                           std::string label,
                                           u32 passIndex,
                                           u32 batchIndex,
                                           u32 commandBufferIndex)
        {
            VulkanExecutionCommand command{};
            command.kind = kind;
            command.queue = queue;
            command.label = std::move(label);
            command.passIndex = passIndex;
            command.batchIndex = batchIndex;
            command.commandBufferIndex = commandBufferIndex;
            command.valid = !command.label.empty();
            return command;
        }

        u32 FindBatchIndexForPass(const std::vector<VulkanFrameGraphQueueBatch>& batches, u32 passIndex)
        {
            for (u32 i = 0; i < static_cast<u32>(batches.size()); ++i)
            {
                const VulkanFrameGraphQueueBatch& batch = batches[i];
                if (passIndex >= batch.firstPass && passIndex < batch.firstPass + batch.passCount)
                {
                    return i;
                }
            }
            return InvalidIndex;
        }

        bool BarrierBelongsToPass(const VulkanCompiledBarrier& barrier, u32 passIndex)
        {
            return barrier.valid && barrier.afterPass == passIndex;
        }


        VulkanExecutionFrameResourcePlan BuildFrameResourcePlan(u32 frameIndex,
                                                                const VulkanCompiledFrameGraph& compiled,
                                                                const VulkanDrawSubmissionPlan& submission,
                                                                const VulkanExecutionConfig& config)
        {
            VulkanExecutionFrameResourcePlan frame{};
            frame.frameIndex = frameIndex;
            frame.primaryCommandBuffers = std::max(1u, static_cast<u32>(compiled.queueBatches.size()));
            frame.secondaryCommandBuffers = config.useSecondaryCommandBuffers ? compiled.secondaryCommandBuffers : 0;
            frame.commandPoolResets = config.resetCommandPoolsPerFrame ? frame.primaryCommandBuffers + frame.secondaryCommandBuffers : 0;
            frame.descriptorArenaResets = 1;
            frame.transientHeapAliases = static_cast<u32>(compiled.aliasGroups.size());
            frame.transientHeapBytes = compiled.transientBytesAliased;
            frame.uploadScratchBytes = std::max<u64>(submission.upload.stagingBytes, submission.upload.totalUploadBytes);
            frame.timelineBaseValue = static_cast<u64>(frameIndex) * 1024u;
            frame.deferredReleaseQueue = true;
            frame.valid = frame.primaryCommandBuffers > 0 && frame.transientHeapBytes <= compiled.transientBytesUnaliased;
            return frame;
        }

        void AppendBarrierCommands(const VulkanCompiledFrameGraph& compiled,
                                   const VulkanCompiledPass& compiledPass,
                                   VulkanExecutionPassPlan& passPlan,
                                   std::vector<VulkanExecutionCommand>& commands,
                                   u32 batchIndex,
                                   u32 commandBufferIndex)
        {
            for (u32 barrierIndex = 0; barrierIndex < static_cast<u32>(compiled.barriers.size()); ++barrierIndex)
            {
                const VulkanCompiledBarrier& barrier = compiled.barriers[barrierIndex];
                if (!BarrierBelongsToPass(barrier, compiledPass.passIndex))
                {
                    continue;
                }

                VulkanExecutionCommand command = MakeCommand(VulkanExecutionCommandKind::PipelineBarrier2,
                                                             compiledPass.queue,
                                                             "vkCmdPipelineBarrier2: " + barrier.resource,
                                                             compiledPass.passIndex,
                                                             batchIndex,
                                                             commandBufferIndex);
                command.resource = barrier.resource;
                command.barrierIndex = barrierIndex;
                command.valid = barrier.valid;
                commands.push_back(command);
                ++passPlan.barrierCommands;
            }
        }

        void AppendUploadCommands(const VulkanDrawSubmissionPlan& submission,
                                  const VulkanCompiledPass& compiledPass,
                                  VulkanExecutionPassPlan& passPlan,
                                  std::vector<VulkanExecutionCommand>& commands,
                                  u32 batchIndex,
                                  u32 commandBufferIndex)
        {
            for (u32 copyIndex = 0; copyIndex < static_cast<u32>(submission.upload.copies.size()); ++copyIndex)
            {
                const VulkanUploadCopyPlan& copy = submission.upload.copies[copyIndex];
                VulkanExecutionCommand command = MakeCommand(VulkanExecutionCommandKind::CopyBuffer,
                                                             compiledPass.queue,
                                                             "vkCmdCopyBuffer: " + copy.name,
                                                             compiledPass.passIndex,
                                                             batchIndex,
                                                             commandBufferIndex);
                command.copyIndex = copyIndex;
                command.bytes = copy.bytes;
                command.valid = copy.valid && copy.bytes > 0;
                commands.push_back(command);
                ++passPlan.copyCommands;
            }
        }

        void AppendGeometryDrawCommands(const VulkanDrawSubmissionPlan& submission,
                                        const VulkanCompiledPass& compiledPass,
                                        VulkanExecutionPassPlan& passPlan,
                                        std::vector<VulkanExecutionCommand>& commands,
                                        u32 batchIndex,
                                        u32 commandBufferIndex)
        {
            VulkanExecutionCommand begin = MakeCommand(VulkanExecutionCommandKind::BeginDynamicRendering,
                                                       compiledPass.queue,
                                                       "vkCmdBeginRendering: " + compiledPass.name,
                                                       compiledPass.passIndex,
                                                       batchIndex,
                                                       commandBufferIndex);
            begin.dynamicRendering = true;
            commands.push_back(begin);
            passPlan.opensDynamicRendering = true;

            commands.push_back(MakeCommand(VulkanExecutionCommandKind::BindGraphicsPipeline, compiledPass.queue, "vkCmdBindPipeline: primitive_mesh", compiledPass.passIndex, batchIndex, commandBufferIndex));
            commands.push_back(MakeCommand(VulkanExecutionCommandKind::BindDescriptorSets, compiledPass.queue, "vkCmdBindDescriptorSets: frame/object/material", compiledPass.passIndex, batchIndex, commandBufferIndex));
            commands.push_back(MakeCommand(VulkanExecutionCommandKind::SetViewport, compiledPass.queue, "vkCmdSetViewport", compiledPass.passIndex, batchIndex, commandBufferIndex));
            commands.push_back(MakeCommand(VulkanExecutionCommandKind::SetScissor, compiledPass.queue, "vkCmdSetScissor", compiledPass.passIndex, batchIndex, commandBufferIndex));
            commands.push_back(MakeCommand(VulkanExecutionCommandKind::BindVertexBuffer, compiledPass.queue, "vkCmdBindVertexBuffers: packed32_combined", compiledPass.passIndex, batchIndex, commandBufferIndex));
            commands.push_back(MakeCommand(VulkanExecutionCommandKind::BindIndexBuffer, compiledPass.queue, "vkCmdBindIndexBuffer: u32_combined", compiledPass.passIndex, batchIndex, commandBufferIndex));

            const bool useActualDraws = IsForwardGeometryPass(compiledPass) && !submission.upload.draws.empty();
            const u32 drawCount = useActualDraws ? static_cast<u32>(submission.upload.draws.size()) : std::max(1u, compiledPass.estimatedDraws);
            for (u32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
            {
                VulkanExecutionCommand draw = MakeCommand(VulkanExecutionCommandKind::DrawIndexed,
                                                          compiledPass.queue,
                                                          useActualDraws ? "vkCmdDrawIndexed: " + submission.upload.draws[drawIndex].name : "vkCmdDrawIndexed: fullscreen_or_synthetic",
                                                          compiledPass.passIndex,
                                                          batchIndex,
                                                          commandBufferIndex);
                draw.drawIndex = drawIndex;
                if (useActualDraws)
                {
                    const VulkanDrawIndexedRecord& record = submission.upload.draws[drawIndex];
                    draw.firstIndex = record.firstIndex;
                    draw.indexCount = record.indexCount;
                    draw.vertexOffset = record.vertexOffset;
                    draw.instanceCount = record.instanceCount;
                    draw.valid = record.valid && record.indexCount > 0;
                }
                else
                {
                    draw.firstIndex = 0;
                    draw.indexCount = 3;
                    draw.vertexOffset = 0;
                    draw.instanceCount = 1;
                    draw.valid = true;
                }
                commands.push_back(draw);
                ++passPlan.drawCommands;
            }

            VulkanExecutionCommand end = MakeCommand(VulkanExecutionCommandKind::EndDynamicRendering,
                                                     compiledPass.queue,
                                                     "vkCmdEndRendering: " + compiledPass.name,
                                                     compiledPass.passIndex,
                                                     batchIndex,
                                                     commandBufferIndex);
            end.dynamicRendering = true;
            commands.push_back(end);
            passPlan.closesDynamicRendering = true;
        }

        void AppendComputeCommands(const VulkanCompiledPass& compiledPass,
                                   VulkanExecutionPassPlan& passPlan,
                                   std::vector<VulkanExecutionCommand>& commands,
                                   u32 batchIndex,
                                   u32 commandBufferIndex)
        {
            const u32 groups = std::max(1u, compiledPass.estimatedDispatches);
            VulkanExecutionCommand dispatch = MakeCommand(VulkanExecutionCommandKind::Dispatch,
                                                          compiledPass.queue,
                                                          "vkCmdDispatch: " + compiledPass.name,
                                                          compiledPass.passIndex,
                                                          batchIndex,
                                                          commandBufferIndex);
            dispatch.dispatchGroupsX = groups;
            dispatch.dispatchGroupsY = 1;
            dispatch.dispatchGroupsZ = 1;
            commands.push_back(dispatch);
            ++passPlan.dispatchCommands;
        }

        void AppendPresentCommands(const VulkanCompiledPass& compiledPass,
                                   VulkanExecutionPassPlan& passPlan,
                                   std::vector<VulkanExecutionCommand>& commands,
                                   u32 batchIndex,
                                   u32 commandBufferIndex)
        {
            VulkanExecutionCommand present = MakeCommand(VulkanExecutionCommandKind::Present,
                                                         compiledPass.queue,
                                                         "vkQueuePresentKHR: swapchain_color",
                                                         compiledPass.passIndex,
                                                         batchIndex,
                                                         commandBufferIndex);
            present.resource = "swapchain_color";
            commands.push_back(present);
            passPlan.present = true;
        }

        VulkanExecutionStats AnalyzeExecutionPlan(const VulkanFrameGraphExecutionPlan& plan)
        {
            VulkanExecutionStats stats{};
            stats.framesInFlight = static_cast<u32>(plan.frameResources.size());
            stats.passes = static_cast<u32>(plan.passes.size());
            stats.commands = static_cast<u32>(plan.commands.size());
            stats.submitBatches = static_cast<u32>(plan.submits.size());
            stats.transientHeapBytes = plan.compiled.transientBytesAliased;
            stats.savedTransientBytes = plan.compiled.transientBytesSaved;
            stats.usesSynchronization2 = plan.config.useSynchronization2;
            stats.usesDynamicRendering = plan.config.useDynamicRendering;
            stats.usesTimelineSemaphores = plan.config.useTimelineSemaphores;

            for (const VulkanExecutionFrameResourcePlan& frame : plan.frameResources)
            {
                stats.primaryCommandBuffers = std::max(stats.primaryCommandBuffers, frame.primaryCommandBuffers);
                stats.secondaryCommandBuffers = std::max(stats.secondaryCommandBuffers, frame.secondaryCommandBuffers);
            }
            stats.commandBuffers = stats.primaryCommandBuffers + stats.secondaryCommandBuffers;

            for (const VulkanExecutionCommand& command : plan.commands)
            {
                switch (command.kind)
                {
                    case VulkanExecutionCommandKind::PipelineBarrier2:
                        ++stats.barrierCommands;
                        break;
                    case VulkanExecutionCommandKind::CopyBuffer:
                        ++stats.copyCommands;
                        stats.copiedBytes += command.bytes;
                        break;
                    case VulkanExecutionCommandKind::DrawIndexed:
                        ++stats.drawCommands;
                        break;
                    case VulkanExecutionCommandKind::Dispatch:
                        ++stats.dispatchCommands;
                        break;
                    case VulkanExecutionCommandKind::BeginDynamicRendering:
                        ++stats.dynamicRenderingScopes;
                        break;
                    case VulkanExecutionCommandKind::Present:
                        ++stats.presentCommands;
                        break;
                    default:
                        break;
                }
            }

            for (const VulkanExecutionSubmitPlan& submit : plan.submits)
            {
                stats.usesAsyncCompute = stats.usesAsyncCompute || submit.asyncCompute;
                stats.usesDedicatedTransfer = stats.usesDedicatedTransfer || submit.transfer;
            }

            stats.valid = stats.framesInFlight > 0 && stats.passes > 0 && stats.commands > 0 && stats.submitBatches > 0;
            return stats;
        }

        VulkanExecutionValidationReport ValidateExecutionPlan(const VulkanFrameGraphExecutionPlan& plan)
        {
            VulkanExecutionValidationReport validation{};
            validation.missingCompiledGraph = plan.compiled.readyForRenderGraphExecution ? 0 : 1;
            validation.missingDrawSubmission = plan.submission.readyForGpuSubmission ? 0 : 1;
            validation.missingFrameResources = plan.frameResources.empty() ? 1 : 0;
            validation.missingSubmitBatches = plan.submits.empty() ? 1 : 0;
            validation.missingCommandBuffers = plan.stats.commandBuffers == 0 ? 1 : 0;

            for (const VulkanExecutionPassPlan& pass : plan.passes)
            {
                if (pass.queue == VulkanGraphQueueClass::Graphics && pass.drawCommands > 0 && (!pass.opensDynamicRendering || !pass.closesDynamicRendering))
                {
                    ++validation.missingDynamicRenderingScopes;
                }
            }

            const u32 expectedGraphicsDraws = plan.compiled.passes.empty() ? 0 : [&]() {
                u32 count = 0;
                for (const VulkanCompiledPass& pass : plan.compiled.passes)
                {
                    if (IsGraphicsDrawPass(pass))
                    {
                        count += std::max(1u, pass.estimatedDraws);
                    }
                }
                return count;
            }();
            if (expectedGraphicsDraws > 0 && plan.stats.drawCommands == 0)
            {
                ++validation.missingDrawCommands;
            }

            if (plan.config.forbidGpuReadbackInFrame)
            {
                for (const VulkanGpuBufferPlan& buffer : plan.submission.upload.buffers)
                {
                    if (buffer.memory == VulkanMemoryPlanDomain::GpuToCpuReadback)
                    {
                        ++validation.unexpectedReadbacks;
                    }
                }
            }

            if (plan.config.forbidRuntimeAllocationInFrame)
            {
                validation.runtimeAllocationRisks += plan.compiled.readyForRenderGraphExecution && !plan.frameResources.empty() ? 0 : 1;
            }

            validation.commandBudgetExceeded = plan.stats.commands > plan.config.maxCommandsPerFrame ? 1 : 0;
            validation.warnings = validation.missingCompiledGraph + validation.missingDrawSubmission + validation.missingFrameResources +
                validation.missingSubmitBatches + validation.missingCommandBuffers + validation.missingDynamicRenderingScopes +
                validation.missingDrawCommands + validation.unexpectedReadbacks + validation.runtimeAllocationRisks + validation.commandBudgetExceeded;
            validation.valid = validation.warnings == 0;
            return validation;
        }
    }

    VulkanExecutionConfig MakeDefaultVulkanExecutionConfig()
    {
        VulkanExecutionConfig config{};
        config.hazardPolicy = VulkanExecutionHazardPolicy::Strict;
        config.memoryPolicy = VulkanExecutionMemoryPolicy::TransientAliasHeaps;
        config.useSynchronization2 = true;
        config.useDynamicRendering = true;
        config.useTimelineSemaphores = true;
        config.useSecondaryCommandBuffers = true;
        config.resetCommandPoolsPerFrame = true;
        config.allowAsyncCompute = true;
        config.allowDedicatedTransfer = true;
        config.forbidGpuReadbackInFrame = true;
        config.forbidRuntimeAllocationInFrame = true;
        config.maxCommandsPerFrame = 16384;
        return config;
    }

    VulkanFrameGraphExecutionPlan BuildVulkanFrameGraphExecutionPlan(const VulkanCompiledFrameGraph& compiled,
                                                                    const VulkanDrawSubmissionPlan& submission,
                                                                    const VulkanRuntimeArchitectureHardeningPlan& architecture,
                                                                    const VulkanExecutionConfig& requestedConfig)
    {
        VulkanFrameGraphExecutionPlan plan{};
        plan.compiled = compiled;
        plan.submission = submission;
        plan.config = requestedConfig.maxCommandsPerFrame == 0 ? MakeDefaultVulkanExecutionConfig() : requestedConfig;
        plan.config.useSynchronization2 = plan.config.useSynchronization2 && compiled.usesSynchronization2;
        plan.config.useDynamicRendering = plan.config.useDynamicRendering && compiled.usesDynamicRendering;
        plan.config.useTimelineSemaphores = plan.config.useTimelineSemaphores && compiled.usesTimelineSemaphores;

        const u32 framesInFlight = std::max(2u, std::max(compiled.source.framesInFlight, architecture.limits.maxFramesInFlight));
        plan.frameResources.reserve(framesInFlight);
        for (u32 frameIndex = 0; frameIndex < framesInFlight; ++frameIndex)
        {
            plan.frameResources.push_back(BuildFrameResourcePlan(frameIndex, compiled, submission, plan.config));
        }

        plan.commands.reserve(compiled.barriers.size() + submission.upload.copies.size() + submission.upload.draws.size() * 2u + 64u);
        VulkanExecutionCommand beginFrame = MakeCommand(VulkanExecutionCommandKind::BeginFrame, VulkanGraphQueueClass::Graphics, "begin_frame", InvalidIndex, InvalidIndex, InvalidIndex);
        beginFrame.timelineValue = 1;
        plan.commands.push_back(beginFrame);

        for (const VulkanExecutionFrameResourcePlan& frame : plan.frameResources)
        {
            if (frame.frameIndex == 0 && frame.commandPoolResets > 0)
            {
                VulkanExecutionCommand reset = MakeCommand(VulkanExecutionCommandKind::ResetCommandPool, VulkanGraphQueueClass::Graphics, "vkResetCommandPool: frame_0", InvalidIndex, InvalidIndex, 0);
                reset.valid = true;
                plan.commands.push_back(reset);
            }
        }

        for (const VulkanCompiledPass& compiledPass : compiled.passes)
        {
            const u32 batchIndex = FindBatchIndexForPass(compiled.queueBatches, compiledPass.passIndex);
            const u32 commandBufferIndex = batchIndex == InvalidIndex ? compiledPass.passIndex : batchIndex;

            VulkanExecutionPassPlan passPlan{};
            passPlan.name = compiledPass.name;
            passPlan.kind = compiledPass.kind;
            passPlan.queue = compiledPass.queue;
            passPlan.passIndex = compiledPass.passIndex;
            passPlan.primaryCommandBuffer = commandBufferIndex;
            passPlan.firstCommand = static_cast<u32>(plan.commands.size());
            passPlan.secondaryCommandBuffers = plan.config.useSecondaryCommandBuffers ? compiledPass.secondaryCommandBuffers : 0;
            passPlan.present = compiledPass.present;

            VulkanExecutionCommand beginCmd = MakeCommand(VulkanExecutionCommandKind::BeginPrimaryCommandBuffer,
                                                          compiledPass.queue,
                                                          "vkBeginCommandBuffer: " + compiledPass.name,
                                                          compiledPass.passIndex,
                                                          batchIndex,
                                                          commandBufferIndex);
            plan.commands.push_back(beginCmd);

            if (compiledPass.kind == VulkanGraphPassKind::Acquire)
            {
                VulkanExecutionCommand acquire = MakeCommand(VulkanExecutionCommandKind::AcquireSwapchainImage,
                                                             compiledPass.queue,
                                                             "vkAcquireNextImageKHR: swapchain_color",
                                                             compiledPass.passIndex,
                                                             batchIndex,
                                                             commandBufferIndex);
                acquire.resource = "swapchain_color";
                plan.commands.push_back(acquire);
            }

            AppendBarrierCommands(compiled, compiledPass, passPlan, plan.commands, batchIndex, commandBufferIndex);

            if (compiledPass.kind == VulkanGraphPassKind::Upload)
            {
                AppendUploadCommands(submission, compiledPass, passPlan, plan.commands, batchIndex, commandBufferIndex);
            }
            else if (IsGraphicsDrawPass(compiledPass))
            {
                AppendGeometryDrawCommands(submission, compiledPass, passPlan, plan.commands, batchIndex, commandBufferIndex);
            }
            else if (compiledPass.kind == VulkanGraphPassKind::Graphics && IsTonemapPass(compiledPass))
            {
                AppendGeometryDrawCommands(submission, compiledPass, passPlan, plan.commands, batchIndex, commandBufferIndex);
            }
            else if (compiledPass.kind == VulkanGraphPassKind::Compute)
            {
                AppendComputeCommands(compiledPass, passPlan, plan.commands, batchIndex, commandBufferIndex);
            }
            else if (compiledPass.kind == VulkanGraphPassKind::Present)
            {
                AppendPresentCommands(compiledPass, passPlan, plan.commands, batchIndex, commandBufferIndex);
            }

            if (passPlan.secondaryCommandBuffers > 0)
            {
                VulkanExecutionCommand executeSecondary = MakeCommand(VulkanExecutionCommandKind::ExecuteSecondaryCommandBuffers,
                                                                      compiledPass.queue,
                                                                      "vkCmdExecuteCommands: " + compiledPass.name,
                                                                      compiledPass.passIndex,
                                                                      batchIndex,
                                                                      commandBufferIndex);
                executeSecondary.secondary = true;
                executeSecondary.secondaryCommandBufferCount = passPlan.secondaryCommandBuffers;
                plan.commands.push_back(executeSecondary);
            }

            plan.commands.push_back(MakeCommand(VulkanExecutionCommandKind::EndCommandBuffer,
                                                compiledPass.queue,
                                                "vkEndCommandBuffer: " + compiledPass.name,
                                                compiledPass.passIndex,
                                                batchIndex,
                                                commandBufferIndex));

            passPlan.commandCount = static_cast<u32>(plan.commands.size()) - passPlan.firstCommand;
            passPlan.valid = !passPlan.name.empty() && passPlan.commandCount > 0;
            plan.passes.push_back(passPlan);
        }

        plan.submits.reserve(compiled.queueBatches.size());
        for (u32 batchIndex = 0; batchIndex < static_cast<u32>(compiled.queueBatches.size()); ++batchIndex)
        {
            const VulkanFrameGraphQueueBatch& batch = compiled.queueBatches[batchIndex];
            VulkanExecutionSubmitPlan submit{};
            submit.queue = batch.queue;
            submit.batchIndex = batchIndex;
            submit.firstPass = batch.firstPass;
            submit.passCount = batch.passCount;
            submit.firstCommandBuffer = batchIndex;
            submit.commandBufferCount = std::max(1u, batch.commandBuffers);
            submit.waitSemaphoreCount = batch.waitSemaphores;
            submit.signalSemaphoreCount = batch.signalSemaphores;
            submit.timelineWait = batch.timelineWait;
            submit.timelineSignal = batch.timelineSignal;
            submit.asyncCompute = batch.asyncCompute && plan.config.allowAsyncCompute;
            submit.transfer = batch.transfer && plan.config.allowDedicatedTransfer;
            submit.present = batch.present;
            submit.valid = batch.valid && submit.commandBufferCount > 0;
            plan.submits.push_back(submit);

            VulkanExecutionCommand submitCommand = MakeCommand(VulkanExecutionCommandKind::QueueSubmit2,
                                                               batch.queue,
                                                               "vkQueueSubmit2: batch_" + std::to_string(batchIndex),
                                                               InvalidIndex,
                                                               batchIndex,
                                                               submit.firstCommandBuffer);
            submitCommand.timelineValue = submit.timelineSignal;
            submitCommand.secondaryCommandBufferCount = submit.commandBufferCount;
            plan.commands.push_back(submitCommand);
        }

        plan.commands.push_back(MakeCommand(VulkanExecutionCommandKind::EndFrame, VulkanGraphQueueClass::Graphics, "end_frame", InvalidIndex, InvalidIndex, InvalidIndex));

        plan.stats = AnalyzeExecutionPlan(plan);
        plan.validation = ValidateExecutionPlan(plan);
        plan.readyForVkCmdRecording = compiled.readyForVkCommandRecording && submission.readyForCommandRecording && plan.validation.missingCommandBuffers == 0;
        plan.readyForQueueSubmit2 = plan.readyForVkCmdRecording && compiled.usesSynchronization2 && compiled.usesTimelineSemaphores && !plan.submits.empty();
        plan.readyForPersistentRenderer = plan.readyForQueueSubmit2 && plan.validation.valid && architecture.avoidsRuntimeVkAllocateMemory && architecture.readyForRealVkHandles;
        plan.valid = plan.readyForPersistentRenderer;

        std::ostringstream oss;
        oss << "vulkan_framegraph_execution passes=" << plan.passes.size()
            << " commands=" << plan.commands.size()
            << " cmdbufs=" << plan.stats.commandBuffers
            << " barriers=" << plan.stats.barrierCommands
            << " copies=" << plan.stats.copyCommands
            << " draws=" << plan.stats.drawCommands
            << " dispatches=" << plan.stats.dispatchCommands
            << " submits=" << plan.submits.size()
            << " transient=" << plan.stats.transientHeapBytes
            << " saved=" << plan.stats.savedTransientBytes
            << " ready=" << (plan.readyForPersistentRenderer ? "true" : "false")
            << " warnings=" << plan.validation.warnings;
        plan.summary = oss.str();
        return plan;
    }

    VulkanFrameGraphExecutionProbe BuildVulkanFrameGraphExecutionProbe()
    {
        VulkanFrameGraphExecutionProbe probe{};
        probe.compiler = BuildVulkanFrameGraphCompilerProbe();
        probe.execution = BuildVulkanFrameGraphExecutionPlan(probe.compiler.compiled, probe.compiler.draw.submission, probe.compiler.architecture.plan, MakeDefaultVulkanExecutionConfig());
        probe.ok = probe.compiler.ok && probe.execution.valid;

        std::ostringstream oss;
        oss << "vulkan framegraph execution probe ok=" << (probe.ok ? "true" : "false")
            << " passes=" << probe.execution.passes.size()
            << " commands=" << probe.execution.commands.size()
            << " submits=" << probe.execution.submits.size()
            << " warnings=" << probe.execution.validation.warnings;
        probe.summary = oss.str();
        return probe;
    }

    const char* ToString(VulkanExecutionCommandKind kind)
    {
        switch (kind)
        {
            case VulkanExecutionCommandKind::BeginFrame: return "BeginFrame";
            case VulkanExecutionCommandKind::AcquireSwapchainImage: return "AcquireSwapchainImage";
            case VulkanExecutionCommandKind::ResetCommandPool: return "ResetCommandPool";
            case VulkanExecutionCommandKind::BeginPrimaryCommandBuffer: return "BeginPrimaryCommandBuffer";
            case VulkanExecutionCommandKind::BeginSecondaryCommandBuffer: return "BeginSecondaryCommandBuffer";
            case VulkanExecutionCommandKind::PipelineBarrier2: return "PipelineBarrier2";
            case VulkanExecutionCommandKind::CopyBuffer: return "CopyBuffer";
            case VulkanExecutionCommandKind::BeginDynamicRendering: return "BeginDynamicRendering";
            case VulkanExecutionCommandKind::BindGraphicsPipeline: return "BindGraphicsPipeline";
            case VulkanExecutionCommandKind::BindDescriptorSets: return "BindDescriptorSets";
            case VulkanExecutionCommandKind::BindVertexBuffer: return "BindVertexBuffer";
            case VulkanExecutionCommandKind::BindIndexBuffer: return "BindIndexBuffer";
            case VulkanExecutionCommandKind::SetViewport: return "SetViewport";
            case VulkanExecutionCommandKind::SetScissor: return "SetScissor";
            case VulkanExecutionCommandKind::PushConstants: return "PushConstants";
            case VulkanExecutionCommandKind::DrawIndexed: return "DrawIndexed";
            case VulkanExecutionCommandKind::Dispatch: return "Dispatch";
            case VulkanExecutionCommandKind::ExecuteSecondaryCommandBuffers: return "ExecuteSecondaryCommandBuffers";
            case VulkanExecutionCommandKind::EndDynamicRendering: return "EndDynamicRendering";
            case VulkanExecutionCommandKind::EndCommandBuffer: return "EndCommandBuffer";
            case VulkanExecutionCommandKind::QueueSubmit2: return "QueueSubmit2";
            case VulkanExecutionCommandKind::Present: return "Present";
            case VulkanExecutionCommandKind::EndFrame: return "EndFrame";
            case VulkanExecutionCommandKind::DebugLabel: return "DebugLabel";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanExecutionHazardPolicy policy)
    {
        switch (policy)
        {
            case VulkanExecutionHazardPolicy::Strict: return "Strict";
            case VulkanExecutionHazardPolicy::SkipRedundant: return "SkipRedundant";
            case VulkanExecutionHazardPolicy::DiagnosticOnly: return "DiagnosticOnly";
            default: return "Unknown";
        }
    }

    const char* ToString(VulkanExecutionMemoryPolicy policy)
    {
        switch (policy)
        {
            case VulkanExecutionMemoryPolicy::PreallocatedFrameResources: return "PreallocatedFrameResources";
            case VulkanExecutionMemoryPolicy::TransientAliasHeaps: return "TransientAliasHeaps";
            case VulkanExecutionMemoryPolicy::ImportedExternalResources: return "ImportedExternalResources";
            default: return "Unknown";
        }
    }

    std::string ToDebugString(const VulkanExecutionConfig& config)
    {
        std::ostringstream oss;
        oss << "execution_config hazard=" << ToString(config.hazardPolicy)
            << " memory=" << ToString(config.memoryPolicy)
            << " sync2=" << (config.useSynchronization2 ? "true" : "false")
            << " dynamic_rendering=" << (config.useDynamicRendering ? "true" : "false")
            << " timeline=" << (config.useTimelineSemaphores ? "true" : "false")
            << " secondary=" << (config.useSecondaryCommandBuffers ? "true" : "false")
            << " async_compute=" << (config.allowAsyncCompute ? "true" : "false")
            << " dedicated_transfer=" << (config.allowDedicatedTransfer ? "true" : "false")
            << " no_readback=" << (config.forbidGpuReadbackInFrame ? "true" : "false")
            << " no_runtime_alloc=" << (config.forbidRuntimeAllocationInFrame ? "true" : "false")
            << " max_commands=" << config.maxCommandsPerFrame;
        return oss.str();
    }

    std::string ToDebugString(const VulkanExecutionCommand& command)
    {
        std::ostringstream oss;
        oss << "exec_command kind=" << ToString(command.kind)
            << " queue=" << ToString(command.queue)
            << " pass=" << command.passIndex
            << " batch=" << command.batchIndex
            << " cmdbuf=" << command.commandBufferIndex
            << " label=" << command.label
            << " resource=" << command.resource
            << " barrier=" << command.barrierIndex
            << " draw=" << command.drawIndex
            << " copy=" << command.copyIndex
            << " bytes=" << command.bytes
            << " groups=" << command.dispatchGroupsX << "x" << command.dispatchGroupsY << "x" << command.dispatchGroupsZ
            << " index=" << command.firstIndex << "+" << command.indexCount
            << " instances=" << command.instanceCount
            << " timeline=" << command.timelineValue
            << " secondary=" << (command.secondary ? "true" : "false")
            << " valid=" << (command.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanExecutionPassPlan& pass)
    {
        std::ostringstream oss;
        oss << "exec_pass name=" << pass.name
            << " kind=" << ToString(pass.kind)
            << " queue=" << ToString(pass.queue)
            << " index=" << pass.passIndex
            << " cmdbuf=" << pass.primaryCommandBuffer
            << " commands=" << pass.firstCommand << "+" << pass.commandCount
            << " barriers=" << pass.barrierCommands
            << " copies=" << pass.copyCommands
            << " draws=" << pass.drawCommands
            << " dispatches=" << pass.dispatchCommands
            << " secondary=" << pass.secondaryCommandBuffers
            << " dynamic=" << (pass.opensDynamicRendering && pass.closesDynamicRendering ? "true" : "false")
            << " present=" << (pass.present ? "true" : "false")
            << " valid=" << (pass.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanExecutionSubmitPlan& submit)
    {
        std::ostringstream oss;
        oss << "exec_submit queue=" << ToString(submit.queue)
            << " batch=" << submit.batchIndex
            << " passes=" << submit.firstPass << "+" << submit.passCount
            << " cmdbufs=" << submit.firstCommandBuffer << "+" << submit.commandBufferCount
            << " wait=" << submit.waitSemaphoreCount << "/" << submit.timelineWait
            << " signal=" << submit.signalSemaphoreCount << "/" << submit.timelineSignal
            << " async_compute=" << (submit.asyncCompute ? "true" : "false")
            << " transfer=" << (submit.transfer ? "true" : "false")
            << " present=" << (submit.present ? "true" : "false")
            << " valid=" << (submit.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanExecutionFrameResourcePlan& frame)
    {
        std::ostringstream oss;
        oss << "exec_frame index=" << frame.frameIndex
            << " primary=" << frame.primaryCommandBuffers
            << " secondary=" << frame.secondaryCommandBuffers
            << " pool_resets=" << frame.commandPoolResets
            << " descriptor_resets=" << frame.descriptorArenaResets
            << " alias_groups=" << frame.transientHeapAliases
            << " transient_bytes=" << frame.transientHeapBytes
            << " upload_scratch=" << frame.uploadScratchBytes
            << " timeline_base=" << frame.timelineBaseValue
            << " deferred_release=" << (frame.deferredReleaseQueue ? "true" : "false")
            << " valid=" << (frame.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanExecutionValidationReport& validation)
    {
        std::ostringstream oss;
        oss << "exec_validation compiled=" << validation.missingCompiledGraph
            << " submission=" << validation.missingDrawSubmission
            << " frames=" << validation.missingFrameResources
            << " submits=" << validation.missingSubmitBatches
            << " cmdbufs=" << validation.missingCommandBuffers
            << " dynamic_scopes=" << validation.missingDynamicRenderingScopes
            << " draws=" << validation.missingDrawCommands
            << " readbacks=" << validation.unexpectedReadbacks
            << " alloc_risks=" << validation.runtimeAllocationRisks
            << " budget=" << validation.commandBudgetExceeded
            << " warnings=" << validation.warnings
            << " valid=" << (validation.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanExecutionStats& stats)
    {
        std::ostringstream oss;
        oss << "exec_stats frames=" << stats.framesInFlight
            << " passes=" << stats.passes
            << " commands=" << stats.commands
            << " cmdbufs=" << stats.commandBuffers
            << " primary=" << stats.primaryCommandBuffers
            << " secondary=" << stats.secondaryCommandBuffers
            << " barriers=" << stats.barrierCommands
            << " copies=" << stats.copyCommands
            << " copy_bytes=" << stats.copiedBytes
            << " draws=" << stats.drawCommands
            << " dispatches=" << stats.dispatchCommands
            << " dynamic_scopes=" << stats.dynamicRenderingScopes
            << " submits=" << stats.submitBatches
            << " presents=" << stats.presentCommands
            << " transient=" << stats.transientHeapBytes
            << " saved=" << stats.savedTransientBytes
            << " sync2=" << (stats.usesSynchronization2 ? "true" : "false")
            << " dynamic_rendering=" << (stats.usesDynamicRendering ? "true" : "false")
            << " timeline=" << (stats.usesTimelineSemaphores ? "true" : "false")
            << " async_compute=" << (stats.usesAsyncCompute ? "true" : "false")
            << " transfer=" << (stats.usesDedicatedTransfer ? "true" : "false")
            << " valid=" << (stats.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanFrameGraphExecutionPlan& plan)
    {
        std::ostringstream oss;
        oss << plan.summary
            << " cmd_ready=" << (plan.readyForVkCmdRecording ? "true" : "false")
            << " submit_ready=" << (plan.readyForQueueSubmit2 ? "true" : "false")
            << " renderer_ready=" << (plan.readyForPersistentRenderer ? "true" : "false")
            << " valid=" << (plan.valid ? "true" : "false");
        return oss.str();
    }

    std::string ToDebugString(const VulkanFrameGraphExecutionProbe& probe)
    {
        std::ostringstream oss;
        oss << probe.summary
            << " compiler=" << (probe.compiler.ok ? "ok" : "fail")
            << " execution=" << (probe.execution.valid ? "ok" : "fail");
        return oss.str();
    }
}
