# Zero-step end-kinematics correction and validation gate

**Status: user build verified; all three predeclared bounded validation checks pass (2026-11-06).** The measured defect
and frozen baseline remain described in [the real-engine diagnostic](zero-step-engine.md). Candidate results follow below.
This is a shared Mu2eG4 helper correction in the isolated capture checkout, not a CRV response workaround or a change
to the production work area. Baseline outputs, diagnostic interventions and all failed fixtures remain intact.

## Minimal correction

`Mu2eG4UserHelpers::getEndKE` and `getEndMomentum` now return the current track KE and dynamic-particle four-momentum
when `GetCurrentStepNumber() == 0`, before accessing Mu2e user information. That state exists even without a recorder
snapshot. The zero-step four-vector uses the track's actual momentum and total energy, including dynamic mass, rather
than reconstructing it from PDG mass. This agrees with the source of the SimParticle start four-vector.

All positive-step statements are unchanged. Mu2eRecorderProcess records an intermediate state after AlongStep and
before PostStep processes; that is intentionally different from the final G4Track after an interaction or decay.
A blanket replacement with the final track would discard that distinction. The discriminator is step count, not
zero track length, status, step-pointer presence, default KE or zero momentum direction. At-rest processing and a
zero-length step still use the existing branch when their step count is positive. The stepped branch retains its
existing PDG-mass convention; any separate dynamic-mass question is outside this patch.

No changes to time helpers, user-information initialization, tracking actions, observer bridge, capture recorder,
physics configuration, product schema or dictionaries. The helpers do not mutate the track. If another hook has
already changed a zero-step track's energy/direction, the fallback uses that current state; it cannot recover an
unrecorded earlier state. This is not a general missing-user-information or null-step safety patch for other helpers,
and does not implement suspension/resumption or boundary-born ownership.

## Predeclared post-build checks

Only after the user reports a successful full build, verify the build marker, changed Mu2eG4 library and new fixture
plugin timestamps/hashes/dependencies. Freshly check host occupancy and quota. Run each job sequentially in its own
new data directory; preserve all failures and original outputs. Do not rerun the frozen baseline or tune seeds.

1. **Constructed helper contract:** run [end-kinematics-fixture.fcl](end-kinematics-fixture.md). It calls the actual
   helpers on fixed massive/massless, zero/positive-step objects with deliberately distinct track/recorded states.
   Require the entire fixed case census and all assertions; neither an early exit nor a partial tree is success.
   This is a helper contract test, not real transport, secondary scheduling or physical SimParticle persistence.
2. **Same two real-engine cases:** run unchanged `zero-step-engine.fcl`, then separately `zero-step-readback.fcl`.
   First write the ordinary diagnostic report even if endpoints disagree. Then require the stronger acceptance gate
   into a different report path:

   ```bash
   python3 -m mu2efastsim.capture_zero_step component-zero-step.root \
     --readback component-zero-step-readback.root --run 15 --output diagnostic.json
   python3 -m mu2efastsim.capture_zero_step component-zero-step.root \
     --readback component-zero-step-readback.root --run 15 --require-endpoint-match --output acceptance.json
   ```

   The opt-in flag checks the existing endpoint comparison only after every lifecycle/schema/census/persistence check.
   It does not alter the default report or demand that baseline discrepancies disappear from retained evidence.
   On failure it creates no success report and does not overwrite an existing output. Required candidate result:
   two complete diagnostic lifecycles, six callbacks, two finalized SimParticles, exact art readback, endpoint
   agreement at the unchanged diagnostic tolerance. Mu2e user information must still show its untouched defaults;
   fixing the proxy, fabricating a step or initializing information would invalidate this experiment.
