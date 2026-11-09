# Callback and recovery fixtures

**Status: user build verified; both runtime fixtures and their strict diagnostic checkers pass.** These additions do not change
ComponentRecorder, its products, the Mu2eG4 observer bridge, production construction or default configurations.
The successful physical smoke and negative-reset runs in [lifecycle.md](lifecycle.md) remain separate evidence.

## Real transport after a rejected event

`negative-stacking-recovery.fcl` requests four fixed upstream box muon events, run 13, seed base 315159. The new
`ComponentCaptureTestGate` is used only as the stacking-only cut, with explicit `diagnosticOnly:true` and rejected
event numbers `[1,3]`. It returns the declared event decision without changing tracks, statuses or random numbers.
The common cut remains the real ComponentRecorder. Events 2 and 4 run ordinary Mu2eG4 transport through the same
recorder instance; the analyzer retains the rejected-event diagnostics with `failOnInvalid:false`.

Expected sequence: incomplete stacking rejection, completed ordinary transport, incomplete stacking rejection,
completed ordinary transport. Every rejected event must have exactly the known rejection signature; both accepted
events must pass the full positive audit without old errors or duplicate IDs. Check the whole independent census:

```bash
python3 -m mu2efastsim.capture_lifecycle component-capture-smoke.root \
  --kind stacking-recovery --run 13 --expected-events 4 --output recovery.json
```

The checker requires all four events and examines the predeclared rejected/accepted subsets separately. It never
exports selected donors. The whole file must fail the ordinary positive audit and remain bank-inadmissible. This
checks reset after an invalid product was published, not the internal-filter discard path.

## Constructed-object callback matrix

`callback-fixture.fcl` runs a separate `ComponentRecorderCallbackFixture` producer over exactly 13 events in run 14.
There is **no Mu2eG4 producer, physics list, random engine or G4 run manager**. The producer loads the actual recorder
through `art::make_tool<IMu2eG4Cut>`, registers its product through the normal collector, and retains the same tool
instance across events. It calls the real interfaces using constructed G4Track, G4Step, G4StepPoint and touchable-history
objects. This is a callback contract test, **not simulated physics**, even when `recordComplete` is true.

The fixture owns an otherwise isolated two-volume geometry: a 500 mm half-size G4_Galactic world and a centred
50 mm half-size G4_POLYSTYRENE envelope. It asserts the surface/interior test positions and touchable ancestry.
No navigator is asked to choose ownership at the ambiguous surface; inside/outside histories are deliberate inputs.

The scripted photon parent (track1) starts at z=-50.001 mm with pz=10 MeV/c. Three zero-deposit steps enter at z=-50,
reach z=0, then exit at z=50. The recorder must return a cut only on the outward step. The fixture applies the
artificial stop only afterward, so the recorded parent exit must retain its original alive status. One photon child
(track2, 1 MeV/c) is attached to the actual G4Step secondary vector, at that step's exact post-position/time, then
classified and finalized after the parent. All child tracks have zero steps. These hand-constructed energies/times
and terminations do not model a physical interaction or prove energy conservation.

| Event | Child/input case | Expected recorder outcome |
|---:|---|---|
| 1 | Parent exit endpoint, inward direction, inside history | Boundary-born ownership error |
| 2 | Parent exit endpoint, outward direction, inside history | Boundary-born ownership error |
| 3 | Parent exit endpoint, inward direction, outside history | Boundary error, outside birth and never-entered errors |
| 4 | Parent exit endpoint, outward direction, outside history | Same three errors |
| 5 | Interior child, alive at classification, zero-step terminal | Recorder-complete control |
| 6 | Interior child, already terminal at classification | Recorder-complete zero-step control |
| 7 | Interior child ends kill-track-and-secondaries | Unsupported-terminal error, reason3 |
| 8 | Interior child ends suspended | Unsupported-terminal error, reason3 |
| 9 | Interior child ends suspended-and-waiting | Unsupported-terminal error, reason3 |
| 10 | Interior child ends postponed-to-next-event | Unsupported-terminal error, reason3 |
| 11 | Child classified twice | Reclassification and classification/track-census errors |
| 12 | Suspended child begun again | Expected duplicate/resume exception caught; buffer explicitly deleted, no product |
| 13 | Fresh interior zero-step control, reusing IDs1/2 | Recorder-complete, no stale errors/records after discard |

