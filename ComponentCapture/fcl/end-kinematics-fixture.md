# Constructed end-kinematics helper fixture

**Status: full USER build verified; one-event art job and independent latest-cycle ROOT check pass (2026-11-06).**
All 16 cases pass the C++ finite per-double byte comparison and endJob census. The Python checker independently
verifies the full ordered matrix and stored equality; this remains a constructed helper test, not transport.

`end-kinematics-fixture.fcl` runs the serial `ComponentEndKinematicsFixture` EDAnalyzer as `fixture`, with
one EmptyEvent: **run 16, subrun 0, event 1**. The typed configuration requires `diagnosticOnly:true` and
accepts only `expectedRun:16` (also its default). The analyzer rejects an unexpected identity or extra event;
`endJob` requires one seen event, exactly one passing occurrence of every fixed case, and exactly 16 tree rows.
No event products are consumed or produced. There is no generator, Mu2eG4 engine, run manager, geometry,
random-number use, secondary creation, or fabricated SimParticle/StatusG4/capture product.

## Narrow contract

The fixture calls the actual `Mu2eG4UserHelpers::getEndKE` and `getEndMomentum` on deliberately constructed
G4Track/G4DynamicParticle/G4Step and Mu2eG4UserTrackInformation objects. It tests only this split:

- If the **actual `GetCurrentStepNumber()` is zero**, expect `GetKineticEnergy()` and
  `CLHEP::HepLorentzVector(track.GetMomentum(), track.GetTotalEnergy())`, independently of user-information
  presence/content or step-pointer presence. Dynamic mass therefore matters for this branch.
- For a stepped track, retain the recorded KE/direction and the **particle-definition mass**. The expectation
  copies the legacy operation order exactly: `e = ke + mass`, then
  `sqrtOrThrow<double>(e*e-mass*mass, std::numeric_limits<double>::epsilon())` when definition mass exceeds
  epsilon; otherwise `e = p = ke`. The return is `CLHEP::HepLorentzVector(p*pdir,e)`.
  It is not replaced by raw post-process track kinematics, dynamic mass, a zero-info fallback, or a
  rearranged square-root expression. Stepped cases always have valid Mu2e user information and an attached step.

KE and all four momentum/energy components must be finite and compare byte-for-byte per double, including signed
zero, not with a tolerance or float conversion. No padded structure is compared. No time helpers are called.

## Exact fixed matrix: 16 cases

All raw momentum directions are `(0,0,1)`. A `recorded` object explicitly holds direction `(0.6,0.8,0)`,
different from the raw direction even when raw momentum is zero. A `default` object is left at constructor
KE 0 and direction `(0,0,0)`; `absent` means no user information. Stale zero-step recorded KE is 9 MeV.
KE/mass units are Geant4's MeV convention; lengths are mm. Default dynamic masses equal particle-definition
masses; the two override cases set **dynamic mass to 2.5 MeV without modifying the definition**.

| Index / exact case name | Particle | Raw KE | Information / KE | Step attached | Actual steps | Track length | Expected source |
|---|---|---:|---|---|---:|---:|---|
| 1 `zero_gamma_default_step` | gamma | 10 | default / 0 | yes | 0 | 0 | raw G4 track |
| 2 `zero_gamma_absent_no_step` | gamma | 10 | absent | no | 0 | 0 | raw G4 track |
| 3 `zero_gamma_stale_no_step` | gamma | 10 | recorded / 9 | no | 0 | 0 | raw G4 track |
| 4 `zero_electron_absent_step` | electron | 10 | absent | yes | 0 | 0 | raw G4 track |
| 5 `zero_electron_default_no_step` | electron | 10 | default / 0 | no | 0 | 0 | raw G4 track |
| 6 `zero_electron_stale_step` | electron | 10 | recorded / 9 | yes | 0 | 0 | raw G4 track |
| 7 `zero_electron_at_rest_step` | electron | 0 | default / 0 | yes | 0 | 0 | raw: KE/p zero, E = mass |
| 8 `zero_electron_dynamic_mass` | electron | 10 | recorded / 9 | no | 0 | 0 | raw G4 track, dynamic mass 2.5 |
| 9 `one_gamma_raw_zero_recorded_7` | gamma | 0 | recorded / 7 | yes | 1 | 0 | legacy recorded state |
| 10 `one_electron_raw_zero_recorded_7` | electron | 0 | recorded / 7 | yes | 1 | 0 | legacy recorded state |
| 11 `multi_gamma_raw_3_recorded_7` | gamma | 3 | recorded / 7 | yes | 3 | 4 | legacy recorded state |
| 12 `multi_electron_raw_3_recorded_7` | electron | 3 | recorded / 7 | yes | 4 | 5 | legacy recorded state |
| 13 `one_gamma_default_info` | gamma | 5 | default / 0 | yes | 1 | 0 | KE/p/E all zero, no fallback |
| 14 `one_electron_default_info` | electron | 5 | default / 0 | yes | 1 | 0 | KE/p zero, E = definition mass |
| 15 `multi_electron_recorded_zero` | electron | 5 | recorded / 0 | yes | 2 | 2 | recorded KE/p zero, E = definition mass |
| 16 `multi_electron_dynamic_mass` | electron | 3 | recorded / 7 | yes | 2 | 2 | legacy definition mass, NOT dynamic 2.5 |

