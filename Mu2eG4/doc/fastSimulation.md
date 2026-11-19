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

## Calorimeter shower library model

`Mu2eG4FastSimCaloEntrantLibrary` replaces the calorimeter shower of a particle entering a disk by the Geant4 shower
of a donor from a per-entrant library, built by mu2e-fastsim (`calofastsim.showers.entrants`, ROOT export written by
`--root`). An entrant is a particle whose first crossing into a disk precedes any of its ancestors'; its donor pattern
is every CaloShowerStep of its subtree, in both disks. Example: `Mu2eG4/fcl/g4test_fastSimCaloLibrary.fcl`.

- **Envelopes:** `CaloDisk_0` and `CaloDisk_1`. The model triggers only on the envelope surface, for a track moving
  inward through the front face or the inner edge. Tracks born inside a disk, or entering through the back or the
  outer edge, stay with full Geant4.
- **Cell:** species (e-, e+, gamma), face, disk, total energy, face cosine (along the disk axis for the front face,
  along the outward radius for the inner edge) and position (entry radius for the front face, depth along the disk
  axis for the inner edge). These are computed in the disk frame of the geometry service, as the library was.
- **Donor** (`donorSelection: nearest`, the default): among the library entrants of the query's species, face,
  disk, energy bin and cosine bin, the model takes the `nearestK` whose entry points lie closest to the query entry
  and draws one of them uniformly (default 1, the nearest). The distance also counts each donor's image turned by 180 degrees about the disk
  axis. That turn is a symmetry of the crystal lattice; the x and y mirrors are too, but they would flip the shower's
  handedness in the solenoid field, so they are not used. Placement therefore turns a donor by about 0 or 180 degrees
  and moves it a few mm, so its shower stays on the same crystals relative to the stepped inner and outer edges of the
  crystal array. For the front face the distance is between the two entry points on the face. For the inner edge it
  is sqrt(dz^2 + (rho dphi)^2) at the query radius. The track goes back to full Geant4 in four cases: the closest
  usable donor is farther than `maxEntryDistance` (default 20 mm); the entry lies outside the library bins; the donor's total energy
  differs from the entrant's by more than `maxEnergyDeviation` (relative; energies are never rescaled); or the
  species has no library entrants.
- **Donor** (`donorSelection: cell`): any entrant of the query's cell (energy, cosine and radius or depth bins), or of
  the closest occupied cell within `maxCellDistance2`, turned by the full azimuth difference. This is the Python
  sampler's rule. Near the stepped edges of the crystal array it loses energy into the support structure: in a
  400-event CE test, 14.5% of the placed donor energy landed outside the crystals.

In the same test (g4test_03, 400 CE events, one thread), the defaults record 96.6% of the placed donor energy in
crystals, the remainder mostly in crystal wrappers. The model replaces 97.8% of the calorimeter energy, and the
rest is left to Geant4. Mean calorimeter energy per event with energy is 79.1 MeV against 79.0 MeV for Geant4 (median
90.3 against 90.6). Larger `nearestK` or a smaller `maxEntryDistance` leave more entries to Geant4 and lose more
energy at the edges.
- **Placement:** each donor step is turned by the query entry azimuth about the disk axis and translated to the query
  entry, in the frame of the disk the step was in. Its time is the entry time plus the step's relative time. It
  becomes a fast hit of the entrant, with `edep` the Geant4 energy and `visibleEdep` the Birks energy, so
  `CaloShowerStepMaker` and everything downstream are unchanged. Truth is entrant-only: the donor's secondaries are
  not created.
- **Threads:** the library is read once per file and shared read-only by all threads. Donors are drawn with the
  thread's Geant4 engine.
- `contentHash` pins the library: the job stops if the file's `content_hash` differs.

`donorSelection: cell` with a large `maxCellDistance2` and a negative `maxEnergyDeviation` reproduces the Python
sampler (`calofastsim.showers.entrants.sample`). `diagnostics: true` logs each placement (donor and recorded energy,
dropped energy per volume) and each entry left to Geant4, with the reason.
