# AK Engine v7.1 PointCloud Import / Cook Foundation

This patch turns the v7.0 massive point-cloud foundation into an asset-pipeline contract.
It does not embed udSDK, LASzip, E57, GDAL, or a GPU renderer. Those remain optional bridges.
The engine-owned path is now explicit: streamed source reader -> deterministic attribute layout -> chunk pages -> LOD pages -> cooked manifest -> budgeted runtime streaming.

## Goals

- Detect common scan/GIS source containers: XYZ/PTS/CSV, PLY, LAS, LAZ, E57, DEM, UDS-like streams.
- Keep native AK Engine cooking independent from third-party SDK licensing and API keys.
- Build a GPU-ready attribute stream layout before Vulkan point rendering exists.
- Split huge point clouds into deterministic pages with stable page hashes.
- Generate a multi-LOD manifest for out-of-core runtime streaming.
- Preserve georeferencing metadata so large worlds, planet topology, and World Partition can consume the same data.
- Respect material-driven streamed attributes so RGB/intensity/classification/height payloads are not loaded blindly.

## New runtime concepts

```text
PointCloudSourceDesc
PointCloudAttributeLayout
PointCloudCookPolicy
PointCloudCookPageRecord
PointCloudCookManifest
PointCloudCookReport
```

## Cook pipeline

```text
source path / streamed reader
        ↓
source format detection
        ↓
PointCloudSourceDesc
        ↓
PointCloudAttributeLayout
        ↓
chunk target adjusted by max payload bytes
        ↓
LOD page generation
        ↓
page payload hash
        ↓
root manifest hash
        ↓
runtime streaming plan
```

## Attribute stream layout

Attributes are grouped into deterministic streams:

```text
Core            position, normal
Color           RGB
Classification  intensity, classification, return index
Auxiliary       height, scan angle, custom fields
```

This is intentionally not a final Vulkan buffer ABI. It is a stable foundation for the next renderer pass:

```text
page table SSBO
attribute stream SSBOs
GPU culling / LOD selection
compute expansion or direct point draw
```

## External SDK policy

The patch keeps external SDKs optional:

```text
allowExternalSdkBridge=false by default
native AK cook manifest remains authoritative
external decoders can feed the same source/chunk/page contracts later
```

This avoids making AK Engine dependent on udSDK accounts, API keys, closed-source decoders, or platform-specific SDK binaries.

## Probe

```powershell
cmake --build --preset windows-vs-debug --target ak_pointcloudcookprobe
.\build\windows-vs-debug\bin\Debug\ak_pointcloudcookprobe.exe
```

Expected result includes:

```text
[ ok ] pointcloud cook probe ...
```

## Next step

A good follow-up is v7.2: point-cloud runtime page table and Vulkan renderer bridge foundation.
That should convert cooked pages into render-page descriptors, GPU buffer upload requests, and FrameGraph-visible draw/dispatch work without requiring the final high-quality point splat renderer yet.
