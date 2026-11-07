# Opt-in `outwardCylinder` cut

`outwardCylinder` implements the `fieldsteps.Envelope.leaving` predicate in
`Mu2eG4Cuts.cc`. It is independent of tracker geometry and adds no library
dependencies. No defaults or existing cuts change when it is absent.

## Configuration

Select `type: "outwardCylinder"` in an existing Mu2eG4 cut configuration. All
geometry fields are required, with no defaults:

| Field | Meaning (Mu2e coordinates, mm) | Validation |
| --- | --- | --- |
| `axisXY` | Cylinder axis `[x, y]`, parallel to Mu2e z | Exactly two finite doubles |
| `radius` | Cylinder radius | Finite and strictly positive |
| `zMin` | Lower endcap z | Finite |
| `zMax` | Upper endcap z | Finite, strictly greater than `zMin` |

For example, this illustrative cylinder can be placed in the Mu2eG4 producer's
configuration; its numbers are not geometry recommendations or defaults:

```fcl
Mu2eG4CommonCut: {
  type: "outwardCylinder"
  axisXY: [0.0, 0.0]
  radius: 1000.0
  zMin: 7000.0
  zMax: 13000.0
}
```

`Mu2eG4CommonCut` applies to both stacking and stepping. Use
`Mu2eG4SteppingOnlyCut` or `Mu2eG4StackingOnlyCut` for just one callback. To retain
existing cuts, compose this cut with them using the existing `union` or
`intersection` and `pars` mechanism instead of replacing their configuration.

An optional `write: "instanceName"` uses the existing `IOHelper` output behavior:
accepted stepping cuts call `addHit` to record the existing `StepPointMC` step
information, subject to the existing collection limit. This is not an
interpolated cylinder-crossing hit; existing pre/post-step recording and
`preSimulatedHits` handling remain unchanged. Stacking cuts do not write hits.

## Predicate and coordinate convention

For Mu2e position `(x, y, z)` and momentum direction `(ux, uy, uz)`, define
`dx = x - axisXY[0]`, `dy = y - axisXY[1]`, and `r = hypot(dx, dy)`. The cut is:

```text
(r > radius AND dx*ux + dy*uy > 0)
OR (z < zMin AND uz < 0)
OR (z > zMax AND uz > 0)
```

Every comparison is strict, with no tolerance. Equality at a side or endcap
boundary does not trigger that branch. An exterior point moving inward or
exactly tangent does not trigger that branch either. At corners, any outward
branch suffices even if the direction is inward relative to another surface.

Stepping evaluates the **post-step position and post-step momentum direction**.
Stacking evaluates the track's initial position and momentum direction. Both
subtract the `mu2eOrigin_` translation established by `finishConstruction` from
the world position before applying the predicate. Directions are unchanged;
there is no rotation or implicit detector/tracker-origin offset.

This is a **step-end predicate, not an exact intersection** algorithm. It does
not constrain a step to land on the cylinder or interpolate a crossing. A step
can overshoot, and a track outside but pointing inward is retained by this cut.
Matching the predicate alone does not imply identical stepping or transport to
fieldsteps. The original maximum-path and chord/step-count controls remain
separate; this cut neither changes nor replaces them.

## Standalone predicate test

`Mu2eG4/src/OutwardCylinderTest_main.cc` tests the shared header without Geant4, CLHEP,
art, FHiCL, or tracker dependencies. It covers interior and boundary points,
exterior inward/outward/tangent directions, corners, `nextafter` neighbors of
boundaries and zero direction components, finite-parameter validation, and a
nonzero Mu2e origin with a nonzero cylinder axis. Translation tests exercise the
same helper used by both Geant4 callbacks. They do not replace a configured
Mu2eG4 integration test of factory parsing or output products.

The executable is included in the normal SCons and CMake builds, with no external library dependency.
After the user completes the full area build and sets up that area, run:

```sh
OutwardCylinderTest
```

The test returns nonzero and prints failing case names on failure. It passed after the user build.
A separate primary-muon Geant4 pilot exercised factory parsing, stepping callbacks and the write product:
all 13 transported primaries ended via the cut with outward endpoints and one boundary hit each.
This integration check does not establish transport accuracy or identical exit positions between simulators.
