# AK Engine — план внедрения рендера

Версия документа: 1.0  
Цель: зафиксировать поэтапный план внедрения production-рендера в AK Engine так, чтобы он не был отдельной «картинкой поверх ECS», а стал частью общей архитектуры живого мира: большие координаты, World Partition, VFS/streaming, материалы, физика, разрушения, вода, дым, снег, кровь, толпы, астероидные поля и будущий hybrid raster/ray tracing renderer.

---

## 0. Главная цель рендера AK Engine

Рендер AK Engine должен быть:

```text
Vulkan-first
RenderGraph-driven
camera-relative
reversed-Z
hybrid deferred/forward
raster-first
ray tracing optional
material-unified
streaming-aware
budget-driven
GPU-driven-ready
large-world-safe
```

Главный контракт:

```text
Материал один.
Мир один.
Renderer backend может быть разный:
  Raster
  Hybrid Ray Tracing
  Path Tracing Preview later
```

Разработчик не должен делать отдельные материалы под raster и ray tracing. Материал описывает физику поверхности и PBR-свойства, а renderer выбирает способ вычисления.

---

## 1. Исходное состояние проекта

Уже есть фундамент, который нельзя ломать:

```text
C++20/23
CMake + MSVC / Visual Studio 2022
Windows-first
Vulkan-first later
Y-up
right-handed world
meters / seconds / kilograms
radians internally, degrees only in UI
Large World Coordinates
WorldPosition = int64 cell + double local
camera-relative rendering
reversed-Z depth policy
fixed simulation tick
generational EntityId
AssetGuid
ResourceHandle generation
atomic save
schema migration
VFS over loose/package
budgeted streaming
World Partition
terrain as policy
world topology as policy
gravity as field, not hardcoded -Y
RHI/Vulkan preparation
RenderGraph planning structures
```

Текущий ближайший путь проекта после CSG foundation:

```text
v3.7 CSG -> Physics Destruction Bridge
v3.8 Physics Foundation
v3.9 Collision Foundation
v4.0 Destruction Remesh Foundation
v4.1 Material / Surface System
v4.2 RHI Foundation
v4.3 Vulkan Surface / Device / Swapchain
v4.4 Shader Pipeline
v4.5 ImGui/Vulkan Editor
```

Рендер надо внедрять не раньше, чем будут зафиксированы базовые контракты RHI, материалов, ресурсов, Scene -> RenderScene sync и RenderGraph.

---

## 2. Главные архитектурные правила

### 2.1. Renderer не должен ходить напрямую по ECS каждый кадр

Нужен отдельный retained render world:

```text
ECS Scene
  ↓ dirty sync
RenderScene
  ↓ culling / sorting / GPU upload
RenderGraph
  ↓ Vulkan commands
```

Причина: ECS удобен для gameplay/editor, но renderer должен иметь плотные data-oriented структуры.

### 2.2. Renderer получает camera-relative данные

World authority:

```text
WorldPosition = int64 cell + double local
```

GPU data:

```text
GpuPosition = WorldPosition - CameraWorldPosition
float3 camera-relative
```

В absolute world coordinates в GPU нельзя отправлять большие float-значения.

### 2.3. Все ресурсы проходят через ResourceHandle

Renderer не должен держать raw pointers на mesh/texture/material/GPU buffer.

```text
AssetGuid -> ResourceManager -> ResourceHandle<T> -> RenderResource
```

Удаление GPU-ресурсов только через deferred release после fence/frame-in-flight.

### 2.4. RenderGraph владеет frame resources

Нельзя вручную плодить хаотичные Vulkan image transitions.

```text
Pass declares reads
Pass declares writes
Graph resolves barriers
Graph tracks lifetimes
Graph aliases transient resources
```

### 2.5. Все expensive systems должны иметь budget

Рендер не должен быть набором галочек Ultra. Должна быть политика:

```text
FrameBudgetController
  RenderBudget
  ShadowBudget
  VolumetricBudget
  WaterBudget
  SurfaceUpdateBudget
  StreamingUploadBudget
```

Если кадр дорогой, renderer снижает качество управляемо.

---

## 3. Модули рендера

Рекомендуемая структура:

