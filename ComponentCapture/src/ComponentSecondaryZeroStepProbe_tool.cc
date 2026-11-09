// Diagnostic status injection on engine-created children; never a physical response donor.
#include "Offline/Mu2eG4/inc/IMu2eG4Cut.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Observer.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4UserTrackInformation.hh"
#include "Offline/MCDataProducts/inc/ProcessCode.hh"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Utilities/ToolMacros.h"
#include "art/Utilities/make_tool.h"
#include "art_root_io/TFileService.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "Geant4/G4DynamicParticle.hh"
#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4StepPoint.hh"
#include "Geant4/G4StepStatus.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4TrackStatus.hh"
#include "Geant4/G4VProcess.hh"
#include "Geant4/G4Version.hh"
#include "RtypesCore.h"
#include "TTree.h"
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <string>

namespace mu2e {
namespace {
void check(bool condition, const char* message) {
  if (!condition) throw cet::exception("ComponentSecondaryZeroStepProbe") << message;
}
fhicl::ParameterSet recorderConfig() {
  fhicl::ParameterSet p;
  p.put("tool_type", std::string("ComponentRecorder"));
  p.put("envelope", std::string("CaptureEnvelope"));
  p.put("geometryTag", std::string("diagnostic-secondary-zero-step-box-v1"));
  p.put("physicsTag", std::string("DIAGNOSTIC-SECONDARY-STATUS-INJECTION-NOT-RESPONSE-v1"));
  p.put("maxSteps", 1000000u); p.put("maxTracks", 100000u);
  return p;
}
template<class T> void branch(TTree* tree, const std::string& name, T& value) {
  check(tree->Branch(name.c_str(), &value)!=nullptr, "Failed to book callback branch");
}
void branch(TTree* tree, const std::string& name, Long64_t& value) {
  check(tree->Branch(name.c_str(), &value, (name+"/L").c_str())!=nullptr, "Failed to book int64 callback branch");
}
struct State {
  double x=0., y=0., z=0., px=0., py=0., pz=0., energy=0., kinetic=0., time=0., properTime=0.;
  void book(TTree* tree, const std::string& p) {
    branch(tree,p+"x",x); branch(tree,p+"y",y); branch(tree,p+"z",z);
    branch(tree,p+"px",px); branch(tree,p+"py",py); branch(tree,p+"pz",pz);
    branch(tree,p+"energy",energy); branch(tree,p+"kinetic",kinetic);
    branch(tree,p+"time",time); branch(tree,p+"properTime",properTime);
  }
  template<class T> void read(const T& object) {
    const auto position=object.GetPosition(), momentum=object.GetMomentum();
    x=position.x(); y=position.y(); z=position.z();
    px=momentum.x(); py=momentum.y(); pz=momentum.z();
    energy=object.GetTotalEnergy(); kinetic=object.GetKineticEnergy();
    time=object.GetGlobalTime(); properTime=object.GetProperTime();
  }
  bool finite() const {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
      std::isfinite(px) && std::isfinite(py) && std::isfinite(pz) &&
      std::isfinite(energy) && std::isfinite(kinetic) && std::isfinite(time) && std::isfinite(properTime);
  }
  bool same(const State& other) const {
    return x==other.x && y==other.y && z==other.z && px==other.px && py==other.py && pz==other.pz &&
      energy==other.energy && kinetic==other.kinetic && time==other.time && properTime==other.properTime;
  }
};
struct Point {
  bool present=false, processPresent=false;
  int status=-1;
  std::string process;
  State state;
  void book(TTree* tree, const std::string& p) {
    branch(tree,p+"present",present); branch(tree,p+"processPresent",processPresent);
    branch(tree,p+"status",status); branch(tree,p+"process",process); state.book(tree,p);
  }
  void read(const G4StepPoint* point) {
    if (!point) return;
    present=true; state.read(*point); status=static_cast<int>(point->GetStepStatus());
    if (const auto* proc=point->GetProcessDefinedStep()) { processPresent=true; process=proc->GetProcessName(); }
  }
};
struct Row {
  Long64_t run=0, subrun=0, event=0, index=0, track=0, parent=0, steps=0;
  int phase=-1, pdg=0, statusIn=-1, statusForward=-1, statusOut=-1, g4Version=G4VERSION_NUMBER;
  bool diagnosticOnly=true, bankAdmissible=false, killedValid=false, killed=false, mutated=false, forwarded=false;
  bool stepPresent=false, userInfoPresent=false, mu2eInfoPresent=false, forced=false;
  double trackLength=0., infoKE=0., infoDx=0., infoDy=0., infoDz=0., infoTime=0., infoProperTime=0.;
  std::string error, infoCode;
  State state;
  Point pre, post;
  std::string mode, creator;
  bool creatorPresent=false, cutEvaluated=false, cutResult=false, parentEnded=false;
  Long64_t stepSecondaries=0, parentEndIndex=-1;
  double weight=0., dynamicMass=0.;
};
struct Lifecycle {
  unsigned classified=0, begun=0, ended=0, mutations=0;
  int pdg=0;
  std::string creator;
  State birth;
  double mass=0.;
};
}

class ComponentSecondaryZeroStepProbe : public IMu2eG4Cut, public IMu2eG4Observer {
public:
  explicit ComponentSecondaryZeroStepProbe(const fhicl::ParameterSet& p)
    : expectedRun_(p.get<unsigned>("expectedRun")), mode_(p.get<std::string>("mode")),
      cut_(art::make_tool<IMu2eG4Cut>(recorderConfig())) {
    check(p.get<bool>("diagnosticOnly"), "diagnosticOnly must be true");
    check(expectedRun_==10, "expectedRun must be 10");
    check(mode_=="classification" || mode_=="begin", "mode must be classification or begin");
    observer_=dynamic_cast<IMu2eG4Observer*>(cut_.get());
    check(observer_!=nullptr, "Nested recorder must implement IMu2eG4Observer");
  }
  void declareProducts(art::ProducesCollector& pc, art::ConsumesCollector& cc) override { cut_->declareProducts(pc,cc); }
  void finishConstruction(const CLHEP::Hep3Vector& origin) override { cut_->finishConstruction(origin); }
  void beginEvent(const art::Event&, const SimParticleHelper&) override;
  bool stackingActionCut(const G4Track*) override;
  bool steppingActionCut(const G4Step*) override;
  void classifiedTrack(const G4Track*, bool) override;
  void beginTrack(const G4Track*) override;
  void endTrack(const G4Track*) override;
  void put(art::Event&) override;
  void deleteCutsData() override;
private:
  const unsigned expectedRun_;
  const std::string mode_;
  std::unique_ptr<IMu2eG4Cut> cut_;
  IMu2eG4Observer* observer_=nullptr;
  TTree* tree_=nullptr;
  Row row_;
  std::map<std::int64_t,Lifecycle> tracks_;
  Long64_t run_=0, subrun_=0, event_=0, index_=0;
  Long64_t primarySteps_=0, stepSecondaries_=0, parentEndIndex_=-1;
  unsigned seen_=0;
  bool active_=false, parentEnded_=false;
  void book();
  void resetEvent();
  void snapshot(const G4Track*, int, const G4Step* = nullptr);
  void validate(const G4Track*, int, const G4Step*) const;
  void inject(const G4Track*);
  void writeRow();
  void failure(const G4Track* track, const std::string& error) {
    row_.error=error;
    if (track) row_.statusOut=static_cast<int>(track->GetTrackStatus());
    if (tree_) writeRow();
  }
  template<class F> void observe(const G4Track* track, int phase, F action,
                                 const G4Step* step=nullptr, bool killed=false) {
    try {
      snapshot(track,phase,step);
      if (phase==0) { row_.killedValid=true; row_.killed=killed; }
      validate(track,phase,step);
      action();
      row_.statusOut=static_cast<int>(track->GetTrackStatus());
      check(row_.statusOut==(row_.mutated ? static_cast<int>(fStopAndKill) : row_.statusForward),
            "Nested callback changed track status");
      State after;
      after.read(*track);
      check(after.same(row_.state) && track->GetTrackID()==row_.track && track->GetParentID()==row_.parent &&
            track->GetCurrentStepNumber()==row_.steps && track->GetTrackLength()==row_.trackLength &&
            track->GetWeight()==row_.weight && track->GetDynamicParticle()->GetMass()==row_.dynamicMass,
            "Forwarding/injection changed track state beyond the declared status mutation");
    }
    catch (const std::exception& ex) { failure(track,ex.what()); throw; }
    catch (...) { failure(track,"non-standard callback exception"); throw; }
    // Never retry Fill or AutoSave when the output write itself fails.
    writeRow(); ++index_;
  }
};

void ComponentSecondaryZeroStepProbe::book() {
  art::ServiceHandle<art::TFileService> fs;
  tree_=fs->make<TTree>("secondaryZeroStepCallbacks","Engine-created secondary callbacks with diagnostic-only status injection");
  check(tree_!=nullptr, "Failed to book callback tree");
  branch(tree_,"run",row_.run); branch(tree_,"subrun",row_.subrun); branch(tree_,"event",row_.event);
  branch(tree_,"index",row_.index); branch(tree_,"track",row_.track); branch(tree_,"parent",row_.parent);
  branch(tree_,"steps",row_.steps); branch(tree_,"phase",row_.phase); branch(tree_,"pdg",row_.pdg);
  branch(tree_,"statusIn",row_.statusIn); branch(tree_,"statusForward",row_.statusForward);
  branch(tree_,"statusOut",row_.statusOut); branch(tree_,"g4Version",row_.g4Version);
  branch(tree_,"diagnosticOnly",row_.diagnosticOnly); branch(tree_,"bankAdmissible",row_.bankAdmissible);
  branch(tree_,"killedValid",row_.killedValid); branch(tree_,"killed",row_.killed);
  branch(tree_,"mutated",row_.mutated); branch(tree_,"forwarded",row_.forwarded);
  branch(tree_,"stepPresent",row_.stepPresent); branch(tree_,"userInfoPresent",row_.userInfoPresent);
  branch(tree_,"mu2eInfoPresent",row_.mu2eInfoPresent); branch(tree_,"forced",row_.forced);
  branch(tree_,"trackLength",row_.trackLength); branch(tree_,"infoKE",row_.infoKE);
  branch(tree_,"infoDx",row_.infoDx); branch(tree_,"infoDy",row_.infoDy); branch(tree_,"infoDz",row_.infoDz);
  branch(tree_,"infoTime",row_.infoTime); branch(tree_,"infoProperTime",row_.infoProperTime);
  branch(tree_,"infoCode",row_.infoCode); branch(tree_,"error",row_.error);
  branch(tree_,"mode",row_.mode); branch(tree_,"creator",row_.creator);
  branch(tree_,"creatorPresent",row_.creatorPresent);
  branch(tree_,"cutEvaluated",row_.cutEvaluated); branch(tree_,"cutResult",row_.cutResult);
  branch(tree_,"stepSecondaries",row_.stepSecondaries);
  branch(tree_,"parentEnded",row_.parentEnded); branch(tree_,"parentEndIndex",row_.parentEndIndex);
  branch(tree_,"weight",row_.weight); branch(tree_,"dynamicMass",row_.dynamicMass);
  row_.state.book(tree_,"track_"); row_.pre.book(tree_,"pre_"); row_.post.book(tree_,"post_");
}

void ComponentSecondaryZeroStepProbe::writeRow() {
  check(tree_!=nullptr, "Missing callback tree");
  const auto filled=tree_->Fill();
  const auto saved=tree_->AutoSave("SaveSelf");
  if (filled<=0 || saved<=0)
    throw cet::exception("ComponentSecondaryZeroStepProbe")
      << "Callback Fill/AutoSave failed: " << filled << '/' << saved << "; original error: " << row_.error;
}

void ComponentSecondaryZeroStepProbe::resetEvent() {
  tracks_.clear(); row_=Row{};
  run_=0; subrun_=0; event_=0; index_=0;
  primarySteps_=0; stepSecondaries_=0; parentEndIndex_=-1;
  active_=false; parentEnded_=false;
}

void ComponentSecondaryZeroStepProbe::beginEvent(const art::Event& event, const SimParticleHelper& helper) {
  try {
    snapshot(nullptr,-1);
    check(!active_, "Previous diagnostic event did not finish");
    resetEvent();
    run_=event.id().run(); subrun_=event.id().subRun(); event_=event.id().event();
    active_=true;
    if (!tree_) book();
    snapshot(nullptr,-1);
    check(seen_==0 && run_==expectedRun_ && subrun_==0 && event_==1, "Require exactly one event 10:0:1");
    ++seen_;
    cut_->beginEvent(event,helper);
  }
  catch (const std::exception& ex) { failure(nullptr,ex.what()); throw; }
  catch (...) { failure(nullptr,"non-standard beginEvent exception"); throw; }
}

void ComponentSecondaryZeroStepProbe::snapshot(const G4Track* track, int phase, const G4Step* suppliedStep) {
  row_=Row{};
  row_.run=run_; row_.subrun=subrun_; row_.event=event_; row_.index=index_; row_.phase=phase;
  row_.mode=mode_; row_.parentEnded=parentEnded_; row_.parentEndIndex=parentEndIndex_;
  if (!track) return;
  row_.track=track->GetTrackID(); row_.parent=track->GetParentID();
  row_.steps=track->GetCurrentStepNumber(); row_.trackLength=track->GetTrackLength();
  row_.statusIn=static_cast<int>(track->GetTrackStatus()); row_.weight=track->GetWeight();
  if (const auto* particle=track->GetDynamicParticle()) {
    row_.dynamicMass=particle->GetMass(); row_.state.read(*track);
    if (const auto* definition=track->GetDefinition()) row_.pdg=definition->GetPDGEncoding();
  }
  if (const auto* creator=track->GetCreatorProcess()) {
    row_.creatorPresent=true; row_.creator=creator->GetProcessName();
  }
  const auto* step=phase==3 ? suppliedStep : track->GetStep();
  if (step) {
    row_.stepPresent=true; row_.pre.read(step->GetPreStepPoint()); row_.post.read(step->GetPostStepPoint());
    if (phase==3) row_.stepSecondaries=step->GetNumberOfSecondariesInCurrentStep();
  }
  row_.userInfoPresent=track->GetUserInformation()!=nullptr;
  if (const auto* info=dynamic_cast<const Mu2eG4UserTrackInformation*>(track->GetUserInformation())) {
    row_.mu2eInfoPresent=true; row_.forced=info->isForced(); row_.infoCode=info->code().name();
    row_.infoKE=info->GetKineticEnergy(); row_.infoTime=info->GetGlobalTime(); row_.infoProperTime=info->GetProperTime();
    const auto& direction=info->GetMomentumDirection();
    row_.infoDx=direction.x(); row_.infoDy=direction.y(); row_.infoDz=direction.z();
  }
}

void ComponentSecondaryZeroStepProbe::validate(const G4Track* track, int phase, const G4Step* step) const {
  check(active_ && tree_ && track && phase>=0 && phase<=3, "Callback outside active event or without track");
  check(track->GetDynamicParticle() && track->GetDefinition(), "Missing dynamic particle/definition");
  check(row_.state.finite() && std::isfinite(row_.trackLength) && row_.trackLength>=0. &&
        std::isfinite(row_.weight) && row_.weight==1. && std::isfinite(row_.dynamicMass) && row_.dynamicMass>=0. &&
        row_.state.kinetic>=0. && row_.state.energy>=0., "Invalid track state/weight/mass");
  check((!row_.pre.present || row_.pre.state.finite()) && (!row_.post.present || row_.post.state.finite()),
        "Nonfinite attached step state");
  check(!row_.mu2eInfoPresent || (std::isfinite(row_.infoKE) && std::isfinite(row_.infoDx) &&
        std::isfinite(row_.infoDy) && std::isfinite(row_.infoDz) && std::isfinite(row_.infoTime) &&
        std::isfinite(row_.infoProperTime)), "Nonfinite Mu2e track information");
  check(row_.track>0 && ((row_.track==1 && row_.parent==0 && row_.pdg==-13) ||
        (row_.track!=1 && row_.parent==1)), "Unexpected primary, nonpositive ID, or grandchild");
  const bool primary=row_.track==1;
  const auto it=tracks_.find(row_.track);
  if (phase==0) {
    check(it==tracks_.end(), "Repeated classification or reused track ID");
    check(!row_.killed && row_.statusIn==fAlive, "Stacking rejection or unexpected classification status");
  } else {
    check(it!=tracks_.end() && it->second.classified==1, "Callback without unique classification");
    const auto& life=it->second;
    check(life.ended==0 && life.begun==(phase==1 ? 0u : 1u), "Repeated/missing/resumed lifecycle callback");
    check(row_.pdg==life.pdg && row_.creator==life.creator && row_.dynamicMass==life.mass,
          "Track identity/creator/mass changed");
  }
  if (primary) {
    check(!row_.creatorPresent && !parentEnded_, "Unexpected primary creator or callback after end");
    if (phase==0 || phase==1) {
      const auto& s=row_.state;
      check(row_.steps==0 && row_.trackLength==0. && row_.statusIn==fAlive &&
            s.x==0. && s.y==0. && std::abs(s.z+50.001)<1e-10 && std::abs(s.px)<1e-10 &&
            std::abs(s.py)<1e-10 && std::abs(s.pz-20.)<1e-10 && s.kinetic>0. &&
            s.time==0. && s.properTime==0., "Unexpected diagnostic incoming muon");
      check(phase!=0 || (index_==0 && tracks_.empty()), "Primary was not the first classification");
    } else if (phase==3) {
      check(step && step->GetTrack()==track && track->GetStep()==step && row_.pre.present && row_.post.present,
            "Missing/mismatched actual primary step");
      check(row_.steps==primarySteps_+1 && row_.stepSecondaries>=0 &&
            (row_.statusIn==fAlive || row_.statusIn==fStopButAlive || row_.statusIn==fStopAndKill),
            "Missing/repeated primary step or unexpected primary status");
    } else {
      check(primarySteps_>0 && row_.steps==primarySteps_ && row_.statusIn==fStopAndKill &&
            track->GetStep() && track->GetStep()->GetTrack()==track && row_.pre.present && row_.post.present &&
            row_.post.processPresent && row_.post.process=="Decay" && row_.post.status==fAtRestDoItProc &&
            row_.pre.state.kinetic==0. && row_.state.kinetic==0. && stepSecondaries_>0,
            "Primary did not finish stepped ordinary at-rest Decay with children");
    }
    check(it==tracks_.end() || it->second.mutations==0, "Primary was mutated");
    return;
  }
  check(parentEnded_ && parentEndIndex_>=0 && parentEndIndex_<index_, "Child callback before primary end completed");
  check(row_.creatorPresent && !row_.creator.empty() && row_.state.kinetic>0., "Child lacks creator or positive finite KE");
  check(phase!=3 && row_.steps==0 && row_.trackLength==0., "Child performed a step");
  if (phase!=0) {
    const auto& life=it->second;
    check(row_.state.same(life.birth), "Zero-step child track state changed");
    const auto expectedMutations=(phase==1 && mode_=="begin") ? 0u : 1u;
    const auto expectedStatus=(phase==1 && mode_=="begin") ? fAlive : fStopAndKill;
    check(life.mutations==expectedMutations && row_.statusIn==expectedStatus, "Unexpected child status/injection count");
    check(row_.userInfoPresent && row_.mu2eInfoPresent && !row_.forced && row_.infoCode==ProcessCode{}.name() &&
          row_.infoKE==0. && row_.infoDx==0. && row_.infoDy==0. && row_.infoDz==0. &&
          row_.infoTime==-1. && row_.infoProperTime==-1., "Child Mu2e information is not at original defaults");
    if (phase==2) {
      check(track->GetStep() && track->GetStep()->GetTrack()==track && row_.pre.present && row_.post.present &&
            row_.pre.status==fUndefined && row_.post.status==fUndefined &&
            !row_.pre.processPresent && !row_.post.processPresent, "Unexpected zero-step child end points/process");
    }
  }
}

void ComponentSecondaryZeroStepProbe::inject(const G4Track* track) {
  auto& life=tracks_.at(row_.track);
  check(row_.track!=1 && row_.parent==1 && track->GetTrackStatus()==fAlive && life.mutations==0,
        "Injection requires an unmutated live direct child");
  const_cast<G4Track*>(track)->SetTrackStatus(fStopAndKill);
  row_.mutated=true; ++life.mutations;
}

bool ComponentSecondaryZeroStepProbe::stackingActionCut(const G4Track* track) {
  try {
    snapshot(track,-1);
    check(active_ && track, "Stacking cut outside active event or without track");
    return cut_->stackingActionCut(track);
  }
  catch (const std::exception& ex) { failure(track,ex.what()); throw; }
  catch (...) { failure(track,"non-standard stacking cut exception"); throw; }
}

void ComponentSecondaryZeroStepProbe::classifiedTrack(const G4Track* track, bool killed) {
  observe(track,0,[&] {
    auto& life=tracks_.emplace(row_.track,Lifecycle{}).first->second;
    life.pdg=row_.pdg; life.creator=row_.creator; life.birth=row_.state; life.mass=row_.dynamicMass;
    if (row_.parent==1 && mode_=="classification") inject(track);
    row_.statusForward=static_cast<int>(track->GetTrackStatus());
    observer_->classifiedTrack(track,killed); row_.forwarded=true; ++life.classified;
  },nullptr,killed);
}

void ComponentSecondaryZeroStepProbe::beginTrack(const G4Track* track) {
  observe(track,1,[&] {
    row_.statusForward=static_cast<int>(track->GetTrackStatus());
    observer_->beginTrack(track); row_.forwarded=true;
    if (row_.parent==1 && mode_=="begin") inject(track);
    ++tracks_.at(row_.track).begun;
  });
}

void ComponentSecondaryZeroStepProbe::endTrack(const G4Track* track) {
  observe(track,2,[&] {
    row_.statusForward=static_cast<int>(track->GetTrackStatus());
    observer_->endTrack(track); row_.forwarded=true; ++tracks_.at(row_.track).ended;
  });
  if (row_.track==1) { parentEnded_=true; parentEndIndex_=index_-1; }
}

bool ComponentSecondaryZeroStepProbe::steppingActionCut(const G4Step* step) {
  bool result=false;
  const auto* track=step ? step->GetTrack() : nullptr;
  observe(track,3,[&] {
    row_.statusForward=static_cast<int>(track->GetTrackStatus()); row_.cutEvaluated=true;
    result=cut_->steppingActionCut(step); row_.cutResult=result; row_.forwarded=true;
    check(!result, "Unexpected primary first-exit/common cut decision");
    check(row_.stepSecondaries<=std::numeric_limits<Long64_t>::max()-stepSecondaries_, "Secondary count overflow");
    ++primarySteps_; stepSecondaries_+=row_.stepSecondaries;
  },step);
  return result;
}

void ComponentSecondaryZeroStepProbe::put(art::Event& event) {
  try {
    snapshot(nullptr,-1);
    check(active_ && seen_==1 && event.id().run()==run_ && event.id().subRun()==subrun_ &&
          event.id().event()==event_, "Publication outside the single diagnostic event");
    const auto primary=tracks_.find(1);
    check(primary!=tracks_.end() && primary->second.classified==1 && primary->second.begun==1 &&
          primary->second.ended==1 && primary->second.mutations==0 && parentEnded_ && primarySteps_>0,
          "Incomplete primary lifecycle");
    check(tracks_.size()>1 && stepSecondaries_>0, "No engine-created direct children");
    const auto children=static_cast<Long64_t>(tracks_.size()-1);
    for (const auto& entry : tracks_) {
      if (entry.first==1) continue;
      const auto& life=entry.second;
      check(life.classified==1 && life.begun==1 && life.ended==1 && life.mutations==1,
            "Incomplete/repeated child callbacks or injection");
    }
    check(children==stepSecondaries_, "Primary step-secondary/direct-child census mismatch");
    check(index_==3*(1+children)+primarySteps_ && tree_->GetEntries()==index_, "Callback row census mismatch");
    cut_->put(event);
    active_=false;
  }
  catch (const std::exception& ex) { failure(nullptr,ex.what()); throw; }
  catch (...) { failure(nullptr,"non-standard put exception"); throw; }
}

void ComponentSecondaryZeroStepProbe::deleteCutsData() {
  try {
    snapshot(nullptr,-1);
    cut_->deleteCutsData();
    check(!active_, "Diagnostic event discarded before successful publication");
  }
  catch (const std::exception& ex) {
    try { failure(nullptr,ex.what()); }
    catch (...) { resetEvent(); throw; }
    resetEvent(); throw;
  }
  catch (...) {
    try { failure(nullptr,"non-standard deleteCutsData exception"); }
    catch (...) { resetEvent(); throw; }
    resetEvent(); throw;
  }
  resetEvent();
}
}
DEFINE_ART_CLASS_TOOL(mu2e::ComponentSecondaryZeroStepProbe)
