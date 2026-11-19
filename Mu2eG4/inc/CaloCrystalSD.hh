#ifndef Mu2eG4_CaloCrystalSD_hh
#define Mu2eG4_CaloCrystalSD_hh
//
// Define a sensitive detector for calorimetric crystals
//
// Original author Ivan Logashenko
//

// Mu2e includes
#include "Offline/Mu2eG4/inc/Mu2eG4SensitiveDetector.hh"

namespace mu2e {

  class CaloCrystalSD : public Mu2eG4SensitiveDetector, public IMu2eG4FastHitSD {

  public:

    CaloCrystalSD(G4String, const SimpleConfig& config);

    G4bool ProcessHits(G4Step*, G4TouchableHistory*) override;

    void processFastHit(const Mu2eG4FastHit& hit, const G4ThreeVector& globalPosition,
                        const G4FastTrack& track, const G4VTouchable& touchable) override;

  };

} // namespace mu2e

#endif /* Mu2eG4_CaloCrystalSD_hh */
