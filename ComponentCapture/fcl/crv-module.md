# Canonical CRV module candidate

`crv-r1.fcl` captures one event through isolated `CRV_R1` module 0 from pinned
`Offline/Mu2eG4/geom/crv_counters_v10.txt`. It is not the Run 1A or extracted KPP construction.
**The 2026-11-06 user build, nominal/transformed one-event strict audits and runtime inventory checks pass.**
These are bounded construction/coordinate checks, not physics-distribution equivalence or bank certification.

The tool uses CosmicRayShieldMaker data and constructs module-layer air volumes, bars, absorbers, strongback/cover,
and all module FEB boxes beneath one finite air envelope. The source CMB switch is respected: v10 hides CMBs.
There are 0 placed/128 omitted CMBs; this source-faithful variant is not a physically complete-electronics module.
A CMB-enabled variant needs an explicit config change and distinct geometry tag.

All geometry uses source Mu2e-aligned axes about the computed bounds center. The envelope alone receives
`captureCRV.localToWorld` (proper active row-major R) and `captureCRV.translation`. Its clearance is 0.001 mm.
Global supports are not copied; any source-frame overlap with the envelope aborts. This isolates module transport,
not a valid neighboring-module partition: staggered air envelopes can overlap and require explicit ownership.

The nominal source is a 1000 MeV/c mu- from (-1000,0,0) mm along +x, perpendicular to R1's side layers; world
half-size is 5000 mm, run 5, seed base 314359. Its recorded local entrance is (-111.58850000000007,0,0) mm,
p=999.9999999999998 MeV/c along +x, time 2.9799172350778145 ns. The primary traverses 888.4115 mm of vacuum,
then 99.526 mm of envelope air before aluminum at x=-12.0625 mm, then layer/bar (3,9), (2,8), (1,7), (0,6).
The recorded primary traversal confirms this sequence and misses FEBs; it does not test electronics transport.
Finite-density vacuum approach deposition is preserved separately, not forced to zero.

Every placement checks overlaps and logs its name, mother, source copy, offset, box half-size and material.
Per-constituent dimensions are archived at full precision; the source-origin summary was printed at shorter
precision, so it is not full-precision evidence for all source bounds. Bar rows give local ID and source
shield/module/layer/bar identity. The persistent volume table has names/copy/material/density/relative transforms,
but not constituent dimensions; preserve the geometry log alongside ROOT output. It is not a complete
material/isotope manifest.

## Runtime-confirmed inventory and traversal

All overlap checks pass. The source-derived inventory is now confirmed at runtime: 4 layers, 64 bars (source
copies 0--63), 3 absorbers, 2 aluminum sheets, 6 FEBs, 0 placed/128 omitted CMBs, and 1 checked global support.
There are 80 component placements including the envelope, or 82 volume rows including the two vacuum ancestors,
with 64 mapped bar IDs. Material census: 64 scintillator placements at 1.06 g/cm3, 6 ElectronicsFEB at 0.58,
5 G4_Al at 2.699, 5 air at 0.00120479, and 2 vacuum ancestors.

Source-derived bounds are (-6641.775,-2019.470,-2266.020) to (-6418.600,2570.530,-1317.180) mm,
origin (-6530.1875,275.530,-1791.600) mm; envelope half-size including clearance is (111.5885,2295.001,474.421) mm.
The shared support is well separated for this candidate; other module selections must still check overlap.

| Fresh one-event test | Tracks | Steps | Volume rows | First exits | Natural ends | Component deposit (MeV) |
|---|---:|---:|---:|---:|---:|---:|
| Box regression | 57 | 116 | 3 | 1 | 56 | 18.541152425084263 |
| Nominal R1 | 90 | 202 | 82 | 1 | 89 | 36.961493634894936 |
| Transformed R1 | 86 | 199 | 82 | 1 | 85 | 33.59866941329343 |
| FEB-directed R1 | 129 | 272 | 82 | 1 | 128 | 32.759215921177955 |

All four exit art successfully and pass strict audits against one expected completed event. Nominal R1 has 89
children; both R1 trials have approach deposit 2.470540323626122e-23 MeV. The vectorized companion
`mu2efastsim.capture_inventory` strictly audits first, then reports inventory and primary traversal; `inventory.json`
is archived for both R1 trials. The combined local suite passes 203 tests, including 18 new inventory tests.

`crv-r1-rotated.fcl` uses run 7, seed base 314559, R=[[.48,-.8,.36],[.64,.6,.48],[-.6,0,.8]],
t=(123,-87,41) mm, source=(-357,-727,641) mm and direction=(.48,.64,-.6). Local entrance agrees with nominal to
8.6e-14 mm and 5.7e-14 MeV/c. This one-event geometry covariance check does not establish response-distribution
equivalence. An initial transformed attempt failed on a SimpleConfig duplicate `point` from an include/override;
its failed evidence is preserved. The successful fixture uses standalone explicit lists, not `allowReplacement`.

## Material extraction and remaining checks

`constructCRVElectronicsMaterials` shares the exact Polyethylene/Electronics/FEB/CMB recipes with standard
`ConstructMaterials`. Standard construction invokes the two helper stages at the original positions, preserving
material/element creation order and duplicate-name errors. Study geometry invokes the whole closure once. No global
standard-material initialization is enabled in study mode. Recipes remain Polyethylene 0.956 g/cm3 (C:H atoms 1:2),
Electronics 0.58 g/cm3 (Si/Al/Au/Cu/Polyethylene mass fractions .103/.034/.034/.691/.138), and same-density aliases.
These electronics measurements are attributed in the pinned source to Gary Drake at Argonne, conveyed by Vitaly Pronskikh.

The new plugin and shared-helper symbols were verified after the user build. `standard-materials.fcl`, derived from
stock `gdmldump_run1_a`, completed successfully (run 6, seed base 314459); its full material table confirms those
densities and source element fractions. It uses Minimal physics and a constant stacking cut rejecting **all primaries**:
not physics capture. There is no pre-extraction runtime A/B; recipe and creation-order comparison is static only.

`crv-r1-feb.fcl` passes its one-event strict audit: run 8, seed base 314659, source=(-1000,-2035,-63) mm along +x.
The recorded entrance is (-111.58850000000007,-2035,-63) mm. The primary traverses FEBs 3 and 0 in 32 steps,
20.00144444840868 mm total path and 1.2344484228089634 MeV direct primary deposit in ElectronicsFEB. These are
primary-only FEB sums, not full-shower electronics energy; all 129 tracks/272 steps remain in the audited capture.
Its `crv-r1-feb.gdml` records FEB density 0.58 g/cm3 and elemental mass fractions Si .103, Al .034, Au .034,
Cu .691, C .118166876291217, H .0198331237087827, consistent with the shared recipe. GDML uses build-local pointer
suffixes and contains only referenced geometry/materials; it is not a canonical complete physics/runtime manifest.

Dedicated module neutral/stopping transport, broader passive/electronics coverage, independent response-distribution
comparisons, verified manifests, boundary-born ownership, physics accounting and neighbor partition remain open.
The simple-box neutral/stopping fixtures and consecutive rejection/reset diagnostics now pass their respective
checks; see [box fixtures](README.md) and [lifecycle evidence](lifecycle.md), not module-physics certification.
`bankAdmissible=false` always. Earlier box baselines are preserved; no production library, tarball or UVA dispatch
is implied by these checks.
