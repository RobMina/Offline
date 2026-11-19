#ifndef Mu2eG4_IMu2eG4FastSimModel_hh
#define Mu2eG4_IMu2eG4FastSimModel_hh
//
// art tool interface of the Geant4 fast-simulation models configured in physics.fastSim
// (see Mu2eG4/doc/fastSimulation.md)
//

#include <string>

class G4Region;
class G4VFastSimulationModel;

namespace mu2e {

  class IMu2eG4FastSimModel {
  public:
    virtual ~IMu2eG4FastSimModel() noexcept = default;

    // Build this thread's model on the envelope region; the tool keeps ownership
    virtual G4VFastSimulationModel* construct(const std::string& name, G4Region* envelope) = 0;
  };

} // namespace mu2e

#endif /* Mu2eG4_IMu2eG4FastSimModel_hh */
