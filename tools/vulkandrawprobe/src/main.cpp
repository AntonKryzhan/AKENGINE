#include <AK/Render/VulkanDrawSubmission.hpp>

#include <iostream>

int main()
{
    const AK::VulkanDrawSubmissionProbe probe = AK::BuildVulkanDrawSubmissionProbe();
    const auto& submission = probe.submission;

    std::cout << (probe.ok ? "[ ok ]" : "[ fail ]")
              << " vulkan shader toolchain / draw command path foundation"
              << " commands=" << submission.commands.size()
              << " draws=" << submission.batchStats.drawCalls
              << " materials=" << submission.upload.uniforms.materialCount
              << " shader_entries=" << submission.shaderCache.size()
              << " compile_invocations=" << submission.compilerInvocations.size()
              << " descriptor_bindings=" << submission.layout.descriptorBindings.size()
              << " pipeline_ready=" << (submission.readyForPipelineCreation ? "true" : "false")
              << " recording_ready=" << (submission.readyForCommandRecording ? "true" : "false")
              << " submit_ready=" << (submission.readyForGpuSubmission ? "true" : "false")
              << " warnings=" << submission.warnings
              << '\n';

    std::cout << AK::ToDebugString(submission) << '\n';
    std::cout << AK::ToDebugString(submission.renderTarget) << '\n';
    std::cout << AK::ToDebugString(submission.layout) << '\n';
    if (!submission.shaderCache.empty())
    {
        std::cout << AK::ToDebugString(submission.shaderCache[0]) << '\n';
        if (submission.shaderCache.size() > 1)
        {
            std::cout << AK::ToDebugString(submission.shaderCache[1]) << '\n';
        }
    }
    if (!submission.compilerInvocations.empty())
    {
        std::cout << AK::ToDebugString(submission.compilerInvocations[0]) << '\n';
    }
    std::cout << AK::ToDebugString(submission.batchStats) << '\n';
    std::cout << AK::ToDebugString(probe) << '\n';

    return probe.ok ? 0 : 1;
}
