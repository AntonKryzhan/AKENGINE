# AK Engine v3.4 — Neural Texture Compression Foundation

This module does not vendor or compile NVIDIA RTXNTC yet. It defines the engine-side contract that will let the cooker, package system, VFS, streaming layer, Vulkan backend and material system integrate neural material texture compression without rewriting texture assets later.

## Why this exists

Modern materials are texture sets, not one image:

- base color / albedo
- normal
- roughness
- metalness
- ambient occlusion
- height / displacement
- opacity
- emissive or custom BRDF data

Neural Texture Compression is valuable because a material texture set usually contains correlation across channels, across space and across mip levels. The renderer should be able to keep a compact representation in memory and decode material values through a shader path when hardware support is present.

## Engine rule

Do not treat material textures as unrelated single images. Import them as a `TextureSetDesc`, validate semantic channels, normalize resolution if needed, and choose a runtime compression policy.

## Runtime modes

`NtcRuntimeMode::InferenceOnLoadToBCn`

- stores NTC payload in package/cache
- transcodes or expands to BCn-compatible runtime textures on load
- saves disk/package/download size
- does not provide maximal VRAM saving
- safest fallback mode before Vulkan shader decode is ready

`NtcRuntimeMode::InferenceOnSample`

- keeps NTC feature grids and network weights resident
- material shader decodes texels on demand
- provides the strongest VRAM reduction path
- requires Vulkan feature detection, shader compiler support, subgroup operations, cooperative matrix fast path or a fallback decode path
- should be paired with temporal reconstruction/stochastic filtering policy

`NtcRuntimeMode::Hybrid`

- allows selected materials to use sample-mode decode while others fall back to BCn
- useful for low-VRAM budgets, high-resolution terrain layers and planet surface materials

## Integration stages now recorded in code

1. Foundation
   - `engine/texture`
   - `engine/ntc`
   - texture set descriptions
   - NTC profiles and policy contracts
   - `ak_ntcprobe.exe`

2. Cooker SDK bridge
   - connect RTXNTC command/library path
   - validate SDK availability
   - compress material texture sets into `.akntc` payloads
   - emit deterministic cooked targets in asset database

3. Vulkan feature detection
   - detect subgroup support
   - detect cooperative matrix support
   - detect shader compiler path
   - choose sample-mode / load-mode / fallback

4. Shader decode path
   - bind feature grids and network weights
   - provide material sampling function
   - support fallback to BCn/raw textures

5. Temporal filtering
   - stochastic filter policy
   - TAA/DLSS/FSR-compatible history validation later
   - avoid shimmer when NTC sample-mode uses stochastic filtering

6. Material policy
   - pick profile per material class
   - keep opacity or height separate when a render pass needs cheap single-channel access
   - expose editor diagnostics and runtime budget decisions

## Current behavior

v3.4 only estimates, validates and plans. It does not depend on RTXNTC binaries and will build on machines without NVIDIA SDK files.

`ak_ntcprobe.exe` creates a 4k 9-channel PBR texture set and reports:

- uncompressed estimate
- BCn estimate
- NTC estimate
- current foundation mode
- future Vulkan sample-mode plan

## Important limitations to keep in the architecture

- NTC is best for multi-channel material texture sets.
- Single RGB images may not justify the runtime decode complexity.
- All channels in one NTC material should be normalized to one resolution before compression.
- Runtime sample-mode decodes all or most material channels, so opacity-only/depth-only passes may still need separate compact textures.
- Stochastic filtering needs temporal reconstruction to avoid visible noise/flicker.
- Misaligned material channels can create cross-channel leakage artifacts.
- NTC must remain optional. Every project needs BCn/raw fallback paths.

## Files

- `engine/texture/include/AK/Texture/TextureSet.hpp`
- `engine/texture/src/TextureSet.cpp`
- `engine/ntc/include/AK/NTC/NeuralTextureCompression.hpp`
- `engine/ntc/src/NeuralTextureCompression.cpp`
- `tools/ntcprobe/src/main.cpp`