3. **Stepped transport smoke:** run unchanged one-event `smoke-photon.fcl` and `smoke-stopping.fcl` with their original
   run/seed/input configurations. Reuse their saved baseline capture files, not new baseline simulations. Audit both
   sides and compare all five capture tables by full-int64 event/row identity, excluding only StatusG4 CPU/real time:

   ```bash
   python3 -m mu2efastsim.capture_compare BASELINE.root CANDIDATE.root \
     --expected-events 1 --exact --output comparison.json
   ```

   Exact mode allows no finite-value tolerance or selected event subset. It retains all fields and checks matching
   shapes/dtypes/field censuses; timings must still pass ordinary finite-value auditing. The existing distribution
   comparison remains separate. Failure stops this gate, never triggers seed hunting or relaxed tolerances.
   Photon transmission includes zero deposit; the positive-muon fixture includes stopping, an at-rest decay and its
   children. Capture equality is not an exact SimParticle endpoint comparison: those saved baselines did not export
   the finalization tables. The constructed helper tests directly check unchanged positive-step kinematics; a broad
   physical SimParticle regression campaign is not claimed by these smoke runs.

## Source-level checks

Read-only review found no concrete helper/fixture source blocker; this is not compilation. The helper diff contains
only the two early returns and one-line comments. Installed Geant4 11.3.2 headers confirm the track counter, length,
step attachment, dynamic-mass and total-energy APIs used by the fixture. GeneralUtilities exports the required
`throwHelperForSqrtOrThrow(double,double)` symbol; both CMake and SCons plugin lists include that direct dependency.
The fixture has 16 fixed cases (eight zero-step, eight stepped) and compares finite results byte-for-byte per double.

The source-preparation local suite passed 983 tests before the user build. The independent vectorized
`mu2efastsim.capture_end_kinematics` checker adds 234 cases, bringing the combined suite to **1217 tests**, no skips:
31.94 s locally and 78.11 s on GPVM01. These test acceptance/report behavior, separately from actual C++ runtime.

## Measured candidate results (2026-11-06)

The full user build marker is **15:34:03 to 15:34:40 CST**. Helper/fixture source hashes match the prepared revision;
Mu2eG4 and fixture libraries postdate their sources and all checked dependencies resolve. The unchanged probe/dump
plugins retain their baseline hashes. User completion confirmation, marker, artifact metadata and runtime banners
establish the build; no build tmux pane remained. Known setup hook/link-order warnings are retained, not repaired.

All five serial art invocations on idle GPVM01 exit 0, with no changed seeds/configurations or baseline reruns:

| Check | Measured result |
|---|---|
| Constructed helper | One event 16:0:1; all 16 cases (8 zero-step, 8 stepped) pass exact C++ equality and endJob census. Independent highest-cycle ROOT schema/matrix/byte-equality checker passes. |
| Two-event engine | Two events, six callbacks, two finalized SimParticles; endpoint KE/E = 10 MeV and pz = 10 MeV/c in both cases, all compared endpoint deltas zero. |
| Separate art readback | Every exported persisted field matches exactly. Ordinary diagnostic saved first, then opt-in endpoint gate passes into a separate, byte-identical report. |
| Invalid-history exclusion | Original and readback positive audits still exit 1 with `event complete is not true`; neither writes a success report. Mu2e information remains at its original defaults. |
| Photon capture regression | One event, 1 track/2 zero-deposit steps/1 exit; all five capture tables exactly match the preserved baseline apart from CPU/real timing values. |
| Stopping/decay capture regression | One event, 69 tracks/138 steps/68 children/6 exits/63 natural ends; same exact-field comparison passes. Component deposit remains 21.284270872713662 MeV. |

Actual wall times/peak RSS (not runtime projections): helper 5.57 s/317272 KiB; engine 5.88 s/525328 KiB;
readback 1.38 s/449012 KiB; photon 1.86 s/360716 KiB; stopping 1.83 s/361096 KiB.
Expanded FCLs, invocation/setup/resource/exit logs, source and library hashes, reports and original outputs are retained
in a new candidate checkpoint, separate from the frozen defect baseline and source-only preparation checkpoint.
No C++ or FCL changed after this build; the added independent Python checker does not require a rebuild.

## Limits and next gates

Although all three checks pass, the two real-engine corrected cases are artificially terminated primaries. Genuine
secondary zero-step finalization, suspension/postponement, internal-filter reject-then-accept, resource failures,
boundary ownership and unbiased component-bank admission remain separate. A helper-only fix cannot make an upstream
never-entered capture valid: both diagnostic histories must still fail ordinary positive audit and stay bank-inadmissible.
No production campaign, worker, submission, tarball, manifest completion or statistical response-equivalence claim
is implied. Build and runtime evidence must be archived separately from source preparation.