```text
engine/rhi/
  AkRhi.h
  AkRhiDevice.h
  AkRhiSwapchain.h
  AkRhiCommandQueue.h
  AkRhiCommandBuffer.h
  AkRhiBuffer.h
  AkRhiImage.h
  AkRhiSampler.h
  AkRhiFence.h
  AkRhiPipeline.h
  AkRhiDescriptor.h

engine/rhi/vulkan/
  AkVulkanInstance.h
  AkVulkanDevice.h
  AkVulkanSwapchain.h
  AkVulkanCommandPool.h
  AkVulkanCommandBuffer.h
  AkVulkanBuffer.h
  AkVulkanImage.h
  AkVulkanDescriptor.h
  AkVulkanPipeline.h
  AkVulkanMemoryAllocator.h

engine/render/
  AkRenderScene.h
  AkRenderWorldSync.h
  AkRenderView.h
  AkRenderFrame.h
  AkRenderFeature.h
  AkRenderQuality.h
  AkFrameBudgetController.h

engine/rendergraph/
  AkRenderGraph.h
  AkRenderGraphBuilder.h
  AkRenderGraphResource.h
  AkRenderGraphPass.h
  AkRenderGraphCompiler.h
  AkRenderGraphExecutor.h
  AkRenderGraphProfiler.h

engine/shaders/
  AkShaderLibrary.h
  AkShaderCompiler.h
  AkShaderCache.h
  AkShaderPermutation.h
  AkPipelineCache.h

engine/material/
  AkMaterial.h
  AkMaterialClosure.h
  AkMaterialFeatureFlags.h
  AkMaterialRuntime.h
  AkMaterialCompiler.h

engine/lighting/
  AkLightComponent.h
  AkGpuLightBuffer.h
  AkClusteredLighting.h
  AkManyLights.h

engine/shadows/
  AkShadowSystem.h
  AkVirtualShadowPages.h
  AkShadowPageCache.h
  AkShadowBudget.h

engine/postprocess/
  AkTAA.h
  AkTonemap.h
  AkBloom.h
  AkExposure.h
  AkUpscaler.h

engine/virtualtexture/
  AkVirtualTextureStack.h
  AkVirtualTexturePageTable.h
  AkVirtualTextureFeedback.h
  AkVirtualTexturePhysicalCache.h

engine/volumetrics/
  AkVolumetricClouds.h
  AkVolumetricFog.h
  AkGpuSmoke.h

engine/water/
  AkWaterRender.h
  AkOceanRender.h
  AkRiverRender.h
```

---

## 4. RenderScene

### 4.1. Цель

`RenderScene` — это компактное представление мира для renderer. Оно синхронизируется из ECS только по dirty-флагам.

### 4.2. Данные

```cpp
struct AkRenderObject
{
    EntityId entity;
    MeshHandle mesh;
    MaterialHandle material;
    Aabb3 worldBounds;
    Mat4 worldFromLocal;
    uint32_t renderLayer;
    uint32_t flags;
};

struct AkRenderLight
{
    EntityId entity;
    LightType type;
    Vec3 worldPosition;
    Vec3 direction;
    Vec3 color;
    float intensity;
    float radius;
    uint32_t flags;
};

struct AkRenderView
{
    WorldPosition cameraWorldPosition;
    Mat4 view;
    Mat4 projection;
    Mat4 viewProjection;
    Aabb3 viewBounds;
    float nearPlane;
    float farPlane;
};
```

### 4.3. Sync pipeline

```text
ECS changes Transform/Mesh/Material/Light
  ↓
Dirty flags
  ↓
RenderWorldSync
  ↓
RenderScene hot arrays
  ↓
GPU upload buffers
```

### 4.4. Нельзя

```text
Нельзя каждый кадр перестраивать RenderScene целиком.
Нельзя хранить editor metadata в hot render arrays.
Нельзя renderer напрямую заставлять читать все ECS components.
```

---

## 5. RHI foundation

### 5.1. Цель

RHI должен быть тонким, но не бесполезным. Он скрывает Vulkan boilerplate, но не превращает renderer в black box.

### 5.2. Обязательные сущности

```cpp
enum class AkRhiBackend
{
    Vulkan,
    Null
};

struct AkRhiDeviceDesc
{
    bool enableValidation;
    bool enableDebugNames;
    bool enableRayTracingIfAvailable;
    bool enableDescriptorIndexingIfAvailable;
};

struct AkRhiImageDesc
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    AkFormat format;
    AkImageUsageFlags usage;
    uint32_t mipLevels;
    uint32_t arrayLayers;
};
```

### 5.3. Vulkan objects

Порядок внедрения:

```text
1. VkInstance
2. Debug messenger
3. Surface
4. Physical device selection
5. Logical device
6. Queues
7. Swapchain
8. Command pools
9. Command buffers
10. Fences/semaphores
11. Buffers/images
12. Descriptor layouts/pools
13. Pipeline layouts
14. Graphics pipelines
```

### 5.4. Пробы

```text
ak_rhiprobe.exe
  checks null backend
  creates RHI device
  creates dummy buffer/image desc
  validates handle generation

ak_vulkanprobe.exe
  creates instance/device
  enumerates adapters
  checks validation layer availability
  prints supported features

ak_swapchainprobe.exe
  opens window
  creates swapchain
  clears color
  presents frames
```

---

## 6. Vulkan Surface / Device / Swapchain

### 6.1. Цель первого Vulkan-патча

Не полноценный renderer. Только stable Vulkan clear window.

```text
Win32 window
  ↓
VkSurfaceKHR
  ↓
VkSwapchainKHR
  ↓
acquire image
  ↓
clear color
  ↓
present
```

