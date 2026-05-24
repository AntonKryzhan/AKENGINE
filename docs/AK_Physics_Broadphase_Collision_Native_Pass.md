# AK Engine v4.5 Physics Broadphase & Collision Native Pass

This pass moves the physics layer from a simple collision foundation toward a runtime-ready engine system.
It does not attempt to replace a mature production physics SDK in one patch; it establishes the native API and data contracts that later solver, CCD, GJK/EPA, and editor tooling can build on without another rewrite.

## Added

- Spatial hash broadphase with deterministic pair sorting.
- Broadphase stats: occupied cells, candidate pairs, duplicates, accepted pairs, skipped pairs.
- Capsule collider descriptor and heightfield proxy collider descriptor.
- ECS-style component contracts:
  - `RigidBodyComponent`
  - `ColliderComponent`
- Contact manifold foundation over the existing contact result.
- Physics island extraction from broadphase connectivity.
- Query API:
  - `RaycastPhysicsScene`
  - `SweepSpherePhysicsScene`
- `PhysicsSystemState` and `FixedUpdatePhysicsSystem` accumulator shell.
- Solver iteration count in `PhysicsWorldConfig`.
- Extended `ak_physicsprobe` coverage.

## Current collider quality

| Collider | Contact support | Query support | Notes |
|---|---|---|---|
| Sphere | Native sphere-sphere / sphere-AABB | Raycast + sweep via shape/bounds | Good for foundation tests |
| Box / ProxyAABB | Native AABB manifold seed | Raycast + sweep via bounds | Used by CSG destruction proxies |
| Capsule | Bounds-backed contact fallback | Sweep/raycast through broad bounds | Contract is in place; exact capsule narrowphase comes later |
| Heightfield | Proxy AABB bounds | Raycast + sweep via bounds | Contract for terrain integration |

## Next physics tasks

- Dynamic AABB tree or SAP broadphase for moving collider sets.
- Exact capsule/OBB/triangle/heightfield narrowphase.
- GJK/EPA + SAT contact generation.
- Persistent contact manifolds with warm starting.
- Continuous collision detection: capsule cast, convex cast, TOI solver.
- Constraint solver: joints, motors, limits, breakable constraints.
- Data-oriented SoA storage and jobified broadphase/narrowphase batches.
- Editor debug draw: colliders, manifolds, islands, broadphase cells, ray/sweep queries.
