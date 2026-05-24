# AK Engine v6.7 — Vulkan Runtime Architecture Hardening

This pass locks the renderer-facing Vulkan runtime contracts before the engine starts creating the full set of real `VkBuffer`, `VkPipeline`, descriptor, command pool and submit objects.

The goal is not to hide Vulkan behind a Unity-like black box. The goal is to preserve explicit control while preventing the common expensive failure modes:

- runtime `vkAllocateMemory` in hot frames;
- per-draw descriptor updates;
- pipeline creation stutter;
- command pool contention across threads;
- redundant bind/state commands;
- missing frame-resource retirement;
- ad-hoc shader variant keys;
- accidental fallback to OpenGL-style state mutation.

## Added contracts

### Feature profile

`VulkanFeatureRequirement` describes the baseline and optional fast paths:

- Vulkan 1.3 desktop baseline;
- dynamic rendering;
- synchronization2;
- timeline semaphores;
- descriptor indexing;
- buffer device address;
- scalar block layout;
- extended dynamic state;
- optional shader objects;
- optional pipeline binary;
- optional push descriptors;
- optional device generated commands.

### Device limits snapshot

`VulkanDeviceLimitsSnapshot` captures the limits that must feed allocator, descriptor, uniform and copy alignment decisions.

This is intentionally a snapshot contract rather than direct `VkPhysicalDeviceLimits` exposure so platform-specific probing can be added without changing renderer code.

### Memory allocator plan

`VulkanMemoryBlockAllocatorPlan` defines block-based suballocation domains:

- persistent upload ring;
- transient upload scratch;
- device static geometry;
- device streaming geometry;
- device object/material tables;
- readback diagnostics;
- RenderGraph transient images;
- pipeline compile scratch.

The policy is: allocate large blocks, suballocate inside them, retire by frame/timeline fence, never allocate/free per draw.

### Descriptor frequency model

`VulkanDescriptorSetFrequencyPlan` locks descriptor sets by update frequency:

- set 0: frame/camera;
- set 1: view/lighting;
- set 2: material/bindless table;
- set 3: object/draw tables.

Per-draw descriptor writes are explicitly rejected. Draws pass object/material/draw indices through push constants and storage table indexing.

### Pipeline cache and shader variants

`VulkanPipelineCacheKeyPlan` and `VulkanShaderVariantKeyPlan` provide stable keys that include:

- shader source hash;
- vertex layout hash;
- render state hash;
- pipeline layout hash;
- render target format class;
- future device identity and driver version.

`VulkanAsyncPipelineCompileQueuePlan` enables async compile and fallback/uber shader behavior so editor hot reload and first-time material variants do not hitch the frame.

### Command pools and submit batches

`VulkanCommandPoolFrameThreadPlan` creates the contract for per-frame/per-thread command pools:

- primary graphics;
- secondary graphics;
- worker secondary;
- upload transfer;
- async compute.

`VulkanFrameSubmitBatchPlan` defines the frame timeline:

1. acquire swapchain image;
2. upload transfer;
3. async compute future slot;
4. graphics draw indexed;
5. present.

### Redundant bind filtering

`VulkanRedundantBindFilterPlan` analyzes the recorded draw command model and estimates redundant pipeline/descriptor/buffer/dynamic-state writes before they become real `vkCmd*` calls.

## Probe

New target:

```powershell
cmake --build --preset windows-vs-debug --target ak_vulkanarchprobe
.\build\windows-vs-debug\bin\Debug\ak_vulkanarchprobe.exe
```

Expected status:

```text
[ ok ] vulkan runtime architecture hardening ... warnings=0
```

## Next pass

The next Vulkan pass can now safely instantiate real objects:

- block allocator-backed `VkBuffer` creation;
- staging upload ring;
- descriptor set layouts / descriptor pools;
- pipeline cache object;
- shader module creation from cached SPIR-V;
- graphics pipeline creation;
- command buffer recording for `vkCmdDrawIndexed`.
