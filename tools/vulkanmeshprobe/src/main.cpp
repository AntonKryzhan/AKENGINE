#include <AK/RHI/VulkanRHI.hpp>
#include <AK/Render/PrimitiveMesh.hpp>
#include <AK/Render/RenderFoundation.hpp>

#include <iostream>

int main()
{
    AK::VulkanRhiConfig config = AK::MakeDefaultVulkanRhiConfig("AK Vulkan Mesh Probe");
    config.framesInFlight = 3;
    config.enableValidation = true;
    config.enableDebugUtils = true;
    config.window.platform = AK::VulkanWindowPlatform::Win32;
    config.window.width = 1280;
    config.window.height = 720;
    config.window.valid = true;
    config.window.nativeHandle = reinterpret_cast<void*>(0x1);
    config.requireSwapchain = true;
    config.requireDynamicRendering = true;
    config.requireSynchronization2 = true;
    config.requireTimelineSemaphore = true;

    const AK::PrimitiveTestScene scene = AK::BuildDefaultPrimitiveTestScene();
    const AK::PrimitiveMeshStats meshStats = AK::AnalyzePrimitiveTestScene(scene);
    const std::vector<AK::VulkanDrawCallPlan> drawCalls = AK::BuildDrawCallPlan(scene);
    const AK::VulkanPrimitiveRendererProbe probe = AK::BuildVulkanPrimitiveRendererProbe(config, drawCalls);
    const AK::RenderGraphStats graph = AK::BuildDefaultRenderGraphPlan().Build();

    const bool ok = scene.valid
        && meshStats.valid
        && probe.readyForGpuDraw
        && graph.validationWarnings == 0
        && probe.pipeline.vertexLayout.strideBytes == sizeof(AK::PrimitiveVertex);

    std::cout << (ok ? "[ ok ]" : "[ fail ]") << " vulkan graphics pipeline + primitive mesh renderer "
              << "meshes=" << meshStats.meshes
              << " objects=" << meshStats.objects
              << " draw_calls=" << probe.drawCalls.size()
              << " vertices=" << probe.totalVertices
              << " indices=" << probe.totalIndices
              << " triangles=" << probe.totalTriangles
              << " stride=" << probe.pipeline.vertexLayout.strideBytes
              << " attrs=" << probe.pipeline.vertexLayout.attributes.size()
              << " pipeline=" << (probe.pipeline.valid ? "ok" : "bad")
              << " validation=" << (probe.validation.ok ? "ok" : "bad")
              << " graph_passes=" << graph.passes
              << " warnings=" << (probe.warnings + graph.validationWarnings + meshStats.validationWarnings)
              << '\n';

    std::cout << AK::ToDebugString(meshStats) << '\n';
    std::cout << AK::ToDebugString(probe.pipeline) << '\n';
    std::cout << AK::ToDebugString(probe.validation) << '\n';
    std::cout << AK::ToDebugString(probe) << '\n';

    return ok ? 0 : 1;
}
