# Capture lifecycle checks

These are software rejection/reset checks, not response-library trials. The recorder and analyzer binaries are
unchanged from the verified canonical-module build. Positive capture keeps internal filtering off and the analyzer's
`failOnInvalid` default true. No production configuration is changed by the negative fixtures below.

## Consecutive stacking rejection

`negative-stacking-reset.fcl` requests three events, run 11, seed base 314959, with the existing constant stacking-only
rejection and `dump.failOnInvalid: false`. This permits all invalid records to be inspected; it does not mark them
complete. Runtime completed all three events. Each has zero tracks/steps, one killed classification with event-local
track ID 1, three volume rows, and exactly these recorder errors:

- `classification_rejection`
- `expected_one_incident_primary`
- `classification_tracking_census_mismatch`

There is no accumulated classification/error state or `reclassification_unsupported` in later events. All three retain
matching product/framework identities and remain incomplete. The ordinary strict capture audit rejects the file.

## Internally discarded events

`negative-internal-filter.fcl` requests three events, run 12, seed base 315059. It enables `G4InteralFiltering` in the
box with no sensitive detectors, so no event can satisfy the SD filters. Source inspection shows the rejected-event
path calls `Mu2eG4PerThreadStorage::clearData()`, including `commonCuts->deleteCutsData()`, instead of publishing products.

The default art producer contract aborts on event 1: declared SimParticle, StatusG4 and ComponentCapture products were
not put. The run exits 1 and the strict audit rejects the missing expected census. This failure is retained.

`negative-internal-filter-reset.fcl` adds only the framework's `g4run.errorOnFailureToPut: false` diagnostic override.
With `dump.failOnInvalid: false` inherited, the analyzer can record three consecutive missing-product events. Runtime
completed all three: zero tracks, steps, classifications and volumes; false capture/status-presence and completion
flags; empty geometry metadata; default record identities 0 and StatusG4 status 2147483647. Each row explicitly reports
missing ComponentCapture and StatusG4. These missing histories are **not physical zero responses**.

The consecutive discard path runs without an exception or stale published rows. This is not an allocation/leak
measurement and does not prove reject-then-accept recovery on the same recorder instance; that remains a separate test.

## Independent negative-fixture checker

From the companion core package:

```bash
python3 -m mu2efastsim.capture_lifecycle INPUT --kind stacking-rejection --run 11 --expected-events 3 --output REPORT
python3 -m mu2efastsim.capture_lifecycle INPUT --kind internal-filter --run 12 --expected-events 3 --output REPORT
```

The checker requires the supplied full integer event census (default subrun 0, first event 1), per-event row counts,
known invalid flags/errors and clean reset signatures. It also requires the ordinary capture audit to reject the
input. A passing report means only `negative_fixture_passed=true`, with `diagnostic_only=true` and
`bank_admissible=false`. It cannot accept the positive photon zero-response fixture as a negative fixture. Missing,
duplicate, stale or mismatched rows fail before a report is written; highest ROOT cycles and int64 identities are used.

## Status and boundary review

The strict positive auditor now checks classification statuses against the pinned Geant4 11.3.2 enumeration. Allowed
initial statuses are alive (0), stop-but-alive (1), or stop-and-kill (2) only for a zero-step child. Every nonfinal step
must be alive or stop-but-alive, preserving ordinary at-rest physics. Kill-with-secondaries, suspend, suspend-and-wait,
postpone, unknown statuses and premature terminal classifications fail. These independent checks supplement, rather
than replace, the recorder's duplicate/resumed-track and final-status checks. Synthetic mutation tests and the
constructed-object runtime matrix cover these contracts; real engine scheduling and zero-step finalization remain open.

Boundary-born-secondary ownership remains unresolved. The recorder rejects births for which the G4 solid reports
`kSurface`; the independent auditor conservatively requires births to lie more than its configured `atol` inside every
box face. This numerical exclusion can be wider than G4 surface tolerance. Exact-face, just-inside and just-outside
synthetic tests under a nontrivial transform lock in that conservative behavior. An audit failure stops the sample;
never drop such a trial or enlarge tolerances to obtain acceptance. This is not an ownership prescription.

The [callback/recovery fixtures](callback-fixture.md) now pass after their verified user build. Four real Mu2eG4
trials follow the predeclared reject/accept/reject/accept sequence, with exact rejection signatures and strict positive
audits on both accepted events. This establishes recovery after publishing an invalid record, not an internal-filter
reject-then-accept sequence. No accepted subset is exported and the whole mixed input remains invalid.

All 13 constructed-object cases also pass: children at exact parent exit endpoints with both directions/touchable
histories, interior zero-step controls, unsupported terminal statuses, reclassification and resume rejection. The
explicitly discarded event 12 leaves no capture rows; clean event 13 reuses the IDs without stale state. These are
real recorder callbacks on constructed G4 objects, not engine-produced showers or proof of engine scheduling.
Boundary ownership, genuine secondary scheduling and additional resource/default-cut paths remain open.
The frozen [real-engine zero-step baseline](zero-step-engine.md) passes two controlled primary lifecycles and exact
persisted readback but exposes zero finalized kinematics for actual 10 MeV photon endpoints. The subsequent
[zero-step-only helper correction](zero-step-fix.md) passes 16 constructed helper cases, unchanged primary engine/readback
with corrected endpoints and exact stepped capture regressions; all invalid histories remain invalid. The engine supplies
a step at endTrack, not at beginTrack. This does not establish genuine secondary scheduling. A separate
[engine-created secondary diagnostic](secondary-zero-step.md) is now source-prepared; another USER build and runtime
are required. Its deliberate terminal-status injection is not physical response even if structural capture is complete.

## Regression evidence

The earlier combined suite passed 373 tests locally and on GPVM04; callback/recovery and explicit ragged-metadata
rejection regressions brought the capture/inventory/lifecycle/callback/replay/saved-record/site suite to 632
passing tests in both environments. Adding 281 zero-step tests brings it to 913 passing locally and on GPVM01.
The unchanged auditor also passes all eight existing positive files: nominal/transformed 100-event boxes, the fresh
post-module box, nominal/transformed/FEB module events, and the photon/stopping box events. They were re-audited,
not re-simulated. All remain bank-inadmissible; broader module neutral/stopping coverage and complete manifests are open.
