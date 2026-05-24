# AK Engine v3.8 — Future Limit Guards

Status: v3.8 foundation.

This stage adds explicit policy checks for engine limits that are expensive to fix after the renderer, editor and asset pipeline grow.

The previous foundation already contains generational `EntityId`, `AssetGuid`, `Result<T>`, fixed tick time, path normalization, atomic saves, resource handles, deferred resource releases, bounds, streaming budgets, reversed-Z and a RenderGraph skeleton. This patch adds the missing guard layer around the contracts that were still only documented.

## 1. Build mode policy

AK Engine now has a formal `BuildModePolicy`:

```text
Debug        — asserts, validation, profiling, editor and hot reload enabled
Development  — asserts, validation, profiling, editor and hot reload enabled
Release      — profiling/crash dumps, no editor or hot reload by default
Shipping     — no editor, no hot reload, no verbose logging, no validation dependency
```

The policy layer rejects shipping builds that accidentally keep editor-only systems, hot reload, verbose logging or validation-layer dependencies enabled.

## 2. Unit policy

The canonical runtime policy is now explicit:

```text
1 engine unit = 1 meter
time          = seconds
mass          = kilograms
angles        = radians inside math/physics
degrees       = UI/import/export boundary only
```

This prevents long-term scale drift between physics, animation, camera, imported assets and lighting.

## 3. Coordinate policy

The canonical coordinate contract is now explicit:

```text
world handedness      = right-handed
world up-axis         = Y-up
matrix policy         = column-major
clip depth            = 0..1
front face            = counter-clockwise
GPU positions         = camera-relative
renderer depth        = reversed-Z
```

The goal is not to force every importer/backend to use the same source convention. The goal is to make every conversion explicit at boundaries.

## 4. GPU synchronization policy

The renderer backend must respect:

```text
frames in flight                 = 3
deferred GPU resource release     >= frames in flight
per-frame writable GPU resources  = duplicated
hot-frame GPU readback            = forbidden
device/queue idle inside frame     = forbidden
timeline semaphore path            = preferred
async upload queues                = allowed
```

This turns a common future FPS killer into a validation rule before Vulkan/DX12 integration.

## 5. Shader permutation budget

AK Engine now has a `ShaderPermutationPlan` and budget analysis:

```text
max keywords per shader       = 16
max permutations per shader   = 1024
max cached pipelines          = 65536
fallback variant              = required
runtime shader compile in shipping = forbidden
```

The default material feature set is intentionally kept inside budget. A synthetic 20-feature binary shader is rejected by the probe to prove that permutation explosion is blocked before it reaches the pipeline cache.

## Probe

Run:

```powershell
.\build\windows-vs-debug\bin\Debug\ak_policyprobe.exe
```

Expected result:

```text
[ ok ] Future limit guards: ok ... shader_safe=256 shader_rejected=1048576
[ ok ] unsafe gpu sync rejected ...
[ ok ] unsafe units rejected ...
[ ok ] shipping runtime shader compile rejected
```

## Rule going forward

New renderer, physics, animation, asset and editor systems must consume these policies instead of hardcoding their own conventions. New exceptions are allowed only if they are represented as explicit conversion or compatibility layers.
