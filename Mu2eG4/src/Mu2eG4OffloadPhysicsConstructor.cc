#include "Offline/Mu2eG4/inc/Mu2eG4OffloadPhysicsConstructor.hh"

#include <string>
#include <vector>

#include "art/Utilities/make_tool.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"

#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4ParticleTable.hh"
#include "Geant4/G4Threading.hh"

#include "Offline/Mu2eG4/inc/IMu2eG4Offload.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4OffloadTrackingManager.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4OffloadTruth.hh"

namespace mu2e {

  Mu2eG4OffloadPhysicsConstructor::Mu2eG4OffloadPhysicsConstructor(const Mu2eG4Config::Physics* phys)
    : G4VPhysicsConstructor("Mu2eG4OffloadPhysicsConstructor")
    , phys_(phys)
  {}

  void Mu2eG4OffloadPhysicsConstructor::ConstructProcess() {
    Mu2eG4Config::GpuOffload config;
    if ( !phys_->gpuOffload(config) ) return;

    // never deleted: at thread exit Geant4's allocators, which tracks and touchables use, are gone
    static thread_local Mu2eG4OffloadTrackingManager* manager = nullptr;
    if ( manager == nullptr ) {
      IMu2eG4Offload* engine = art::make_tool<IMu2eG4Offload>(config.engine.get<fhicl::ParameterSet>()).release();
      engine->construct(G4Threading::IsMasterThread());

      std::vector<Mu2eG4OffloadTrackingManager::KillRule> rules;
      std::vector<fhicl::Table<Mu2eG4Config::GpuOffloadKill>::value_type> killConfig;
      if ( config.handOverKill(killConfig) ) {
        for ( const auto& k : killConfig ) {
          Mu2eG4OffloadTrackingManager::KillRule rule;
          for ( int pdg : k.pdgIds() ) rule.pdgIds.insert(pdg);
          rule.maxKineticEnergy = k.maxKineticEnergy();
          rule.volumes = k.volumes();
          rules.push_back(rule);
        }
      }

      std::vector<std::string> regions{"Mu2eG4Offload"};
      for ( const auto& name : config.extraRegions() ) regions.push_back(name);

      manager = new Mu2eG4OffloadTrackingManager(engine, regions, rules, engine->routesByRegion(), config.verbose());
      Mu2eG4OffloadTruth::instance().setActive();
    }

    for ( const auto& name : config.particles() ) {
      G4ParticleDefinition* particle = G4ParticleTable::GetParticleTable()->FindParticle(name);
      if ( particle == nullptr ) {
        throw cet::exception("CONFIG") << "physics.gpuOffload: unknown particle " << name << "\n";
      }
      particle->SetTrackingManager(manager);
    }
  }

} // namespace mu2e
