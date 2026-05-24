#include <AK/Render/VulkanUploadPlan.hpp>

#include <iostream>

int main()
{
    const AK::VulkanMeshUploadProbe probe = AK::BuildVulkanMeshUploadProbe();
    const auto& upload = probe.upload;

    std::cout << (probe.ok ? "[ ok ]" : "[ fail ]")
              << " vulkan gpu upload / draw indexed foundation"
              << " buffers=" << upload.buffers.size()
              << " copies=" << upload.copies.size()
              << " draws=" << upload.drawCount
              << " meshlets=" << upload.meshletDrawCount
              << " lods=" << upload.lodDrawCount
              << " staging=" << upload.stagingBytes
              << " device=" << upload.deviceLocalBytes
              << " uniform=" << upload.uniforms.totalBytes
              << " packed_vertices=" << probe.optimizationStats.outputVertices
              << " indices=" << probe.optimizationStats.outputIndices
              << " ready_buffers=" << (upload.readyForVkBufferCreation ? "true" : "false")
              << " ready_draw=" << (upload.readyForDrawIndexedRecording ? "true" : "false")
              << " warnings=" << (upload.warnings + probe.pipelineValidation.warnings + probe.optimizationStats.validationWarnings)
              << '\n';

    std::cout << AK::ToDebugString(upload) << '\n';
    if (!upload.buffers.empty())
    {
        std::cout << AK::ToDebugString(upload.buffers[0]) << '\n';
        if (upload.buffers.size() > 2)
        {
            std::cout << AK::ToDebugString(upload.buffers[1]) << '\n';
            std::cout << AK::ToDebugString(upload.buffers[2]) << '\n';
        }
    }
    std::cout << AK::ToDebugString(upload.uniforms) << '\n';
    std::cout << AK::ToDebugString(upload.descriptors) << '\n';
    std::cout << AK::ToDebugString(upload.shaders) << '\n';
    std::cout << AK::ToDebugString(probe) << '\n';

    return probe.ok ? 0 : 1;
}