### 6.2. Definition of Done

```text
ak_vulkan_clear.exe opens window
resize does not crash
validation layers clean or documented
swapchain recreate works
frame-in-flight fences work
GPU resources named in debug mode
```

### 6.3. Запрещено в этом этапе

```text
Не внедрять PBR.
Не внедрять ImGui.
Не внедрять complex scene renderer.
Не внедрять shader permutation system.
```

---

## 7. RenderGraph production foundation

### 7.1. Цель

Текущий RenderGraph foundation надо превратить в runtime frame graph.

### 7.2. Основные объекты

```cpp
struct AkRgResourceHandle
{
    uint32_t index;
    uint32_t generation;
};

struct AkRgImageDesc
{
    AkFormat format;
    AkExtent3D extent;
    AkImageUsageFlags usage;
    bool transient;
    bool imported;
};

struct AkRgPassDesc
{
    StringView name;
    AkQueueType queue;
    SmallVector<AkRgResourceAccess> reads;
    SmallVector<AkRgResourceAccess> writes;
};
```

### 7.3. Pipeline

```text
Build graph
  ↓
Validate resources
  ↓
Compile lifetimes
  ↓
Insert barriers
  ↓
Alias transient images
  ↓
Execute passes
  ↓
Collect timings
```

### 7.4. Начальные passes

```text
ClearColorPass
ClearDepthPass
TrianglePass
BlitToSwapchainPass
```

### 7.5. Позже

```text
DepthPrePass
GBufferPass
LightingPass
ShadowPass
HZBPass
PostProcessPass
VolumetricPass
WaterPass
ForwardTransparencyPass
```

---

## 8. Shader Pipeline

### 8.1. Цель

Нужен управляемый shader pipeline, чтобы не получить хаос файлов и бесконечную компиляцию.

### 8.2. Язык

Основной путь:

```text
Slang/HLSL
  ↓
SPIR-V
  ↓
Vulkan pipeline
```

Fallback на первом этапе:

```text
precompiled SPIR-V test shaders
```

### 8.3. Шейдерные библиотеки

```text
shaders/common/ak_math.hlsl
shaders/common/ak_camera.hlsl
shaders/common/ak_pbr.hlsl
shaders/common/ak_material.hlsl
shaders/common/ak_gbuffer.hlsl
shaders/common/ak_lighting.hlsl
shaders/common/ak_shadow.hlsl
shaders/common/ak_wave.hlsl
```

### 8.4. Shader cache

```cpp
struct AkShaderKey
{
    AssetGuid shaderAsset;
    uint64_t permutationHash;
    AkShaderStage stage;
    AkShaderTarget target;
};
```

### 8.5. Permutation policy

Сразу запретить взрыв permutations:

```text
Static permutations:
  крупные классы материала и geometry path

Runtime flags:
  мелкие feature flags

Specialization constants:
  Vulkan-friendly compile-time options

Fallback shader:
  если material shader ещё компилируется
```

---

## 9. Material / PBR foundation

### 9.1. Цель

Единый material contract для raster, RT, physics, surface systems.

### 9.2. Material Closure

```cpp
struct AkMaterialClosure
{
    Vec3 baseColor;
    Vec3 normal;
    float roughness;
    float metallic;
    float ao;
    float opacity;
    float emissiveStrength;
    uint32_t shadingModel;
};
```

### 9.3. Texture set

```text
BaseColor
Normal
ORM
Height
Emissive
Opacity optional
Transmission optional
```

### 9.4. Runtime material buffer

```cpp
struct AkGpuMaterial
{
    uint32_t baseColorTexture;
    uint32_t normalTexture;
    uint32_t ormTexture;
    uint32_t heightTexture;
    uint32_t flags;
    float roughnessMultiplier;
    float metallicMultiplier;
    float normalStrength;
    float heightScaleMeters;
};
```

### 9.5. PBR MVP

Первый PBR renderer:

```text
Opaque only
Directional light only
One material class: OpaqueLit
BaseColor + Normal + ORM
No transparency
No decals
No RT
```

---

## 10. Texture and upload system

### 10.1. Source formats

Поддержать в importer/cooker:

```text
PNG
JPG/JPEG
TGA
HDR
EXR later
DDS/KTX2 later
```

JPG разрешён, но с warnings для data maps:

```text
Normal / Roughness / Metallic / AO / Height / Opacity should not use JPEG unless explicitly allowed.
```

### 10.2. Runtime upload

```text
VFS read
  ↓
staging buffer
  ↓
copy to VkImage
  ↓
generate/upload mips
  ↓
transition to shader read
```

### 10.3. Upload budget

Texture upload должен идти через budget:

```text
max bytes per frame
max uploads per frame
priority by visibility/material importance
```

---

## 11. First real scene renderer

### 11.1. Цель

Показать ECS scene в Vulkan viewport.

