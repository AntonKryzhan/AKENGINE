# AK Engine v5.2 — Soft Body / Cloth / XPBD Foundation

This patch adds the first deformable-physics layer without mixing cloth, ropes, or soft debris into the rigid-body solver.

## Goals

- Keep `engine/physics` focused on rigid bodies, collision queries, events, and constraints.
- Add a separate `engine/softbody` module for particle-based deformable simulation.
- Use XPBD-style constraints so stiffness is controlled by compliance and remains stable under substepping.
- Reuse `PhysicsScene` queries for collision projection against rigid geometry.
- Provide a clean component/system shell for future ECS integration.

## Added systems

- `SoftBodyParticle`
- `SoftBodyDistanceConstraint`
- `SoftBodyPinConstraint`
- `SoftBodyInstance`
- `SoftBodyConfig`
- `SoftBodyComponent`
- `MakeClothGridSoftBody()`
- `MakeRopeSoftBody()`
- `StepSoftBody()`
- `FixedUpdateSoftBodies()`

## Simulation model

The solver uses:

1. semi-implicit particle prediction;
2. XPBD distance/pin/bending constraint projection;
3. scene collision projection using sphere sweeps;
4. velocity reconstruction from corrected positions;
5. bounds rebuild after each step.

## Current scope

This is a foundation pass, not a final cloth production solver. It intentionally does not yet include:

- self-collision;
- spatial hashing for soft-body particles;
- tearing;
- aerodynamic lift/drag for cloth triangles;
- FEM/tetrahedral soft bodies;
- GPU solver path;
- skinning/render mesh attachment.

Those features should build on top of this API instead of changing the rigid physics core.

## Probe

`ak_softbodyprobe` creates:

- a pinned cloth grid;
- a pinned rope;
- a static ground collider;
- several XPBD simulation steps;
- one component-system update.

The probe validates finite state, constraints, collision-query path, bounds, and system stats.
