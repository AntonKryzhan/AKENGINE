# AK Engine v4.9 — Character Controller, Scene Queries & Grounding Foundation

This patch adds an engine-native kinematic character controller layer on top of the physics query API.

The controller is intentionally separate from `engine/physics`: physics owns rigid bodies, colliders, materials, layers and scene queries; `engine/character` owns player/NPC-style kinematic movement policy.

## Added systems

- `CharacterControllerConfig`
- `CharacterControllerState`
- `CharacterControllerInput`
- `CharacterGroundHit`
- `CharacterControllerResult`
- `CharacterControllerComponent`
- `FixedUpdateCharacterControllers`
- `ak_characterprobe`

## Behavior

The foundation supports:

- capsule-like controller dimensions;
- deterministic sweep-based movement;
- ground probing;
- ground snap;
- slope classification;
- wall sliding;
- ceiling hit classification;
- simple stair/step-up logic;
- collision flags;
- fixed update over multiple controllers;
- finite-state reporting: airborne, grounded, sliding.

The current implementation uses physics sphere sweeps as the stable portable foundation. A later pass can replace the internal sweep primitive with exact capsule casts once the physics core adds production-grade convex casts.

## Integration policy

The controller uses the existing physics layer/mask and trigger-aware query flags. It does not use Unity-style `CharacterController`, `GameObject`, `Rigidbody`, `MonoBehaviour` or per-frame hidden engine callbacks.

## Next steps

- exact capsule cast;
- moving platform support;
- crouch/resize validation;
- character depenetration pass;
- foot material sampling;
- editor debug draw for capsule, ground ray/sweep and collision flags.
