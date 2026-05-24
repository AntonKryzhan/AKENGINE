#include <AK/Platform/Window.hpp>
#include <AK/RHI/VulkanRHI.hpp>
#include <AK/Render/RenderFoundation.hpp>

#include <iostream>

int main()
{
    AK::WindowDesc windowDesc{};
    windowDesc.title = "AK Engine Vulkan Probe";
    windowDesc.width = 960;
    windowDesc.height = 540;

    AK::Window window(windowDesc);
    for (int i = 0; i < 4 && window.IsOpen(); ++i)
    {
        window.PollEvents();
    }

    AK::VulkanRhiConfig config = AK::MakeDefaultVulkanRhiConfig("AK Vulkan Probe");
    config.framesInFlight = 3;
    config.enableValidation = true;
    config.enableDebugUtils = true;
    config.presentModePolicy = AK::VulkanPresentModePolicy::MailboxPreferLowLatency;
    config.window = AK::MakeNativeWindowSurfaceDesc(window.NativeHandle(), window.Width(), window.Height());

    if (!config.window.valid)
    {
        config.requireSwapchain = false;
        config.window.platform = AK::VulkanWindowPlatform::Headless;
        config.window.width = windowDesc.width;
        config.window.height = windowDesc.height;
    }

    const AK::VulkanRuntimeResult runtime = AK::RunVulkanRuntimeBootstrap(config, true);
    const AK::RenderGraphStats graph = AK::BuildDefaultRenderGraphPlan().Build();

    const bool ok = runtime.ok
        && runtime.instanceCreated
        && runtime.deviceCreated
        && (runtime.headless || (runtime.surfaceCreated && runtime.swapchainCreated && runtime.clearSubmitted && runtime.presented))
        && graph.validationWarnings == 0;

    std::cout << (ok ? "[ ok ]" : "[ fail ]") << " vulkan runtime/device/swapchain/clear "
              << "runtime=" << (runtime.ok ? "ok" : "bad")
              << " headless=" << (runtime.headless ? "true" : "false")
              << " devices=" << runtime.physicalDeviceCount
              << " instance=" << (runtime.instanceCreated ? "yes" : "no")
              << " device=" << (runtime.deviceCreated ? "yes" : "no")
              << " surface=" << (runtime.surfaceCreated ? "yes" : "no")
              << " swapchain=" << (runtime.swapchainCreated ? "yes" : "no")
              << " clear=" << (runtime.clearSubmitted ? "yes" : "no")
              << " present=" << (runtime.presented ? "yes" : "no")
              << " images=" << runtime.swapchainImageCount
              << " graph_passes=" << graph.passes
              << " warnings=" << runtime.warnings
              << '\n';

    std::cout << AK::ToDebugString(runtime) << '\n';

    if (window.IsOpen())
    {
        window.RequestClose();
        window.PollEvents();
    }

    return ok ? 0 : 1;
}
