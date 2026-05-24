# AK Engine v1.3 Time & Tick System

AK Engine separates render frames from simulation ticks.

The previous editor loop used a visible frame counter and a sleep-based loop. That is acceptable for a temporary shell, but it is not a safe foundation for runtime simulation, physics, networking, replay, or animation. The engine now has a `GameClock` in `AK/Core/Time.hpp`.

## Problems this prevents

### Float time precision drift

Long-running gameplay must not store authoritative time as `float`. After enough elapsed seconds, small animation/cooldown/input deltas lose precision. AK uses `double` for time accumulation and `uint64_t` for frame/tick counters.

### Frame-rate dependent simulation

Simulation must not directly depend on render FPS. Render frames can be variable; gameplay/physics/network prediction need a fixed tick. AK now tracks:

```text
render frame delta     variable, measured from wall clock
fixed simulation tick  deterministic step, usually 1/60 second
interpolation alpha    render smoothing between fixed states
```

### Spiral of death

If a frame stalls for a long time, blindly running every missed physics tick can freeze the engine. `GameClock` clamps large frame deltas and limits fixed steps per frame. Excess steps are reported as dropped so the engine remains responsive.

### Replay/network instability

Replay, rollback, lockstep, deterministic AI and future multiplayer need input stamped by tick, not by arbitrary render frame time.

## Current API

```cpp
AK::GameClock clock({1.0 / 60.0, 0.25, 8});
clock.BeginFrame(realFrameDeltaSeconds);
while (clock.ConsumeFixedStep())
{
    // fixed update systems go here
}
AK::FrameTiming timing = clock.Snapshot();
```

## Current integration

The editor now displays the Time Core state in Inspector:

```text
frame
tick
frame dt
fixed dt
fixed steps
interpolation alpha
dropped steps
delta clamp
```

## Next use

The next runtime layers should use this split:

```text
Input sampling   -> frame timestamp + target tick
Gameplay update  -> fixed tick
Physics update   -> fixed tick/substep
Animation        -> fixed tick state + render interpolation
Renderer         -> variable frame with interpolation alpha
Network later    -> tick-stamped commands and snapshots
```
