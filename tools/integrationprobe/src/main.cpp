#include <AK/Integration/EngineIntegration.hpp>
#include <AK/RHI/VulkanRHI.hpp>
#include <AK/Render/RenderFoundation.hpp>

#include <iostream>

int main()
{
    AK::EngineIntegrationConfig integrationConfig{};
    integrationConfig.enablePhysics = true;
    integrationConfig.enableStreaming = true;
    integrationConfig.enableRenderExtraction = true;
    integrationConfig.enableVulkanRhi = true;
    integrationConfig.enableDiagnostics = true;

    const AK::EngineIntegrationProbe integration = AK::BuildEngineIntegrationProbe(integrationConfig);

    AK::VulkanRhiConfig rhiConfig = AK::MakeDefaultVulkanRhiConfig("AK Integration Probe");
    rhiConfig.window.platform = AK::VulkanWindowPlatform::Win32;
    rhiConfig.window.nativeHandle = reinterpret_cast<void*>(0x1);
    rhiConfig.window.width = 1920;
    rhiConfig.window.height = 1080;
    rhiConfig.window.valid = true;

    const AK::VulkanRhiProbe rhi = AK::BuildVulkanRhiProbe(rhiConfig, false);
    const AK::RenderGraphStats renderGraph = AK::BuildDefaultRenderGraphPlan().Build();

    const bool ok = integration.ok && rhi.ok && renderGraph.validationWarnings == 0;

    std::cout << "[ ok ] runtime integration + vulkan frame pipeline "
              << "stages=" << integration.stats.stages
              << " jobs=" << integration.stats.jobStages
              << " render=" << integration.stats.renderThreadStages
              << " gpu=" << integration.stats.gpuStages
              << " rhi=" << (rhi.ok ? "ok" : "bad")
              << " graph_passes=" << renderGraph.passes
              << " warnings=" << (integration.stats.dependencyWarnings + integration.stats.orderingWarnings + rhi.warnings)
              << '\n';

    std::cout << AK::ToDebugString(integration.stats) << '\n';
    std::cout << AK::ToDebugString(integration) << '\n';
    std::cout << AK::ToDebugString(rhi.plan) << '\n';

    return ok ? 0 : 1;
}
