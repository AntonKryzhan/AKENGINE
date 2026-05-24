# AK Engine v5.0 — Vehicle Physics Foundation

This patch adds the first native vehicle layer on top of `engine/physics` scene queries. The goal is to keep the rigid-body core clean while giving gameplay/editor systems a deterministic, data-oriented vehicle controller foundation.

## Scope

Added module:

```text
engine/vehicle
```

Added tool:

```text
ak_vehicleprobe
```

## Core contracts

The vehicle layer uses the same units and coordinate policy as the rest of AK Engine:

```text
meters
seconds
kilograms
radians internally
Y-up / right-handed world
```

The system does not copy Unity wheel colliders. Wheels are explicit data records:

```text
VehicleWheelConfig
VehicleWheelState
VehicleConfig
VehicleBodyState
VehicleControlInput
VehicleComponent
```

## Simulation model

The first implementation is a raycast/suspension vehicle foundation:

```text
wheel attach point
  -> raycast along vehicle -up
  -> suspension compression
  -> spring/damper normal force
  -> longitudinal drive/brake force
  -> lateral tire force
  -> surface friction clamp
  -> integrate body linear velocity/position
```

This is intentionally separated from final rigid-body torque and solver integration. It creates a stable API and validation target before adding wheel contact constraints, tire curves, drivetrain, gearbox and angular impulse coupling.

## Integration points

The vehicle layer already uses:

```text
PhysicsScene
RaycastPhysicsScene
PhysicsMaterialDesc dynamic friction
PhysicsQueryFlags
```

This allows vehicles to obey collision layers, trigger/query exclusions and per-surface friction from the physics material system.

## Implemented features

```text
VehicleDriveMode: FWD / RWD / AWD
VehicleWheelRole: front/rear/auxiliary
per-wheel steer/drive/brake/handbrake flags
spring + damper suspension
surface friction from raycast hit material
aerodynamic drag
rolling resistance
simple anti-roll foundation
fixed-update component shell
probe coverage
```

## Known limitations

The current layer is a foundation, not a full vehicle dynamics package. The next passes should add:

```text
wheel angular inertia
engine torque curve
gearbox/clutch/differential
tire Pacejka-like curves or brush tire model
angular body integration
contact constraints instead of pure force integration
wheel collision against non-height surfaces
ABS/TCS/ESP policy hooks
debug draw for suspension and tire forces
```