### 11.2. Passes

```text
DepthClear
ColorClear
SimpleMeshPass
EditorGridPass
EditorSelectionOutlinePass
BlitToSwapchain
```

### 11.3. Renderable components

```text
TransformComponent
WorldPositionComponent
MeshComponent
MaterialComponent
CameraComponent
LightComponent
BoundsComponent
```

### 11.4. Definition of Done

```text
Editor viewport renders meshes
camera movement works
selection outline works
large-world camera-relative position works
reversed-Z projection validated
window resize works
no GPU validation errors
```

---

## 12. ImGui/Vulkan Editor

### 12.1. Цель

Заменить GDI bootstrap shell на Vulkan + ImGui docking editor.

### 12.2. Этапы

```text
1. ImGui context
2. Vulkan ImGui backend
3. Dockspace
4. Main menu
5. Hierarchy panel
6. Inspector panel
7. Asset Browser
8. Console
9. Scene View panel
10. Render viewport image inside ImGui
```

### 12.3. Контракт

Editor UI не должен владеть renderer. Он только показывает `SceneViewportTexture`.

---

## 13. Hybrid deferred/forward renderer

### 13.1. Основной pipeline

```text
DepthPrePass
  ↓
GBufferPass
  ↓
BuildHZB
  ↓
DeferredLightingPass
  ↓
SkyAtmospherePass
  ↓
VolumetricPass
  ↓
WaterPass
  ↓
ForwardTransparencyPass
  ↓
PostProcessPass
  ↓
EditorOverlayPass
```

### 13.2. GBuffer layout MVP

```text
Depth:
  D32F reversed-Z

GBuffer0:
  BaseColor RGB + Metallic A

GBuffer1:
  EncodedNormal XY + Roughness + AO

GBuffer2 optional:
  Emissive / MaterialId / ShadingModel

Velocity:
  RG16F later
```

### 13.3. Forward path

Forward используется для:

```text
transparent glass
water
particles
smoke composite
hair later
editor gizmos
special materials
```

---

## 14. Lighting foundation

### 14.1. MVP

```text
Directional light
Point light
Spot light
Ambient term
IBL placeholder
```

### 14.2. Clustered lighting

```text
View frustum
  ↓
3D cluster grid
  ↓
light assignment compute pass
  ↓
lighting shader reads only cluster lights
```

### 14.3. Shared clustered data

Cluster lists должны использоваться и deferred, и forward transparency.

```text
Opaque deferred:
  read cluster lights in lighting pass

Transparent forward:
  read same cluster light list
```

### 14.4. Many-lights later

```text
GpuLightBuffer
Light importance
Candidate sampling
Reservoir per pixel/tile
Temporal/spatial reuse
Denoise
```

---

## 15. Shadow foundation

### 15.1. MVP

```text
Directional light CSM/PSSM
Spot light shadow map
Shadow atlas
PCF
Stable cascades
```

### 15.2. Next step: Adaptive Virtual Shadows

```text
Virtual shadow pages
Physical page cache
Page request from visible pixels
Static/dynamic split
Shadow budget
Contact shadows
SDF/far shadows later
Ray query refinement later
```

### 15.3. Shadow policy

```cpp
enum class AkShadowMethod
{
    None,
    ContactOnly,
    ShadowMap,
    VirtualShadowMap,
    SdfShadow,
    RayQueryRefinement,
    ReservoirVisibility
};
```

### 15.4. Shadow LOD

```text
Near hero object:
  full shadow caster

Mid:
  shadow LOD mesh

Far:
  HLOD shadow proxy

Very far:
  density/SDF/no individual shadow
```

---

## 16. Postprocess foundation

### 16.1. MVP

```text
HDR render target
Exposure
Tonemap
Gamma/output transform
Bloom simple
FXAA optional
```

### 16.2. Production direction

```text
TAA
TAAU
motion vectors
reactive masks
transparency masks
SSR
SSAO/GTAO
depth of field
motion blur
color grading LUT
FSR/DLSS/XeSS abstraction later
```

### 16.3. Required render outputs for future

```text
Depth
Normals
Motion vectors
MaterialId
ObjectId
Reactive mask
Transparency mask
Disocclusion mask
```

---

## 17. GPU culling and indirect rendering

### 17.1. Foundation

```text
CPU frustum culling first
HZB build
GPU object culling
GPU LOD selection
indirect draw command generation
MultiDrawIndirect
```

### 17.2. Data

```cpp
struct AkGpuDrawObject
{
    uint32_t meshId;
    uint32_t materialId;
    uint32_t transformIndex;
    uint32_t boundsIndex;
    uint32_t flags;
};
```

### 17.3. Future

```text
meshlets
cluster culling
GPU-driven material sorting
device generated commands
mesh shaders optional
GPU work graph research
```

---

## 18. Virtual Texturing

### 18.1. Foundation

