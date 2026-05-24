# AK Engine v5.9 — Physics Telemetry / Determinism / Replay Foundation

## Goal

This patch adds a renderer/editor-independent validation layer for the physics runtime. The physics stack now has rigid bodies, broadphase, CCD, contacts, materials, character, vehicles, ragdolls, soft bodies, fluids, debris, damage, explosions, force fields and debug draw. The next risk is silent instability: frame budget spikes, non-finite states, event storms and nondeterministic stepping.

The new `engine/physicstelemetry` module makes those failures observable before the solver becomes more complex.

## Added module

```text
engine/physicstelemetry
```

Public API:

- `PhysicsHashConfig`
- `PhysicsFrameBudget`
- `PhysicsFrameTelemetry`
- `PhysicsReplayConfig`
- `PhysicsReplayFrame`
- `PhysicsReplayResult`
- `HashPhysicsScene()`
- `HashPhysicsStepStats()`
- `EvaluatePhysicsBudget()`
- `CapturePhysicsFrameTelemetry()`
- `RunPhysicsDeterminismReplay()`

## Deterministic scene hashing

`HashPhysicsScene()` hashes the physics scene with deterministic sorting:

- bodies sorted by body id;
- colliders sorted by body id / kind / debug name;
- events sorted by body ids and event kind;
- contact cache sorted by pair key.

Float fields are quantized before hashing so the hash is useful for gameplay replay validation without being too fragile for tiny numerical noise.

Hashed state includes:

- body kind, position, orientation, velocity, mass, damping and sleep state;
- collider kind, local transform, size, material and collision filter;
- constraints;
- contact cache;
- optional events.

## Frame telemetry

`CapturePhysicsFrameTelemetry()` records:

- hash before step;
- hash after step;
- stats hash;
- body/collider/contact/event/CCD/constraint counts;
- finite-state flag;
- budget status;
- warnings.

This is meant to feed future editor panels, automated tests, crash repros and frame captures.

## Budget validation

`EvaluatePhysicsBudget()` checks the current `PhysicsStepStats` against a configurable budget:

- maximum bodies;
- maximum colliders;
- maximum broadphase pairs;
- maximum contacts;
- maximum manifold points;
- maximum events;
- maximum CCD sweeps;
- warning count;
- finite-state requirement;
- simulated seconds per frame.

The result is one of:

```text
Ok
Warning
Critical
```

## Replay determinism foundation

`RunPhysicsDeterminismReplay()` runs the same scene twice using the same fixed delta and compares per-frame hashes:

```text
initial scene
  -> run A step 0..N
  -> run B step 0..N
  -> compare before/after/stats hashes per frame
```

This catches nondeterminism caused by unordered pair iteration, unstable event ordering, hidden random numbers or unsafe mutable state.

## Probe

New tool:

```text
ak_physicstelemetryprobe
```

It creates a small mixed physics scene:

- static floor;
- dynamic crate;
- fast CCD projectile-like body;
- query-only trigger anchor;
- capsule pendulum;
- distance constraint.

Then it captures one frame and runs a deterministic replay check.

## Current limitations

This is a foundation layer. It does not serialize full replay inputs yet. Future patches should add:

- input/event stream capture;
- binary replay file format;
- cross-platform strict hash mode;
- per-system telemetry channels for projectile, vehicle, ragdoll, soft body and debris;
- editor timeline visualization;
- CI regression baselines.
