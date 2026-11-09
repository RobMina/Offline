# Optional cut tools and lifecycle observation

A top-level cut can be supplied as an art class tool:

```text
Mu2eG4CommonCut: {
  type: "tool"
  tool: { tool_type: "ExampleCut" }
}
```

`ExampleCut` implements `IMu2eG4Cut`. The constructor receives only the nested `tool` parameter set, not the
outer predicate table or `Mu2eG4ResourceLimits`. Any recorder limits must therefore be configured explicitly and
must not silently truncate training events. Existing built-in predicate syntax and decisions remain available.

A tool that also implements `IMu2eG4Observer` receives lifecycle notifications **only when it is the top-level
common cut**. No callbacks are forwarded through composite predicates or from the other two cut slots.

- `classifiedTrack(track, killed)` follows the existing short-circuited stacking/common decision; it also runs if
  the stacking-only predicate rejects the track. It cannot change the resulting `fKill`/`fUrgent` decision.
- `beginTrack(track)` follows SimParticle-start creation.
- `endTrack(track)` precedes SimParticle-end serialization, exposing the actual G4Track state.

Classification may repeat and a rejected track need not have a final persisted identity or a SimParticle. Tracking
may suspend/postpone and resume: a post-tracking callback is not necessarily a terminal death. Consumers must use
G4 status and cannot assume exactly one callback per physical particle.

Constructors are called for temporary declaration-only instances as well as runtime instances. Keep construction
geometry-independent; do not book output trees there. Use the cut's existing beginEvent/put/deleteCutsData lifecycle
for event-local buffers, including discarded events. `finishConstruction` can be called on every event. Never retain
G4Step/G4Track pointers beyond their callbacks. Ordinary built-in cuts do not implement the observer interface.

A first-exit recorder belongs in this interface but is not supplied by the bridge itself. It must use a real finite
geometry envelope, save the exact crossing state before artificial killing, and distinguish physical termination,
resource failures and boundary handoff. This bridge alone does not certify transport or physics completeness.

Validation after the user build must exercise default cuts, tool loading, accepted/rejected classification,
zero-step tracks and tracking callback order before starting a training campaign.
