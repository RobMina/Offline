#include "Offline/Mu2eG4/inc/Mu2eG4OffloadTrackingManager.hh"

#include "cetlib_except/exception.h"

#include "Geant4/G4EventManager.hh"
#include "Geant4/G4LogicalVolume.hh"
#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4PhysicalVolumeStore.hh"
#include "Geant4/G4ProcessManager.hh"
#include "Geant4/G4ProcessVector.hh"
#include "Geant4/G4Region.hh"
#include "Geant4/G4RegionStore.hh"
#include "Geant4/G4StackManager.hh"
#include "Geant4/G4TouchableHandle.hh"
#include "Geant4/G4TouchableHistory.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4TrackStatus.hh"
#include "Geant4/G4TrackingManager.hh"
#include "Geant4/G4TransportationManager.hh"
#include "Geant4/G4VPhysicalVolume.hh"
#include "Geant4/G4VProcess.hh"
#include "Geant4/G4ios.hh"

#include "Offline/MCDataProducts/inc/ProcessCode.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Offload.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4TrackingAction.hh"

namespace mu2e {

  Mu2eG4OffloadTrackingManager::Mu2eG4OffloadTrackingManager(IMu2eG4Offload* engine,
                                                             std::vector<std::string> regionNames,
                                                             std::vector<KillRule> killRules,
                                                             bool routeAll,
                                                             int verbose)
    : engine_(engine)
    , regionNames_(std::move(regionNames))
    , killRules_(std::move(killRules))
    , routeAll_(routeAll)
    , verbose_(verbose)
  {}

  // The particle's CPU processes keep their tables: tracks outside the offload regions use them
  void Mu2eG4OffloadTrackingManager::PreparePhysicsTable(const G4ParticleDefinition& particle) {
    G4ProcessManager* manager = particle.GetProcessManager();
    const bool master = manager == particle.GetMasterProcessManager();
    G4ProcessVector* processes = manager->GetProcessList();
    for ( G4int i = 0; i < static_cast<G4int>(processes->size()); ++i ) {
      if ( master ) (*processes)[i]->PreparePhysicsTable(particle);
      else (*processes)[i]->PrepareWorkerPhysicsTable(particle);
    }
  }

  void Mu2eG4OffloadTrackingManager::BuildPhysicsTable(const G4ParticleDefinition& particle) {
    G4ProcessManager* manager = particle.GetProcessManager();
    const bool master = manager == particle.GetMasterProcessManager();
    G4ProcessVector* processes = manager->GetProcessList();
    for ( G4int i = 0; i < static_cast<G4int>(processes->size()); ++i ) {
      if ( master ) (*processes)[i]->BuildPhysicsTable(particle);
      else (*processes)[i]->BuildWorkerPhysicsTable(particle);
    }
    engine_->physicsTableBuilt(particle);
  }

  // Regions and volumes exist once the geometry is built; resolved at the first track of the thread
  void Mu2eG4OffloadTrackingManager::resolve() {
    for ( const auto& name : regionNames_ ) {
      const G4Region* region = G4RegionStore::GetInstance()->GetRegion(name, false);
      if ( region == nullptr ) {
        throw cet::exception("CONFIG") << "physics.gpuOffload: no G4Region named " << name << "\n";
      }
      regions_.insert(region);
    }
    for ( const auto& rule : killRules_ ) {
      std::set<const G4VPhysicalVolume*> volumes;
      for ( const G4VPhysicalVolume* pv : *G4PhysicalVolumeStore::GetInstance() ) {
        for ( const auto& name : rule.volumes ) {
          if ( pv->GetName() == name ) volumes.insert(pv);
        }
      }
      for ( const auto& name : rule.volumes ) {
        bool found = false;
        for ( const G4VPhysicalVolume* pv : volumes ) found = found || pv->GetName() == name;
        if ( !found ) throw cet::exception("CONFIG") << "physics.gpuOffload.handOverKill: no volume " << name << "\n";
      }
      killVolumes_.push_back(std::move(volumes));
    }
    navigator_.SetWorldVolume(
      G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume());
    resolved_ = true;
  }

