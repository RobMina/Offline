# Engine-created secondary zero-step diagnostic

**Status: design/source preparation only; a new full USER build and runtime validation are required.** No secondary
runtime result is claimed. This follows the [validated zero-step helper correction](zero-step-fix.md), without changing
that helper or the completed primary diagnostic. It tests engine-created children under explicit status intervention,
not the natural frequency of zero-step children, a physical termination mechanism, or response-bank admission.

## Fixed inputs and two separate arms

Both one-event configurations inherit `smoke-stopping.fcl` unchanged: run 10, subrun 0, event 1, seed base 314859,
one 20 MeV/c positive muon, identical entrance/geometry/materials/physics/cuts and one art thread/schedule. The primary
undergoes ordinary transport and stopping/decay. No generated children, step counters, track IDs, user information,
energies, process codes or event products are fabricated. No decay is forced, no seed search or energy/PDG selection.

- `secondary-zero-step-classification.fcl`: every secondary is changed from `fAlive` to `fStopAndKill` during its
  classification callback, **before** forwarding classification to the unchanged recorder.
- `secondary-zero-step-begin.fcl`: classification and recorder beginTrack see `fAlive`; every secondary is changed to
  `fStopAndKill` **after** beginTrack is forwarded.

Each arm runs in its own new data directory. Same event identity is intentional: compare the same primary history,
not two different RNG trials. The common cut decision remains false (source-derived `fUrgent`, not `fKill`). The new
`ComponentSecondaryZeroStepProbe` wraps the unchanged recorder and uses a diagnostic-only const-cast on engine-owned
tracks. Primary callbacks/steps are forwarded without mutation. Any child step, grandchild, stacking rejection,
reclassification/resumption, unexpected input, missing callback or unfinished census fails explicitly; no trial is
silently discarded. Positive KE is a required fixture precondition, not a selection: any nonpositive child fails.

The prior stopping capture is reused to check exact primary track/step fields and the full direct-child census and
birth states. No new uninjected baseline is run. The saved primary has nine steps including an ordinary at-rest Decay.
The exact direct-child count is taken from the retained baseline, not guessed from its 68 total descendants.

Pinned upstream Geant4 v11.3.2 source (`source/event/src/G4EventManager.cc`, `G4StackManager.cc`, and
`source/tracking/src/G4TrackingManager.cc`, `G4SteppingManager.cc`) supports the predeclared ordering: ProcessOneTrack
finishes post-tracking before EventManager stacks its accumulated secondary vector; StackTracks assigns IDs then calls
PushOneTrack/ClassifyNewTrack for each child before the next track is processed. Positive-KE terminal status is not
reset by SetInitialStep (which resets suspend/postpone and changes nonpositive-KE tracks to stop-but-alive). PreUserTracking
runs before the G4Track gets the initialized engine step pointer; PostUserTracking runs after the skipped stepping loop.
These are pinned reference-source predictions, not installed-runtime measurements. Record and check their actual
ordering/status/step presence in both new arms, never rescue an unexpected result with a later mutation.

## Diagnostic schema and lifecycle

`g4run/secondaryZeroStepCallbacks` records all classification(0), begin(1), end(2) and primary-step(3) callbacks in a
single event-local consecutive index. It contains all fields of the primary diagnostic's `zeroStepCallbacks`, plus:

- string `mode`: `classification` or `begin`;
- string `creator` and bool `creatorPresent`: actual G4 creator-process presence/name;
- bool `cutEvaluated`, `cutResult`: primary step-cut return, false/false for non-step callbacks;
- int64 `stepSecondaries`: current-step child count for primary step callbacks, zero otherwise;
- bool `parentEnded` and int64 `parentEndIndex`: primary-end completion state at callback entry (index -1 before it);
- doubles `weight`, `dynamicMass`: actual track weight and dynamic-particle mass.

Every row carries `diagnosticOnly=true`, `bankAdmissible=false`; the existing track/step/user-information presence
flags and guarded state values remain explicit. This is a serial runtime tree, booked lazily in beginEvent, never
in declaration-only constructors. Stable member backing, checked Fill/AutoSave and failure-row checkpointing are
required. Output-write failures propagate without retrying Fill/AutoSave on the same callback; a failed write is not
assumed to leave a durable checkpoint. Only IDs/counters/value copies may be retained, never G4Track/G4Step pointers.
Parent/child maps reset at event begin and deleteCutsData. Discard before publication is an explicit diagnostic failure.
put requires the complete one-event primary/secondary callback census and one mutation per child; successful event
publication plus the external full-census check is required.

The unchanged `ComponentZeroStepDump` exports every real SimParticle and product-presence row in
`finalization/zeroStepEvents` and `finalization/zeroStepSims`. RootOutput retains all products. Separate no-G4
`secondary-zero-step-readback.fcl` reads every persisted event; both arms use process name `ComponentSecondaryZeroStep`
so the readback InputTags are identical. Output names are `component-secondary-zero-step.root`,
`component-secondary-zero-step.art`, and `component-secondary-zero-step-readback.root`.

## Independent checker contract