The first case has valid default information so the original unconditional-information helper can report a
checkpointed mismatch before reaching the intentionally absent-information zero-step cases. An arbitrary
broken helper can still crash; C++ exception handling cannot guarantee a row for a signal/process failure.
These are a fixed compact matrix, not the full Cartesian product of information and attachment possibilities.

Each track is newly constructed. Its actual counter is advanced with `IncrementCurrentStepNumber`; attachment
alone does not increment it. Track length is independently assigned via `AddTrackLength`. The fixture asserts
the actual identity, KE, counter, length, direction, attachments, recorded state and mass before calling helpers.
The attached G4Step is a default constructed object, not an initialized transport step: the helpers under test
must not require its points. No navigation/material/physics state is created.

## Ownership and diagnostics

G4Track owns the dynamic particle and attached user information. Ownership is transferred from unique_ptrs only
after successful construction/attachment. G4Track does **not** own the step. A case-local owner retains the step,
clears track/step back-pointers on exit (also during exceptions), destroys the track first, then the step.
No secondary vector is requested or populated by fixture code. Particle definitions remain Geant4-owned; only dynamic-particle mass is changed.
No production-owned track or information object is mutated.

TFileService books one tree in `beginJob`: **`fixture/endKinematics`**, in
**`component-end-kinematics-fixture.root`**. Branch backing is a stable analyzer member, not a case-local address.
Rows retain full int64 run/subrun/event IDs, one-based case index/name, `infoKind`, PDG, actual step count,
step/info presence, dynamic/definition masses, track length and raw/recorded directions. The `raw*`, `actual*`,
`expected*` groups each contain `KE`, `Px`, `Py`, `Pz`, `E` as doubles. `actual*` is the helper result.
`helperKEPresent` and `helperMomentumPresent` distinguish completed calls from uncomputed fields; absent recorded
fields and uncomputed kinematics use NaN, never an invented physical zero. `pass` and `error` retain the result.
Every row explicitly has **`diagnosticOnly=true`, `bankAdmissible=false`, `realEngine=false`**.

Each case is filled and `AutoSave("SaveSelf")` is called before any comparison/construction/helper exception is
reported as a detailed cet exception. Fill/save return values are checked. A failed comparison includes the event,
case, count/attachment/mass/info state, raw, actual and expected kinematics at 17-digit precision. No later cases
run after failure. Autosave is diagnostic checkpointing, not crash durability or permission to accept a partial file.
The full 16-case passing census and successful job completion are mandatory. Any independent inspection must use
the latest tree cycle and verify the full ordered names/indices, event identity, flags and all pass values.

## Build/runtime gate and limits

CMake registers `ComponentEndKinematicsFixture` with `src/ComponentEndKinematicsFixture_module.cc`; SCons discovers
that source through `make_plugins`. Both plugin library lists explicitly include GeneralUtilities because
`sqrtOrThrow<double>` references its out-of-line `throwHelperForSqrtOrThrow`. Other direct dependencies are Mu2eG4
(helpers/user information), Geant4 particles/track, CLHEP, art framework/services, TFileService, ROOT Tree, fhicl types
and cet exceptions. The production-helper source change is limited to the two zero-step early returns described in
[the correction plan](zero-step-fix.md); no other fixture is modified. Installed Geant4 ownership/API behavior and
link resolution were checked against the full USER build; installed behavior is covered by the bounded runtime below.

Only after that full USER build is verified, a user may run in a fresh data directory with the intended capture
workDir already set up:

```bash
mu2e -c Offline/ComponentCapture/fcl/end-kinematics-fixture.fcl
```

Preserve the build/setup logs, expanded FCL, source/plugin hashes, exit status and ROOT output, including failures.
No build or run is embedded in this fixture. This contract test does not establish genuine secondary creation or
scheduling, real-engine zero-step finalization, persisted SimParticle readback, time-helper behavior, physical
transport or energy conservation, boundary ownership, unbiased response-bank admission, or the frequency of
zero-step tracks. The separate [real-engine diagnostic](zero-step-engine.md) is independent evidence and must be
rebuilt/rerun separately to test any production-helper correction in that path.

## Measured runtime and independent readback

The verified 2026-11-06 15:34 CST user build supplies the new plugin and corrected shared helpers. The unchanged
one-event configuration completes on GPVM01 with art exit 0 and full endJob census: wall 5.57 s, peak RSS 317272 KiB.
The separate vectorized checker reads the highest `fixture/endKinematics` cycle and accepts exactly 16 ordered rows,
8 zero-step/8 stepped, correct identities/flags/matrix, finite kinematics and NaN only for absent information.
Actual/expected float64 values match bit-for-bit, signed zero included. It also checks zero-step expected/raw bits,
positive-step recorded KE and mass-source/physical constraints. It does not recompute the C++ libm reference as a
second bitwise oracle, and ROOT alone does not prove endJob or process success; the art exit/log are required too.

```bash
python3 -m mu2efastsim.capture_end_kinematics component-end-kinematics-fixture.root --output helper-report.json
```

The checker refuses existing report paths and aliases of its input. Its 234 synthetic cases cover schema, full census,
flags, one-ULP/signed-zero discrepancies, absent-value contracts, highest/partial cycles and no-clobber failures.
The combined 1217-test suite passes locally and remotely; actual fixture readback is separate from synthetic tests.
See [correction results](zero-step-fix.md) for the independent engine/readback and stepped capture regressions.
