# AK Engine v5.1 — Ragdoll & Animation Physics Bridge Foundation

This pass introduces a native ragdoll bridge that keeps animation, gameplay hit reactions and physics constraints separated.

## Goals

- Represent a humanoid ragdoll as physics bodies and colliders.
- Keep the physics core generic: ragdoll logic lives in `engine/ragdoll`.
- Instantiate a ragdoll into `PhysicsScene` using dynamic capsule/box bodies.
- Generate distance-constraint foundations for pelvis/spine/head/limbs.
- Extract physics poses for animation write-back.
- Drive bodies toward animation poses for partial ragdoll / hit reaction blending.
- Apply impulse hit reactions from projectile/melee/explosion systems.
- Preserve collision layers/materials and physics events.

## Current scope

The implementation is a foundation, not a final production ragdoll solver.

Implemented:

- `RagdollDescription`
- `RagdollBoneDesc`
- `RagdollJointDesc`
- `RagdollConfig`
- `RagdollInstance`
- `RagdollPoseSample`
- `RagdollHitReaction`
- humanoid ragdoll description
- physics-scene instantiation
- dynamic capsule and box colliders
- distance-constraint skeleton foundation
- animation-drive velocity bridge
- physics pose extraction
- impulse hit reactions
- `ak_ragdollprobe`

Not final yet:

- angular cone/twist solver
- joint motors
- ragdoll self-collision groups
- per-limb damage routing
- blend tree integration
- editor bone/collider gizmos
- network replication
- GPU/parallel ragdoll batching

## Runtime pipeline

```text
Animation pose / gameplay hit
        ↓
Ragdoll bridge
        ↓
Physics bodies + colliders + constraints
        ↓
Physics step
        ↓
Pose extraction
        ↓
Animation write-back / gameplay events
```

## Design notes

The ragdoll module depends on `AK::Physics`, but physics does not depend on ragdoll. This keeps the solver reusable for vehicles, characters, destruction debris and projectiles.
