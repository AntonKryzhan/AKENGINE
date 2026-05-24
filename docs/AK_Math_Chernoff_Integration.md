# AK Engine Math Core: Chernoff / Laplace Resolvent Layer

Source note: `2301.06765v4.pdf`, Ivan D. Remizov, arXiv:2301.06765v4.

## Engine-level idea

The paper gives a useful architecture pattern for numerical evolution systems:

```text
local generator L
    -> cheap Chernoff step S(dt)
    -> product S(t / n)^n approximates exp(tL)
    -> Laplace integral of the approximation gives resolvent (lambda I - L)^-1
```

For AK Engine this is not a replacement for normal physics, renderer or ECS. It is a reusable math layer for systems where we need a stable approximation of a variable-coefficient linear evolution operator without building a full grid solver first.

## First practical use in v0.8

`engine/math` contains a small deterministic 1D translation-based Chernoff resolvent probe:

```text
a(x) f'' + b(x) f' + c(x) f
```

The current implementation uses a finite product of translation-style steps and a truncated Laplace integral. It is intentionally small and CPU-side because the engine core is still being built.

## Where this can become useful later

- AI influence fields and cost fields with variable local diffusion/drift.
- Soft global relaxation for navigation heatmaps.
- Procedural simulation kernels that can be sampled point-by-point.
- Offline asset baking where every point/domain sample can be processed independently.
- Future GPU compute experiments, because independent point samples map naturally to parallel execution.

## Current executables

```text
ak_editor.exe     shows the Chernoff probe summary in Inspector/Console
ak_mathprobe.exe  command-line validation of the math module
```

## Guardrails

This module must remain optional and deterministic. Do not couple renderer, ECS storage or editor state to this approximation. Use it as a numerical service layer only.