```text
VirtualTextureStack
PageId
PageTable
PhysicalCache
FeedbackBuffer
PageUploader
```

### 18.2. Feedback pipeline

```text
Frame N shader writes page requests
  ↓
Frame N+1 CPU/GPU resolves requests
  ↓
VFS streams pages from .akpak
  ↓
Upload budget uploads pages
  ↓
Page table updated
```

### 18.3. Layers

```text
BaseColor
Normal
ORM
Height
Emissive optional
Opacity optional
RuntimeSurface layer optional
```

---

## 19. Surface detail renderer

### 19.1. Modes

```cpp
enum class AkSurfaceDetailMode
{
    None,
    NormalOnly,
    ParallaxOffset,
    ParallaxOcclusion,
    ReliefMapping,
    SilhouetteParallax,
    HardwareTessellation,
    OfflineDisplacementMesh,
    VirtualGeometry
};
```

### 19.2. LOD routing

```text
Far:
  normal map

Mid:
  POM with low step count

Near:
  POM/relief high quality

Visible silhouette / hero:
  tessellation or real displaced geometry

High-poly asset:
  virtual geometry
```

### 19.3. Shadow and RT contracts

```text
POM affects material shading only.
Real displaced geometry affects silhouette, shadows, collision and RT proxy.
```

---

## 20. Virtualized geometry / Nanite-like path

### 20.1. Not first renderer

Virtual geometry не должен блокировать первый Vulkan renderer. Сначала classic mesh path.

### 20.2. Later pipeline

```text
High-poly source
  ↓
Model optimizer
  ↓
cluster hierarchy
  ↓
meshlet pages
  ↓
compressed cluster data
  ↓
GPU culling
  ↓
LOD selection
  ↓
indirect draw / mesh shader / compute raster later
```

### 20.3. Integration

Virtual geometry должен иметь:

```text
material section support
shadow proxy support
RT fallback proxy
VT-compatible material sampling
WorldPartition streaming
```

---

## 21. Water renderer

### 21.1. Render order

```text
Deferred opaque lighting
  ↓
Water surface pass
  ↓
Water volume / underwater pass
  ↓
Spray/Foam/Mist pass
  ↓
Forward transparency
```

### 21.2. Water types

```text
Ocean:
  projected grid / clipmap
  FFT/Gerstner
  reflection/refraction

River:
  spline mesh
  flow map
  obstacle interaction normal map
  foam map
  splash/mist emitters

Physical water:
  gameplay compartment state
  local GPU visual fluid near active flows
```

### 21.3. Required textures

```text
WaterNormal
FlowMap
FoamMap
Depth/SceneDepth
Reflection source
Refraction source
Wetness runtime surface layer
```

---

## 22. Volumetric renderer

### 22.1. Clouds

```text
WeatherMap
CloudNoise3D
DetailNoise3D
CloudShadowMap
CloudColorHalfRes
CloudTransmittanceHalfRes
CloudHistory
```

Pipeline:

```text
CloudShadowPass
VolumetricCloudRaymarchPass
CloudTemporalReprojectionPass
CloudBilateralUpsamplePass
CloudCompositePass
```

### 22.2. Smoke/fire/dust

```text
GPU compute sim
  density
  velocity
  temperature
  pressure
  vorticity
  obstacle SDF

Raymarch render
  absorption
  scattering
  self-shadow
  temporal reprojection
```

### 22.3. Performance

```text
half/quarter resolution
blue noise jitter
temporal accumulation
empty-space skipping
sparse bricks later
async compute later
```

---

## 23. Surface runtime layers

### 23.1. Purpose

Renderer читает surface state, но не симулирует gameplay logic.

### 23.2. RuntimeSurfaceVT

```text
R = height delta
G = wetness
B = dirt/blood/snow/mud mask
A = age/compaction/burn packed policy
```

### 23.3. Consumers

```text
Terrain material
Character material
Water wetness
Snow deformation
Mud tracks
Blood stains
Burn marks
Decals
AI evidence debug
```

---

## 24. Ray tracing optional layer

### 24.1. Rule

RT не является отдельным движком. Это слой качества поверх raster foundation.

### 24.2. Capabilities

```cpp
struct AkRayTracingCapabilities
{
    bool accelerationStructure;
    bool rayQuery;
    bool rayTracingPipeline;
    bool shaderBindingTable;
};
```

### 24.3. RT scene

```text
RenderScene
  ↓
BLAS per mesh/proxy
  ↓
TLAS per visible/static/dynamic instance
  ↓
RT material table
```

### 24.4. Feature routing

```text
RT shadows -> Adaptive Virtual Shadows fallback
RT reflections -> SSR + probes fallback
RTAO -> GTAO/SSAO fallback
RTGI -> DDGI/SSGI fallback
```

### 24.5. Start with ray queries

First useful RT features:

```text
selected contact shadows
reflection validation
RTAO
hero light visibility
```

---

## 25. Massive rendering: crowds and asteroid fields

