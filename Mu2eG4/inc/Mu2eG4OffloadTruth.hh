#ifndef Mu2eG4_Mu2eG4OffloadTruth_hh
#define Mu2eG4_Mu2eG4OffloadTruth_hh
//
// Per-thread registry of the tracks an offload engine transported, turned into SimParticles at the end of the event.
// Engine tracks get Geant4 track IDs from firstTrackId on, so their SimParticle keys never collide with CPU tracks;
// the steps an engine hands to a sensitive detector carry these IDs.
//

#include <vector>

#include "CLHEP/Vector/LorentzVector.h"
#include "CLHEP/Vector/ThreeVector.h"

#include "Offline/MCDataProducts/inc/ProcessCode.hh"

class G4VPhysicalVolume;

namespace mu2e {

  struct Mu2eG4OffloadParticle {
    int trackId = 0;                    // from Mu2eG4OffloadTruth::newTrackId()
    int parentTrackId = 0;              // CPU track it descends from, or another engine track registered before it
    int pdgId = 0;
    ProcessCode creationCode;
    CLHEP::Hep3Vector startPosition;    // Geant4 global frame, mm
    CLHEP::HepLorentzVector startMomentum;
    double startGlobalTime = 0.;
    const G4VPhysicalVolume* startVolume = nullptr;  // null if the engine does not know it
    bool hasEnd = false;
    CLHEP::Hep3Vector endPosition;
    CLHEP::HepLorentzVector endMomentum;
    double endGlobalTime = 0.;
    const G4VPhysicalVolume* endVolume = nullptr;
    ProcessCode stoppingCode;
  };

  class Mu2eG4OffloadTruth {
  public:
    static constexpr int firstTrackId = 1 << 28;

    static bool isOffloadTrackId(int trackId) { return trackId >= firstTrackId; }

    // Creation code from the parent and child species, for engines that do not return the process
    static ProcessCode inferCreationCode(int parentPdgId, int pdgId);

    // This thread's registry
    static Mu2eG4OffloadTruth& instance();

    // True once a physics.gpuOffload engine is attached on this thread
    bool active() const { return active_; }
    void setActive() { active_ = true; }

    void beginEvent();
    int newTrackId() { return next_++; }
    void add(const Mu2eG4OffloadParticle& particle) { particles_.push_back(particle); }
    const std::vector<Mu2eG4OffloadParticle>& particles() const { return particles_; }

  private:
    bool active_ = false;
    int next_ = firstTrackId;
    std::vector<Mu2eG4OffloadParticle> particles_;
  };

} // namespace mu2e

#endif /* Mu2eG4_Mu2eG4OffloadTruth_hh */
