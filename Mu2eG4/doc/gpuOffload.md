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

## Tests

- `Mu2eG4/fcl/g4test_gpuOffload.fcl` and `g4test_gpuOffloadMT.fcl`: the test engine on the DS vacuum volumes.
- Feature off: `g4test_03.fcl` must give output identical to the base commit.
