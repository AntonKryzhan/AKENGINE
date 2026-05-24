# AK Engine v6.0 — Physics Anti-Tunneling / Leakage Guards

This pass hardens AK Physics against pressure tunneling and small-proxy leakage through thin or flush collider seams.

## Problem

Discrete rigid-body solvers normally detect contacts after integration. If a small dynamic body or particle proxy is squeezed by strong forces, high pressure, large substep displacement, or collider seams, it can end the step behind a barrier before the solver creates a stable non-penetration constraint.

This is distinct from classic fast-projectile tunneling: the object does not need to start extremely fast. It can be pushed through by solver correction, stacked contacts, weak contact margins, or small proxy radius relative to the timestep.

## Added policy

AK Physics now keeps contact generation predictive instead of purely reactive:

- `PhysicsCollider.contactOffset`
- `PhysicsCollider.restOffset`
- `PhysicsWorldConfig.defaultContactOffset`
- `PhysicsWorldConfig.defaultRestOffset`
- speculative/motion-expanded broadphase bounds
- static leakage guard sweeps
- static barrier post-solve projection
- max depenetration velocity clamp

## Runtime behavior

### Contact offset / rest offset

Colliders generate contacts before true geometric interpenetration when their contact skins overlap. This is critical for small particles and GPU-fluid proxy colliders because a proxy should see the wall while it is still outside the wall.

### Motion-expanded broadphase

Dynamic collider broadphase bounds are expanded from both previous and current body positions. This keeps a moved particle in candidate-pair range even when a substep displacement crosses a thin wall.

### Static leakage guard

After the normal solver pass, dynamic bodies are checked against static/kinematic barriers using a swept sphere backstop from `previousPosition` to `position`. If a crossing is detected, the body is clamped to the safe side and inward normal velocity is removed.

### Static barrier projection

A short second pass resolves remaining dynamic-vs-static contacts. This gives wall/pool boundaries priority over dynamic pressure and reduces squeeze-through at flush walls.

## New stats

`PhysicsStepStats` now reports:

- `speculativeContactCount`
- `leakageGuardSweepCount`
- `leakageGuardCorrectionCount`
- `maxPenetrationBeforeSolve`
- `maxPenetrationAfterGuard`

These counters are intentionally exposed for telemetry and debug draw. A water/metaball proxy test should track these values when dense particles press into a pool boundary.

## Limits

This is not a final CFD/fluid boundary solver. It is a rigid/proxy anti-leakage guard for the current AK Physics foundation. Future work should add:

- signed-distance-field boundary projection;
- voxel/mesh watertight boundary tests;
- particle radius/rest-offset policy per fluid material;
- jobified particle broadphase;
- persistent manifold warm starting;
- shape cast for capsule/convex bodies, not only sphere backstops.

