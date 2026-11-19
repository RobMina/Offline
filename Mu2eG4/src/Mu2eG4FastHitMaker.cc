//
// Places a fast-simulation hit in the mass geometry and hands it to a fast-hit sensitive detector
//

#include "Offline/Mu2eG4/inc/Mu2eG4FastHitMaker.hh"

#include "Geant4/G4AffineTransform.hh"
#include "Geant4/G4FastTrack.hh"
#include "Geant4/G4LogicalVolume.hh"
#include "Geant4/G4Navigator.hh"
#include "Geant4/G4TouchableHistory.hh"
#include "Geant4/G4TransportationManager.hh"
#include "Geant4/G4VPhysicalVolume.hh"
#include "Geant4/G4VSensitiveDetector.hh"

namespace mu2e {

  Mu2eG4FastHitMaker::Mu2eG4FastHitMaker()
    : navigator_(std::make_unique<G4Navigator>())
    , touchable_(new G4TouchableHistory())
  {}

  Mu2eG4FastHitMaker::~Mu2eG4FastHitMaker() = default;

  const G4VPhysicalVolume* Mu2eG4FastHitMaker::lastVolume() const {
    return located_ ? touchable_->GetVolume() : nullptr;
  }

  bool Mu2eG4FastHitMaker::make(const Mu2eG4FastHit& hit, const G4FastTrack& track) {
    if (hit.edep <= 0.) return false;

    if (!located_) {
      navigator_->SetWorldVolume(G4TransportationManager::GetTransportationManager()
                                 ->GetNavigatorForTracking()->GetWorldVolume());
    }
    const G4ThreeVector global = track.GetInverseAffineTransformation()->TransformPoint(hit.localPosition);
    // relative search from the last located point once there is one
    navigator_->LocateGlobalPointAndUpdateTouchable(global, touchable_(), located_);
    located_ = true;

    G4VPhysicalVolume* volume = touchable_->GetVolume();
    if (volume == nullptr) return false;
    G4VSensitiveDetector* sd = volume->GetLogicalVolume()->GetSensitiveDetector();
    if (sd == nullptr || !sd->isActive()) return false;
    auto fastSD = dynamic_cast<IMu2eG4FastHitSD*>(sd);
    if (fastSD == nullptr) return false;

    fastSD->processFastHit(hit, global, track, *touchable_());
    return true;
  }

} // namespace mu2e
