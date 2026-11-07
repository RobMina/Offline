// Constructed-object callback tests only; no physics transport or response-bank donors.
#include "Offline/MCDataProducts/inc/SimParticle.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Cut.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Observer.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4Inputs.hh"
#include "Offline/Mu2eG4/inc/SimParticleHelper.hh"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Utilities/make_tool.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Table.h"
#include "Geant4/G4Box.hh"
#include "Geant4/G4DynamicParticle.hh"
#include "Geant4/G4Gamma.hh"
#include "Geant4/G4LogicalVolume.hh"
#include "Geant4/G4NavigationHistory.hh"
#include "Geant4/G4NistManager.hh"
#include "Geant4/G4PVPlacement.hh"
#include "Geant4/G4PhysicalVolumeStore.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4TouchableHistory.hh"
#include "Geant4/G4Track.hh"
#include <iostream>
#include <memory>
#include <string>

namespace mu2e {
namespace {
void check(bool value, const char* message) {
  if (!value) throw cet::exception("ComponentCallbackFixture") << message;
}

struct OwnedStep : G4Step {
  OwnedStep() { NewSecondaryVector(); }
  ~OwnedStep() { DeleteSecondaryVector(); }
};

fhicl::ParameterSet recorderConfig() {
  fhicl::ParameterSet p;
  p.put("tool_type", std::string("ComponentRecorder"));
  p.put("envelope", std::string("CallbackFixtureEnvelope"));
  p.put("geometryTag", std::string("diagnostic-constructed-objects-v1"));
  p.put("physicsTag", std::string("NO-PHYSICS-TRANSPORT-v1"));
  p.put("maxSteps", 100u); p.put("maxTracks", 100u);
  return p;
}
}

class ComponentRecorderCallbackFixture : public art::EDProducer {
public:
  struct Config {
    fhicl::Atom<bool> diagnosticOnly{fhicl::Name("diagnosticOnly")};
    fhicl::Table<Mu2eG4Config::Inputs_> inputs{fhicl::Name("inputs")};
  };
  using Parameters = art::EDProducer::Table<Config>;
  explicit ComponentRecorderCallbackFixture(const Parameters& p)
    : art::EDProducer(p), inputs_(p().inputs()), cut_(art::make_tool<IMu2eG4Cut>(recorderConfig())) {
    check(p().diagnosticOnly(), "diagnosticOnly must be true; this is not transport");
    check(inputs_.primaryType().id()==Mu2eG4PrimaryType::GenParticles, "Fixture requires GenParticles helper mode");
    observer_=dynamic_cast<IMu2eG4Observer*>(cut_.get());
    check(observer_!=nullptr, "Loaded recorder has no observer interface");
    produces<SimParticleCollection>();
    cut_->declareProducts(producesCollector(), consumesCollector());
  }
  void beginJob() override;
  void produce(art::Event&) override;
  void endJob() override {
    check(seen_==13, "The complete fixed 13-event callback matrix is required");
  }
private:
  Mu2eG4Inputs inputs_;
  std::unique_ptr<IMu2eG4Cut> cut_;
  IMu2eG4Observer* observer_=nullptr;
  std::unique_ptr<G4Box> worldSolid_, envelopeSolid_;
  std::unique_ptr<G4LogicalVolume> worldLogical_, envelopeLogical_;
  std::unique_ptr<G4PVPlacement> world_, envelope_;
  G4TouchableHandle outside_, inside_;
  unsigned seen_=0, run_=0, subrun_=0;
  std::unique_ptr<G4Track> makeTrack(int id, int parent, double z, double time, double direction,
                                   double energy, const G4TouchableHandle&) const;
  void start(G4Track&);
  G4ThreeVector step(G4Track&, double z, double time, const G4TouchableHandle&, const G4TouchableHandle&,
                     G4StepStatus, G4Track* child=nullptr);
};

void ComponentRecorderCallbackFixture::beginJob() {
  check(G4PhysicalVolumeStore::GetInstance()->empty(), "Callback fixture requires an otherwise empty geometry store");
  auto* nist=G4NistManager::Instance();
  auto* vacuum=nist->FindOrBuildMaterial("G4_Galactic");
  auto* material=nist->FindOrBuildMaterial("G4_POLYSTYRENE");
  check(vacuum && material, "Missing fixture materials");
  worldSolid_=std::make_unique<G4Box>("CallbackFixtureWorldSolid",500.,500.,500.);
  envelopeSolid_=std::make_unique<G4Box>("CallbackFixtureEnvelopeSolid",50.,50.,50.);
  worldLogical_=std::make_unique<G4LogicalVolume>(worldSolid_.get(),vacuum,"CallbackFixtureWorldLogical");
  envelopeLogical_=std::make_unique<G4LogicalVolume>(envelopeSolid_.get(),material,"CallbackFixtureEnvelopeLogical");
  world_=std::make_unique<G4PVPlacement>(nullptr,G4ThreeVector{},worldLogical_.get(),
                                       "CallbackFixtureWorld",nullptr,false,0,false);
  envelope_=std::make_unique<G4PVPlacement>(nullptr,G4ThreeVector{},envelopeLogical_.get(),
                                          "CallbackFixtureEnvelope",worldLogical_.get(),false,0,false);
  G4NavigationHistory history;
  history.SetFirstEntry(world_.get());
  outside_=new G4TouchableHistory(history);
  history.NewLevel(envelope_.get(),kNormal,0);
  inside_=new G4TouchableHistory(history);
  check(outside_->GetHistoryDepth()==0 && outside_->GetVolume()==world_.get(), "Bad outside history");
  check(inside_->GetHistoryDepth()==1 && inside_->GetVolume()==envelope_.get(), "Bad inside history");
  check(envelopeSolid_->Inside(G4ThreeVector(0.,0.,50.))==kSurface, "Fixture endpoint is not on the surface");
  check(envelopeSolid_->Inside(G4ThreeVector{})==kInside, "Fixture control is not inside");
  std::cout << "ComponentRecorderCallbackFixture: constructed objects only, NO PHYSICS TRANSPORT\n";
}

std::unique_ptr<G4Track> ComponentRecorderCallbackFixture::makeTrack(
    int id, int parent, double z, double time, double direction, double energy, const G4TouchableHandle& touch) const {
  auto dynamic=std::make_unique<G4DynamicParticle>(G4Gamma::GammaDefinition(),G4ThreeVector(0.,0.,direction),energy);
  auto track=std::make_unique<G4Track>(dynamic.release(),time,G4ThreeVector(0.,0.,z));
  track->SetTrackID(id); track->SetParentID(parent); track->SetWeight(1.);
  track->SetTouchableHandle(touch); track->SetNextTouchableHandle(touch); track->SetOriginTouchableHandle(touch);
  track->SetProperTime(0.);
  return track;
}

void ComponentRecorderCallbackFixture::start(G4Track& track) {
  const bool killed=cut_->stackingActionCut(&track);
  check(!killed, "Recorder unexpectedly rejects a scripted classification");
  observer_->classifiedTrack(&track,killed);
  observer_->beginTrack(&track);
}

G4ThreeVector ComponentRecorderCallbackFixture::step(
    G4Track& track, double z, double time, const G4TouchableHandle& preTouch,
    const G4TouchableHandle& postTouch, G4StepStatus status, G4Track* child) {
  OwnedStep current;
  current.SetTrack(&track);
  auto* pre=current.GetPreStepPoint();
  pre->SetPosition(track.GetPosition()); pre->SetMomentumDirection(track.GetMomentumDirection());
  pre->SetKineticEnergy(track.GetKineticEnergy()); pre->SetGlobalTime(track.GetGlobalTime());
  pre->SetLocalTime(track.GetLocalTime()); pre->SetProperTime(track.GetProperTime());
  pre->SetMass(0.); pre->SetCharge(0.); pre->SetWeight(1.); pre->SetStepStatus(fUndefined);
  pre->SetTouchableHandle(preTouch); pre->SetMaterial(preTouch->GetVolume()->GetLogicalVolume()->GetMaterial());
  auto* post=current.GetPostStepPoint();
  *post=*pre;
  post->SetPosition(G4ThreeVector(0.,0.,z)); post->SetGlobalTime(time); post->SetStepStatus(status);
  post->SetTouchableHandle(postTouch); post->SetMaterial(postTouch->GetVolume()->GetLogicalVolume()->GetMaterial());
  current.SetStepLength((post->GetPosition()-pre->GetPosition()).mag());
  if (child) {
    child->SetPosition(post->GetPosition()); child->SetGlobalTime(post->GetGlobalTime());
    current.GetfSecondary()->push_back(child);
  }
  check(current.GetNumberOfSecondariesInCurrentStep()==(child ? 1u : 0u), "Bad current-step secondary bucket");
  track.SetPosition(post->GetPosition()); track.SetGlobalTime(time); track.SetTouchableHandle(preTouch);
  track.SetNextTouchableHandle(postTouch); track.IncrementCurrentStepNumber(); track.SetStep(&current);
  const bool exit=cut_->steppingActionCut(&current);
  const bool outward=preTouch->GetVolume()==envelope_.get() && postTouch->GetVolume()==world_.get();
  check(exit==outward, "Recorder returned an unexpected step decision");
  if (exit) {
    track.SetTrackStatus(fStopAndKill);
    observer_->endTrack(&track);
  }
  track.SetTouchableHandle(postTouch); track.SetStep(nullptr);
  return post->GetPosition();
}

void ComponentRecorderCallbackFixture::produce(art::Event& event) {
  const unsigned number=event.id().event();
  check(number==seen_+1 && number<=13, "Fixture requires consecutive event IDs 1 through 13");
  if (!seen_) { run_=event.id().run(); subrun_=event.id().subRun(); }
  check(event.id().run()==run_ && event.id().subRun()==subrun_, "Fixture must stay in one run/subrun");
  ++seen_;
  const auto id=event.getProductID<SimParticleCollection>();
  SimParticleHelper helper(0u,inputs_,id,&event,event.productGetter(id));
  cut_->finishConstruction(CLHEP::Hep3Vector{});
  cut_->beginEvent(event,helper);
  const bool surface=number<=4;
  const bool outside=number==3 || number==4;
  const double direction=(number==1 || number==3) ? -1. : 1.;
  auto parent=makeTrack(1,0,-50.001,0.,1.,10.,outside_);
  auto child=makeTrack(2,1,surface ? 50. : 0.,surface ? 3. : 2.,direction,1.,outside ? outside_ : inside_);
  start(*parent);
  step(*parent,-50.,1.,outside_,inside_,fGeomBoundary);
  step(*parent,0.,2.,inside_,inside_,fPostStepDoItProc,surface ? nullptr : child.get());
  const auto endpoint=step(*parent,50.,3.,inside_,outside_,fGeomBoundary,surface ? child.get() : nullptr);
  if (surface) check(child->GetPosition()==endpoint, "Child is not at the parent's exact outward endpoint");
  check(child->GetCurrentStepNumber()==0 && child->GetStep()==nullptr, "Child is not a zero-step object");
  if (number==6) child->SetTrackStatus(fStopAndKill);
  start(*child);
  if (number==11) observer_->classifiedTrack(child.get(),false);
  G4TrackStatus terminal=fStopAndKill;
  if (number==7) terminal=fKillTrackAndSecondaries;
  if (number==8 || number==12) terminal=fSuspend;
  if (number==9) terminal=fSuspendAndWait;
  if (number==10) terminal=fPostponeToNextEvent;
  child->SetTrackStatus(terminal);
  observer_->endTrack(child.get());
  if (number==12) {
    bool caught=false;
    try { observer_->beginTrack(child.get()); }
    catch (const cet::exception& ex) {
      if (std::string(ex.what()).find("Resumed/duplicate track is unsupported in schema 1")==std::string::npos) throw;
      caught=true;
    }
    check(caught, "Resumed track did not throw the expected diagnostic exception");
    cut_->deleteCutsData();
    std::cout << "ComponentRecorderCallbackFixture event 12: expected resume exception caught; buffer discarded\n";
  } else {
    cut_->put(event);
  }
  event.put(std::make_unique<SimParticleCollection>());
  std::cout << "ComponentRecorderCallbackFixture completed constructed-object case " << number << '\n';
}
}
DEFINE_ART_MODULE(mu2e::ComponentRecorderCallbackFixture)
