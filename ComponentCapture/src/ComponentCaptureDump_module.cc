// Diagnostic schema-1 export only: these records are never response-bank admissible.
#include <cstdint>
#include <string>

#include "TFile.h"
#include "TTree.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"
#include "art/Framework/Principal/Handle.h"
#include "canvas/Utilities/InputTag.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "Offline/MCDataProducts/inc/ComponentCapture.hh"
#include "Offline/MCDataProducts/inc/StatusG4.hh"

namespace mu2e {
namespace {
template <class T> void branch(TTree* tree, std::string const& name, T& value) {
  tree->Branch(name.c_str(), &value);
}
// int64_t can be long rather than ROOT's long long. Specify the ROOT leaf type.
void branch(TTree* tree, std::string const& name, std::int64_t& value) {
  static_assert(sizeof(std::int64_t) == sizeof(Long64_t), "64-bit ROOT identity required");
  tree->Branch(name.c_str(), &value, (name + "/L").c_str());
}
void stateBranches(TTree* tree, std::string const& prefix, ComponentState& s) {
  branch(tree, prefix + "x", s.x); branch(tree, prefix + "y", s.y); branch(tree, prefix + "z", s.z);
  branch(tree, prefix + "px", s.px); branch(tree, prefix + "py", s.py); branch(tree, prefix + "pz", s.pz);
  branch(tree, prefix + "lx", s.lx); branch(tree, prefix + "ly", s.ly); branch(tree, prefix + "lz", s.lz);
  branch(tree, prefix + "lpx", s.lpx); branch(tree, prefix + "lpy", s.lpy); branch(tree, prefix + "lpz", s.lpz);
  branch(tree, prefix + "energy", s.energy); branch(tree, prefix + "kinetic", s.kinetic);
  branch(tree, prefix + "mass", s.mass); branch(tree, prefix + "charge", s.charge);
  branch(tree, prefix + "time", s.time); branch(tree, prefix + "properTime", s.properTime);
  branch(tree, prefix + "weight", s.weight); branch(tree, prefix + "volume", s.volume);
  branch(tree, prefix + "placement", s.placement); branch(tree, prefix + "material", s.material);
  branch(tree, prefix + "process", s.process); branch(tree, prefix + "stepStatus", s.stepStatus);
}
void append(std::string& text, std::string const& message) {
  if (!text.empty()) text += '\n';
  text += message;
}
} // namespace

class ComponentCaptureDump : public art::EDAnalyzer {
 public:
  struct Config {
    fhicl::Atom<art::InputTag> source{fhicl::Name("source"), art::InputTag("g4run:componentCapture")};
    fhicl::Atom<art::InputTag> status{fhicl::Name("status"), art::InputTag("g4run")};
    fhicl::Atom<bool> failOnInvalid{fhicl::Name("failOnInvalid"), true};
  };
  using Parameters = art::EDAnalyzer::Table<Config>;
  explicit ComponentCaptureDump(Parameters const& p)
    : art::EDAnalyzer(p), source_(p().source()), statusSource_(p().status()),
      failOnInvalid_(p().failOnInvalid()) {
    consumes<ComponentCapture>(source_);
    consumes<StatusG4>(statusSource_);
  }
  void beginJob() override;
  void analyze(art::Event const&) override;

