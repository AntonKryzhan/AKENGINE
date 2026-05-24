# AK Engine v4.7 — Physics Convex Collision & Manifold Accuracy Pass

This patch moves the physics core beyond AABB-only fallback contacts.

## Added

- body orientation and collider local rotation in the physics data model;
- oriented box world-shape extraction;
- capsule world-shape extraction;
- oriented bounds generation for broadphase;
- sphere vs oriented box narrowphase;
- oriented box vs oriented box SAT narrowphase;
- capsule vs sphere narrowphase;
- capsule vs capsule segment-distance narrowphase;
- capsule vs oriented box foundation;
- OBB-aware raycast;
- OBB/capsule-aware sphere sweep foundation;
- manifold generation overload that can emit multi-point contact patches for box-family contacts;
- physics stats for OBB contacts, capsule contacts, fallback contacts, manifold points and clipped manifolds.

## Why it matters

The previous physics layer could move and resolve simple bodies, but many contacts were reduced to inflated AABBs. That was acceptable for broadphase and early destruction proxies, but it is not accurate enough for characters, projectiles, tilted geometry, vehicle bodies, destructible fragments or editor collider previews.

v4.7 keeps all old sphere/AABB/proxy behavior compatible while adding the first native convex narrowphase contracts needed by future GJK/EPA, convex casts and production contact manifolds.

## Current limits

- Capsule-vs-box uses a deterministic closest-segment refinement, not a final analytic segment/OBB distance solver.
- Box contact manifolds are stable foundation patches, not full incident/reference-face clipping yet.
- Triangle mesh, heightfield triangles and convex hull colliders are still future work.
- Angular velocity/inertia tensors are not added in this patch; orientation is supported for collision shapes first.

## Next physics work

- inertia tensors and angular velocity;
- reference/incident face clipping for OBB manifolds;
- convex hull descriptors;
- GJK distance;
- EPA penetration depth;
- triangle mesh and heightfield narrowphase;
- jobified batch narrowphase.
