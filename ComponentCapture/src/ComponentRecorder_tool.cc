#include "Offline/MCDataProducts/inc/ComponentCapture.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Cut.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Observer.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4UserTrackInformation.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4UserHelpers.hh"
#include "CLHEP/Units/SystemOfUnits.h"
#include "art/Framework/Core/ProducesCollector.h"
#include "art/Framework/Principal/Event.h"
#include "art/Utilities/ToolMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "Geant4/G4Box.hh"
#include "Geant4/G4DynamicParticle.hh"
#include "Geant4/G4RotationMatrix.hh"
#include "Geant4/G4StepPoint.hh"
#include "Geant4/G4VPhysicalVolume.hh"
#include "Geant4/G4LogicalVolume.hh"
#include "Geant4/G4Material.hh"
#include "Geant4/G4PhysicalVolumeStore.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4VProcess.hh"
#include "Geant4/G4VTouchable.hh"
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace mu2e {
  namespace {
    std::vector<double> matrix(const G4RotationMatrix& r) {
      return {r.xx(), r.xy(), r.xz(), r.yx(), r.yy(), r.yz(), r.zx(), r.zy(), r.zz()};
    }
  }

  class ComponentRecorder : public IMu2eG4Cut, public IMu2eG4Observer {
  public:
    explicit ComponentRecorder(const fhicl::ParameterSet& p)
      : envelopeName_(p.get<std::string>("envelope")), geometryTag_(p.get<std::string>("geometryTag")),
        physicsTag_(p.get<std::string>("physicsTag")), maxSteps_(p.get<unsigned>("maxSteps")),
        maxTracks_(p.get<unsigned>("maxTracks")) {
      if (!maxSteps_ || !maxTracks_ || geometryTag_.empty() || physicsTag_.empty())
        throw cet::exception("ComponentCapture") << "Nonempty tags and positive limits required";
    }
    void declareProducts(art::ProducesCollector& pc, art::ConsumesCollector&) override {
      pc.produces<ComponentCapture>("componentCapture");
    }
    void finishConstruction(const CLHEP::Hep3Vector&) override;
    void beginEvent(const art::Event&, const SimParticleHelper&) override;
    void put(art::Event&) override;
    void deleteCutsData() override { data_.reset(); tracks_.clear(); classified_.clear(); }
    bool stackingActionCut(const G4Track*) override { return false; }
    bool steppingActionCut(const G4Step*) override;
    void classifiedTrack(const G4Track*, bool) override;
    void beginTrack(const G4Track*) override;
    void endTrack(const G4Track*) override;
  private:
    std::string envelopeName_, geometryTag_, physicsTag_;
    unsigned maxSteps_, maxTracks_;
    G4VPhysicalVolume* envelope_=nullptr;
    G4RotationMatrix rotation_;
    G4ThreeVector translation_;
    std::vector<double> halfSize_;
    std::vector<ComponentVolume> volumes_;
    std::map<const G4VPhysicalVolume*, std::int64_t> volumeIds_;
    std::map<std::int64_t, std::size_t> tracks_;
    std::set<std::int64_t> classified_;
    std::unique_ptr<ComponentCapture> data_;
    bool inside(const G4VTouchable*) const;
    ComponentState state(const G4Track*) const;
    ComponentState state(const G4StepPoint*, double) const;
    void coordinates(ComponentState&, const G4ThreeVector&, const G4ThreeVector&) const;
    void placement(ComponentState&, const G4VTouchable*) const;
    void error(const std::string& text) {
      if (std::find(data_->errors.begin(), data_->errors.end(), text)==data_->errors.end())
        data_->errors.push_back(text);
    }
    ComponentTrack& track(std::int64_t id) {
      const auto it=tracks_.find(id);
      if (it==tracks_.end()) throw cet::exception("ComponentCapture") << "Callback without beginTrack: " << id;
      return data_->tracks.at(it->second);
    }
  };

  void ComponentRecorder::finishConstruction(const CLHEP::Hep3Vector&) {
    envelope_=nullptr;
    volumes_.clear(); volumeIds_.clear();
    const auto* store=G4PhysicalVolumeStore::GetInstance();
    for (auto* pv : *store) {
      if (pv->IsReplicated() || pv->IsParameterised())
        throw cet::exception("ComponentCapture") << "Replica/parameterised geometry is not supported by schema 1";
      const auto id=static_cast<std::int64_t>(volumes_.size());
      volumeIds_.emplace(pv,id);
      ComponentVolume v;
      v.id=id; v.copy=pv->GetCopyNo(); v.name=pv->GetName();
      const auto* lv=pv->GetLogicalVolume();
      v.logical=lv->GetName(); v.solid=lv->GetSolid()->GetEntityType();
      v.material=lv->GetMaterial()->GetName(); v.density=lv->GetMaterial()->GetDensity()/(CLHEP::g/CLHEP::cm3);
      if (pv->GetMotherLogical()) v.motherLogical=pv->GetMotherLogical()->GetName();
      const auto t=pv->GetObjectTranslation();
      v.translation={t.x(),t.y(),t.z()}; v.rotation=matrix(pv->GetObjectRotationValue());
      volumes_.push_back(std::move(v));
      if (pv->GetName()==envelopeName_) {
        if (envelope_) throw cet::exception("ComponentCapture") << "Envelope placement is not unique";
        envelope_=pv;
      }
    }
    if (!envelope_) throw cet::exception("ComponentCapture") << "Missing envelope " << envelopeName_;
    auto* mother=envelope_->GetMotherLogical();
    std::set<const G4LogicalVolume*> ancestors;
    while (mother) {
      if (!ancestors.insert(mother).second) throw cet::exception("ComponentCapture") << "Cyclic geometry";
      const G4VPhysicalVolume* parent=nullptr;
      for (auto* pv : *store) if (pv->GetLogicalVolume()==mother) {
        if (parent) throw cet::exception("ComponentCapture") << "Envelope ancestors must have unique placements";
        parent=pv;
      }
      if (!parent || parent->GetObjectTranslation().mag2()!=0 ||
          parent->GetObjectRotationValue()!=G4RotationMatrix() || mother->GetMaterial()->GetName()!="G4_Galactic")
        throw cet::exception("ComponentCapture") << "Envelope ancestors must be identity-placed vacuum volumes";
      mother=parent->GetMotherLogical();
    }
    const auto* box=dynamic_cast<const G4Box*>(envelope_->GetLogicalVolume()->GetSolid());
    if (!box) throw cet::exception("ComponentCapture") << "Schema 1 supports only a finite box envelope";
    halfSize_={box->GetXHalfLength(),box->GetYHalfLength(),box->GetZHalfLength()};
    rotation_=envelope_->GetObjectRotationValue(); translation_=envelope_->GetObjectTranslation();
  }

  bool ComponentRecorder::inside(const G4VTouchable* t) const {
    if (!t) return false;
    for (int i=0; i<=t->GetHistoryDepth(); ++i) if (t->GetVolume(i)==envelope_) return true;
    return false;
  }

  void ComponentRecorder::coordinates(ComponentState& s, const G4ThreeVector& p, const G4ThreeVector& m) const {
    const auto local=rotation_.inverse()*(p-translation_);
    const auto momentum=rotation_.inverse()*m;
    s.x=p.x(); s.y=p.y(); s.z=p.z(); s.px=m.x(); s.py=m.y(); s.pz=m.z();
    s.lx=local.x(); s.ly=local.y(); s.lz=local.z(); s.lpx=momentum.x(); s.lpy=momentum.y(); s.lpz=momentum.z();
  }

  void ComponentRecorder::placement(ComponentState& s, const G4VTouchable* t) const {
    if (!t || !t->GetVolume()) return;
    std::ostringstream path;
    for (int i=t->GetHistoryDepth(); i>=0; --i) {
      const auto* pv=t->GetVolume(i);
      if (!pv) continue;
      const auto it=volumeIds_.find(pv);
      if (it==volumeIds_.end()) throw cet::exception("ComponentCapture") << "Unknown physical placement";
      path << '/' << it->second << ':' << t->GetCopyNumber(i);
    }
    s.placement=path.str(); s.volume=volumeIds_.at(t->GetVolume());
    s.material=t->GetVolume()->GetLogicalVolume()->GetMaterial()->GetName();
  }

  ComponentState ComponentRecorder::state(const G4Track* t) const {
    ComponentState s;
    coordinates(s,t->GetPosition(),t->GetMomentum()); placement(s,t->GetTouchable());
    s.energy=t->GetTotalEnergy(); s.kinetic=t->GetKineticEnergy();
    s.mass=t->GetDynamicParticle()->GetMass(); s.charge=t->GetDynamicParticle()->GetCharge();
    s.time=t->GetGlobalTime(); s.properTime=t->GetProperTime(); s.weight=t->GetWeight();
    if (t->GetCreatorProcess()) s.process=t->GetCreatorProcess()->GetProcessName();
    return s;
  }

  ComponentState ComponentRecorder::state(const G4StepPoint* p, double weight) const {
    ComponentState s;
    coordinates(s,p->GetPosition(),p->GetMomentum()); placement(s,p->GetTouchable());
    s.energy=p->GetTotalEnergy(); s.kinetic=p->GetKineticEnergy(); s.mass=p->GetMass(); s.charge=p->GetCharge();
    s.time=p->GetGlobalTime(); s.properTime=p->GetProperTime(); s.weight=weight;
    s.stepStatus=static_cast<int>(p->GetStepStatus());
    if (p->GetMaterial()) s.material=p->GetMaterial()->GetName();
    if (p->GetProcessDefinedStep()) s.process=p->GetProcessDefinedStep()->GetProcessName();
    return s;
  }

  void ComponentRecorder::beginEvent(const art::Event& event, const SimParticleHelper&) {
    deleteCutsData();
    if (!envelope_) throw cet::exception("ComponentCapture") << "Geometry has not been resolved";
    data_=std::make_unique<ComponentCapture>();
    data_->run=event.id().run(); data_->subrun=event.id().subRun(); data_->event=event.id().event();
    data_->geometryTag=geometryTag_; data_->physicsTag=physicsTag_; data_->envelope=envelopeName_;
    data_->translation={translation_.x(),translation_.y(),translation_.z()};
    data_->rotation=matrix(rotation_); data_->halfSize=halfSize_; data_->volumes=volumes_;
  }

  void ComponentRecorder::classifiedTrack(const G4Track* t, bool killed) {
    if (!data_) throw cet::exception("ComponentCapture") << "Classification outside an event";
    if (data_->classifications.size()>=maxTracks_)
      throw cet::exception("ComponentCapture") << "Classification capacity exceeded; no truncation allowed";
    ComponentClassification c;
    c.track=t->GetTrackID(); c.parent=t->GetParentID(); c.pdg=t->GetDefinition()->GetPDGEncoding();
    c.status=static_cast<int>(t->GetTrackStatus()); c.killed=killed; c.state=state(t);
    data_->classifications.push_back(std::move(c));
    if (killed) error("classification_rejection");
    if (t->GetTrackID()<=0) error("classification_without_final_id");
    if (!classified_.insert(t->GetTrackID()).second) error("reclassification_unsupported");
  }

  void ComponentRecorder::beginTrack(const G4Track* t) {
    if (!data_) throw cet::exception("ComponentCapture") << "Tracking outside an event";
    if (data_->tracks.size()>=maxTracks_) throw cet::exception("ComponentCapture") << "Track capacity exceeded";
    if (!tracks_.emplace(t->GetTrackID(),data_->tracks.size()).second)
      throw cet::exception("ComponentCapture") << "Resumed/duplicate track is unsupported in schema 1";
    ComponentTrack r;
    r.track=t->GetTrackID(); r.parent=t->GetParentID(); r.pdg=t->GetDefinition()->GetPDGEncoding(); r.begin=state(t);
    r.creator=r.begin.process;
    if (t->GetTrackID()<=0 || !classified_.count(t->GetTrackID())) error("track_without_classification");
    if (t->GetWeight()!=1.) error("biased_track_weight");
    if (inside(t->GetTouchable())) { r.entered=true; r.entrance=r.begin; }
    if (r.parent) {
      if (!r.entered) error("secondary_born_outside_envelope");
      const G4ThreeVector local(r.begin.lx,r.begin.ly,r.begin.lz);
      if (envelope_->GetLogicalVolume()->GetSolid()->Inside(local)==kSurface)
        error("boundary_born_secondary_ownership_unresolved");
    } else if (r.entered) error("primary_must_start_in_vacuum_upstream");
    data_->tracks.push_back(std::move(r));
  }

  bool ComponentRecorder::steppingActionCut(const G4Step* step) {
    if (data_->steps.size()>=maxSteps_) throw cet::exception("ComponentCapture") << "Step capacity exceeded";
    auto& r=track(step->GetTrack()->GetTrackID());
    if (r.reason==2 || r.ended) throw cet::exception("ComponentCapture") << "Step after first exit/terminal";
    ComponentStep s;
    s.track=r.track; s.number=step->GetTrack()->GetCurrentStepNumber();
    s.secondaries=step->GetNumberOfSecondariesInCurrentStep();
    s.deposit=step->GetTotalEnergyDeposit(); s.nonIonizingDeposit=step->GetNonIonizingEnergyDeposit();
    s.length=step->GetStepLength(); s.trackStatus=static_cast<int>(step->GetTrack()->GetTrackStatus());
    s.pre=state(step->GetPreStepPoint(),step->GetPreStepPoint()->GetWeight());
    s.post=state(step->GetPostStepPoint(),step->GetPostStepPoint()->GetWeight());
    s.preInside=inside(step->GetPreStepPoint()->GetTouchable());
    s.postInside=inside(step->GetPostStepPoint()->GetTouchable());
    if (s.number!=++r.steps) error("missing_step_callback");
    r.secondaries+=s.secondaries;
    if (!s.preInside && s.postInside) {
      if (r.entered) error("multiple_component_entries");
      r.entered=true; r.entrance=s.post;
      if (s.post.stepStatus!=fGeomBoundary) error("entry_without_geometry_boundary");
    }
    if (s.preInside && !r.entered) error("unrecorded_component_entry");
    if (!s.preInside && (r.parent || s.secondaries!=0 || s.pre.material!="G4_Galactic"))
      error("nontrivial_outside_transport");
    if (s.post.process=="mu2eSpecialCutsProcess") error("resource_kill_step");
    const auto* info=dynamic_cast<const Mu2eG4UserTrackInformation*>(step->GetTrack()->GetUserInformation());
    if (info && info->isForced()) error("external_forced_stop");
    s.firstExit=s.preInside && !s.postInside;
    if (s.firstExit) {
      if (s.post.stepStatus!=fGeomBoundary) error("exit_without_geometry_boundary");
      if (s.trackStatus!=fAlive && s.trackStatus!=fStopButAlive) error("nontransport_terminal_at_boundary");
      r.reason=2; r.end=s.post; r.status=s.trackStatus; r.stopProcess=s.post.process;
    }
    r.stopProcess=s.post.process;
    data_->steps.push_back(std::move(s));
    return data_->steps.back().firstExit;
  }

  void ComponentRecorder::endTrack(const G4Track* t) {
    auto& r=track(t->GetTrackID());
    if (r.ended) error("duplicate_end_track");
    r.ended=true;
    if (r.reason!=2) {
      r.end=state(t); r.end.process=r.stopProcess; r.status=static_cast<int>(t->GetTrackStatus());
      r.reason=1;
      const auto* info=dynamic_cast<const Mu2eG4UserTrackInformation*>(t->GetUserInformation());
      if (info && info->isForced()) { error("external_forced_stop"); r.reason=3; }
      if (t->GetTrackStatus()!=fStopAndKill) { error("nonterminal_or_kill_secondaries_status"); r.reason=3; }
      if (t->GetStep() && t->GetTrackStatus()==fStopAndKill &&
          Mu2eG4UserHelpers::isTrackKilledByFieldPropagator(t,0)) {
        error("transportation_resource_kill"); r.reason=3;
      }
      if (!r.entered) error("track_never_entered_component");
    } else if (t->GetTrackStatus()!=fStopAndKill) error("first_exit_not_stopped");
    if (r.steps!=t->GetCurrentStepNumber()) error("track_step_count_mismatch");
  }

  void ComponentRecorder::put(art::Event& event) {
    if (!data_) throw cet::exception("ComponentCapture") << "Missing event buffer";
    std::map<std::int64_t,std::int64_t> children;
    unsigned primaries=0;
    for (const auto& r : data_->tracks) {
      if (!r.ended) error("unclosed_track");
      if (r.parent) {
        if (!tracks_.count(r.parent)) error("missing_parent");
        ++children[r.parent];
      } else ++primaries;
    }
    if (primaries!=1) error("expected_one_incident_primary");
    for (const auto& r : data_->tracks)
      if (children[r.track]!=r.secondaries) error("secondary_census_mismatch");
    if (data_->tracks.size()!=data_->classifications.size()) error("classification_tracking_census_mismatch");
    data_->complete=data_->errors.empty();
    event.put(std::move(data_),"componentCapture");
    tracks_.clear(); classified_.clear();
  }
}
DEFINE_ART_CLASS_TOOL(mu2e::ComponentRecorder)
