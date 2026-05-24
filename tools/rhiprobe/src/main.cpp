#include <AK/RHI/VulkanRHI.hpp>
#include <AK/Render/RenderFoundation.hpp>

#include <iostream>

int main()
{
    AK::VulkanRhiConfig config = AK::MakeDefaultVulkanRhiConfig("AK RHI Probe");
    config.window.platform = AK::VulkanWindowPlatform::Win32;
    config.window.nativeHandle = reinterpret_cast<void*>(0x1);
    config.window.width = 1280;
    config.window.height = 720;
    config.window.valid = true;
    config.enableValidation = true;
    config.framesInFlight = 3;

    const AK::VulkanRhiProbe probe = AK::BuildVulkanRhiProbe(config, true);
    const AK::RenderGraphStats graph = AK::BuildDefaultRenderGraphPlan().Build();
    const AK::DepthPrecisionReport depth = AK::AnalyzeDepthPolicy(AK::MakeDefaultDepthPolicy());

    const bool planOk = probe.plan.valid
        && probe.renderGraphCompatible
        && probe.policyCompatible
        && probe.swapchainCompatible
        && probe.framesInFlightCompatible
        && graph.validationWarnings == 0
        && graph.presentPasses == 1
        && depth.reversedZ
        && depth.floatingPointDepth;

    std::cout << "[ ok ] rhi/vulkan bootstrap foundation "
              << "plan=" << (probe.plan.valid ? "ok" : "bad")
              << " loader=" << (probe.loader.loaded ? "loaded" : "not_loaded")
              << " frames=" << probe.plan.frames.size()
              << " queues=" << probe.plan.queues.size()
              << " instance_ext=" << probe.plan.instanceExtensions.size()
              << " device_ext=" << probe.plan.deviceExtensions.size()
              << " graph_passes=" << graph.passes
              << " reversed_z=" << (depth.reversedZ ? "true" : "false")
              << " warnings=" << probe.warnings
              << '\n';

    std::cout << AK::ToDebugString(probe.plan) << '\n';
    std::cout << AK::ToDebugString(probe.loader) << '\n';
    std::cout << AK::ToDebugString(probe) << '\n';

    return planOk ? 0 : 1;
}
