# Offloading e+-/gamma transport (physics.gpuOffload)

Off by default: without `physics.gpuOffload` the job is unchanged. With it, Mu2eG4 attaches one Geant4 tracking
manager (`Mu2eG4OffloadTrackingManager`, a `G4VTrackingManager`) per thread to the configured particles (default
e-, e+, gamma). It hands tracks to a transport engine, an art tool implementing `IMu2eG4Offload` (a GPU engine
such as AdePT or Celeritas, or the test engine `Mu2eG4OffloadKill`).

```fcl
physics.producers.g4run.physics.gpuOffload : {
  regionVolumes : [ "DS2Vacuum", "DS3Vacuum" ]       # roots of the G4Region Mu2eG4Offload
  extraRegions : []                                   # other G4Regions inside it that are offloaded too
  particles : [ "e-", "e+", "gamma" ]
  handOverKill : [ { pdgIds : [ 11, -11 ] maxKineticEnergy : 1.0 volumes : [ "DS2Vacuum" ] } ]
  engine : { tool_type : "Mu2eG4OffloadKill" registerDaughter : true }
}
```

## What happens to a track

1. **Hand-over kill.** If a `handOverKill` rule matches (particle, kinetic energy below the limit, start volume
   by physical-volume name), the track ends at once, with stopping code `mu2eKillerVolume`. These are the
   `Mu2eG4CommonCut` terms an engine cannot apply. A track that would enter such a volume later is not covered.
2. **Routing.** The start volume is found by a separate navigator in the direction of motion, so a primary on a
   volume face belongs to the volume ahead. If its region is `Mu2eG4Offload` (or one of `extraRegions`), the track's
   SimParticle is made and ended at the hand-over point (stopping code `mu2eGpuTransport`) and the engine takes the
   track. Otherwise stock Geant4 tracks it on the CPU, with the Mu2e user actions; its secondaries go back to the stack.
   An engine whose `routesByRegion()` is true receives every track instead, and does its own routing and SimParticles.
   An engine that brings its own Geant4 tracking manager (`IMu2eG4Offload::trackingManager()`, e.g. AdePT's, which
   tracks e+-/gamma outside its regions on the CPU with G4HepEm and returns tracks that leave them) gets every track
   after the kill rules, and the physics tables and the event flush too; `offload()` and `physicsTableBuilt()` are
   then not called.
3. **End of event.** Geant4 calls `FlushEvent` when its stack is empty; the engine finishes its tracks, sends the
   steps in sensitive volumes to the Mu2e sensitive detectors and gives back any track that leaves its region.

## Truth

Engine tracks get Geant4 track IDs from `Mu2eG4OffloadTruth::firstTrackId` (2^28) on, so their SimParticle keys
never collide with CPU tracks. The engine registers each track it wants kept (at least those with steps in a
sensitive detector and their parents) with `Mu2eG4OffloadTruth`. At the end of the event they become SimParticles,
daughters of their registered parent, before the cross-reference check. Steps of these tracks have stopping
process `mu2eGpuTransport`. If the engine returns no creation process, `Mu2eG4OffloadTruth::inferCreationCode`
gives `eIoni` (e- from e+-), `eBrem` (photon from e+-), `mu2eGpuPhotonSecondary` (e+- from a photon) or
`mu2eGpuSecondary`. With `eIoni`, `MakeStrawGasSteps` folds short delta rays as in stock.

### Engines that call the Mu2e tracking action

An engine with its own tracking manager can make SimParticles through the stock `Mu2eG4TrackingAction` instead of
`Mu2eG4OffloadTruth`: AdePT (with `returnFirstAndLastStep` and `callUserTrackingAction`) calls
`PreUserTrackingAction` for each device-born track and `PostUserTrackingAction` at each device track's last step,
when it replays returned steps, outside any CPU stepping and interleaved across tracks. Its device-born tracks get
Geant4 track IDs counting down from 2^31-1, above `firstTrackId`.

`Mu2eG4SteppingAction` keeps one trajectory buffer, which belongs to the track begun last
(`BeginOfTrack(trackId)`), or to a track that steps without being begun again (a track the engine returns to the
CPU). `PostUserTrackingAction` moves the buffer into the track's `MCTrajectory` only if the track owns it
(`ownsTrajectory`). So device tracks get no `MCTrajectory`, or only their CPU part, and never another track's points.
Without an engine every track owns its buffer, so stock output is unchanged.

### Geant4 trajectories

Only `G4TrackingManager::ProcessOneTrack` creates Geant4 trajectories (`G4Trajectory`, which no Mu2e product uses;
`MCTrajectory` comes from `Mu2eG4SteppingAction`). `Mu2eG4UserHelpers::controlTrajectorySaving` therefore asks for one
only for particles without their own tracking manager. With an engine such as AdePT, e-, e+ and gamma tracked on the
CPU go through G4HepEm, which takes `G4TrackingManager::GimmeTrajectory()` whenever trajectory storing is on: that
would be the trajectory of the last track `G4TrackingManager` tracked, owned by the `G4Event`, and G4HepEm would append
to it and delete it (a use-after-free and a double delete). The flag is global: a track the engine returns to the CPU
gets no new `PreUserTrackingAction`, and would see the value the last track (e.g. a proton) left. So
`Mu2eG4OffloadTrackingManager` clears it (`SetStoreTrajectory(0)`) before every hand-over to the engine's manager and
before its `FlushEvent`.

## Tests

- `Mu2eG4/fcl/g4test_gpuOffload.fcl` and `g4test_gpuOffloadMT.fcl`: the test engine on the DS vacuum volumes.
- `Mu2eG4/fcl/g4test_gpuOffloadCPU.fcl`: region DS3Vacuum only, so the CPU path, the offload and the hand-over
  kill (physical-volume names, e.g. `TargetFoil_00`, not logical-volume names) all run.
- Feature off: `g4test_03.fcl` must give output identical to the base commit.
