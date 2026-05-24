#include <AK/Render/VulkanDrawSubmission.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace AK
{
    namespace
    {
        constexpr u64 FnvOffset = 1469598103934665603ull;
        constexpr u64 FnvPrime = 1099511628211ull;

        u64 HashBytes(const char* data, u64 size)
        {
            u64 h = FnvOffset;
            for (u64 i = 0; i < size; ++i)
            {
                h ^= static_cast<unsigned char>(data[i]);
                h *= FnvPrime;
            }
            return h;
        }

        u64 HashFileIfPresent(const std::string& path, bool& present)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                present = false;
                return 0;
            }
            std::ostringstream ss;
            ss << file.rdbuf();
            const std::string data = ss.str();
            present = true;
            return HashBytes(data.data(), static_cast<u64>(data.size()));
        }

        u32 EstimateSpvBytes(const std::string& stage)
        {
            if (stage == "vertex")
            {
                return 4096;
            }
            if (stage == "fragment")
            {
                return 3584;
            }
            return 2048;
        }

        VulkanRecordedDrawCommand MakeCommand(VulkanDrawCommandKind kind, const std::string& label)
        {
            VulkanRecordedDrawCommand command{};
            command.kind = kind;
            command.label = label;
            command.valid = true;
            return command;
        }
    }

    VulkanPipelineLayoutPlan BuildVulkanPrimitivePipelineLayoutPlan(const VulkanMeshUploadPlan& upload)
    {
        VulkanPipelineLayoutPlan plan{};
        plan.descriptorSetCount = 2;
        plan.pushConstantBytes = 16;
        plan.bindlessMaterialSet = upload.descriptors.bindlessReady;
        plan.objectTableStorage = upload.descriptors.objectTableAsStorage;
        plan.materialTableStorage = upload.descriptors.materialTableAsStorage;

        VulkanDescriptorBindingPlan camera{};
        camera.set = 0;
        camera.binding = 0;
        camera.resourceKind = VulkanDescriptorResourceKind::UniformBuffer;
        camera.name = "FrameCameraUniform";
        camera.dynamicOffset = true;
        camera.valid = upload.uniforms.valid;
        plan.descriptorBindings.push_back(camera);

        VulkanDescriptorBindingPlan objects{};
        objects.set = 0;
        objects.binding = 1;
        objects.resourceKind = VulkanDescriptorResourceKind::StorageBuffer;
        objects.name = "ObjectTable";
        objects.valid = upload.descriptors.objectTableAsStorage;
        plan.descriptorBindings.push_back(objects);

        VulkanDescriptorBindingPlan materials{};
        materials.set = 0;
        materials.binding = 2;
        materials.resourceKind = VulkanDescriptorResourceKind::StorageBuffer;
        materials.name = "MaterialTable";
        materials.valid = upload.descriptors.materialTableAsStorage;
        plan.descriptorBindings.push_back(materials);

        VulkanDescriptorBindingPlan textures{};
        textures.set = 1;
        textures.binding = 0;
        textures.resourceKind = VulkanDescriptorResourceKind::SampledImage;
        textures.name = "MaterialTextures";
        textures.descriptorCount = 1024;
        textures.partiallyBound = true;
        textures.updateAfterBind = true;
        textures.valid = upload.descriptors.bindlessReady;
        plan.descriptorBindings.push_back(textures);

        VulkanDescriptorBindingPlan sampler{};
        sampler.set = 1;
        sampler.binding = 1;
        sampler.resourceKind = VulkanDescriptorResourceKind::Sampler;
        sampler.name = "LinearSampler";
        sampler.valid = true;
        plan.descriptorBindings.push_back(sampler);

        plan.valid = upload.valid && plan.descriptorSetCount == 2 && plan.pushConstantBytes <= 128;
        for (const VulkanDescriptorBindingPlan& binding : plan.descriptorBindings)
        {
            if (!binding.valid)
            {
                ++plan.warnings;
                plan.valid = false;
            }
        }
        return plan;
    }

    VulkanRenderTargetPlan BuildVulkanDefaultRenderTargetPlan(const VulkanRhiConfig& config)
    {
        VulkanRenderTargetPlan plan{};
        plan.width = config.window.width == 0 ? 1280 : config.window.width;
        plan.height = config.window.height == 0 ? 720 : config.window.height;
        plan.dynamicRendering = config.requireDynamicRendering;
        plan.reversedZ = true;
        plan.valid = plan.width > 0 && plan.height > 0 && plan.dynamicRendering;
        return plan;
    }

    std::vector<VulkanShaderCacheEntry> BuildVulkanPrimitiveShaderCacheEntries(const VulkanShaderBuildPlan& shaders)
    {
        std::vector<VulkanShaderCacheEntry> entries;
        entries.reserve(2);

        VulkanShaderCacheEntry vs{};
        vs.sourcePath = shaders.shaderSourcePath;
        vs.spvPath = shaders.vertexSpvPath;
        vs.stage = "vertex";
        vs.entryPoint = shaders.vertexEntry;
        vs.sourceHash = HashFileIfPresent(vs.sourcePath, vs.sourcePresent);
        vs.cachePathValid = !vs.spvPath.empty() && vs.spvPath.find(".spv") != std::string::npos;
        vs.estimatedSpvBytes = EstimateSpvBytes(vs.stage);
        vs.needsCompile = true;
        vs.valid = vs.cachePathValid && !vs.entryPoint.empty() && !vs.sourcePath.empty();
        entries.push_back(vs);

        VulkanShaderCacheEntry fs{};
        fs.sourcePath = shaders.shaderSourcePath;
        fs.spvPath = shaders.fragmentSpvPath;
        fs.stage = "fragment";
        fs.entryPoint = shaders.fragmentEntry;
        fs.sourceHash = HashFileIfPresent(fs.sourcePath, fs.sourcePresent);
        fs.cachePathValid = !fs.spvPath.empty() && fs.spvPath.find(".spv") != std::string::npos;
        fs.estimatedSpvBytes = EstimateSpvBytes(fs.stage);
        fs.needsCompile = true;
        fs.valid = fs.cachePathValid && !fs.entryPoint.empty() && !fs.sourcePath.empty();
        entries.push_back(fs);

        return entries;
    }

    std::vector<VulkanShaderCompilerInvocation> BuildVulkanShaderCompilerInvocations(const VulkanShaderBuildPlan& shaders)
    {
        std::vector<VulkanShaderCompilerInvocation> invocations;
        invocations.reserve(4);

        VulkanShaderCompilerInvocation vs{};
        vs.compiler = VulkanShaderCompilerKind::Slang;
        vs.executable = "slangc";
        vs.inputPath = shaders.shaderSourcePath;
        vs.outputPath = shaders.vertexSpvPath;
        vs.entryPoint = shaders.vertexEntry;
        vs.profile = "spirv_1_5 -stage vertex";
        vs.arguments = {"-target", "spirv", "-profile", "spirv_1_5", "-entry", shaders.vertexEntry, "-o", shaders.vertexSpvPath, shaders.shaderSourcePath};
        vs.valid = !vs.inputPath.empty() && !vs.outputPath.empty() && !vs.entryPoint.empty();
        invocations.push_back(vs);

        VulkanShaderCompilerInvocation fs = vs;
        fs.outputPath = shaders.fragmentSpvPath;
        fs.entryPoint = shaders.fragmentEntry;
        fs.profile = "spirv_1_5 -stage fragment";
        fs.arguments = {"-target", "spirv", "-profile", "spirv_1_5", "-entry", shaders.fragmentEntry, "-o", shaders.fragmentSpvPath, shaders.shaderSourcePath};
        fs.valid = !fs.inputPath.empty() && !fs.outputPath.empty() && !fs.entryPoint.empty();
        invocations.push_back(fs);

        VulkanShaderCompilerInvocation glslangVs{};
        glslangVs.compiler = VulkanShaderCompilerKind::GlslangValidator;
        glslangVs.executable = "glslangValidator";
        glslangVs.inputPath = shaders.shaderSourcePath;
        glslangVs.outputPath = shaders.vertexSpvPath;
        glslangVs.entryPoint = shaders.vertexEntry;
        glslangVs.profile = "fallback planned";
        glslangVs.arguments = {"-V", "-e", shaders.vertexEntry, "-o", shaders.vertexSpvPath, shaders.shaderSourcePath};
        glslangVs.valid = vs.valid;
        invocations.push_back(glslangVs);

        VulkanShaderCompilerInvocation glslangFs = glslangVs;
        glslangFs.outputPath = shaders.fragmentSpvPath;
        glslangFs.entryPoint = shaders.fragmentEntry;
        glslangFs.arguments = {"-V", "-e", shaders.fragmentEntry, "-o", shaders.fragmentSpvPath, shaders.shaderSourcePath};
        glslangFs.valid = fs.valid;
        invocations.push_back(glslangFs);

        return invocations;
    }

    std::vector<VulkanRecordedDrawCommand> RecordVulkanDrawCommandPlan(const VulkanMeshUploadPlan& upload, const VulkanGraphicsPipelinePlan& pipeline, const VulkanPipelineLayoutPlan& layout, const VulkanRenderTargetPlan& target)
    {
        std::vector<VulkanRecordedDrawCommand> commands;
        commands.reserve(16 + upload.draws.size() * 2u);

        commands.push_back(MakeCommand(VulkanDrawCommandKind::BeginFrame, "begin_frame"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::TransitionColorAttachment, "swapchain_present_to_color_attachment"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::TransitionDepthAttachment, "depth_undefined_to_depth_attachment"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::BeginDynamicRendering, target.dynamicRendering ? "begin_dynamic_rendering" : "begin_render_pass"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::BindGraphicsPipeline, pipeline.valid ? "bind_primitive_pipeline" : "bind_invalid_pipeline"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::BindDescriptorSets, layout.valid ? "bind_frame_and_material_sets" : "bind_invalid_descriptor_sets"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::BindVertexBuffer, "bind_combined_packed_vertex_buffer"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::BindIndexBuffer, "bind_combined_u32_index_buffer"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::SetViewport, "set_viewport"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::SetScissor, "set_scissor"));

        u32 ordinal = 0;
        for (const VulkanDrawIndexedRecord& draw : upload.draws)
        {
            if (!draw.valid)
            {
                continue;
            }

            VulkanRecordedDrawCommand push = MakeCommand(VulkanDrawCommandKind::PushConstants, "push_object_index_" + std::to_string(draw.firstInstance));
            push.drawIndex = ordinal;
            push.materialId = draw.materialId;
            commands.push_back(push);

            VulkanRecordedDrawCommand cmd = MakeCommand(VulkanDrawCommandKind::DrawIndexed, draw.name);
            cmd.drawIndex = ordinal;
            cmd.materialId = draw.materialId;
            cmd.firstIndex = draw.firstIndex;
            cmd.indexCount = draw.indexCount;
            cmd.vertexOffset = draw.vertexOffset;
            cmd.instanceCount = draw.instanceCount;
            commands.push_back(cmd);
            ++ordinal;
        }

        commands.push_back(MakeCommand(VulkanDrawCommandKind::EndDynamicRendering, "end_dynamic_rendering"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::TransitionPresent, "color_attachment_to_present"));
        commands.push_back(MakeCommand(VulkanDrawCommandKind::EndFrame, "submit_and_present"));
        return commands;
    }

    VulkanDrawBatchStats AnalyzeVulkanRecordedCommands(const std::vector<VulkanRecordedDrawCommand>& commands)
    {
        VulkanDrawBatchStats stats{};
        bool firstDraw = true;
        u32 lastMaterial = 0;
        bool frontToBackKnown = true;

        for (const VulkanRecordedDrawCommand& command : commands)
        {
            if (!command.valid)
            {
                continue;
            }
            ++stats.recordedCommands;
            switch (command.kind)
            {
            case VulkanDrawCommandKind::BindGraphicsPipeline:
                ++stats.pipelineBinds;
                break;
            case VulkanDrawCommandKind::BindDescriptorSets:
                ++stats.descriptorBinds;
                break;
            case VulkanDrawCommandKind::BindVertexBuffer:
                ++stats.vertexBufferBinds;
                break;
            case VulkanDrawCommandKind::BindIndexBuffer:
                ++stats.indexBufferBinds;
                break;
            case VulkanDrawCommandKind::SetViewport:
            case VulkanDrawCommandKind::SetScissor:
                ++stats.dynamicStateWrites;
                break;
            case VulkanDrawCommandKind::PushConstants:
                ++stats.pushConstantWrites;
                break;
            case VulkanDrawCommandKind::DrawIndexed:
                ++stats.drawCalls;
                if (!firstDraw && command.materialId != lastMaterial)
                {
                    ++stats.materialSwitches;
                }
                lastMaterial = command.materialId;
                firstDraw = false;
                break;
            default:
                break;
            }
        }

        stats.frontToBackSorted = frontToBackKnown;
        stats.materialCoherent = stats.materialSwitches <= stats.drawCalls;
        stats.estimatedCpuBytes = stats.recordedCommands * 64u + stats.drawCalls * 32u;
        stats.valid = stats.drawCalls > 0 && stats.pipelineBinds == 1 && stats.vertexBufferBinds == 1 && stats.indexBufferBinds == 1;
        return stats;
    }

    VulkanDrawSubmissionPlan BuildVulkanDrawSubmissionPlan(const OptimizedRenderGeometry& geometry, const VulkanRhiConfig& config)
    {
        VulkanDrawSubmissionPlan plan{};
        plan.upload = BuildVulkanMeshUploadPlan(geometry, config, VulkanDrawSubmissionMode::DirectDrawIndexed);
        plan.pipeline = BuildVulkanPrimitiveGraphicsPipelinePlan(config);
        plan.pipeline.vertexLayout = BuildPackedVulkanMeshVertexLayout32();
        plan.layout = BuildVulkanPrimitivePipelineLayoutPlan(plan.upload);
        plan.renderTarget = BuildVulkanDefaultRenderTargetPlan(config);
        plan.shaderCache = BuildVulkanPrimitiveShaderCacheEntries(plan.upload.shaders);
        plan.compilerInvocations = BuildVulkanShaderCompilerInvocations(plan.upload.shaders);
        plan.commands = RecordVulkanDrawCommandPlan(plan.upload, plan.pipeline, plan.layout, plan.renderTarget);
        plan.batchStats = AnalyzeVulkanRecordedCommands(plan.commands);

        plan.readyForShaderCompilation = !plan.compilerInvocations.empty();
        for (const VulkanShaderCompilerInvocation& invocation : plan.compilerInvocations)
        {
            plan.readyForShaderCompilation = plan.readyForShaderCompilation && invocation.valid;
        }
        for (const VulkanShaderCacheEntry& entry : plan.shaderCache)
        {
            if (!entry.valid)
            {
                ++plan.warnings;
            }
        }

        plan.readyForPipelineCreation = plan.pipeline.valid && plan.layout.valid && plan.renderTarget.valid;
        plan.readyForCommandRecording = plan.upload.readyForDrawIndexedRecording && plan.readyForPipelineCreation && plan.batchStats.valid;
        plan.readyForGpuSubmission = plan.readyForCommandRecording && plan.readyForShaderCompilation;
        plan.valid = plan.readyForGpuSubmission && plan.warnings == 0;
        plan.summary = plan.valid ? "ok" : "invalid";
        return plan;
    }

    VulkanDrawSubmissionProbe BuildVulkanDrawSubmissionProbe()
    {
        VulkanDrawSubmissionProbe probe{};
        probe.scene = BuildDefaultPrimitiveTestScene();
        const RenderMeshOptimizationConfig optConfig = MakeDefaultRenderMeshOptimizationConfig();
        probe.optimized = OptimizePrimitiveSceneForVulkan(probe.scene, optConfig);
        probe.optimizationStats = AnalyzeOptimizedRenderGeometry(probe.scene, probe.optimized, optConfig);

        VulkanRhiConfig config = MakeDefaultVulkanRhiConfig("AK Vulkan Draw Submission Probe");
        config.window.platform = VulkanWindowPlatform::Win32;
        config.window.width = 1280;
        config.window.height = 720;
        config.window.valid = true;
        config.window.nativeHandle = reinterpret_cast<void*>(0x1);
        config.requireSwapchain = true;
        config.requireDynamicRendering = true;
        config.requireDescriptorIndexing = true;
        config.requireSynchronization2 = true;
        config.requireTimelineSemaphore = true;

        probe.upload = BuildVulkanMeshUploadPlan(probe.optimized, config, VulkanDrawSubmissionMode::DirectDrawIndexed);
        probe.submission = BuildVulkanDrawSubmissionPlan(probe.optimized, config);
        probe.ok = probe.scene.valid && probe.optimized.valid && probe.optimizationStats.valid && probe.upload.valid && probe.submission.valid;
        probe.summary = probe.ok ? "ok" : "invalid";
        return probe;
    }

    const char* ToString(VulkanDrawCommandKind kind)
    {
        switch (kind)
        {
        case VulkanDrawCommandKind::BeginFrame: return "BeginFrame";
        case VulkanDrawCommandKind::TransitionColorAttachment: return "TransitionColorAttachment";
        case VulkanDrawCommandKind::TransitionDepthAttachment: return "TransitionDepthAttachment";
        case VulkanDrawCommandKind::BeginDynamicRendering: return "BeginDynamicRendering";
        case VulkanDrawCommandKind::BindGraphicsPipeline: return "BindGraphicsPipeline";
        case VulkanDrawCommandKind::BindDescriptorSets: return "BindDescriptorSets";
        case VulkanDrawCommandKind::BindVertexBuffer: return "BindVertexBuffer";
        case VulkanDrawCommandKind::BindIndexBuffer: return "BindIndexBuffer";
        case VulkanDrawCommandKind::SetViewport: return "SetViewport";
        case VulkanDrawCommandKind::SetScissor: return "SetScissor";
        case VulkanDrawCommandKind::PushConstants: return "PushConstants";
        case VulkanDrawCommandKind::DrawIndexed: return "DrawIndexed";
        case VulkanDrawCommandKind::EndDynamicRendering: return "EndDynamicRendering";
        case VulkanDrawCommandKind::TransitionPresent: return "TransitionPresent";
        case VulkanDrawCommandKind::EndFrame: return "EndFrame";
        default: return "Unknown";
        }
    }

    const char* ToString(VulkanDescriptorResourceKind kind)
    {
        switch (kind)
        {
        case VulkanDescriptorResourceKind::UniformBuffer: return "UniformBuffer";
        case VulkanDescriptorResourceKind::StorageBuffer: return "StorageBuffer";
        case VulkanDescriptorResourceKind::SampledImage: return "SampledImage";
        case VulkanDescriptorResourceKind::Sampler: return "Sampler";
        default: return "Unknown";
        }
    }

    const char* ToString(VulkanShaderCompilerKind kind)
    {
        switch (kind)
        {
        case VulkanShaderCompilerKind::Slang: return "slangc";
        case VulkanShaderCompilerKind::GlslangValidator: return "glslangValidator";
        case VulkanShaderCompilerKind::DxcSpirv: return "dxc";
        case VulkanShaderCompilerKind::EmbeddedFallback: return "embedded";
        default: return "unknown";
        }
    }

    std::string ToDebugString(const VulkanShaderCompilerInvocation& invocation)
    {
        std::ostringstream ss;
        ss << "shader_compile compiler=" << ToString(invocation.compiler)
           << " entry=" << invocation.entryPoint
           << " input=" << invocation.inputPath
           << " output=" << invocation.outputPath
           << " profile=\"" << invocation.profile << "\""
           << " deterministic=" << (invocation.deterministic ? "true" : "false")
           << " valid=" << (invocation.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanShaderCacheEntry& entry)
    {
        std::ostringstream ss;
        ss << "shader_cache stage=" << entry.stage
           << " entry=" << entry.entryPoint
           << " source=" << entry.sourcePath
           << " present=" << (entry.sourcePresent ? "true" : "false")
           << " hash=0x" << std::hex << entry.sourceHash << std::dec
           << " spv=" << entry.spvPath
           << " estimated_spv=" << entry.estimatedSpvBytes
           << " needs_compile=" << (entry.needsCompile ? "true" : "false")
           << " valid=" << (entry.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanPipelineLayoutPlan& plan)
    {
        std::ostringstream ss;
        ss << "pipeline_layout sets=" << plan.descriptorSetCount
           << " bindings=" << plan.descriptorBindings.size()
           << " push_constants=" << plan.pushConstantBytes
           << " bindless=" << (plan.bindlessMaterialSet ? "true" : "false")
           << " object_table=" << (plan.objectTableStorage ? "true" : "false")
           << " material_table=" << (plan.materialTableStorage ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false")
           << " warnings=" << plan.warnings;
        return ss.str();
    }

    std::string ToDebugString(const VulkanRenderTargetPlan& plan)
    {
        std::ostringstream ss;
        ss << "render_target " << plan.width << "x" << plan.height
           << " color=" << plan.colorFormat
           << " depth=" << plan.depthFormat
           << " dynamic_rendering=" << (plan.dynamicRendering ? "true" : "false")
           << " reversed_z=" << (plan.reversedZ ? "true" : "false")
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanDrawBatchStats& stats)
    {
        std::ostringstream ss;
        ss << "draw_batch draws=" << stats.drawCalls
           << " commands=" << stats.recordedCommands
           << " pipeline_binds=" << stats.pipelineBinds
           << " descriptor_binds=" << stats.descriptorBinds
           << " vb_binds=" << stats.vertexBufferBinds
           << " ib_binds=" << stats.indexBufferBinds
           << " push_constants=" << stats.pushConstantWrites
           << " material_switches=" << stats.materialSwitches
           << " cpu_bytes~" << stats.estimatedCpuBytes
           << " valid=" << (stats.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanDrawSubmissionPlan& plan)
    {
        std::ostringstream ss;
        ss << "vulkan_draw_submission " << plan.summary
           << " commands=" << plan.commands.size()
           << " draws=" << plan.batchStats.drawCalls
           << " shader_entries=" << plan.shaderCache.size()
           << " compiler_invocations=" << plan.compilerInvocations.size()
           << " ready_shader=" << (plan.readyForShaderCompilation ? "true" : "false")
           << " ready_pipeline=" << (plan.readyForPipelineCreation ? "true" : "false")
           << " ready_recording=" << (plan.readyForCommandRecording ? "true" : "false")
           << " ready_submit=" << (plan.readyForGpuSubmission ? "true" : "false")
           << " warnings=" << plan.warnings
           << " valid=" << (plan.valid ? "true" : "false");
        return ss.str();
    }

    std::string ToDebugString(const VulkanDrawSubmissionProbe& probe)
    {
        std::ostringstream ss;
        ss << "vulkan draw submission probe ok=" << (probe.ok ? "true" : "false")
           << " source_vertices=" << probe.optimizationStats.sourceVertices
           << " packed_vertices=" << probe.optimizationStats.outputVertices
           << " indices=" << probe.optimizationStats.outputIndices
           << " commands=" << probe.submission.commands.size()
           << " draws=" << probe.submission.batchStats.drawCalls
           << " ready_submit=" << (probe.submission.readyForGpuSubmission ? "true" : "false")
           << " warnings=" << probe.submission.warnings;
        return ss.str();
    }
}
