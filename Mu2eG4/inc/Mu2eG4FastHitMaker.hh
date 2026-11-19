#ifndef Mu2eG4_Mu2eG4FastHitMaker_hh
#define Mu2eG4_Mu2eG4FastHitMaker_hh
//
// Places a fast-simulation hit in the mass geometry and hands it to the sensitive detector of the
// volume it falls in, if that detector accepts fast hits (one instance per model and thread)
//

#include <memory>

#include "Geant4/G4TouchableHandle.hh"

#include "Offline/Mu2eG4/inc/Mu2eG4FastHit.hh"

class G4Navigator;
class G4VPhysicalVolume;

namespace mu2e {

  class Mu2eG4FastHitMaker {
  public:
    Mu2eG4FastHitMaker();
    ~Mu2eG4FastHitMaker();

    // True if a sensitive detector recorded the hit
    bool make(const Mu2eG4FastHit& hit, const G4FastTrack& track);

    // Volume the last hit fell in (nullptr before the first hit or outside the world), for diagnostics
    const G4VPhysicalVolume* lastVolume() const;

  private:
    std::unique_ptr<G4Navigator> navigator_;
    G4TouchableHandle touchable_;
    bool located_ = false;
  };

} // namespace mu2e

#endif /* Mu2eG4_Mu2eG4FastHitMaker_hh */