The companion vectorized checker requires one complete run10:0:1 in the original, readback and supplied saved stopping
baseline; highest ROOT cycles, strict schemas and full signed-int64 joins only. It checks actual product presence,
StatusG4 counters, every callback, primary/child ancestry and count closure, all finalized SimParticles and exact
persisted-field equality. It audits the complete baseline and compares all primary track/step fields exactly, not
selected steps. Direct-child identities/PDG/creator/birth states must equal the baseline; no descendant subset is
exported as a bank. Child count equals the primary's summed step-secondary counts and finalized daughter census.

For each child, require classification/begin/end exactly once and no steps, unchanged positive track energy/state,
terminal status2, zero count/length and no daughters. Classification injection should yield startStatus2; begin
injection startStatus0 because SimParticle start precedes the observer. All begin/end child Mu2e information must
retain its original defaults. At end, the engine step/pre/post points must be present with undefined step status7
and no defining process; their states must match the current child. Presence at earlier phases is measured with
flags, not inferred from zero length or zero KE. No broad positive-step raw-track endpoint comparison is allowed.

Finalized child positions are double-backed; KE, four-momentum, times and length are float-backed in SimParticle.
Compare to the captured values after the **declared float32 round-trip**, not a tuned tolerance or a false claim of
full G4 double precision. Start-state round trips and parent/creation identity are checked too. Ordinary diagnostic
output is written first even for finite endpoint disagreement; the opt-in endpoint acceptance writes a different
report and must leave no success file on failure. Exact readback comparison includes all exported fields/timings.

Because all children are born inside and their lifecycles can close, the recorder may report complete=true and the
ordinary structural audit may pass. That is not physical validity: the explicit artificial status intervention is
recorded in the physics tag/callbacks and all products/reports remain bank-inadmissible. Do not falsify recorder errors,
mark physical zero outcomes, or claim natural stopping from its generic terminal reason. No response export is made.

## Saved-baseline inspection and source checks

The new checker inspected the preserved one-event stopping capture without running Geant4: strict audit passes with
69 tracks/138 steps. The primary's nine steps produce **six direct children**, not 68 (the latter includes descendants).
Direct IDs 2/3/4 are muIoni electrons, IDs 5/6/7 are Decay anti-muon-neutrino/electron-neutrino/positron. Their saved
birth KEs are respectively 0.015003688265778574, 0.019090243229576784, 0.022015958588002857,
47.54262586021683, 32.80569276044273 and 24.288055059340437 MeV. All are strictly positive and born inside the box.
Decay children are born at global time 307.92173965952543 ns, exposing real float-storage rounding absent from the
time-zero photon diagnostic. This predicts seven tracks, nine primary steps and 30 callback rows per new arm;
none of those candidate censuses has been measured yet. The checker still derives the complete census from the baseline.

Source review identified and corrected a duplicate-Fill hazard on an AutoSave failure in the new proxy; writes now
occur outside the callback-validation exception handler. CMake registers the new conventional tool source; SCons
`make_plugins` discovers it automatically. Existing direct dependency lists in both build systems already cover the
new source (including ProcessCode's name array in MCDataProducts); no dependency change or helper edit is needed.

The vectorized checker and 232 new synthetic tests cover full schemas/censuses, integer joins above 2^53, all-field
readback, matched baseline primary/birth states, callback/absence flags, float32 storage and no-clobber failures.
Parent review added masked-array refusal, bounded secondary counts before reductions, absent-information payload
checks and primary SimParticle start/Decay/length checks while preserving the stepped-endpoint exemption.
**1449 combined local tests pass**, no skips, in 87.49 s. These are Python tests, not a C++ compile or secondary run.
An over-broad exploratory full-core pytest invocation encountered capability failures (`beam_point` unavailable) and
timed out; it is not a full-core passing claim. No new C++/FCL execution or remote regression suite has occurred.

```bash
python3 -m mu2efastsim.capture_secondary_zero_step --inspect-baseline --baseline BASELINE.root --output inspection.json
python3 -m mu2efastsim.capture_secondary_zero_step component-secondary-zero-step.root \
  --readback component-secondary-zero-step-readback.root --baseline BASELINE.root --mode classification \
  --output diagnostic.json
python3 -m mu2efastsim.capture_secondary_zero_step component-secondary-zero-step.root \
  --readback component-secondary-zero-step-readback.root --baseline BASELINE.root --mode classification \
  --require-endpoint-match --output acceptance.json
```

Use `--mode begin` for the separate begin arm. Both ordinary and acceptance reports are diagnostic-only; CLI refuses
existing paths and aliases of any input. The ordinary report includes finite endpoint disagreements; the opt-in gate
fails before writing. Keep both reports and the actual process exit/log rather than using partial ROOT as success.

## Post-build procedure and limits

After the USER full build, verify the new plugin/helper library hashes, timestamps and dependencies before any run.
Freshly check host occupancy/quota, run the classification arm once, then its readback and checker; repeat separately
for the begin arm. Preserve all failures and partial products. Stop on unexpected scheduling/census/primary differences;
do not re-seed, loosen a tolerance, patch the helper or force a shower to make the fixture pass. Use the existing
stopping baseline; no campaign, tarball, worker or submission is authorized. A successful result would establish
secondary scheduling/finalization only for these pinned artificial interventions, not suspension/postponement,
boundary-born ownership, internal-filter reject-accept, all resource cuts or statistical geometry equivalence.
