#ifndef Mu2eG4_Mu2eG4OffloadTrackingManager_hh
#define Mu2eG4_Mu2eG4OffloadTrackingManager_hh
//
// Geant4 tracking manager of the physics.gpuOffload particles (one per thread, shared by the particles): applies the
// hand-over kill rules, routes tracks starting in the offload regions to the engine and tracks the others on the CPU
// with the stock Geant4 tracking. An engine with its own tracking manager (IMu2eG4Offload::trackingManager) gets
// everything after the kill rules instead (Mu2eG4/doc/gpuOffload.md)
//

#include <set>
#include <string>
#include <vector>

#include "Geant4/G4Navigator.hh"
#include "Geant4/G4VTrackingManager.hh"

class G4Region;
class G4VPhysicalVolume;
class G4VTrackingManager;

namespace mu2e {

  class IMu2eG4Offload;
  class Mu2eG4TrackingAction;

  class Mu2eG4OffloadTrackingManager : public G4VTrackingManager {
  public:
    struct KillRule {
      std::set<int> pdgIds;
      double maxKineticEnergy = 0.;
      std::vector<std::string> volumes;
    };

    // routeAll: the engine receives every track and routes by region itself
    Mu2eG4OffloadTrackingManager(IMu2eG4Offload* engine,
                                 std::vector<std::string> regionNames,
                                 std::vector<KillRule> killRules,
                                 bool routeAll,
                                 int verbose);

    void PreparePhysicsTable(const G4ParticleDefinition& particle) override;
    void BuildPhysicsTable(const G4ParticleDefinition& particle) override;
    void HandOverOneTrack(G4Track* track) override;
    void FlushEvent() override;

    unsigned long nOffloaded() const { return nOffloaded_; }
    unsigned long nKilled() const { return nKilled_; }
    unsigned long nOnCPU() const { return nOnCPU_; }

  private:
    void resolve();
    const G4VPhysicalVolume* locate(const G4Track* track);
    bool killedAtHandOver(const G4Track* track, const G4VPhysicalVolume* volume) const;
    void attachTouchable(G4Track* track);
    void trackOnCPU(G4Track* track);
    static void clearTrajectoryRequest();
    void handToDelegate(G4Track* track);
    bool hasKillRules() const { return !killRules_.empty(); }
    Mu2eG4TrackingAction* mu2eTrackingAction() const;

    IMu2eG4Offload* engine_;
    G4VTrackingManager* delegate_;  // the engine's own tracking manager, or null
    std::vector<std::string> regionNames_;
    std::vector<KillRule> killRules_;
    bool routeAll_;
    int verbose_;

    bool resolved_ = false;
    std::set<const G4Region*> regions_;
    std::vector<std::set<const G4VPhysicalVolume*>> killVolumes_;
    G4Navigator navigator_;

    unsigned long nOffloaded_ = 0;
    unsigned long nKilled_ = 0;
    unsigned long nOnCPU_ = 0;
  };

} // namespace mu2e

#endif /* Mu2eG4_Mu2eG4OffloadTrackingManager_hh */
