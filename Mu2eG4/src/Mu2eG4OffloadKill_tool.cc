//
// physics.gpuOffload test engine without a device: offloaded tracks end at hand-over (the ceiling of an
// infinitely fast engine); optionally one daughter per track is registered to exercise the truth path
//

#include "art/Utilities/ToolConfigTable.h"
#include "art/Utilities/ToolMacros.h"
#include "fhiclcpp/types/Atom.h"

#include "Geant4/G4Track.hh"
#include "Geant4/G4ios.hh"

#include "Offline/Mu2eG4/inc/IMu2eG4Offload.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4OffloadTruth.hh"

namespace mu2e {

  class Mu2eG4OffloadKill : public IMu2eG4Offload {
  public:
    struct Config {
      fhicl::Atom<bool> registerDaughter{fhicl::Name("registerDaughter"),
          fhicl::Comment("Register a photon daughter of each offloaded track with Mu2eG4OffloadTruth"), false};
      fhicl::Atom<int> verbose{fhicl::Name("verbose"), 0};
    };
    using Parameters = art::ToolConfigTable<Config>;

    explicit Mu2eG4OffloadKill(const Parameters& conf)
      : registerDaughter_(conf().registerDaughter())
      , verbose_(conf().verbose())
    {}

    void construct(bool) override {}

    void offload(G4Track* track) override {
      if ( registerDaughter_ ) {
        Mu2eG4OffloadParticle p;
        p.trackId = Mu2eG4OffloadTruth::instance().newTrackId();
        p.parentTrackId = track->GetTrackID();
        p.pdgId = 22;
        p.creationCode = Mu2eG4OffloadTruth::inferCreationCode(track->GetDefinition()->GetPDGEncoding(), p.pdgId);
        p.startPosition = track->GetPosition();
        p.startMomentum = CLHEP::HepLorentzVector(track->GetMomentum(), track->GetMomentum().mag());
        p.startGlobalTime = track->GetGlobalTime();
        p.startVolume = track->GetVolume();
        Mu2eG4OffloadTruth::instance().add(p);
      }
      ++nEvent_;
      delete track;
    }

    void flushEvent() override {
      if ( verbose_ > 0 ) G4cout << "Mu2eG4OffloadKill: " << nEvent_ << " tracks this event" << G4endl;
      nEvent_ = 0;
    }

  private:
    bool registerDaughter_;
    int verbose_;
    unsigned long nEvent_ = 0;
  };

} // namespace mu2e

DEFINE_ART_CLASS_TOOL(mu2e::Mu2eG4OffloadKill)