### 25.1. Three object levels

```text
Entity:
  full gameplay

MassInstance:
  compact instanced object

DensityField:
  very far aggregate representation
```

### 25.2. Crowd rendering

```text
LOD0 full entity
LOD1 shared animation + GPU skinning
LOD2 VAT / bone LOD
LOD3 impostor cards
LOD4 density field
```

### 25.3. Asteroid rings

```text
Analytic ring
  ↓
Density particle field
  ↓
Impostors
  ↓
Instanced asteroid meshes
  ↓
Physical entities near camera
```

### 25.4. Renderer requirements

```text
GPU instance culling
LOD selection
indirect draw
bindless materials
impostor atlases
promotion/demotion hooks
```

---

## 26. Render quality and budget controller

### 26.1. Quality tiers

```text
Ultra
High
Medium
Low
Fallback
Off
```

### 26.2. Budget knobs

```text
resolution scale
shadow page updates per frame
cloud raymarch steps
smoke volume resolution
water simulation active cells
surface VT writes per frame
crowd LOD distance
texture streaming bytes per frame
GPU upload budget
RT rays per pixel
```

### 26.3. Degradation policy

If frame time exceeds target:

```text
1. reduce dynamic resolution
2. reduce volumetric quality
3. reduce shadow page updates
4. reduce surface update rate
5. reduce particle count
6. increase texture mip bias
7. downgrade distant mass/crowd LOD
8. disable optional RT features
```

---

## 27. Diagnostics and debug views

### 27.1. Mandatory profiler counters

```text
GPU frame ms
CPU render sync ms
RenderGraph pass ms
VRAM by pool
transient memory
shadow page count
shadow dirty pages
VT resident pages
VT page faults
cluster light count
draw calls
indirect draw count
visible objects
culled objects
HZB cull count
upload bytes/frame
shader compile time
pipeline cache misses
```

### 27.2. Debug views

```text
GBuffer BaseColor
GBuffer Normal
GBuffer Roughness
Depth
Motion vectors
Material ID
Shader LOD
Material Feature LOD
Light clusters
Shadow method
Shadow pages
VT residency
Surface wetness
Snow depth
Blood amount
Water flow
Smoke density
Cloud density
Crowd LOD
Asteroid cell LOD
```

### 27.3. Tools

```text
ak_renderprobe.exe
ak_rhiprobe.exe
ak_vulkanprobe.exe
ak_rendergraphprobe.exe
ak_shaderprobe.exe
ak_materialprobe.exe
ak_gbufferprobe.exe
ak_lightingprobe.exe
ak_shadowprobe.exe
ak_vtprobe.exe
ak_massrenderprobe.exe
```

---

## 28. Versioned implementation roadmap

## v4.2 — RHI Foundation

Цель: стабильный RHI contract без production renderer.

Deliverables:

```text
engine/rhi
RHI handles
RHI device desc
RHI buffer/image desc
Null backend
Vulkan backend skeleton
ak_rhiprobe.exe
```

DoD:

```text
CMake build passes
RHI handles generation-safe
Null backend test passes
No renderer behavior changed
```

---

## v4.3 — Vulkan Surface / Device / Swapchain

Deliverables:

```text
Vulkan instance
Validation layer setup
Debug messenger
Win32 surface
Physical/logical device
Graphics/present queues
Swapchain
Frame fences/semaphores
Clear color present
ak_vulkanprobe.exe
ak_swapchainprobe.exe
```

DoD:

```text
Window opens
Swapchain recreates on resize
Validation clean enough
No GPU resource lifetime hazards
```

---

## v4.4 — Shader Pipeline Foundation

Deliverables:

```text
Shader asset type
SPIR-V load path
Slang/HLSL compile path optional
Shader cache metadata
Shader permutation key
Fallback shader
ak_shaderprobe.exe
```

DoD:

```text
Can compile/load vertex+fragment shader
Can cache compiled shader
Can render triangle through Vulkan pipeline
```

---

## v4.5 — ImGui/Vulkan Editor Foundation

Deliverables:

```text
ImGui Vulkan backend
Dockspace
Scene View texture
Basic panels ported
Vulkan viewport clear
Editor overlays foundation
```

DoD:

```text
Editor runs on Vulkan/ImGui
Old GDI path can remain as fallback during transition
No regression in project loading
```

---

## v4.6 — RenderScene Sync Foundation

Deliverables:

```text
RenderScene
RenderObject arrays
RenderLight arrays
Dirty sync from ECS
camera-relative GPU transform upload
bounds-based visible list
ak_rendersceneprobe.exe
```

DoD:

```text
ECS scene can be mirrored into RenderScene
dirty updates only
large-world camera-relative transform validated
```

---

## v4.7 — First Mesh Renderer

Deliverables:

```text
Vertex/index buffers
Mesh upload
Simple material
Depth buffer reversed-Z
Camera UBO
Basic mesh draw pass
Grid/gizmo overlay path
```

