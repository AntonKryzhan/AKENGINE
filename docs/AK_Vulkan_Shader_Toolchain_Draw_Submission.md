# AK Engine v6.6 — Vulkan Shader Toolchain / Draw Submission Foundation

This patch connects the optimized render mesh path to a Vulkan-style draw submission contract.

It adds:

- Slang/HLSL primitive mesh shader source.
- deterministic shader cache entries and source hashing;
- shader compiler invocation plans for `slangc` and `glslangValidator` fallback;
- pipeline layout plan with camera uniform, object table, material table and bindless material texture set;
- dynamic-rendering render target plan;
- packed 32-byte vertex draw command recording;
- explicit command sequence model for future `vkCmd*` emission;
- a probe that validates shader cache, descriptor layout, draw commands and upload compatibility.

This is still a foundation layer. It does not yet create real `VkShaderModule`, `VkPipeline`, `VkDescriptorSetLayout`, or record a live `VkCommandBuffer`. The next Vulkan patch can implement those handles using this contract without changing the render extraction API.
