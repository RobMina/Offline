// Diagnostic export of finalized SimParticles, never response-bank admissible.
#include <cstdint>
#include <string>

#include "TTree.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"
#include "canvas/Utilities/InputTag.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "Offline/MCDataProducts/inc/ComponentCapture.hh"
#include "Offline/MCDataProducts/inc/SimParticle.hh"
#include "Offline/MCDataProducts/inc/StatusG4.hh"

namespace mu2e {
namespace {
// Explicit leaf types keep signed 64-bit identities portable across ROOT platforms.
void branch(TTree* tree, std::string const& name, std::int64_t& value) {
  static_assert(sizeof(std::int64_t) == sizeof(Long64_t), "64-bit ROOT identity required");
  tree->Branch(name.c_str(), &value, (name + "/L").c_str());
}
void branch(TTree* tree, std::string const& name, int& value) {
  tree->Branch(name.c_str(), &value, (name + "/I").c_str());
}
void branch(TTree* tree, std::string const& name, bool& value) {
  tree->Branch(name.c_str(), &value, (name + "/O").c_str());
}
void branch(TTree* tree, std::string const& name, double& value) {
  tree->Branch(name.c_str(), &value, (name + "/D").c_str());
}
void branch(TTree* tree, std::string const& name, std::string& value) {
  tree->Branch(name.c_str(), &value);
}

struct StateRow {
  double x{}, y{}, z{}, px{}, py{}, pz{}, energy{}, time{}, properTime{};
};

void stateBranches(TTree* tree, std::string const& prefix, StateRow& row) {
  branch(tree, prefix + "x", row.x);
  branch(tree, prefix + "y", row.y);
  branch(tree, prefix + "z", row.z);
  branch(tree, prefix + "px", row.px);
  branch(tree, prefix + "py", row.py);
  branch(tree, prefix + "pz", row.pz);
  branch(tree, prefix + "energy", row.energy);
  branch(tree, prefix + "time", row.time);
  branch(tree, prefix + "properTime", row.properTime);
}

StateRow state(CLHEP::Hep3Vector const& position, CLHEP::HepLorentzVector const& momentum,
               double time, double properTime) {
  return {position.x(), position.y(), position.z(), momentum.px(), momentum.py(),
          momentum.pz(), momentum.e(), time, properTime};
}

struct EventRow {
  std::int64_t run{}, subrun{}, event{}, nSims{};
  bool diagnosticOnly{true}, bankAdmissible{false};
  bool capturePresent{}, simsPresent{}, statusPresent{};
};

struct SimRow {
  std::int64_t index{}, key{}, track{}, parent{}, nDaughters{}, simStage{};
  int pdg{}, startStatus{}, endStatus{}, nSteps{};
  bool endDefined{}, isPrimary{}, genPresent{}, parentPresent{};
  std::string creationCode, stoppingCode;
  double trackLength{}, endKE{};
  StateRow start, end;
};
} // namespace

class ComponentZeroStepDump : public art::EDAnalyzer {
 public:
  struct Config {
    fhicl::Atom<bool> diagnosticOnly{fhicl::Name("diagnosticOnly")};
    fhicl::Atom<art::InputTag> source{fhicl::Name("source"), art::InputTag("g4run:componentCapture")};
    fhicl::Atom<art::InputTag> sims{fhicl::Name("sims"), art::InputTag("g4run")};
    fhicl::Atom<art::InputTag> status{fhicl::Name("status"), art::InputTag("g4run")};
  };
  using Parameters = art::EDAnalyzer::Table<Config>;
  explicit ComponentZeroStepDump(Parameters const& p)
    : art::EDAnalyzer(p), source_(p().source()), simsSource_(p().sims()),
      statusSource_(p().status()) {
    if (!p().diagnosticOnly()) {
      throw cet::exception("ComponentZeroStepDump") << "diagnosticOnly must be true\n";
    }
    consumes<ComponentCapture>(source_);
    consumes<SimParticleCollection>(simsSource_);
    consumes<StatusG4>(statusSource_);
  }
  void beginJob() override;
  void analyze(art::Event const&) override;

