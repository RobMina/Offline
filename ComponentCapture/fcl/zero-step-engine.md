# Real-engine zero-step finalization diagnostic

**Frozen baseline: two-event engine lifecycle and exact persisted readback pass, endpoint kinematics disagree.** This test is separate from
[the completed constructed callback matrix](callback-fixture.md). It uses the real Mu2eG4 engine and real event products,
but intentionally changes track status. It is not passive observation, physical transport evidence or a response donor.
In this measured baseline, ComponentRecorder, the observer bridge, SimParticle schema, helper implementation and
production defaults were unchanged. The subsequent [zero-step-only helper correction](zero-step-fix.md) now passes
these unchanged engine/readback inputs: both finalized endpoints retain 10 MeV. The baseline evidence below remains
frozen and is not a corrected-run result; correction results and limits are recorded separately.

## Question and source prediction

Mu2eG4 saves SimParticle start information before calling the common-cut observer's `beginTrack`; it calls `endTrack`
before `saveSimParticleEnd`. A successful recorder callback therefore does not prove successful or correct SimParticle
finalization. Mu2eG4UserTrackInformation starts with KE/direction zero and times -1. Mu2eRecorderProcess normally fills
these values during stepping, while the end-KE/momentum helpers read them directly. A positive-energy zero-step track
may therefore serialize zero end KE/momentum although its actual G4Track retains the incoming energy. This is a
source-based prediction, **now measured in both controlled primary cases below**. The test does not initialize that information to hide it.

The pinned Geant4 11.3.2 installed `G4Step.icc::InitializeStep` copies the track state into the pre-point, clears its
process, sets undefined step status, then copies pre to post. Reference v11.3.2 tracking code assigns the engine step
to the track before the stepping loop and calls post-tracking even when that loop executes zero times. Consequently,
zero steps need not mean a null step. The fixture observes pointer/point presence independently at every callback and
does not attach a step, set a counter, force a process code or call tracking actions itself. Installed runtime/header
hashes and build metadata are retained separately; the measurements below confirm these expectations for both cases.

## Fixed two-event intervention

`zero-step-engine.fcl` inherits the ordinary photon box fixture: one 10 MeV/c photon at `(0,0,-50.001)` mm in vacuum,
pointing along +z, with no position/momentum/time spread. Run 15, subrun 0, events 1/2, seed base 315259, one art
thread/schedule, no internal filtering, no trajectories, no added processes and unchanged ShieldingM physics.

A new top-level common tool `ComponentZeroStepProbe` loads the unchanged ComponentRecorder and forwards its product,
construction, event, cut and observer interfaces. Both plugins require `diagnosticOnly:true`; the proxy also checks the
expected run, consecutive IDs, single engine-assigned primary and fixed photon state. Only the proxy mutates status:

| Event | Injection | Recorder classification | Recorder begin | Actual final status |
|---:|---|---:|---:|---:|
| 1 | During classification, before forwarding it | stop-and-kill (2) | stop-and-kill (2) | stop-and-kill (2) |
| 2 | After forwarding beginTrack | alive (0) | alive (0) | stop-and-kill (2) |

There is exactly one alive-to-stop-and-kill mutation per event, through a diagnostic-only const-cast of the mutable
engine-owned G4Track. The cut decision stays false: Mu2eG4StackingAction therefore returns `fUrgent`, not `fKill`.
This return is source-derived from the recorded decision, not a separately observed stack-manager return value.
Positive energy avoids Geant4's zero-energy conversion to stop-but-alive. No child generation or seed search is used.

Expected engine sequence per event: one classification, one begin, no steps, one end, then actual SimParticle
finalization and event publication. Any additional callback or step is checkpointed and fails explicitly. The fixed
census is independently required by the Python checker; exiting early cannot be mistaken for two successful trials.

## Independent evidence streams

1. **`g4run/zeroStepCallbacks`:** the proxy lazily books a TFileService tree during the active serial event, never in
   declaration-only constructors. Six rows are expected, with full int64 identities, phase/index, incoming/forwarded/
   outgoing status, killed-valid/killed flags, mutation/forwarding flags, current step number/length, G4 version,
   actual track state, guarded pre/post-step states/processes and guarded Mu2e-user-information values. No track or
   step pointers survive a callback. Absent fields use explicit presence flags, not inferred zero physics.
   Each callback is autosaved, including exceptions before rethrow. That aids diagnosis, not guaranteed crash durability.
2. **`dump/*`:** the unchanged ComponentCaptureDump retains all real recorder and StatusG4 information with
   `failOnInvalid:false`. Expected record error is `track_never_entered_component`: the primary was stopped upstream.
   This is intentionally incomplete capture, never physical zero response. Both StatusG4 and capture must actually exist.
3. **`finalization/zeroStepEvents` and `zeroStepSims`:** a new ComponentZeroStepDump analyzer exports presence/counts
   for every event and every actual SimParticle, without filtering or endpoint-value assertions. It records keys,
   ancestry, creation/stopping codes, endDefined, step count/length, statuses, positions, four-momenta, KE and times.
   Float-backed SimParticle values are widened for output, not promoted to original G4 precision. Missing products remain
   visible. A predicted endpoint discrepancy does not abort event 1 and prevent event 2 from being written.
4. **`component-zero-step.art`:** RootOutput retains all real event products with `keep *`, no selection or compression
   module. `zero-step-readback.fcl` reads every persisted event without running Geant4 and dumps the same capture/status/
   SimParticle fields, using explicit original-process tags. The checker requires exact field equality under full-int64
   row joins between original and readback files, not merely matching event counts or endpoint energies.

