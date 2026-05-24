# AK Engine v6.9 — Vulkan FrameGraph Execution / Command Recording Foundation

This patch consumes the v6.8 compiled Vulkan frame graph and turns it into a deterministic command-recording and queue-submit plan.

The layer still avoids hard dependency on Vulkan SDK headers in public engine code. It is a renderer/RHI contract that maps one-to-one to real Vulkan calls:

```text
CompiledFrameGraph
  -> frame resources
  -> command buffers
  -> vkCmdPipelineBarrier2
  -> vkCmdCopyBuffer
  -> vkCmdBeginRendering / vkCmdEndRendering
  -> vkCmdBindPipeline / descriptors / vertex/index buffers
  -> vkCmdDrawIndexed
  -> vkCmdDispatch
  -> vkQueueSubmit2
  -> vkQueuePresentKHR
```

## Added

- `VulkanExecutionConfig`
- `VulkanExecutionCommand`
- `VulkanExecutionPassPlan`
- `VulkanExecutionSubmitPlan`
- `VulkanExecutionFrameResourcePlan`
- `VulkanFrameGraphExecutionPlan`
- `BuildVulkanFrameGraphExecutionPlan()`
- `ak_vulkanexecprobe`

## Why this patch matters

The renderer now has a stable bridge between graph compilation and actual Vulkan command emission. The previous layer knew resource lifetimes, barriers and queue batches. This layer decides exactly which command buffers are recorded, which passes open dynamic rendering, where draw indexed commands go, where transfer copies happen, and how submit batches wait/signal timeline values.

This prevents common Vulkan regressions before handle creation becomes dense:

- manually scattered barriers;
- draw passes without matching dynamic rendering scopes;
- hidden GPU readbacks in the frame;
- runtime allocation risk in hot paths;
- queue submit order drift;
- missing command buffers for async transfer/compute/present batches.

## Current default execution

The default probe records this logical frame:

```text
begin_frame
reset frame command pool
acquire_swapchain
upload_geometry_and_tables
  vkCmdCopyBuffer for packed geometry / tables

depth_prepass
  vkCmdBeginRendering
  vkCmdDrawIndexed for primitive geometry
  vkCmdEndRendering

forward_primitive_lit
  vkCmdBeginRendering
  vkCmdDrawIndexed for primitive geometry
  vkCmdEndRendering

visibility_reduce_compute
  vkCmdDispatch

tonemap_to_swapchain
  vkCmdBeginRendering
  fullscreen/synthetic draw
  vkCmdEndRendering

present_swapchain
queue submit batches
end_frame
```

## Validation

The execution validation checks:

- compiled graph readiness;
- draw submission readiness;
- frame resources;
- submit batches;
- command buffer availability;
- dynamic rendering begin/end scopes;
- draw command presence;
- in-frame GPU readback risk;
- runtime allocation risk;
- command budget overflow.

## Probe

```powershell
Set-Location D:\AKENGINE
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug --target ak_vulkanexecprobe
.\build\windows-vs-debug\bin\Debug\ak_vulkanexecprobe.exe
```

Expected result:

```text
[ ok ] vulkan framegraph execution / command recording foundation ... ready=true warnings=0
```

## Next step

The next Vulkan patch can replace this logical command stream with real persistent Vulkan objects:

- actual transient image heap allocation;
- actual `VkBuffer` allocation through block allocators;
- shader module creation from cached SPIR-V;
- descriptor set layout/pool creation;
- graphics pipeline creation;
- real `VkCommandBuffer` recording over this execution plan.
