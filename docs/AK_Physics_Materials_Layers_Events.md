# AK Engine v4.8 — Physics Materials, Layers & Events Foundation

This pass turns the v4.7 collision foundation into a more usable runtime physics layer.

## Goals

- Avoid the "everything collides with everything" trap before gameplay, projectiles, destructibles and editor picking grow.
- Move friction/restitution from body-only approximation to per-collider material mixing.
- Emit deterministic physics events so gameplay/editor/debug systems do not poll contacts manually.
- Preserve the existing broadphase, CCD, sleeping, constraints and manifold APIs.

## Added contracts

### Collision filtering

Each `PhysicsCollider` now owns a `PhysicsCollisionFilter`:

```cpp
struct PhysicsCollisionFilter
{
    u32 layerMask;
    u32 collidesWithMask;
    u32 eventMask;
    bool queryOnly;
};
```

Built-in layer bits:

- `PhysicsLayer_Default`
- `PhysicsLayer_Static`
- `PhysicsLayer_Dynamic`
- `PhysicsLayer_Character`
- `PhysicsLayer_Projectile`
- `PhysicsLayer_Destructible`
- `PhysicsLayer_Trigger`
- `PhysicsLayer_QueryOnly`
- `PhysicsLayer_All`

A pair is accepted only when both masks allow the other collider layer. This is applied before narrowphase, so filtered pairs do not spend solver work.

### Collider material mixing

Each collider now owns a sanitized `PhysicsMaterialDesc`:

- static friction;
- dynamic friction;
- restitution;
- rolling resistance;
- density;
- hardness;
- friction combine mode;
- restitution combine mode.

Contacts carry a resolved `PhysicsContactMaterial`. The solver now uses the resolved contact material instead of relying only on body friction/restitution.

### Trigger/contact events

`PhysicsScene` now owns an event queue:

```cpp
std::vector<PhysicsEvent> events;
```

Generated event kinds:

- `ContactStarted`
- `ContactStayed`
- `ContactEnded`
- `TriggerEntered`
- `TriggerStayed`
- `TriggerExited`
- `BodySlept`
- `BodyWoke`

Events are generated from the contact cache. This keeps started/stayed/ended semantics deterministic across fixed ticks.

## Probe coverage

`ak_physicsprobe` now validates:

- material pair resolution;
- trigger contacts;
- physics event queue;
- layer filtering;
- existing OBB/capsule manifold contacts;
- CCD hit path;
- constraints;
- raycast/sweep queries.

## Next physics step

The next major physics pass should move toward persistent solver data:

- warm-start impulses;
- split impulse / Baumgarte policy;
- friction anchors;
- rolling friction;
- angular velocity/inertia tensors;
- hinge/fixed/ball-socket constraints;
- collider debug draw integration in editor.
