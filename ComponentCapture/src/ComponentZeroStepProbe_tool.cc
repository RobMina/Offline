// Diagnostic status injection on engine-owned tracks; never a physical response donor.
#include "Offline/Mu2eG4/inc/IMu2eG4Cut.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Observer.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4UserTrackInformation.hh"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Utilities/ToolMacros.h"
#include "art/Utilities/make_tool.h"
#include "art_root_io/TFileService.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4StepPoint.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4VProcess.hh"
#include "Geant4/G4Version.hh"
#include "TTree.h"
#include <cmath>
#include <exception>
#include <memory>
#include <string>

namespace mu2e {
namespace {
void check(bool condition, const char* message) {
  if (!condition) throw cet::exception("ComponentZeroStepProbe") << message;
}
fhicl::ParameterSet recorderConfig() {
  fhicl::ParameterSet p;
  p.put("tool_type", std::string("ComponentRecorder"));
  p.put("envelope", std::string("CaptureEnvelope"));
  p.put("geometryTag", std::string("diagnostic-engine-zero-step-box-v1"));
  p.put("physicsTag", std::string("DIAGNOSTIC-STATUS-INJECTION-NOT-RESPONSE-v1"));
  p.put("maxSteps", 100u); p.put("maxTracks", 100u);
  return p;
}
template<class T> void branch(TTree* tree, const std::string& name, T& value) {
  tree->Branch(name.c_str(), &value);
}
void branch(TTree* tree, const std::string& name, Long64_t& value) {
  tree->Branch(name.c_str(), &value, (name+"/L").c_str());
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
};
}

class ComponentZeroStepProbe : public IMu2eG4Cut, public IMu2eG4Observer {
public:
  explicit ComponentZeroStepProbe(const fhicl::ParameterSet& p)
    : expectedRun_(p.get<unsigned>("expectedRun")), cut_(art::make_tool<IMu2eG4Cut>(recorderConfig())) {
    check(p.get<bool>("diagnosticOnly"), "diagnosticOnly must be true");
    check(expectedRun_>0, "expectedRun must be positive");
    observer_=dynamic_cast<IMu2eG4Observer*>(cut_.get());
    check(observer_!=nullptr, "Nested recorder must implement IMu2eG4Observer");
  }
  void declareProducts(art::ProducesCollector& pc, art::ConsumesCollector& cc) override { cut_->declareProducts(pc,cc); }
  void finishConstruction(const CLHEP::Hep3Vector& origin) override { cut_->finishConstruction(origin); }
  void beginEvent(const art::Event&, const SimParticleHelper&) override;
  bool stackingActionCut(const G4Track* track) override { return cut_->stackingActionCut(track); }
  bool steppingActionCut(const G4Step*) override;
  void classifiedTrack(const G4Track*, bool) override;
  void beginTrack(const G4Track*) override;
  void endTrack(const G4Track*) override;
  void put(art::Event& event) override {
    check(active_ && index_==3 && mutations_==1, "Incomplete zero-step callback sequence");
    cut_->put(event); active_=false;
  }
  void deleteCutsData() override { cut_->deleteCutsData(); active_=false; index_=0; mutations_=0; }
private:
  const unsigned expectedRun_;
  std::unique_ptr<IMu2eG4Cut> cut_;
  IMu2eG4Observer* observer_=nullptr;
  TTree* tree_=nullptr;
  Row row_;
  Long64_t run_=0, subrun_=0, event_=0, index_=0;
  unsigned seen_=0, mutations_=0;
  bool active_=false;
  void book();
  void snapshot(const G4Track*, int phase);
  void validate(const G4Track*, int phase) const;
  void inject(const G4Track* track) {
    check(track->GetTrackStatus()==fAlive && mutations_==0, "Unexpected status or repeated injection");
    const_cast<G4Track*>(track)->SetTrackStatus(fStopAndKill);
    row_.mutated=true; ++mutations_;
  }
  template<class F> void observe(const G4Track* track, int phase, F action) {
    snapshot(track,phase);
    try { validate(track,phase); action(); row_.statusOut=static_cast<int>(track->GetTrackStatus()); }
    catch (const std::exception& ex) {
      row_.error=ex.what(); row_.statusOut=static_cast<int>(track->GetTrackStatus());
      tree_->Fill(); tree_->AutoSave("SaveSelf"); throw;
    }
    catch (...) { row_.error="non-standard callback exception"; tree_->Fill(); tree_->AutoSave("SaveSelf"); throw; }
    tree_->Fill(); tree_->AutoSave("SaveSelf"); ++index_;
  }
};

void ComponentZeroStepProbe::book() {
  art::ServiceHandle<art::TFileService> fs;
  tree_=fs->make<TTree>("zeroStepCallbacks","Engine callbacks with explicit diagnostic-only status injection");
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
  row_.state.book(tree_,"track_"); row_.pre.book(tree_,"pre_"); row_.post.book(tree_,"post_");
}

void ComponentZeroStepProbe::beginEvent(const art::Event& event, const SimParticleHelper& helper) {
  check(!active_, "Previous diagnostic event did not finish");
  check(event.id().run()==expectedRun_ && event.id().subRun()==0 &&
        event.id().event()==seen_+1 && seen_<2, "Require exactly events 1/2 in expectedRun subrun 0");
  if (!tree_) book();
  run_=event.id().run(); subrun_=event.id().subRun(); event_=event.id().event();
  index_=0; mutations_=0; ++seen_; active_=true;
  cut_->beginEvent(event,helper);
}

void ComponentZeroStepProbe::snapshot(const G4Track* track, int phase) {
  check(tree_ && track, "Callback before diagnostic event or without track");
  row_=Row{};
  row_.run=run_; row_.subrun=subrun_; row_.event=event_; row_.index=index_; row_.phase=phase;
  row_.track=track->GetTrackID(); row_.parent=track->GetParentID(); row_.pdg=track->GetDefinition()->GetPDGEncoding();
  row_.steps=track->GetCurrentStepNumber(); row_.trackLength=track->GetTrackLength();
  row_.statusIn=static_cast<int>(track->GetTrackStatus()); row_.state.read(*track);
  if (const auto* step=track->GetStep()) {
    row_.stepPresent=true; row_.pre.read(step->GetPreStepPoint()); row_.post.read(step->GetPostStepPoint());
  }
  row_.userInfoPresent=track->GetUserInformation()!=nullptr;
  if (const auto* info=dynamic_cast<const Mu2eG4UserTrackInformation*>(track->GetUserInformation())) {
    row_.mu2eInfoPresent=true; row_.forced=info->isForced(); row_.infoCode=info->code().name();
    row_.infoKE=info->GetKineticEnergy(); row_.infoTime=info->GetGlobalTime(); row_.infoProperTime=info->GetProperTime();
    const auto& direction=info->GetMomentumDirection();
    row_.infoDx=direction.x(); row_.infoDy=direction.y(); row_.infoDz=direction.z();
  }
}

void ComponentZeroStepProbe::validate(const G4Track* track, int phase) const {
  check(active_ && index_==phase && phase<3, "Unexpected/repeated callback or any stepping callback");
  check(track->GetTrackID()==1 && track->GetParentID()==0 && row_.pdg==22, "Expected one engine-assigned primary photon");
  check(track->GetCurrentStepNumber()==0 && track->GetTrackLength()==0., "Engine performed a step");
  const auto& position=track->GetPosition(); const auto momentum=track->GetMomentum();
  check(position.x()==0. && position.y()==0. && std::abs(position.z()+50.001)<1e-10 &&
        std::abs(momentum.x())<1e-10 && std::abs(momentum.y())<1e-10 && std::abs(momentum.z()-10.)<1e-10 &&
        std::abs(track->GetKineticEnergy()-10.)<1e-10 && track->GetGlobalTime()==0. &&
        track->GetProperTime()==0. && track->GetWeight()==1., "Unexpected diagnostic incoming state");
}

void ComponentZeroStepProbe::classifiedTrack(const G4Track* track, bool killed) {
  observe(track,0,[&] {
    row_.killedValid=true; row_.killed=killed;
    check(!killed && track->GetTrackStatus()==fAlive, "Unexpected stacking rejection/status");
    if (event_==1) inject(track);
    row_.statusForward=static_cast<int>(track->GetTrackStatus());
    observer_->classifiedTrack(track,killed); row_.forwarded=true;
  });
}

void ComponentZeroStepProbe::beginTrack(const G4Track* track) {
  observe(track,1,[&] {
    check(track->GetTrackStatus()==(event_==1 ? fStopAndKill : fAlive), "Unexpected pretracking status");
    row_.statusForward=static_cast<int>(track->GetTrackStatus());
    observer_->beginTrack(track); row_.forwarded=true;
    if (event_==2) inject(track);
  });
}

void ComponentZeroStepProbe::endTrack(const G4Track* track) {
  observe(track,2,[&] {
    check(track->GetTrackStatus()==fStopAndKill && mutations_==1, "Unexpected posttracking status");
    row_.statusForward=static_cast<int>(track->GetTrackStatus());
    observer_->endTrack(track); row_.forwarded=true;
  });
}

bool ComponentZeroStepProbe::steppingActionCut(const G4Step* step) {
  check(step && step->GetTrack(), "Missing step/track in unexpected stepping callback");
  observe(step->GetTrack(),3,[&] { cut_->steppingActionCut(step); });
  return false;
}
}
DEFINE_ART_CLASS_TOOL(mu2e::ComponentZeroStepProbe)