The callback tree's module directory and actual zero-step finalization are runtime acceptance checks, not established
by compilation. A failed/crashed/partial job must be preserved and rejected, without inventing missing final products.

## Checking and interpretation

After the full user build is verified, run in a fresh data directory, never the app/source tree:

```bash
mu2e -c Offline/ComponentCapture/fcl/zero-step-engine.fcl
mu2e -c Offline/ComponentCapture/fcl/zero-step-readback.fcl
python3 -m mu2efastsim.capture_zero_step component-zero-step.root \
  --readback component-zero-step-readback.root --run 15 --output zero-step-report.json
```

Set up the capture workDir before entering the data directory and abort on a mismatched MUSE_WORK_DIR. Run the commands
separately, record setup/resource/exit logs, and do not run readback until the engine job has finished. Preserve all
failures. The checker reads highest ROOT cycles, requires both complete event censuses and exact diagnostic contracts,
and requires ordinary capture auditing to reject the whole input. Missing histories are not silently dropped.

Lifecycle success and endpoint agreement are separate report fields. End KE/momentum/energy are measured targets:
zero, unchanged or another finite discrepancy is reported without being tuned away. Nonfinite/malformed products or
readback discrepancies fail the diagnostic. The expected source signature is end KE/energy/momentum zero with a
10 MeV photon still in the captured endpoint. A matching endpoint would instead refute that prediction for these cases.
Neither result establishes energy conservation for physical showers or admits a response bank.
The prepared checker has 281 synthetic regression tests; the combined local suite passes 913 tests. These numbers
validate Python contracts, exact readback comparison, highest cycles and failure handling, not compiled C++ or engine execution.

## Measured result (2026-11-06)

The successful full user rebuild was verified from its marker, source/plugin timestamps and hashes, resolved libraries
and the dump plugin's explicit DataProducts dependency. Engine and separate RootInput jobs both exited 0 on an idle
GPVM, and the unchanged diagnostic checker exited 0. Actual census: two events, two tracks, zero steps, two
classifications, six volume rows, six callback rows and two finalized SimParticles. Both StatusG4 records are clean.

| Quantity, both events | G4Track / initialized step at end | Finalized and reread SimParticle |
|---|---:|---:|
| Kinetic energy (MeV) | 10 | 0 |
| Total energy (MeV) | 10 | 0 |
| Momentum z (MeV/c) | 10 | 0 |
| Position z (mm) | -50.001 | -50.001 |
| Global/proper time (ns) | 0 / 0 | 0 / 0 |

The callback step pointer is absent at classification and beginTrack, present at endTrack. Both initialized step
points retain the photon state, undefined status 7 and no defining process. Mu2e user information is absent at
classification and present at begin/end, retaining KE/direction zero, times -1 and an uninitialized code. No stepping
callback occurred. Finalized SimParticles have zero steps/length, endDefined true and stopping code NotSpecified.
This confirms an endpoint-finalization discrepancy for these interventions, not a null-step crash or a measurement
of how frequently physical secondary tracks encounter it. The source mechanism is the unconditional end-KE/momentum
read from information that Mu2eRecorderProcess did not populate; no helper correction was made in this experiment.

The report records `engine_lifecycle_passed:true`, `readback_identical:true`, `endpoint_kinematics_match:false`, with
predicted-zero endpoints for both cases. Every exported capture/status/SimParticle field agrees exactly after art
persistence, not only the discrepant energies. Both ordinary-audit CLI checks exit 1 with `event complete is not true`
and create no success report. The upstream `track_never_entered_component` errors remain invalid histories, never
physical-zero trials. All records remain diagnostic-only and bank-inadmissible.

The 913-test combined suite passes locally and on the GPVM (281 zero-step synthetic tests); no checker changes were
needed for the real output. Measured engine wall time is 8.20 s, maximum RSS 525320 KiB; readback 1.47 s and 448764 KiB.
These are diagnostic job measurements, not campaign extrapolations. Preserve original root/art/readback, report,
expanded configs, setup/resource/exit logs and build/source/runtime hashes together; do not overwrite prior failures.

## Build and scope

CMake registers the two new plugins; SCons `make_plugins` discovers their ordinary source filenames. Both plugin
library lists explicitly include DataProducts (`Offline::DataProducts` / `mu2e_DataProducts`). SimParticle's inline
start/end position and momentum accessors call GenVector conversions implemented in `DataProducts/src/GenVector.cc`.
The initial user build failed linking ComponentZeroStepDump with undefined `GenVector::Hep3Vec(XYZVectorD const&)`
and `GenVector::HepLorentzVec(XYZTVectorF const&)`; both symbols were verified in the area's DataProducts shared library.
SCons dependencies are not transitive, so MCDataProducts alone does not supply this direct dependency. The earlier
no-new-dependency statement was incorrect. The fix only adds that library to both build configurations; no analyzer,
recorder, helper, dictionary or product schema changes are made. The full user rebuild subsequently succeeded and was
verified before runtime; the partial failed build was never used. Future C++ changes require another full user build.
No build is launched automatically or embedded in a driver.

This tests primary scheduling and finalization under two explicit artificial interventions. It does not test genuine
secondary production/scheduling, boundary-born ownership, resumed tracks, a physical kill mechanism, memory leaks or
unbiased physics-bank capture. Existing completed callback/recovery evidence and all failed fixtures remain unchanged.
