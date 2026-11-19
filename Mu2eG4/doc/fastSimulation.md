# Geant4 fast simulation in Mu2eG4

Mu2eG4 can hand the tracks that enter chosen volumes to Geant4 fast-simulation models
(`G4VFastSimulationModel`). A model replaces the full simulation inside its envelope: it decides from the entering
track which energy deposits and outgoing particles to produce, and kills or continues the track. Geant4 keeps
transporting everything outside the envelopes.

Nothing changes unless `physics.fastSim` is present in the `Mu2eG4` (or `Mu2eG4MT`) configuration.

## Configuration

```fhicl
physics.producers.g4run.physics.fastSim : {
  verbose : false                              # G4FastSimulationPhysics printout
  models : [
    {
      name : "caloDepositAtEntry"              # model name and the name of its G4Region
      envelopes : [ "CaloDisk_0", "CaloDisk_1" ] # volumes registered with Mu2eG4Helper
      particles : [ "e-", "e+", "gamma" ]      # particles that get the fast-simulation process
      tool : { tool_type : "Mu2eG4FastSimDepositAtEntry" depth : 100. }
    }
  ]
}
```

- Each model gets one `G4Region` holding its envelope logical volumes as region roots. A volume that is already the
  root of another region (for example from `physics.minRangeRegionCuts`) is a configuration error.
- `G4FastSimulationPhysics` is registered with the reference physics list and activated for the union of the
  models' particles. The `Minimal`, `MinDEDX` and `ErrorPhysicsList` lists do not support it.
- Tracks stopped by a model end with the process `fastSimProcess_massGeom` (`ProcessCode::fastSimProcess_massGeom`).

## Writing a model

A model is an art tool implementing `IMu2eG4FastSimModel` (`Mu2eG4/inc/IMu2eG4FastSimModel.hh`). Its
`construct(name, region)` builds the thread's `G4VFastSimulationModel` on the region and keeps ownership; Mu2eG4
calls it once per Geant4 thread, from `Mu2eWorld::constructSDandField`. The tools, and so the models, live until the
process ends and are never deleted, as in the Geant4 examples: by the time thread-local storage is destroyed, the
Geant4 allocators behind navigators and touchables are gone.

Energy deposits go through `Mu2eG4FastHitMaker` (one per model and thread). A `Mu2eG4FastHit` carries the position
in the envelope frame of the `G4FastTrack` (as `G4FastSimHitMaker` does), the deposited and visible energy, a time
and a step length. The hit maker locates the volume at that position and passes the hit to its sensitive detector
if the detector implements `IMu2eG4FastHitSD`: `CaloCrystalSD` and `CRVSD` do. The detector writes a `StepPointMC`
attributed to the track that entered the envelope, with the volume id of the struck crystal or bar, the primary
track's momentum, and `fastSimProcess_massGeom` as end process. Hits that fall outside a fast-hit detector are
dropped (`make` returns false).

Notes for model authors:
- Do not put energy on the `G4FastStep` itself when the hit maker records it: a sensitive detector on the envelope
  volume would see the fast step too.
- Secondaries created with `G4FastStep::CreateSecondaryTrack` are transported by Geant4 and get SimParticles like
  any other secondary.
- Visible energy: a negative `visibleEdep` records the deposited energy as visible; models of scintillators should
  set the quenched value.

## Test model

`Mu2eG4FastSimDepositAtEntry` deposits the kinetic energy of each triggering track as one hit `depth` mm along its
entry direction and kills it. `Mu2eG4/fcl/g4test_fastSim.fcl` (and `g4test_fastSimMT.fcl`) run it on the two
calorimeter disks with conversion electrons.
