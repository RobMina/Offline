//
// Test fast-simulation model: on its first step in the envelope a track deposits its kinetic energy
// as one hit, a fixed depth along its entry direction, and is killed (see Mu2eG4/doc/fastSimulation.md)
//

#include <memory>
#include <string>

#include "art/Utilities/ToolConfigTable.h"
#include "art/Utilities/ToolMacros.h"
#include "fhiclcpp/types/Atom.h"

#include "Geant4/G4FastStep.hh"
#include "Geant4/G4FastTrack.hh"
#include "Geant4/G4Region.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4VFastSimulationModel.hh"

#include "Offline/Mu2eG4/inc/IMu2eG4FastSimModel.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4FastHit.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4FastHitMaker.hh"

namespace mu2e {

  namespace {

    class DepositAtEntryModel : public G4VFastSimulationModel {
    public:
      DepositAtEntryModel(const std::string& name, G4Region* envelope, double minKineticEnergy, double depth)
        : G4VFastSimulationModel(name, envelope)
        , minKineticEnergy_(minKineticEnergy)
        , depth_(depth)
      {}

      // particles are chosen by physics.fastSim.models[].particles
      G4bool IsApplicable(const G4ParticleDefinition&) override { return true; }

      G4bool ModelTrigger(const G4FastTrack& track) override {
        return track.GetPrimaryTrack()->GetKineticEnergy() > minKineticEnergy_;
      }

      void DoIt(const G4FastTrack& track, G4FastStep& step) override {
        Mu2eG4FastHit hit;
        hit.localPosition = track.GetPrimaryTrackLocalPosition() + depth_ * track.GetPrimaryTrackLocalDirection();
        hit.edep = track.GetPrimaryTrack()->GetKineticEnergy();
        hitMaker_.make(hit, track);
        // no energy on the fast step itself: the hit maker records it, and an envelope SD would count it twice
        step.KillPrimaryTrack();
        step.ProposePrimaryTrackPathLength(0.);
      }

    private:
      double minKineticEnergy_;
      double depth_;
      Mu2eG4FastHitMaker hitMaker_;
    };

  } // namespace

  class Mu2eG4FastSimDepositAtEntry : public IMu2eG4FastSimModel {
  public:
    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;
      fhicl::Atom<double> minKineticEnergy {Name("minKineticEnergy"), Comment("Trigger threshold, MeV"), 0.};
      fhicl::Atom<double> depth {Name("depth"), Comment("Distance of the hit from the entry point along the entry direction, mm"), 0.};
    };
    using Parameters = art::ToolConfigTable<Config>;

    explicit Mu2eG4FastSimDepositAtEntry(const Parameters& conf)
      : minKineticEnergy_(conf().minKineticEnergy())
      , depth_(conf().depth())
    {}

    G4VFastSimulationModel* construct(const std::string& name, G4Region* envelope) override {
      model_ = std::make_unique<DepositAtEntryModel>(name, envelope, minKineticEnergy_, depth_);
      return model_.get();
    }

  private:
    double minKineticEnergy_;
    double depth_;
    std::unique_ptr<DepositAtEntryModel> model_;
  };

} // namespace mu2e

DEFINE_ART_CLASS_TOOL(mu2e::Mu2eG4FastSimDepositAtEntry)
