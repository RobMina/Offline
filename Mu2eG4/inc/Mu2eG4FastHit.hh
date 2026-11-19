#ifndef Mu2eG4_Mu2eG4FastHit_hh
#define Mu2eG4_Mu2eG4FastHit_hh
//
// One energy deposit made by a Geant4 fast-simulation model, and the interface of the
// sensitive detectors that accept it (see Mu2eG4/doc/fastSimulation.md)
//

#include "Geant4/G4ThreeVector.hh"
#include "Geant4/G4VTouchable.hh"

class G4FastTrack;

namespace mu2e {

  // Name of the process G4FastSimulationPhysics attaches in the mass geometry (ProcessCode::fastSimProcess_massGeom)
  inline constexpr char fastSimProcessName[] = "fastSimProcess_massGeom";

  struct Mu2eG4FastHit {
    G4ThreeVector localPosition;  // in the envelope frame of the G4FastTrack
    double edep = 0.;             // total deposited energy
    double visibleEdep = -1.;     // quenched (Birks) energy; negative means equal to edep
    double time = -1.;            // global time; negative means the time of the primary track
    double stepLength = 0.;
  };

  // Mixin for the sensitive detectors that record fast-simulation hits
  class IMu2eG4FastHitSD {
  public:
    virtual ~IMu2eG4FastHitSD() = default;
    virtual void processFastHit(const Mu2eG4FastHit& hit,
                                const G4ThreeVector& globalPosition,
                                const G4FastTrack& track,
                                const G4VTouchable& touchable) = 0;
  };

} // namespace mu2e

#endif /* Mu2eG4_Mu2eG4FastHit_hh */
