# AK Engine Math Roadmap

This document fixes the math reading list inside the project so it is not only chat context. The goal is to turn the papers into concrete engine subsystems, one controlled layer at a time.

## Stage 1: numeric correctness layer

Use first for low-level correctness, diagnostics and deterministic behaviour:

- Floating-point rules, tolerances and replay diagnostics.
- Robust geometric predicates for orientation, intersection and degeneracy handling.
- Stable transform math for cameras, scene objects, physics poses and animation.

Planned AK modules:

```text
engine/math/core
engine/math/geometry
engine/math/transform
engine/math/tests
```

## Stage 2: collision and physics math

Use after the scene/editor core is stable:

- GJK/EPA-style convex collision queries.
- Sequential impulse rigid-body solver.
- PBD/XPBD-style constraints for ropes, cloth and soft attachments.
- Fixed/substepped simulation loop with deterministic frame boundaries.

Planned AK modules:

```text
engine/physics/collision
engine/physics/solver
engine/physics/constraints
```

## Stage 3: geometry, streaming and acceleration structures

Use for big-world rendering and editor tooling:

- Quadric-error mesh simplification for offline LODs.
- Geometry clipmaps for terrain.
- BVH/octree/k-d tree construction for culling, ray queries and broadphase.
- Later: virtualized geometry cluster hierarchy.

Planned AK modules:

```text
engine/geometry
engine/world
engine/render/visibility
engine/assets/cookers
```

## Stage 4: renderer math

Use when Vulkan/RenderGraph is in place:

- Disney/UE4-style PBR material model.
- GGX/Smith/Fresnel BRDF utilities.
- HDR lighting/probe math.
- Temporal accumulation and history validation.
- Later: reservoir sampling / ReSTIR-style light reuse.

Planned AK modules:

```text
engine/render/shading
engine/render/lighting
engine/render/temporal
engine/render/raytracing
```

## Stage 5: navigation, AI fields and Chernoff probes

Use for gameplay systems and AI tools:

- Any-angle grid pathfinding and navmesh pathfinding.
- RVO/ORCA-style local avoidance.
- Tactical cost fields.
- Chernoff/Laplace-resolvent probes for variable-coefficient influence fields and offline heatmap experiments.

Planned AK modules:

```text
engine/navigation
engine/ai
engine/math/chernoff
```

## Stage 6: animation math

Use after asset import and skeleton data exist:

- FABRIK inverse kinematics.
- Joint constraints.
- Motion blending.
- Later: motion matching.

Planned AK modules:

```text
engine/animation/skeleton
engine/animation/ik
engine/animation/blend
```

## Rule for integration

A paper is not integrated into AK Engine until it becomes one of these concrete outputs:

```text
source module
unit/integration test
editor-visible diagnostic
asset cooker step
runtime feature behind a clean API
```

Do not paste research formulas into gameplay code directly. Each mathematical method must sit behind a small deterministic API with validation, bounds and profiler markers.
