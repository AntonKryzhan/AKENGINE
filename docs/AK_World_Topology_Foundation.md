# AK Engine v3.1 World Topology Foundation

AK Engine must not assume that every world is a flat Unity-like plane. The world layer now has an explicit topology policy.

## Supported topology modes

```text
PlanarTerrain
  classic heightfield/chunked terrain
  Y-up, local gravity usually (0, -g, 0)
  useful for standard levels and Unity-like terrain workflows

SphericalPlanet
  Earth-scale or custom spherical planets
  position can be represented as geodetic latitude/longitude/altitude
  up vector is radial from the planet center
  gravity points toward the planet center
  movement can happen in local east/north tangent directions

ToroidalWrap
  Pac-Man style wrap-around maps
  leaving one edge re-enters from the opposite edge
  supports a local wrapped coordinate and an unwrapped tile counter
```

## Why this is necessary

Flat-world assumptions create future bugs:

```text
hardcoded global Y-up gravity
  breaks planet walking and spherical physics

absolute flat terrain coordinates
  do not represent Earth-sized curvature

no wrap topology layer
  Pac-Man maps become hacks in gameplay code

single terrain model
  blocks planet terrain, standard heightfields and repeated/infinite maps from sharing systems
```

## Planet mode contract

The spherical planet path uses:

```text
PlanetSurfaceConfig
GeodeticPosition
SurfaceFrame
PlanetGeodeticToCartesian
PlanetCartesianToGeodetic
BuildPlanetSurfaceFrame
MoveOnPlanetSurface
PlanetGravityAtAltitude
```

The important rule is:

```text
object up    = normalize(object_position - planet_center)
gravity      = -object_up * gravity_magnitude
movement     = east/north tangent movement on the planet surface
render input = camera-relative float after large-world conversion
```

For Earth-sized worlds the default radius is `6,371,000 m` and gravity is `9.80665 m/s^2`.

## Toroidal mode contract

The toroidal mode uses:

```text
ToroidalWorldConfig
ToroidalPosition
WrapToroidalPosition
MoveToroidal
ShortestToroidalDelta
```

The wrapped coordinate remains inside `[0,width) x [0,depth)`, while `tileX/tileZ` preserve how many times the object crossed the border.

This means gameplay can have Pac-Man wrapping without destroying large-world history.

## Standard terrain contract

The planar terrain path remains supported through:

```text
PlanarTerrainConfig
PlanarTerrainCellFromXZ
PlanarTerrainLocalInCell
BuildPlanarTerrainFrame
```

It is intended for standard terrain, chunked heightfields and future heightmap/clipmap terrain.

## Design rule

Systems must not hardcode one world shape.

```text
Physics asks topology for gravity/up.
Navigation asks topology for local frame and surface movement.
Renderer receives camera-relative coordinates.
Streaming/world partition uses spatial cells appropriate to the active topology.
Terrain tools choose planar, spherical or toroidal policy explicitly.
```
