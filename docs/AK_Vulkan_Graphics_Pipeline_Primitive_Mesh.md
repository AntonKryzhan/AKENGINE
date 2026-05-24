# AK Engine v6.3 — Vulkan Graphics Pipeline / Primitive Mesh Renderer Foundation

This patch moves the Vulkan path beyond a swapchain clear frame and adds the first renderer-side geometry contract.

## Scope

- Vulkan graphics pipeline planning for primitive mesh rendering.
- Vertex layout contract shared by CPU mesh generation and Vulkan draw planning.
- Reversed-Z depth policy for mesh rendering.
- Dynamic viewport/scissor policy.
- Primitive mesh generation for renderer tests:
  - triangle;
  - plane/floor;
  - cube;
  - UV sphere;
  - cylinder;
  - capsule;
  - debug grid mesh.
- Default primitive test scene.
- Draw-call planning for indexed mesh rendering.
- `ak_vulkanmeshprobe` validation tool.

## Vertex layout

The primitive renderer uses a fixed foundation layout:

```text
POSITION  float3  location 0  offset 0
NORMAL    float3  location 1  offset 12
TEXCOORD0 float2  location 2  offset 24
COLOR0    float4  location 3  offset 32
stride            48 bytes
```

This is intentionally simple and stable. Later material, tangent, skinning, instancing and GPU-driven paths can extend it without changing the initial mesh/pipeline validation contracts.

## Vulkan pipeline policy

The primitive pipeline plan assumes:

- triangle-list topology;
- back-face culling;
- color + depth attachments;
- reversed-Z depth;
- `GreaterOrEqual` depth compare;
- dynamic viewport and scissor;
- shader cache paths under `.akcache/shaders`;
- Slang/HLSL to SPIR-V as the planned shader toolchain.

This patch does not yet submit indexed mesh draw commands to Vulkan. The next step should create real shader modules, buffers, pipeline state, and a draw frame using the validated primitive scene.

## Validation

Build and run:

```powershell
cmake --build --preset windows-vs-debug --target ak_vulkanmeshprobe
.\build\windows-vs-debug\bin\Debug\ak_vulkanmeshprobe.exe
```

Expected result:

```text
[ ok ] vulkan graphics pipeline + primitive mesh renderer ... pipeline=ok validation=ok ... warnings=0
```

## Next step

The next controlled patch should be `v6.4 Vulkan Real Mesh Draw Frame`:

```text
shader module creation
vertex/index GPU buffers
uniform buffer for MVP
pipeline layout
graphics pipeline
depth image/view
indexed draw for the primitive scene
present rendered frame
```
