# CaloDQM

Calorimeter clients of the [`DQMHelpers`](../DQMHelpers/README.md) core: the
histograms that the online DQM (otsdaq) and the offline DQM jobs (Mu2e/DQM)
book and fill, from one class, so the two sides cannot drift. The art modules
that drive the clients live outside Offline:

| Client | Input | Offline module (Mu2e/DQM) | Online module (otsdaq-mu2e-dqm) | FHiCL tables |
|---|---|---|---|---|
| `CaloDigiDQM` | `CaloDigiCollection` | `DqmCaloDigi` (`fcl/caloDQM.fcl`) | `CaloDigiDQMOnline` | `CaloDQM.Digi`, `CaloDQM.DigiOnline` |

```
CaloDQM/
  inc/CaloDigiDQM.hh            the client: axes, accessors
  src/CaloDigiDQM.cc            book() and Fill()
  fcl/prolog.fcl                CaloDQM.* tables (copies and publishing, never binning)
  data/CaloDQM_binning_v1.txt   reference catalogue of the histogram set
```

## Status: a starting point

`CaloDigiDQM` books four placeholder histograms, one of each kind of axis a
calorimeter client needs:

| Histogram | Axis | Kind |
|---|---|---|
| `nDigis` | 0-499 digis / event | a count |
| `t0` | 400 bins, 0-4000 digitizer ticks | a raw value |
| `sipmOccupancy` | 2716 offline SiPM IDs (`CaloConst::_nChannel`) | a detector index |
| `peakAdcVsSiPM` | SiPM ID x 64 bins of 64 ADC | a 2D map |

They are there to show the pattern and to make the package, the modules and
the jobs run end to end. The calorimeter group owns the real histogram set:
edit the axes and `book()`/`Fill()`, then follow "Changing binning" below.

The placeholder ranges suit on-spill MC. On KPP calorimeter cosmics (run
125366) 79% of the digi t0s are above 4000 ticks (20 us), so the real axes
must be sized for every running mode the histogram will see. Where one range
cannot serve both, book two views (a short and a full range), as the CRV
client does for digis per event and TDC.

## Adding to the client

1. Declare the axis as a `static constexpr DQMAxis` in `CaloDigiDQM.hh`.
   Detector-indexed axes come from `CaloConst` (`_nChannel` offline SiPM IDs,
   `_nCrystal`, `_nDIRAC` x `_nChPerDIRAC` raw channels), so they cover the whole
   calorimeter whatever is cabled.
2. Book it in `book()` (`hists().book1<TH1F>(...)` / `book2<TH2F>(...)`) and
   add a `DQMH1`/`DQMH2` member and an accessor.
3. Fill it in `Fill()`. A value the data can produce but that has no bin goes
   to an overflow bin and, if it matters, to `diag().Count(...)`, as an
   out-of-range SiPM ID does.
4. Bump `kBinningVersion` and regenerate the catalogue.

Histograms indexed by board and channel need the offline-to-raw channel map
(`CalDAQMap`). The client must not look it up itself: the module reads it
from Proditions and hands the client a plain table, the way the CRV digi
client takes its layout (`Offline/CRVDQM/inc/CRVDQMLayout.hh`). Disk maps
drawn as `THMu2eCaloDisk` are display objects: book a per-crystal histogram
here and let the online module draw the map from it.

A new client (another input tier) is a new class beside this one, added to
`CMakeLists.txt` and `src/SConscript`, with its own table in `fcl/prolog.fcl`
and its own block in the catalogue. Natural next tiers:

- **reco** (`CaloHit`, `CaloCluster`): what Mu2e/DQM `DqmCal` histograms today
  for clusters, which feeds the `cal/*` DQM metrics;
- **DAQ status**: decode errors per DTC, ROC and channel. Offline has no
  persisted calorimeter DAQ-error product yet (the unpackers count
  `CaloHitError`s and print them at end of job; the CRV analogue is
  `CrvDAQerror`), so this client needs the unpacker to persist one first.

Rules that keep files mergeable (from `DQMHelpers/README.md`): binning is a
constant in this package, never FHiCL, conditions or data; everything is
booked in `book()`, never on a first hit; there is no switch that drops a
histogram. FHiCL chooses only extra copies (per subrun, rolling windows) and
what is published online.

## Existing calorimeter DQM to fold in

The histograms below exist today, in other repositories, with their own
binnings. They are the obvious source for the real histogram set:

- Mu2e/DQM `src/DqmCal_module.cc` (digis, clusters), with the metrics in
  `src/DqmMetrics_cal.cc`;
- otsdaq-mu2e-dqm `ArtModules/CaloDigiDQM_module.cc` (the shifter dashboards),
  `CaloDQM_module.cc`, `CaloIERCDQM_module.cc`, `CaloSiDETDQM_module.cc`;
- otsdaq-mu2e-calorimeter `CaloFilterDQM` (artdaq metrics) and
  `CaloDataAnalyzer`.

## Changing binning

1. Edit the `DQMAxis` constant, or add or remove a histogram.
2. Bump `CaloDigiDQM::kBinningVersion`.
3. Regenerate the catalogue: run the offline module with `catalogueFile` set and
   copy the result over `data/CaloDQM_binning_v<N>.txt`:

   ```
   # cat.fcl: #include "DQM/fcl/caloDQM.fcl"
   #          physics.analyzers.caloDigiDQM.catalogueFile : "CaloDQM_binning.txt"
   mu2e -c cat.fcl -s <file with CaloDigis> -n 10
   ```

   The file holds only the `# CaloDigiDQM ...` block and its lines; keep the
   two header comment lines of the reference file above it.

4. Say so in the PR: merging tools and the DQM database see a new version, and
   old files no longer combine with new ones.
