# AK Engine v4.6 — Physics Solver, CCD, Sleep & Constraints Foundation

This pass moves the physics layer beyond basic broadphase/contact queries and adds the first runtime-oriented stability systems required before vehicles, ragdolls, debris and high-speed projectiles.

## Added

- Continuous collision pre-pass for dynamic bodies marked `continuousCollision`.
- `SweepSpherePhysicsSceneExcludingBody()` for self-filtered CCD and gameplay queries.
- Contact cache entries for pair persistence and future warm-starting.
- Sleeping policy fields on `PhysicsBody` and `PhysicsWorldConfig`.
- Distance constraint descriptor and solver foundation.
- Constraint iteration controls in `PhysicsWorldConfig`.
- Additional step statistics for CCD, sleeping, persistent contacts and constraints.

## Design notes

The implementation stays intentionally conservative:

- CCD currently uses a sphere sweep against expanded collider bounds.
- Distance constraints operate on body positions and local anchors without angular bodies yet.
- Contact persistence stores pair identity, age, normal, point and penetration but does not yet apply accumulated impulses as warm-start data.
- Sleeping is data-driven and can be disabled per body or per world.

This is the correct API layer before adding full angular rigid bodies, contact manifolds with warm-start impulses, joints, vehicles and ragdoll constraints.

## Next steps

- Angular velocity, inertia tensors and orientation integration.
- Sequential impulse solver with warm-started normal/tangent impulses.
- Proper capsule, OBB, convex and mesh narrowphase.
- TOI-based CCD for sphere/capsule/convex casts.
- Constraint motors, limits and breakable joints.
