# AK Engine v6.2 — Vulkan Device / Swapchain / Clear Frame Foundation

This patch turns the previous Vulkan RHI bootstrap from a loader/plan check into a real runtime bootstrap path.

## Scope

Added runtime Vulkan creation path:

1. dynamic Vulkan loader resolution;
2. instance creation;
3. optional validation layer filtering;
4. native Win32 surface creation when a real window handle is available;
5. physical device enumeration;
6. graphics/present queue-family selection;
7. logical device creation;
8. swapchain creation;
9. swapchain image view creation;
10. command pool and command buffer allocation;
11. semaphore/fence creation;
12. one transfer-clear command on the acquired swapchain image;
13. queue submit and present.

The runtime entry point is:

```cpp
AK::VulkanRuntimeResult AK::RunVulkanRuntimeBootstrap(const AK::VulkanRhiConfig& config, bool submitClearFrame);
```

The verification tool is:

```text
ak_vulkanprobe
```

## Platform behavior

On Windows, `ak_vulkanprobe` creates a native `AK::Window`, builds a Win32 `VkSurfaceKHR`, creates a swapchain, clears one frame, presents it, and then releases all resources.

On platforms where `AK::Window::NativeHandle()` is not available yet, the probe falls back to headless device bootstrap. In that mode it creates Vulkan instance/device resources when a local Vulkan ICD is available, but it intentionally skips swapchain/present.

## Current limitations

This is still a foundation pass, not the final persistent renderer:

- no persistent `VulkanRenderer` object yet;
- no resize/recreate path yet;
- no RenderGraph execution into command buffers yet;
- no dynamic rendering color/depth pass yet;
- no shader pipeline yet;
- no ImGui/Vulkan editor backend yet.

The next renderer patch should keep the created device/swapchain alive across frames and move clear/present into `Renderer::BeginFrame/EndFrame` or a dedicated `VulkanRenderer` backend.

## Windows command

```powershell
Set-Location D:\AKENGINE
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug --target ak_vulkanprobe
.\build\windows-vs-debug\bin\Debug\ak_vulkanprobe.exe
```

## Vulkan SDK

The RHI uses dynamic loader calls, but Windows development should still use the LunarG Vulkan SDK for headers, validation layers, tools, `vulkaninfo`, and future shader tooling. The runtime loader must find `vulkan-1.dll` and a GPU driver/ICD.
