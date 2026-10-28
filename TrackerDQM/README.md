# TrackerDQM

Tracker clients of the [`DQMHelpers`](../DQMHelpers/README.md) core: the
histograms that the online DQM (otsdaq) and the offline DQM jobs (Mu2e/DQM)
book and fill, from one class, so the two sides cannot drift. The art modules
that drive the clients live outside Offline:

| Client | Input | Offline module (Mu2e/DQM) | Online module (otsdaq-mu2e-dqm) | FHiCL tables |
|---|---|---|---|---|
| `TrackerDigiDQM` | `StrawDigiCollection` | `DqmTrackerDigi` (`fcl/trackerDQM.fcl`) | `TrackerDigiDQMOnline` | `TrackerDQM.Digi`, `TrackerDQM.DigiOnline` |

```
TrackerDQM/
  inc/TrackerDigiDQM.hh            the client: axes, accessors
  src/TrackerDigiDQM.cc            book() and Fill()
  fcl/prolog.fcl                   TrackerDQM.* tables (copies and publishing, never binning)
  data/TrackerDQM_binning_v1.txt   reference catalogue of the histogram set
```

## Status: a starting point

`TrackerDigiDQM` books four placeholder histograms, one of each kind of axis a
tracker client needs:

| Histogram | Axis | Kind |
|---|---|---|
| `nDigis` | 0-199 digis / event | a count |
| `tdcCal` | 100 bins, 0-80000 TDC | a raw value |
| `panelOccupancy` | 216 unique panels | a detector index |
| `strawOccupancy` | 216 panels x 96 straws | a 2D map |

They are there to show the pattern and to make the package, the modules and
the jobs run end to end. The tracker group owns the real histogram set: edit
the axes and `book()`/`Fill()`, then follow "Changing binning" below.

The placeholder ranges suit on-spill MC. On KPP raw data (run 124155, about
400 us readout) every event has more than 199 digis and 98% of the TDCs are
above 80000, so the real axes must be sized for every running mode the
histogram will see. Where one range cannot serve both, book two views (a short
and a full range), as the CRV client does for digis per event and TDC.

## Adding to the client

1. Declare the axis as a `static constexpr DQMAxis` in `TrackerDigiDQM.hh`.
   Detector-indexed axes come from `StrawId` (`_nupanels`, `_nstraws`,
   `_nplanes`), so they cover the whole tracker in every configuration.
2. Book it in `book()` (`hists().book1<TH1F>(...)` / `book2<TH2F>(...)`) and
   add a `DQMH1`/`DQMH2` member and an accessor.
3. Fill it in `Fill()`. A value the data can produce but that has no bin goes
   to an overflow bin and, if it matters, to `diag().Count(...)`, as the
   invalid-`StrawId` case does.
4. Bump `kBinningVersion` and regenerate the catalogue.

A new client (another input tier) is a new class beside this one, added to
`CMakeLists.txt` and `src/SConscript`, with its own table in `fcl/prolog.fcl`
and its own block in the catalogue. Natural next tiers:

- **hits** (`StrawHit` / `ComboHit` and their flags): hit time, energy,
  occupancy after the hit-making cuts;
- **tracks** (`KalSeed`): what Mu2e/DQM `DqmTrk` histograms today, which feeds
  the `trk/*` DQM metrics;
- **DAQ status**: per-DTC-link error and packet counts. Offline has no tracker
  DAQ-status product yet (the CRV analogue is `CrvStatus` / `CrvDAQerror`), so
  this client needs the unpacker to persist one first.

Rules that keep files mergeable (from `DQMHelpers/README.md`): binning is a
constant in this package, never FHiCL or data; everything is booked in
`book()`; there is no switch that drops a histogram. FHiCL chooses only
extra copies (per subrun, rolling windows) and what is published online.

## Existing tracker DQM to fold in

The histograms below exist today, in other repositories, with their own
binnings. They are the obvious source for the real histogram set:

- Mu2e/DQM `src/DqmStr_module.cc` (digis) and `src/DqmTrk_module.cc` (tracks),
  with the metrics in `src/DqmMetrics_str.cc` / `DqmMetrics_trk.cc`;
- otsdaq-mu2e-dqm `ArtModules/TrackerHitOccupancy_module.cc` (per-panel hit
  occupancy) and `ArtModules/TrackerDQM_module.cc`;
- otsdaq-mu2e-tracker `ArtModules/TrackerDQM_module.cc` (DAQ-level, per ROC and
  channel).

## Changing binning

1. Edit the `DQMAxis` constant, or add or remove a histogram.
2. Bump `TrackerDigiDQM::kBinningVersion`.
3. Regenerate the catalogue: run the offline module with `catalogueFile` set and
   copy the result over `data/TrackerDQM_binning_v<N>.txt`:

   ```
   # cat.fcl: #include "DQM/fcl/trackerDQM.fcl"
   #          physics.analyzers.trackerDigiDQM.catalogueFile : "TrackerDQM_binning.txt"
   mu2e -c cat.fcl -s <file with StrawDigis> -n 10
   ```

   The file holds only the `# TrackerDigiDQM ...` block and its lines; keep the
   two header comment lines of the reference file above it.

4. Say so in the PR: merging tools and the DQM database see a new version, and
   old files no longer combine with new ones.
