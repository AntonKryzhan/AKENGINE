# AK Engine v7.0 PointCloud / Out-of-Core Foundation

This patch adds the first AK Engine foundation layer for massive scan datasets: point clouds, DEM/orthophoto geodata, and out-of-core chunk residency. The design is inspired by udCore / udSDK sample architecture, but it is implemented as native AK Engine code and does not depend on udSDK, API keys, cloud accounts, or third-party runtime decoders.

## Goals

- Stream point-cloud pages by budget instead of loading the whole scan.
- Keep point attributes explicit and material-driven.
- Allow unused streamed attributes to be excluded from page requests.
- Provide an out-of-core cache with deterministic LRU eviction and spill-file reload.
- Represent DEM and orthophoto import metadata for future GIS/planet/terrain pipelines.
- Prepare the runtime path for Vulkan storage-buffer rendering without adding real Vulkan draw calls in this patch.

## New modules

```text
engine/outofcore
engine/geodata
engine/pointcloud
tools/pointcloudprobe
```

## Out-of-core layer

`AK::ByteChunkedArray` stores bytes in fixed-size chunks so large datasets do not require one contiguous allocation.

`AK::VirtualChunkCache` adds:

```text
chunk descriptors
resident/spilled state
LRU eviction
spill-file append
spilled chunk reload
deterministic eviction tie-breaks
stats for resident/spilled/logical bytes
```

This is intended for future use by:

```text
point cloud pages
terrain tiles
voxel destruction caches
remesh temporary geometry
physics broadphase pages
asset cooker scratch buffers
```

## Geodata layer

`AK::GeoData` adds:

```text
GeoReference
EpsgCode
DEM grid descriptor
orthophoto descriptor
GeoImportPolicy
import byte/tile estimates
validation reports
```

The layer supports local meters, WGS84-style references, projected meters, and AK planet-surface metadata. It does not yet perform full CRS reprojection; that should become a later optional importer/tool dependency.

## Point cloud layer

`AK::PointCloud` adds:

```text
PointCloudAssetDesc
PointCloudAttributeDesc
PointCloudChunkDesc
PointCloudStreamingPolicy
PointCloudRenderPolicy
PointCloudMaterialPolicy
PointCloudStreamingPlan
```

Supported attribute semantics in the foundation:

```text
position
rgb
intensity
classification
height
normal
return index
scan angle
custom0/custom1
```

The streaming plan sorts chunks by camera distance, applies a byte budget, and computes requested payload size from the material-driven attribute mask. For example, a classification material can request only position + classification instead of loading RGB, intensity, height, normals, and custom attributes.

## Probe

```powershell
Set-Location D:\AKENGINE
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug --target ak_pointcloudprobe
.\build\windows-vs-debug\bin\Debug\ak_pointcloudprobe.exe
```

Expected behavior:

```text
[ ok ] massive pointcloud / scan streaming foundation ...
```

The probe verifies:

```text
geodata DEM/orthophoto descriptors
point cloud validation
chunk generation
material-driven attribute mask
budget clipping
out-of-core LRU spilling
spilled chunk reload
```

## Not included yet

This patch intentionally does not include:

```text
LAS/LAZ/E57/PLY decoder
udSDK bridge
GPU point renderer
Vulkan buffer upload path
octree/LOD hierarchy builder
CRS reprojection
planet tile cooker
```

Those should be separate patches so the core AK Engine remains portable and license-clean.