 private:
  TTree* table(char const* name, char const* title, bool indexed = true);
  art::InputTag source_, statusSource_;
  bool failOnInvalid_;
  TTree *events_{}, *tracks_{}, *steps_{}, *classifications_{}, *volumes_{};
  Long64_t run_{}, subrun_{}, event_{}, index_{};
  Long64_t nTracks_{}, nSteps_{}, nClassifications_{}, nVolumes_{}, nErrors_{};
  // Only header fields are copied here; avoid duplicating the large event collections.
  ComponentCapture header_;
  ComponentTrack track_;
  ComponentStep step_;
  ComponentClassification classification_;
  ComponentVolume volume_;
  bool capturePresent_{}, statusPresent_{}, eventIDsMatch_{}, statusClean_{};
  bool complete_{}, bankAdmissible_{false};
  std::string errors_, validationErrors_;
  int g4Status_{}, nG4Tracks_{}, nKilledStepLimit_{}, nKilledByFieldPropagator_{};
  bool overflowSimParticles_{};
  float cpuTime_{}, realTime_{};
};

TTree* ComponentCaptureDump::table(char const* name, char const* title, bool indexed) {
  art::ServiceHandle<art::TFileService> fs;
  auto* tree = fs->make<TTree>(name, title);
  tree->Branch("run", &run_, "run/L");
  tree->Branch("subrun", &subrun_, "subrun/L");
  tree->Branch("event", &event_, "event/L");
  if (indexed) tree->Branch("index", &index_, "index/L");
  return tree;
}

void ComponentCaptureDump::beginJob() {
  events_ = table("events", "Every diagnostic capture event, including empty or invalid events", false);
  branch(events_, "schema", header_.schema);
  branch(events_, "geometryTag", header_.geometryTag);
  branch(events_, "physicsTag", header_.physicsTag);
  branch(events_, "envelope", header_.envelope);
  branch(events_, "translation", header_.translation);
  branch(events_, "rotation", header_.rotation);
  branch(events_, "halfSize", header_.halfSize);
  branch(events_, "complete", complete_);
  branch(events_, "bankAdmissible", bankAdmissible_);
  branch(events_, "recordComplete", header_.complete);
  branch(events_, "recordBankAdmissible", header_.bankAdmissible);
  // Preserve the supplied identity as well as the framework identity for mismatch diagnostics.
  branch(events_, "recordRun", header_.run);
  branch(events_, "recordSubrun", header_.subrun);
  branch(events_, "recordEvent", header_.event);
  branch(events_, "capturePresent", capturePresent_);
  branch(events_, "statusPresent", statusPresent_);
  branch(events_, "eventIDsMatch", eventIDsMatch_);
  branch(events_, "errors", errors_);
  branch(events_, "validationErrors", validationErrors_);
  branch(events_, "nTracks", nTracks_); branch(events_, "nSteps", nSteps_);
  branch(events_, "nClassifications", nClassifications_); branch(events_, "nVolumes", nVolumes_);
  branch(events_, "nErrors", nErrors_);
  branch(events_, "g4Status", g4Status_);
  branch(events_, "statusClean", statusClean_);
  branch(events_, "nG4Tracks", nG4Tracks_);
  branch(events_, "overflowSimParticles", overflowSimParticles_);
  branch(events_, "nKilledStepLimit", nKilledStepLimit_);
  branch(events_, "nKilledByFieldPropagator", nKilledByFieldPropagator_);
  branch(events_, "cpuTime", cpuTime_); branch(events_, "realTime", realTime_);

  tracks_ = table("tracks", "Unfiltered captured track records; states are not inferred exits");
  branch(tracks_, "track", track_.track); branch(tracks_, "parent", track_.parent);
  branch(tracks_, "steps", track_.steps); branch(tracks_, "secondaries", track_.secondaries);
  branch(tracks_, "pdg", track_.pdg); branch(tracks_, "status", track_.status);
  branch(tracks_, "reason", track_.reason);
  branch(tracks_, "entered", track_.entered); branch(tracks_, "ended", track_.ended);
  branch(tracks_, "creator", track_.creator); branch(tracks_, "stopProcess", track_.stopProcess);
  stateBranches(tracks_, "begin_", track_.begin);
  stateBranches(tracks_, "end_", track_.end);
  stateBranches(tracks_, "entrance_", track_.entrance);

  steps_ = table("steps", "Unfiltered captured raw steps with full pre/post states");
  branch(steps_, "track", step_.track); branch(steps_, "number", step_.number);
  branch(steps_, "secondaries", step_.secondaries);
  branch(steps_, "deposit", step_.deposit); branch(steps_, "nonIonizingDeposit", step_.nonIonizingDeposit);
  branch(steps_, "length", step_.length); branch(steps_, "trackStatus", step_.trackStatus);
  branch(steps_, "preInside", step_.preInside); branch(steps_, "postInside", step_.postInside);
  branch(steps_, "firstExit", step_.firstExit);
  stateBranches(steps_, "pre_", step_.pre); stateBranches(steps_, "post_", step_.post);

  classifications_ = table("classifications", "All captured stacking classifications, including killed tracks");
  branch(classifications_, "track", classification_.track);
  branch(classifications_, "parent", classification_.parent);
  branch(classifications_, "pdg", classification_.pdg);
  branch(classifications_, "status", classification_.status);
  branch(classifications_, "killed", classification_.killed);
  stateBranches(classifications_, "state_", classification_.state);

  volumes_ = table("volumes", "Captured geometry volume records for each event");
  branch(volumes_, "id", volume_.id); branch(volumes_, "copy", volume_.copy);
  branch(volumes_, "name", volume_.name); branch(volumes_, "logical", volume_.logical);
  branch(volumes_, "motherLogical", volume_.motherLogical); branch(volumes_, "material", volume_.material);
  branch(volumes_, "solid", volume_.solid); branch(volumes_, "density", volume_.density);
  branch(volumes_, "translation", volume_.translation); branch(volumes_, "rotation", volume_.rotation);
}

void ComponentCaptureDump::analyze(art::Event const& e) {
  run_ = e.run(); subrun_ = e.subRun(); event_ = e.event();
  art::Handle<ComponentCapture> capture;
  art::Handle<StatusG4> status;
  e.getByLabel(source_, capture);
  e.getByLabel(statusSource_, status);
  capturePresent_ = capture.isValid(); statusPresent_ = status.isValid();
  ComponentCapture const empty;
  StatusG4 const unknown;
  auto const& record = capturePresent_ ? *capture : empty;
  auto const& g4 = statusPresent_ ? *status : unknown;

  header_.schema = record.schema;
  header_.run = record.run; header_.subrun = record.subrun; header_.event = record.event;
  header_.complete = record.complete; header_.bankAdmissible = record.bankAdmissible;
  header_.geometryTag = record.geometryTag; header_.physicsTag = record.physicsTag;
  header_.envelope = record.envelope;
  header_.translation = record.translation; header_.rotation = record.rotation; header_.halfSize = record.halfSize;
  nTracks_ = record.tracks.size(); nSteps_ = record.steps.size();
  nClassifications_ = record.classifications.size(); nVolumes_ = record.volumes.size();
  nErrors_ = record.errors.size();
  errors_.clear();
  for (std::size_t i = 0; i != record.errors.size(); ++i) {
    if (i) errors_ += '\n';
    errors_ += record.errors[i];
  }
  g4Status_ = g4.status(); nG4Tracks_ = g4.nG4Tracks();
  overflowSimParticles_ = g4.overflowSimParticles();
  nKilledStepLimit_ = g4.nKilledStepLimit(); nKilledByFieldPropagator_ = g4.nKilledByFieldPropagator();
  cpuTime_ = g4.cpuTime(); realTime_ = g4.realTime();
  statusClean_ = statusPresent_ && g4Status_ == StatusG4::ok && !overflowSimParticles_
    && nKilledStepLimit_ == 0 && nKilledByFieldPropagator_ == 0;
  complete_ = record.complete && statusClean_;
  bankAdmissible_ = false; // Experimental even if an input record claims otherwise.
  eventIDsMatch_ = capturePresent_ && record.run == run_ && record.subrun == subrun_ && record.event == event_;
  validationErrors_.clear();
  if (!capturePresent_) append(validationErrors_, "Missing ComponentCapture: " + source_.encode());
  if (!statusPresent_) append(validationErrors_, "Missing StatusG4: " + statusSource_.encode());
  if (capturePresent_ && !eventIDsMatch_) append(validationErrors_, "Product/framework event identity mismatch");
  if (record.schema != 1) append(validationErrors_, "Unsupported ComponentCapture schema");
  if (!record.complete) append(validationErrors_, "Capture record is incomplete");
  if (!statusClean_) append(validationErrors_, "StatusG4 is not clean");
  if (!record.errors.empty()) append(validationErrors_, "Capture record contains errors");
  if (nG4Tracks_ != nTracks_) append(validationErrors_, "StatusG4/captured track census mismatch");
  complete_ = complete_ && eventIDsMatch_ && validationErrors_.empty();

  // Never filter a diagnostic row, including incomplete events or killed classifications.
  index_ = 0;
  for (auto const& row : record.volumes) { volume_ = row; volumes_->Fill(); ++index_; }
  index_ = 0;
  for (auto const& row : record.classifications) { classification_ = row; classifications_->Fill(); ++index_; }
  index_ = 0;
  for (auto const& row : record.tracks) { track_ = row; tracks_->Fill(); ++index_; }
  index_ = 0;
  for (auto const& row : record.steps) { step_ = row; steps_->Fill(); ++index_; }
  events_->Fill(); // Also emitted when every collection is empty.

  // Identity mismatches are hard failures even when diagnostic-only invalid events are allowed.
  bool const identityMismatch = capturePresent_ && !eventIDsMatch_;
  if (identityMismatch || (failOnInvalid_ && !validationErrors_.empty())) {
    // Checkpoint *all* tables before throwing; leave file ownership with TFileService.
    for (auto* tree : {events_, tracks_, steps_, classifications_, volumes_})
      tree->AutoSave("FlushBaskets SaveSelf");
    events_->GetCurrentFile()->Flush();
    throw cet::exception("ComponentCaptureDump") << "Invalid capture for " << e.id()
      << " (diagnostic tables checkpointed):\n" << validationErrors_ << '\n' << errors_ << '\n';
  }
}
} // namespace mu2e

DEFINE_ART_MODULE(mu2e::ComponentCaptureDump)
