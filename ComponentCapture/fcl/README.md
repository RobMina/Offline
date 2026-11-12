# Finite-box smoke configuration

**Experimental software fixture, not a physics bank.** User build and plugin/product loading are verified.
One-event and 100-event nominal and translated/rotated runs pass strict record audits. All trials are retained;
geometry overlap checks pass. Forced recorder overflow and stacking rejection fail explicitly, as intended.

## Wiring

`smoke.fcl` follows the pinned `Mu2eG4/g4study/g4study_Box.fcl` pattern, with
100 `EmptyEvent` events, serial `Mu2eG4`, one art thread/schedule, and fixed
`SeedService` auto-increment base seed 314159. The generator inherits `MixMaxRng`;
serial Mu2eG4 requests the framework `G4Engine` kind, also seeded by SeedService.
`generate -> g4run` has no filter; the analyzer runs on the all-event end path.
There is no status filter, SD-hit selection, trajectory dependency, compression,
or output-event selection. Only `TFileService` output is requested, in
`component-capture-smoke.root`; no art event file is written.

The sole common cut is `ComponentRecorder`, with envelope `CaptureEnvelope`,
1,000,000 recorded-step and 100,000 recorded-track caps, and the tags in
`prolog.fcl`. Stepping-only and stacking-only cuts are empty. The recorder kills only
branches at their first outward exit and emits a product even for invalid captures.
`ComponentCaptureDump` consumes `g4run:componentCapture`. Transport resource limits
are also finite safety limits, not physics cuts or a guarantee of complete transport;
truncation/resource failures must be inspected, never filtered out of this smoke sample.
The pinned internal-filter key is deliberately spelled `G4InteralFiltering`.

Physics is `ShieldingM`, 0.01 mm default production range cut, 0 mm proton
production cut, radioactive decay explicitly off, thermal-neutron extension off,
no extra processes, and no user step limiter. Ordinary particle decays remain on.
Other physics settings come from the pinned `mu2eg4DefaultPhysics`; the tag is a
human-readable label, not a configuration hash. Physics-list/data availability
and exact reproducibility depend on the pinned Geant4/Offline environment.

## Geometry and coordinates

`GeometryService.inputFile` loads `geom/smoke-box.txt`, which uses SimpleConfig
`#include "Offline/ComponentCapture/geom/box.txt"`. This is not a FHiCL include.
The geometry tool follows `ConstructEnvBox`: this checkout's study-world interface
is **`InitEnvToolBase`**, not `IWorldConstruct` (absent from the pinned interfaces).
`Mu2eStudyWorld` makes `World` and its `BoxInTheWorld` daughter from `G4_Galactic`;
this tool nests one real `G4Box` solid/logical/physical volume named
`CaptureEnvelope` inside that vacuum. It is not a CRV module.

The default component has half-lengths (50,50,50) mm and NIST material
`G4_POLYSTYRENE`. Configurable `captureBox.translation` is in world mm, and
`captureBox.localToWorld` is a row-major 3x3 active rotation **R**:

```
x_world = R * x_local + translation
p_world = R * p_local
x_local = transpose(R) * (x_world - translation)
```

The tool checks finite values, positive dimensions, orthonormality/determinant +1
(to 1e-10), and strict containment within the 500 mm half-length vacuum parent.
Geant4 placement receives the passive inverse rotation with registry-managed
lifetime. The parent must be the unrotated origin-centred study-world box.
`Mu2eStudyWorld::constructSDandField()` installs no field or SDs; the inherited
`bFieldFile` is parsed by GeometryService but no standard-detector field is built.
No field macro is scheduled. Surface-overlap checking is enabled.

## Deterministic incoming state

Every event generates one PDG 13 muon with p=(0,0,1000) MeV/c at t=0 ns,
x=(0,0,-50.001) mm. Generation has no position/momentum/time spread. The source
is 0.001 mm (1 micrometre) upstream in vacuum rather than on the ambiguous G4
surface. In the supplied identity placement the intended inward crossing is
x=(0,0,-50) mm with local direction +z. The recorder must use the actual boundary
step point, not the upstream generator position, to report entry. Surface location
is exact only to G4 navigation tolerance; runtime checks must confirm the gap
exceeds that tolerance and the entry lies on the face. Vacuum flight adds a small
time increment; decay/rare vacuum interactions are not mathematically excluded.