No StatusG4 is fabricated. An empty SimParticle collection exists only to construct the real SimParticleHelper and
satisfy framework plumbing; it is not simulated truth. `ComponentCaptureDump` therefore marks **all 13 dump events
incomplete**, including the three recorder-complete controls, because StatusG4 is absent. The default missing status
and validation messages are checked explicitly. Event12 has neither capture rows nor metadata. Only this diagnostic
producer sets `errorOnFailureToPut:false` to allow that planned omission. Do not change those flags for ordinary capture.

```bash
python3 -m mu2efastsim.capture_callbacks component-callback-fixture.root --run 14 --output callbacks.json
```

The dedicated checker asserts exact event/track/step/classification/volume census, error/status signatures, full
int64 identities, all scripted states and touchables, the pre-kill parent endpoint and event13 recovery. Its output
says `physical_transport:false`, `diagnostic_only:true`, `bank_admissible:false`. A successful art exit alone is not
sufficient: the checker must pass. The shared reader preserves ragged vector metadata for the deliberate missing
capture among present records, without padding it into invented geometry; regular positive files remain NumPy arrays.

## Measured runtime checkpoint (2026-11-06)

The full user build completed at 12:29:58 CST with both plugins newer than their matching source hashes. Dependencies
resolve in al9-prof-e29-p107. The recorder source is unchanged. Both jobs ran serially on an idle GPVM, in separate fresh
data directories, and exited 0; the dedicated checkers then verified the completed ROOT event censuses and exact cases.

- Recovery: all four events retained. Events 1/3 each have the exact three rejection errors, one classification, three
  volumes and no tracks/steps. Events 2/4 pass the positive audit: 119 tracks, 243 steps, 117 children/natural stops and
  two first exits in total; component deposit 37.984882243574134 MeV. IDs restart without stale records/errors. The
  whole mixed file fails ordinary audit (`event complete is not true`); no accepted subset is exported as a bank.
- Callbacks: all 13 expected cases pass, with 24 tracks, 36 steps, 25 classifications and 24 volume rows. Recorder
  completion is true only for 5/6/13. Event 12 has no capture or geometry metadata after the expected resume exception
  and explicit discard; event 13 reuses IDs cleanly. All dump events remain incomplete because StatusG4 is absent.
- The ordinary auditor also rejects callback input and writes no success report. Ragged missing geometry now yields
  the explicit error `events: malformed translation`, rather than depending on Awkward's conversion-error wording.
  This Python-only diagnostic improvement does not change acceptance or fill missing metadata; original reports remain.
- 632 combined Python tests pass locally and on the GPVM (629 before the explicit-error regression). All eight saved
  positive samples pass the current auditor without re-simulation. Build/source/plugin hashes, expanded FCLs, original
  and final checker reports, test results, output hashes, setup/exit/resource logs are retained in a separate checkpoint.

## Scope and build gate

CMake registers the two plugins with the existing capture library list. SCons `make_plugins` discovers their conventional
source filenames using the same already-listed dependencies. No new dependency or build command in scripts is needed.
Any future C++ changes require a new full user build and verification before runtime.

These fixtures do not validate engine secondary production or scheduling, full Mu2eG4 zero-step SimParticle
finalization, or a resolved boundary ownership prescription. They test the recorder's response to specified callbacks
and its explicit refusal of unsupported cases. Physical transport smoke stays separate from constructed objects.
Full manifests, boundary ownership and unbiased bank admission remain separate gates.
The separate [real-engine zero-step diagnostic](zero-step-engine.md) now passes controlled primary lifecycle and
exact persisted-readback checks, but measures zero finalized endpoint KE/energy/momentum versus the actual 10 MeV
photon endpoints in both cases. Recorder/helper kinematics are unchanged; this does not establish secondary scheduling.