  // The volume the track moves into from its start point (a primary on a volume face belongs to the volume ahead)
  const G4VPhysicalVolume* Mu2eG4OffloadTrackingManager::locate(const G4Track* track) {
    const G4ThreeVector direction = track->GetMomentumDirection();
    return navigator_.LocateGlobalPointAndSetup(track->GetPosition(), &direction, false, false);
  }

  bool Mu2eG4OffloadTrackingManager::killedAtHandOver(const G4Track* track, const G4VPhysicalVolume* volume) const {
    for ( size_t i = 0; i < killRules_.size(); ++i ) {
      const auto& rule = killRules_[i];
      if ( rule.pdgIds.count(track->GetDefinition()->GetPDGEncoding()) == 0 ) continue;
      if ( track->GetKineticEnergy() >= rule.maxKineticEnergy ) continue;
      if ( killVolumes_[i].count(volume) ) return true;
    }
    return false;
  }

  // Valid only right after locate() of the same track
  void Mu2eG4OffloadTrackingManager::attachTouchable(G4Track* track) {
    track->SetTouchableHandle(G4TouchableHandle(navigator_.CreateTouchableHistory()));
  }

  Mu2eG4TrackingAction* Mu2eG4OffloadTrackingManager::mu2eTrackingAction() const {
    return dynamic_cast<Mu2eG4TrackingAction*>(G4EventManager::GetEventManager()->GetUserTrackingAction());
  }

  void Mu2eG4OffloadTrackingManager::HandOverOneTrack(G4Track* track) {
    if ( !resolved_ ) resolve();
    const G4VPhysicalVolume* volume = locate(track);

    if ( volume != nullptr && killedAtHandOver(track, volume) ) {
      attachTouchable(track);
      if ( auto* action = mu2eTrackingAction() ) {
        action->beginOffloadedParticle(track);
        action->endOffloadedParticle(track, ProcessCode(ProcessCode::mu2eKillerVolume));
      }
      ++nKilled_;
      delete track;
      return;
    }

    if ( routeAll_ ) {
      ++nOffloaded_;
      engine_->offload(track);
      return;
    }

    if ( volume != nullptr && regions_.count(volume->GetLogicalVolume()->GetRegion()) ) {
      attachTouchable(track);
      if ( auto* action = mu2eTrackingAction() ) {
        action->beginOffloadedParticle(track);
        action->endOffloadedParticle(track, ProcessCode(ProcessCode::mu2eGpuTransport));
      }
      ++nOffloaded_;
      engine_->offload(track);
      return;
    }

    ++nOnCPU_;
    trackOnCPU(track);
  }

  // Stock Geant4 tracking, user actions included; this manager owns the track and stacks its secondaries
  void Mu2eG4OffloadTrackingManager::trackOnCPU(G4Track* track) {
    G4EventManager* eventManager = G4EventManager::GetEventManager();
    G4TrackingManager* trackingManager = eventManager->GetTrackingManager();
    trackingManager->ProcessOneTrack(track);
    eventManager->StackTracks(trackingManager->GimmeSecondaries());
    const G4TrackStatus status = track->GetTrackStatus();
    if ( status == fSuspend || status == fPostponeToNextEvent ) {
      eventManager->GetStackManager()->PushOneTrack(track);
    } else {
      delete track;
    }
  }

  void Mu2eG4OffloadTrackingManager::FlushEvent() {
    engine_->flushEvent();
    if ( verbose_ > 1 ) {
      G4cout << "Mu2eG4OffloadTrackingManager: offloaded " << nOffloaded_ << " killed at hand-over " << nKilled_
             << " on CPU " << nOnCPU_ << " (thread totals)" << G4endl;
    }
  }

} // namespace mu2e