When changing dimensions or placement, update `smoke-primary.txt` explicitly:
start at `R*(0,0,-halfZ-0.001)+translation`, with direction `R*(0,0,1)` and unchanged
momentum magnitude. Convert that direction to fixed `czmin=czmax` and
`phimin=phimax` (radians). Generator and geometry files are not automatically tied.
Change the geometry/physics tags whenever their meaning changes.

## Additional reproducible fixtures

`smoke-rotated.fcl` uses run 2, seed base 314259 (nominal base 314159), translation (123,-87,41) mm and
`R = [[0.48,-0.8,0.36],[0.64,0.6,0.48],[-0.6,0,0.8]]`. Its separate geometry and generator inputs give the same
local incident state. Local entrance position/momentum agree with nominal to 4e-15 mm/2e-13 MeV/c in the tested
samples. This is a transform check, not statistical equivalence certification or a complete seed manifest.

`negative-capacity.fcl` limits the recorder to one step and must abort on the next callback. No completed event
is published; strict audit must reject the missing expected trial. `negative-stacking.fcl` rejects the primary
with the built-in constant stacking-only cut. The common observer must still record the rejection and mark the
event invalid; the analyzer checkpoints it then fails. Both were exercised and strict audits reject their output.
Run every fixture from a fresh data directory; their relative output filename is intentionally the same.

## Remaining integration gates

- Exhaust default cuts, internal-filter reject-then-accept recovery and real-engine zero-step/suspension scheduling.
- Broader module neutral/stopping/passive coverage and other resource failures. Boundary-born ownership, ancestry
  links, the manifest and the neighbour partition are described in [ownership-accounting.md](ownership-accounting.md).
- Larger independent-placement distributions with predeclared equivalence criteria; the 100-event comparison is diagnostic only.
- Complete material/physics/runtime manifests; canonical module construction now has bounded runtime checks.

Consecutive stacking rejection and internal-discard diagnostics now pass their negative-fixture checker;
[full lifecycle evidence and remaining gaps](lifecycle.md) distinguishes invalid/missing captures from physical zeros.
The verified new build also passes [four-event stacking recovery and all 13 constructed callback cases](callback-fixture.md).
The ordinary auditor rejects both whole diagnostic files; 632 combined tests pass locally and on the GPVM. Constructed
callbacks are not physics transport or proof of engine scheduling/zero-step finalization.
The frozen [two-event real-engine baseline](zero-step-engine.md) passes lifecycle and exact art readback but measures
zero SimParticle end kinematics versus actual 10 MeV photon endpoints. The later [zero-step-only helper fix](zero-step-fix.md)
passes its user build, 16 constructed helper cases, unchanged engine/readback with corrected 10 MeV endpoints, and
exact stepped photon/stopping capture regressions against saved baselines (timing values excluded). Both invalid
engine histories remain rejected. Independent helper-tree validation and 1217 local/GPVM tests pass; no bank admission.
The [next secondary zero-step diagnostic](secondary-zero-step.md) is source-prepared only. Its two separate one-event
arms reuse the stopping input/seed and inject terminal status on every engine-created child; another USER build is
required. Structural completeness in that deliberately modified history must not be called a physical response.

## Neutral transmission and stopping checks

Fixed one-event fixtures `smoke-photon.fcl` (PDG22, p10 MeV/c, run9/seed314759) and
`smoke-stopping.fcl` (PDG-13, p20 MeV/c, run10/seed314859) use the same upstream box entrance.
Both complete and pass strict full-record audits. The photon transmits with one track, two zero-deposit steps,
one first exit and zero component deposit; this physical zero remains a completed trial. The positive muon
stops and decays naturally: nine primary steps including one at-rest step, stopProcess Decay; the full event
retains 69 tracks/138 steps, 68 children, six first exits and 63 natural ends. Component deposition is
21.284270872713662 MeV. Muon decay releases rest energy, so incident kinetic energy alone is not an energy budget.
No forced interaction or seed search is used. These checks do not exhaust zero-step/suspension/postponement,
rejected-event cleanup or boundary-born ownership, and all records remain bank-inadmissible.
