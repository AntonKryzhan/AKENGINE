# AK Engine v5.5 — Damage / Health / Breakage Runtime Foundation

This patch introduces the first engine-level runtime damage pipeline. The goal is to prevent projectile, debris, fluid, collision, destructible and gameplay damage from becoming separate incompatible systems.

## Scope

`engine/damage` provides:

- `DamageableComponent` for health, structural integrity, heat and breakage state.
- `DamageResistanceProfile` for material-driven mitigation.
- `DamageEvent` as a unified event format for projectiles, collisions, debris, explosions and damaging fluids.
- `DamageSystem` queue and result buffers.
- `ApplyDamageEvent()` and `ProcessDamageEvents()`.
- Physics wake-up on damage.
- Fracture/destroy/ignite/melt response classification.
- Optional debris emission through `engine/debris` when structural damage crosses fracture thresholds.

## Integration points

Current adapters:

```text
ProjectileImpactResult -> DamageEvent
PhysicsEvent           -> DamageEvent
DebrisParticle         -> DamageEvent
FluidColliderInteraction -> DamageEvent
Explosion params       -> DamageEvent
```

The system is deliberately separate from the rigid-body solver. Physics produces impulses/events; damage consumes them and decides what gameplay/destruction response should occur.

## Current limitations

This is a runtime foundation, not a final material fracture solver. Next passes should add:

- ECS storage and scene serialization for damageable components.
- Per-region damage and weak points.
- Layered armor and composite material stacks.
- Spall/fragment event generation.
- Better explosion occlusion and falloff queries.
- Direct WorldPartition dirty-cell propagation.
- Editor visualization for health, structure and heat.