DoD:

```text
Scene meshes visible in Vulkan viewport
Camera navigation works
Selection outline or debug bounds visible
```

---

## v4.8 — Material/PBR MVP

Deliverables:

```text
MaterialClosure
GpuMaterial buffer
BaseColor/Normal/ORM texture set
PBR shader library
OpaqueLit material
Directional light
```

DoD:

```text
PBR object renders with basecolor/normal/roughness/metallic
Texture import supports PNG/JPG at minimum
Data-map JPEG warning exists
```

---

## v4.9 — RenderGraph Runtime

Deliverables:

```text
RenderGraph builder/compiler/executor
transient images
resource lifetime tracking
basic barriers
pass profiler
```

DoD:

```text
Frame is built through RenderGraph
Clear/Depth/Mesh/Present passes declared as graph passes
```

---

## v5.0 — Hybrid Deferred/Forward Foundation

Deliverables:

```text
DepthPrePass
GBufferPass
DeferredLightingPass
ForwardTransparencyPass stub
HDR scene color
Tonemap
```

DoD:

```text
Opaque objects use deferred path
Transparent/simple forward path exists
RenderGraph owns intermediate targets
```

---

## v5.1 — Clustered Lighting / Forward+

Deliverables:

```text
GpuLightBuffer
cluster grid
light culling compute pass
cluster debug view
shared light lists for deferred and forward
```

DoD:

```text
Hundreds of unshadowed lights are culled by clusters
No full-light-loop per pixel by default
```

---

## v5.2 — Shadow Foundation

Deliverables:

```text
Directional CSM/PSSM
Spot shadow atlas
Stable cascades
shadow caster culling
shadow LOD policy foundation
contact shadows MVP
```

DoD:

```text
Sun shadows work
Spot shadows work
Shadow debug views show casters/cascades
```

---

## v5.3 — Adaptive Virtual Shadow Foundation

Deliverables:

```text
ShadowPageId
VirtualShadowPageTable
PhysicalShadowPageCache
page demand marking
page scheduler
static/dynamic split foundation
```

DoD:

```text
Only requested shadow pages update
Cached pages visualized
Dirty page reason visible
```

---

## v5.4 — Postprocess / Temporal Foundation

Deliverables:

```text
HDR exposure
tonemap
bloom
TAA
motion vectors foundation
reactive/transparency masks foundation
upscaler abstraction
```

DoD:

```text
TAA stable enough for stochastic effects later
Motion vectors debug view available
```

---

## v5.5 — Virtual Texturing Foundation

Deliverables:

```text
VirtualTextureStack
PageTable
PhysicalCache
Feedback buffer
Page upload budget
VT debug views
```

DoD:

```text
Material can sample VT-backed texture stack
Missing pages fall back to lower mip
Residency debug view works
```

---

## v5.6 — Surface Runtime Layers

Deliverables:

```text
RuntimeSurfaceVT
surface interaction write pass
height delta normal rebuild
wetness/dirt/blood/snow masks
material reads runtime layer
```

DoD:

```text
Footstep/wetness/debug brush modifies visible surface
Runtime pages are sparse/dirty-tracked
```

---

## v5.7 — Water Renderer Foundation

Deliverables:

```text
Ocean surface pass
River spline surface pass
Flow map sampling
Foam map sampling
Scene depth refraction
Underwater detection foundation
```

DoD:

```text
Ocean/lake water renders
River water renders with flow direction
Foam/debug maps visible
```

---

## v5.8 — Volumetric Foundation

Deliverables:

```text
Volumetric fog froxel grid
Cloud density function
Cloud raymarch pass
Smoke volume render pass
Temporal reprojection foundation
```

DoD:

```text
Volumetric fog visible
Cloud layer visible
Smoke volume debug visible
```

---

## v5.9 — GPU Smoke Simulation MVP

Deliverables:

```text
Density/velocity/temperature 3D textures
Advection
Buoyancy
Curl noise
Vorticity confinement
Pressure solve basic
Raymarch render
```

DoD:

```text
Local smoke sim runs on GPU
Obstacle SDF hook planned or stubbed
Quality tiers exist
```

---

## v6.0 — GPU Culling / Indirect Draw

Deliverables:

```text
HZB build
GPU frustum/occlusion culling
LOD selection
indirect command buffer
multi-draw indirect
```

DoD:

```text
Renderer can draw visible objects from GPU-generated commands
CPU draw submission reduced
```

---

## v6.1 — Mass Instance / Crowd Renderer

Deliverables:

```text
MassArchetype
MassInstanceCell
compact instance storage
GPU instance culling
crowd animation sharing
VAT support
impostor LOD
```

DoD:

```text
Thousands of agents render with LOD
Far crowd uses impostors/VAT, not full skeletons
```

---

## v6.2 — Planetary Ring / Asteroid Renderer

Deliverables:

```text
PlanetaryRingDesc
RingCell
procedural asteroid generation
density field render
impostor/mesh transition
promotion to Entity near camera
```

DoD:

```text
Camera can approach asteroid ring
Far ring -> density -> impostor -> mesh transition works without hard popping
```

---

## v6.3 — Ray Tracing Optional Layer

Deliverables:

```text
RT capability detection
BLAS/TLAS abstraction
RT material table
ray query shadows/AO MVP
SSR/probe/VSM fallback routing
```

DoD:

```text
RT can be enabled if GPU supports it
Same material works in raster and RT path
Fallback works on non-RT GPU
```

---

## v6.4 — Many-Lights Reservoir Direct Lighting

Deliverables:

```text
Light candidate sampling
Reservoir buffer
Temporal reuse
Spatial reuse
Denoise
Debug reservoir confidence
```

DoD:

```text
Hundreds/thousands of lights can contribute without every pixel looping all lights
```

---

## v6.5 — Virtual Geometry Foundation

Deliverables:

```text
Meshlet builder
Cluster hierarchy
Cluster page streaming
GPU cluster culling
Virtual geometry material section support
Shadow/RT fallback proxy
```

DoD:

```text
High-poly static asset can render through virtual geometry path
Classic mesh path still works
```

---

## 29. Integration matrix

| System | Renderer dependency | Renderer output consumed by system |
|---|---|---|
| World Partition | visible cells, streaming priority | debug cell overlays |
| VFS/Streaming | texture/mesh/page requests | residency feedback |
| Materials | shader variants, texture bindings | material debug views |
| Surface System | runtime VT writes | wetness/snow/blood render state |
| Destruction/CSG | generated mesh/proxy upload | debris/dust/inner material visualization |
| Physics | collision proxies, debug draw | contact debug, surface events |
| Water | scene depth, normals, flow maps | wetness, foam, underwater output |
| Volumetrics | depth, lights, shadows | cloud/smoke composite |
| Weather/Thermal | wetness, smoke, fire lights | material state changes |
| MassRender | GPU culling, instancing | density/crowd debug |
| AI/Audio | evidence/debug overlays | sound/visibility debug |
| Editor | viewport texture, picking buffers | gizmos, panels, profilers |

---

## 30. Minimum first vertical slice

Самый маленький meaningful Vulkan renderer slice:

```text
1. Vulkan clear window
2. ImGui docking editor
3. RenderGraph clear/present
4. RenderScene sync
5. Camera-relative mesh draw
6. Reversed-Z depth
7. One textured PBR material
8. One directional light
9. Depth + GBuffer + deferred lighting
10. Tonemap
11. Debug overlay with GPU timings
```

После этого можно расширять в стороны: shadows, clustered lights, VT, water, volumetrics, GPU culling.

---

## 31. Основные риски

### 31.1. Слишком ранний production renderer

Риск: попытаться сразу сделать PBR + shadows + water + clouds.  
Решение: vertical slices, каждый этап с probe и DoD.

### 31.2. RenderGraph без реальной синхронизации

Риск: RenderGraph останется только списком pass'ов.  
Решение: early barrier validation, lifetime tracking, imported/transient resources.

### 31.3. Shader permutation explosion

Риск: материалы начнут генерировать тысячи PSO.  
Решение: material class + feature flags + permutation limits + fallback shader.

### 31.4. GPU/CPU sync stalls

Риск: readback/picking/profiling/VT feedback начнут блокировать кадр.  
Решение: delayed readback, frame latency, async queues, no immediate GPU waits.

### 31.5. Потеря large-world precision

Риск: часть renderer начнёт использовать absolute float world positions.  
Решение: tests/probes for camera-relative transform path.

### 31.6. Всё full quality всегда

Риск: красивые системы убьют FPS.  
Решение: budget controller и quality/fallback matrix с первого production этапа.

---

## 32. Итоговый принцип внедрения

Renderer AK Engine должен расти не как один огромный патч, а как цепочка проверяемых слоёв:

```text
RHI
  ↓
Vulkan device/swapchain
  ↓
RenderGraph
  ↓
Shader pipeline
  ↓
RenderScene sync
  ↓
Mesh draw
  ↓
PBR material
  ↓
Hybrid deferred/forward
  ↓
Lighting/shadows
  ↓
Postprocess/temporal
  ↓
Virtual texturing
  ↓
GPU-driven rendering
  ↓
Water/volumetrics/surface runtime layers
  ↓
RT optional layer
  ↓
Mass rendering / virtual geometry / many-lights
```

Главный итоговый контракт:

```text
AK Renderer не просто рисует сцену.
Он является адаптивным слоем визуализации живого мира:
  видимое — качественно,
  важное — точно,
  далёкое — дешево,
  невидимое — не считать,
  нестабильное — кэшировать/temporal,
  огромное — virtualize/stream,
  слабое железо — fallback без переделки контента.
```
