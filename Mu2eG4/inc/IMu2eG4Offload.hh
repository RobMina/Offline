#ifndef Mu2eG4_IMu2eG4Offload_hh
#define Mu2eG4_IMu2eG4Offload_hh
//
// art tool interface of the transport engines configured in physics.gpuOffload (see Mu2eG4/doc/gpuOffload.md)
//

class G4ParticleDefinition;
class G4Track;
class G4VTrackingManager;

namespace mu2e {

  class IMu2eG4Offload {
  public:
    virtual ~IMu2eG4Offload() noexcept = default;

    // Called from ConstructProcess on every thread, master included, before the tracking managers are attached
    virtual void construct(bool isMaster) = 0;

    // An engine that brings its own Geant4 tracking manager (AdePT: CPU tracking outside its regions, hand-over on
    // region entry, step replay to the SDs) returns it here, valid after construct(). Mu2eG4OffloadTrackingManager
    // then applies the hand-over kill rules and passes physics tables, tracks and the event flush to it; offload()
    // and physicsTableBuilt() are not called.
    virtual G4VTrackingManager* trackingManager() { return nullptr; }

    // True: offload() gets every track and the engine routes by region and makes the SimParticles of its tracks
    virtual bool routesByRegion() const { return false; }

    // The CPU physics tables of an offloaded particle are built on this thread
    virtual void physicsTableBuilt(const G4ParticleDefinition&) {}

    // Takes ownership of a track inside the offload region; the engine transports and deletes it
    virtual void offload(G4Track* track) = 0;

    // Finishes the event's offloaded tracks: hits to the SDs, tracks to Mu2eG4OffloadTruth, returned tracks to the stack
    virtual void flushEvent() = 0;
  };

} // namespace mu2e

#endif /* Mu2eG4_IMu2eG4Offload_hh */
