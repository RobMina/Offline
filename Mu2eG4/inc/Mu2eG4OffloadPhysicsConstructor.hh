#ifndef Mu2eG4_Mu2eG4OffloadPhysicsConstructor_hh
#define Mu2eG4_Mu2eG4OffloadPhysicsConstructor_hh
//
// Attaches the physics.gpuOffload tracking manager and engine to the offloaded particles on every thread
//

#include "Geant4/G4VPhysicsConstructor.hh"

#include "Offline/Mu2eG4/inc/Mu2eG4Config.hh"

namespace mu2e {

  class Mu2eG4OffloadPhysicsConstructor : public G4VPhysicsConstructor {
  public:
    explicit Mu2eG4OffloadPhysicsConstructor(const Mu2eG4Config::Physics* phys);

    void ConstructParticle() override {}
    void ConstructProcess() override;

  private:
    const Mu2eG4Config::Physics* phys_;
  };

} // namespace mu2e

#endif /* Mu2eG4_Mu2eG4OffloadPhysicsConstructor_hh */
