# AK Engine v2.9 Project Settings Foundation

This patch introduces a single project settings contract for units, coordinate conventions, runtime budgets and build profile.

## Why this exists

A game engine must not let every subsystem invent its own conventions. Physics, renderer, animation, importers and editor tools must agree on:

- meters / seconds / kilograms as runtime units;
- radians internally and degrees only in UI/editor presentation;
- right-handed, Y-up world coordinates;
- camera-relative rendering for large worlds;
- reversed-Z depth policy for large far/near ranges;
- fixed simulation timestep and frame clamp policy;
- streaming and frames-in-flight budgets.

Without this layer, bugs appear much later as subtle incompatibilities: imported assets have the wrong scale, physics works in different units than rendering, culling assumes a different handedness, and shader pipelines use a different clip-space convention.

## Added API

```text
ProjectSettings
UnitsPolicy
CoordinateConvention
RuntimePolicy
SettingsValidationReport
SerializeProjectSettingsText
ParseProjectSettingsText
SaveProjectSettingsAtomic
LoadProjectSettings
BuildSettingsProbe
```

## Current default policy

```text
1 unit = 1 meter
1 time unit = 1 second
1 mass unit = 1 kilogram
radians internally, degrees in UI
right-handed world
Y-up
column-major math convention
Vulkan 0..1 clip space policy
counter-clockwise front face
camera-relative rendering enabled
reversed-Z enabled
fixed timestep = 1/60 s
max frame delta = 0.25 s
max fixed steps per frame = 8
max frames in flight = 2
```

## Rule

Subsystems should read policy from ProjectSettings or a derived runtime config. New hardcoded coordinate/unit policies should not be added to renderer, physics, asset importers or editor tools.