 private:
  TTree* table(char const* name, char const* title);
  art::InputTag source_, simsSource_, statusSource_;
  TTree *events_{}, *sims_{};
  EventRow eventRow_;
  SimRow simRow_;
};

TTree* ComponentZeroStepDump::table(char const* name, char const* title) {
  // TFileService owns both trees in this module's directory and finalizes the file.
  art::ServiceHandle<art::TFileService> fs;
  auto* tree = fs->make<TTree>(name, title);
  branch(tree, "run", eventRow_.run);
  branch(tree, "subrun", eventRow_.subrun);
  branch(tree, "event", eventRow_.event);
  return tree;
}

void ComponentZeroStepDump::beginJob() {
  events_ = table("zeroStepEvents", "Every diagnostic event, including missing products");
  branch(events_, "diagnosticOnly", eventRow_.diagnosticOnly);
  branch(events_, "bankAdmissible", eventRow_.bankAdmissible);
  branch(events_, "capturePresent", eventRow_.capturePresent);
  branch(events_, "simsPresent", eventRow_.simsPresent);
  branch(events_, "statusPresent", eventRow_.statusPresent);
  branch(events_, "nSims", eventRow_.nSims);

  sims_ = table("zeroStepSims", "All finalized SimParticles, without endpoint or step-count selection");
  branch(sims_, "index", simRow_.index);
  branch(sims_, "key", simRow_.key);
  branch(sims_, "track", simRow_.track);
  branch(sims_, "parent", simRow_.parent);
  branch(sims_, "nDaughters", simRow_.nDaughters);
  // Signed 64-bit storage also preserves the unsigned default simStage sentinel.
  branch(sims_, "simStage", simRow_.simStage);
  branch(sims_, "pdg", simRow_.pdg);
  branch(sims_, "startStatus", simRow_.startStatus);
  branch(sims_, "endStatus", simRow_.endStatus);
  branch(sims_, "nSteps", simRow_.nSteps);
  branch(sims_, "endDefined", simRow_.endDefined);
  branch(sims_, "isPrimary", simRow_.isPrimary);
  branch(sims_, "genPresent", simRow_.genPresent);
  branch(sims_, "parentPresent", simRow_.parentPresent);
  branch(sims_, "creationCode", simRow_.creationCode);
  branch(sims_, "stoppingCode", simRow_.stoppingCode);
  branch(sims_, "trackLength", simRow_.trackLength);
  branch(sims_, "endKE", simRow_.endKE);
  stateBranches(sims_, "start_", simRow_.start);
  stateBranches(sims_, "end_", simRow_.end);
}

void ComponentZeroStepDump::analyze(art::Event const& e) {
  eventRow_ = EventRow{};
  simRow_ = SimRow{};
  eventRow_.run = e.run();
  eventRow_.subrun = e.subRun();
  eventRow_.event = e.event();

  art::Handle<ComponentCapture> capture;
  art::Handle<SimParticleCollection> sims;
  art::Handle<StatusG4> status;
  e.getByLabel(source_, capture);
  e.getByLabel(simsSource_, sims);
  e.getByLabel(statusSource_, status);
  eventRow_.capturePresent = capture.isValid();
  eventRow_.simsPresent = sims.isValid();
  eventRow_.statusPresent = status.isValid();
  if (eventRow_.simsPresent) eventRow_.nSims = static_cast<std::int64_t>(sims->size());
  // Capture/status metadata belong to ComponentCaptureDump; missing inputs remain diagnostic.
  events_->Fill();
  if (!eventRow_.simsPresent) return;

  std::int64_t index = 0;
  for (auto const& item : *sims) {
    simRow_ = SimRow{};
    auto const& sim = item.second;
    simRow_.index = index++;
    simRow_.key = static_cast<std::int64_t>(item.first.asUint());
    simRow_.track = static_cast<std::int64_t>(sim.id().asUint());
    simRow_.parentPresent = sim.parent().isNonnull();
    simRow_.parent = simRow_.parentPresent ? static_cast<std::int64_t>(sim.parent().key()) : 0;
    simRow_.nDaughters = static_cast<std::int64_t>(sim.daughters().size());
    simRow_.simStage = static_cast<std::int64_t>(sim.simStage());
    simRow_.pdg = static_cast<int>(sim.pdgId());
    simRow_.startStatus = static_cast<int>(sim.startG4Status());
    simRow_.endStatus = static_cast<int>(sim.endG4Status());
    simRow_.nSteps = sim.nSteps();
    simRow_.endDefined = sim.endDefined();
    simRow_.isPrimary = sim.isPrimary();
    simRow_.genPresent = sim.genParticle().isNonnull();
    simRow_.creationCode = sim.creationCode().name();
    simRow_.stoppingCode = sim.stoppingCode().name();
    // Float-backed values are widened to double without recovering any lost precision.
    simRow_.trackLength = sim.trackLength();
    simRow_.endKE = sim.endKineticEnergy();
    simRow_.start = state(sim.startPosition(), sim.startMomentum(),
                          sim.startGlobalTime(), sim.startProperTime());
    simRow_.end = state(sim.endPosition(), sim.endMomentum(),
                        sim.endGlobalTime(), sim.endProperTime());
    // Preserve even undefined or unexpected endpoints; expectations are checked externally.
    sims_->Fill();
  }
}
} // namespace mu2e

DEFINE_ART_MODULE(mu2e::ComponentZeroStepDump)
