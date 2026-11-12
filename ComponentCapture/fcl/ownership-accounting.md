# Ancestry links, boundary-born ownership, manifest and neighbour partition

**Diagnostic capture only; never response-bank admissible.**

## Links product

`ComponentRecorder` writes `ComponentCaptureLinks` (`componentCaptureLinks`) next to `ComponentCapture`. It holds
one row per captured track, in the same order:
- `creatorStep`: index into `ComponentCapture::steps` of the parent step that created the track, or -1 for the
  primary. The link comes from the step's own list of current-step secondaries, matched at classification, not
  inferred from positions.
- `bornOnSurface`: the track starts on the envelope surface.
- `exitAtBirth`: see the ownership rule below.

`ComponentCaptureDump` writes the links as the `links` tree (aligned with `tracks`) and the manifest as the
per-event `manifest` tree.

## Boundary-born ownership

Ownership follows the creating step and the starting volume chosen by the Geant4 navigator, never position alone:

- **Owned.** The creating step starts inside the component and the child starts in an envelope volume. The child
  is captured normally; `bornOnSurface` marks a start on the surface. Its first step may be a zero-length first exit.
- **Exit at birth.** The creating step starts inside, but the navigator places the child outside. The recorder
  kills it at stacking and records it with its birth state as `reason=4`, zero steps and a killed classification.
  This is the only stacking kill that is not an error. Such children are not tracked, so they are not in the
  StatusG4 track count.
- **Error.** A child of a step that starts outside the component: `secondary_inside_from_outside_interaction` if it
  starts inside, or `secondary_born_outside_envelope` if it starts outside.

Natural transport makes surface births only through an interaction within the Geant4 surface tolerance of the
envelope face, so they are expected to be essentially absent. `boundary-born.fcl` (run 17, five events) exercises
all the cases with **constructed objects, not physics transport**: owned inward, owned outward with a zero-length
exit, exit at birth, and the two error cases. `callback-fixture.fcl` cases 1–4 now resolve by the same rule.

## Natural stops on a boundary

A track that loses all its kinetic energy on a step that also reaches a boundary ends with `Transportation` as its
stopping process. The recorder classifies it as a natural stop, not a resource kill. `Mu2eG4TrackingAction` counts
it as a field-propagator kill (`nKilledByFieldPropagator`, `StatusG4` status 1) unless it is an electron or a proton,
whose zero range is below `rangeToIgnore`; a 4 MeV deuteron ranging out in 0.09 mm of aluminium is an example.
`ComponentCaptureDump` recomputes that count from the record (`nNaturalBoundaryStops`: tracks ended by
`Transportation` at zero kinetic energy, other than electrons and protons, whose last step has the same pre and post
placement) and accepts status 1 only when there are no step-limit kills, no overflow and `nKilledByFieldPropagator`
equals that count. Any other field-propagator kill still invalidates the event. Mu2eG4 itself is unchanged.

## Manifest

Each event's links product carries a canonical manifest text and its FNV-1a 64-bit hash (`manifestHash`):
- the Geant4 version and the physics tag;
- every material in the volume table: density, mean excitation energy, state, temperature, element mass fractions;
- every used material-cuts couple: range cuts and energy thresholds for gamma, e-, e+ and proton;
- the ordered process list of the nine pilot species.

Geometry is not part of the manifest. The checkers recompute the hash, and comparisons require equal hashes.

## Energy accounting

`mu2efastsim.capture_accounting` balances every inside step in total energy, with rest masses explicit. It also
balances every event: entrance plus medium electron mass = deposits + first exits + exits at birth + energy left
inside + nuclear Q + declared decay Q + residuals. Steps of EM and transport processes must close. Hadronic steps
report Q as an explicit nuclear term.

Decay steps carry a declared term Q_decay = E_parent − ΣE_children, gated step by step. Geant4's
`G4MuonDecayChannel` (μ → e± and two neutrinos) gives daughters that total m_μ − m_e, so its declared Q_decay
is m_e. This is Geant4's own behaviour, not tuned by us, and every manifest declares it
(`declared|G4MuonDecayChannel|...`). Any other decay must conserve energy. A decay step off its declared value
fails and is listed.

## Neighbour partition

Envelopes never overlap; Geant4 overlap checks are fatal. Air inside an envelope belongs to that component; gaps
and supports outside envelopes belong to the caller. `crv-r1-pair.fcl` (run 18) places R1 module 1 with its own
unrecorded `NeighbourEnvelope` touching module 0's envelope on the local +x face. The layout is synthetic: in the
source layout the staggered R1 module envelopes overlap, and the construction log reports the overlap
(`sourceLayoutEnvelopeOverlap`). `mu2efastsim.capture_partition` requires that:
- no recorded step starts in the neighbour;
- the inside flags match the component subtree;
- every crossing into the neighbour is a first exit on the shared face.

Later fixtures: a multi-recorder bridge to capture both modules in one job, and the staggered CRV layout with
trimmed envelopes.
