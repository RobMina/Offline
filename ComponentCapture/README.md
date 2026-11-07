# Experimental component capture

Schema 1 is a **software smoke recorder**, not a certified response library or a CRV geometry model.
It is opt-in through the optional Mu2eG4 common-cut tool bridge. Existing simulation configurations are unchanged.

## Implementation

- `ComponentRecorder` implements `IMu2eG4Cut` and `IMu2eG4Observer`. Constructors only read configuration; the
  existing event lifecycle owns buffers. No G4 track/step pointer survives a callback. Geometry pointers live only
  as geometry references and are re-resolved idempotently at event construction.
- `MCDataProducts::ComponentCapture` (in namespace `mu2e`) has a ROOT dictionary and is emitted as
  `g4run:componentCapture`. It contains event identifiers, geometry inventory, classifications, tracks and steps.
- `ComponentCaptureDump` exports flat `events`, `volumes`, `classifications`, `tracks`, `steps` trees under its module
  directory. It retains empty/invalid diagnostic records, checks StatusG4, and by default fails the job after
  checkpointing an invalid event. Resource-capacity exceptions abort capture rather than silently truncating it.
- `ConstructCaptureBox` makes a real finite G4 envelope in the study world's identity-placed vacuum wrappers.
  It supports a proper rotation and translation; it is not a canonical CRV module.

The common cut must be the recorder alone, with empty stepping-only and stacking-only cuts. Internal filtering must
be off. Schema 1 requires a unique box envelope, identity-placed vacuum ancestors, no replicas or parameterisations,
unit track weights, and one incident primary starting upstream in vacuum. Physical touchable ancestry determines
membership, including daughters. Envelope transforms are derived from constructed geometry, not a second input copy.

All steps are retained, including the upstream vacuum approach, zero deposits, neutral and at-rest steps.
`G4_Galactic` has finite density: its approach deposit is preserved without a zero-deposit requirement or rounding.
The actual boundary entrance conditions the component response; approach energy is not component deposition.
Outside-created children or non-vacuum approach material still invalidate the trial.
The `preInside`/`postInside` flags identify component transport. The entry is the exact post-point of the inward boundary
step, not the generator point. The first outward step records its post-state and original status/process before
returning a stop decision; Mu2e applies `fStopAndKill` only to that branch, leaving produced secondaries alive.
No energy/deposit threshold, interpolation, acceptance filter or trajectory thinning is used.

Track `reason`: 0 unfinished, 1 natural terminal candidate, 2 first-exit handoff, 3 unsupported/resource termination.
For reason 2, `end` and `status` are the pre-artificial-kill exit state; otherwise they describe the actual final
G4Track. Natural end `stepStatus` is unavailable (-1); the final step supplies that value when a step exists.
Classification is a callback log, not a birth table. Rejection, missing final IDs, reclassification, suspension,
postponement, kill-with-secondaries, outside-born children and boundary-born children are invalid for schema 1.
Boundary-born ownership is deliberately unresolved rather than assigned heuristically. Such events cannot be
silently dropped and the remaining events called an unbiased bank. The pilot must stop for investigation.

Parent closure, current-step secondary totals, sequential step numbers and closed-track census are checked. The
analyzer checks captured count against StatusG4 and its overflow/step-limit/field-propagator counters. The recorder
also flags the pinned transportation-kill predicate, excluding its own intentional exits. An invalid event is never
converted to a zero-response outcome. A failed job may lack its final product: compare completed event rows against
an independently supplied expected count, not against logs or merely the rows that survived.

## Coordinates, identities and limits

World and component-local positions/momenta are stored as doubles; units are mm, MeV/c, MeV, ns, elementary charge,
and material density g/cm3. Local-to-world rotation is a row-major 3x3 matrix; vectors rotate without translation.
Full `(run,subrun,event)` and event-local G4 track/parent IDs are signed 64-bit. These are **not** SimParticle keys or
art ProductIDs. Volume IDs index the per-event physical-volume inventory; placement strings list ancestor IDs and
copy numbers. They are diagnostic build-local IDs, not portable sensitive-detector identifiers.

`geometryTag` and `physicsTag` are labels, not verified hashes. `bankAdmissible` is always false, even for complete
smoke records. Material isotope/cut configuration hashes, nuclear/excitation and cutoff energy closure, spin state,
broader geometry validation, general boundary-born ownership and continuous-state sampling are still future work.
No deposit-equals-incoming-energy claim is made. Archive expanded FHiCL, SimpleConfig inputs, seed allocation and
source/runtime checksums alongside every eventual run; a ROOT file alone is not a provenance manifest.

## Canonical module candidate

`ConstructCaptureCRVModule` and [`fcl/crv-r1.fcl`](fcl/crv-r1.fcl) pass nominal and transformed one-event strict
audits after the verified 2026-11-06 user build. See [module construction and checks](fcl/crv-module.md).
The source-faithful v10 R1 module 0 is neither Run 1A nor extracted KPP: 64 bars, 6 FEBs, 0 placed/128 omitted
CMBs, 80 component placements (82 volume rows with vacuum ancestors). All overlap checks pass; runtime inventory
and 64 bar identities agree with the source. It does not change the recorder or production CRV constructor.

Fresh box regression: 57 tracks/116 steps, 18.541152425084263 MeV component deposit. Nominal R1: 90/202,
36.961493634894936 MeV; transformed R1: 86/199, 33.59866941329343 MeV. Each completes one strictly audited event.
Local entrance agrees to 8.6e-14 mm and 5.7e-14 MeV/c: this is one-event geometry covariance, not proof of
physics-distribution equivalence. Both R1 inventories and primary traversals are archived by the vectorized
`mu2efastsim.capture_inventory` after strict auditing; 203 combined local tests pass, including 18 inventory tests.

Standard-material debug initialization confirms the shared recipes/densities, but rejects all primaries and is
not physics capture. No pre-extraction runtime A/B exists; recipe/order review is static. The central R1 primary
misses FEBs; the separate FEB-directed fixture passes a one-event strict audit (129 tracks/272 steps) and crosses
FEBs 3/0. Its GDML records the expected electronics density/element fractions. Verified manifests, boundary-born
ownership, physics accounting and neighbor partition remain open. Existing box-smoke evidence below is retained unchanged.

## Validation gate

See [`fcl/README.md`](fcl/README.md). The user build is verified. One-event and 100-event nominal and transformed-box
runs completed and passed the companion core's strict vectorized auditor (`python3 -m mu2efastsim.capture INPUT
--expected-events 100 --output REPORT`). Nominal: 7,564 tracks/15,437 steps; transformed: 7,111/14,544. All 100 trials
in each run are retained. Local entrance states and world/local transforms pass; no physics equivalence is claimed.
The nominal sample includes 53 neutral tracks, 48 zero-deposit steps and one at-rest step. A forced one-step capacity
aborts; a stacking-only rejection reaches the common observer and produces an invalid checkpoint. The auditor rejects
both. These checks do not exhaust default-cut behavior, zero-step/suspension/postponement and resource lifecycles.
The earlier finite-density-vacuum failure remains failed evidence, not a repaired training trial.

Dedicated photon transmission and ordinary mu+ stopping/decay box fixtures pass strict audits. Consecutive stacking
rejection and internal-discard diagnostics retain three explicitly invalid events each, with no stale published rows;
the default art missing-product abort is preserved separately. See [lifecycle checks](fcl/lifecycle.md). The stricter
status auditor re-accepts eight saved positive samples without re-simulation. The verified new user build also passes
four-event reject/accept/reject/accept recovery and the 13-case [callback matrix](fcl/callback-fixture.md), each checked
against its complete predeclared census. Resume/discard at event 12 is followed by clean ID-reusing recovery at 13.
The callback matrix has no physics transport or StatusG4; all dump events remain incomplete. Both whole diagnostic
files fail ordinary audit and remain bank-inadmissible. That combined local/GPVM04 suite passed 632 tests.
The frozen [two-event engine zero-step baseline](fcl/zero-step-engine.md) passes lifecycle and exact persisted readback
but exposes a controlled-primary defect: actual 10 MeV photon endpoints became zero SimParticle end kinematics.
The subsequent [zero-step-only helper correction](fcl/zero-step-fix.md) now passes its verified full user build,
all 16 constructed helper cases and the unchanged two-event engine plus separate art readback: both finalized
endpoints retain 10 MeV, while user information remains at its original defaults. All invalid histories still fail
ordinary positive audit; none become physical-zero donors. Unchanged photon/stopping capture tables match saved
baselines exactly except CPU/real timings; that is not SimParticle endpoint equality for the positive samples.
The independent latest-cycle helper checker passes; 1217 combined Python tests pass locally and on GPVM01.
Genuine secondary scheduling, boundary ownership and internal-filter reject-then-accept remain open.
The next [engine-created secondary diagnostic](fcl/secondary-zero-step.md) is source-prepared only, behind a new user
build. It uses two separate one-event status-injection arms on unchanged stopping input, never a physical donor run.

The companion `mu2efastsim.capture_compare` reports per-event distribution diagnostics with uncertainty, not an
acceptance decision. Mean component deposits are 21.085 +/- 0.437 and 20.333 +/- 0.348 MeV for independent-seed
nominal/transformed samples. Neither this small comparison nor synthetic tests establish physics-bank completeness.

Run output/logs from a unique data directory, never the source/build area. No UVA worker or submission script is
provided until the recorder/geometry/runtime-closure gates pass. Packaging/tarballs require separate approval.
