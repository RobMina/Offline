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
#include "Geant4/G4Element.hh"
#include "Geant4/G4IonisParamMat.hh"
#include "Geant4/G4RotationMatrix.hh"
#include "Geant4/G4StepPoint.hh"
#include "Geant4/G4VPhysicalVolume.hh"
#include "Geant4/G4LogicalVolume.hh"
#include "Geant4/G4Material.hh"
#include "Geant4/G4MaterialCutsCouple.hh"
#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4ParticleTable.hh"
#include "Geant4/G4PhysicalVolumeStore.hh"
#include "Geant4/G4ProcessManager.hh"
#include "Geant4/G4ProcessVector.hh"
#include "Geant4/G4ProductionCuts.hh"
#include "Geant4/G4ProductionCutsTable.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4VProcess.hh"
#include "Geant4/G4VTouchable.hh"
#include "Geant4/G4Version.hh"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace mu2e {
  namespace {
    std::vector<double> matrix(const G4RotationMatrix& r) {
      return {r.xx(), r.xy(), r.xz(), r.yx(), r.yy(), r.yz(), r.zx(), r.zy(), r.zz()};
    }
    // FNV-1a 64-bit of the canonical manifest text, as 16 lowercase hex digits.
    std::string fnv1a(const std::string& text) {
      std::uint64_t h=14695981039346656037ull;
      for (const unsigned char c : text) { h^=c; h*=1099511628211ull; }
      char out[17];
      std::snprintf(out,sizeof out,"%016llx",static_cast<unsigned long long>(h));
      return out;
    }
    constexpr int pilotSpecies[]={13,-13,11,-11,22,2112,2212,211,-211};
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
      pc.produces<ComponentCaptureLinks>("componentCaptureLinks");
    }
    void finishConstruction(const CLHEP::Hep3Vector&) override;
    void beginEvent(const art::Event&, const SimParticleHelper&) override;
    void put(art::Event&) override;
    void deleteCutsData() override {
      data_.reset(); links_.reset(); tracks_.clear(); classified_.clear();
      pending_.clear(); creatorOf_.clear(); birthExits_.clear();
    }
    bool stackingActionCut(const G4Track*) override;
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
    std::unique_ptr<ComponentCaptureLinks> links_;
    // Unclassified secondaries keyed by engine pointer, mapped to the index of the step that created them.
    std::map<const G4Track*, std::int64_t> pending_;
    std::map<std::int64_t, std::int64_t> creatorOf_;
    std::set<const G4Track*> birthExits_;
    std::string manifest_, manifestHash_;
    bool inside(const G4VTouchable*) const;
    bool onSurface(const ComponentState&) const;
    void buildManifest();
    void pushTrack(ComponentTrack&&, const ComponentTrackLink&);
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

  bool ComponentRecorder::onSurface(const ComponentState& s) const {
    return envelope_->GetLogicalVolume()->GetSolid()->Inside(G4ThreeVector(s.lx,s.ly,s.lz))==kSurface;
  }

  void ComponentRecorder::buildManifest() {
    std::ostringstream m;
    m << std::setprecision(17) << "manifest|1\ng4|" << G4Version << "\nphysicsTag|" << physicsTag_ << '\n';
    // Declared Geant4 behaviour, not a tuned value: muon-decay daughters total m_mu - m_e.
    m << "declared|G4MuonDecayChannel|daughterTotalEnergy=m_mu-m_e\n";
    std::set<std::string> names;
    for (const auto& v : volumes_) names.insert(v.material);
    for (const auto& name : names) {
      const auto* mat=G4Material::GetMaterial(name,false);
      m << "material|" << name;
      if (mat) {
        m << '|' << mat->GetDensity()/(CLHEP::g/CLHEP::cm3) << '|'
          << mat->GetIonisation()->GetMeanExcitationEnergy()/CLHEP::eV << '|' << static_cast<int>(mat->GetState())
          << '|' << mat->GetTemperature()/CLHEP::kelvin;
        const auto* fractions=mat->GetFractionVector();
        for (std::size_t i=0; i<mat->GetNumberOfElements(); ++i)
          m << '|' << mat->GetElement(static_cast<G4int>(i))->GetName() << ':' << fractions[i];
      } else m << "|missing";
      m << '\n';
    }
    auto* cuts=G4ProductionCutsTable::GetProductionCutsTable();
    for (std::size_t i=0; i<cuts->GetTableSize(); ++i) {
      const auto* couple=cuts->GetMaterialCutsCouple(static_cast<G4int>(i));
      if (!couple || !couple->IsUsed()) continue;
      m << "cut|" << couple->GetMaterial()->GetName();
      for (int k=0; k<4; ++k) m << '|' << couple->GetProductionCuts()->GetProductionCut(k)/CLHEP::mm;
      for (int k=0; k<4; ++k) {
        const auto* energies=cuts->GetEnergyCutsVector(k);
        m << '|' << (energies && i<energies->size() ? (*energies)[i]/CLHEP::MeV : -1.);
      }
      m << '\n';
    }
    // Without a physics list the particle table must not be queried; record that instead.
    auto* particles=G4ParticleTable::GetParticleTable();
    for (const int pdg : pilotSpecies) {
      m << "processes|" << pdg;
      if (!particles->GetReadiness()) { m << "|unavailable\n"; continue; }
      const auto* particle=particles->FindParticle(pdg);
      const auto* manager=particle ? particle->GetProcessManager() : nullptr;
      if (manager) {
        const auto* list=manager->GetProcessList();
        for (std::size_t i=0; i<list->size(); ++i) m << '|' << (*list)[i]->GetProcessName();
      }
      m << '\n';
    }
    manifest_=m.str(); manifestHash_=fnv1a(manifest_);
  }

  void ComponentRecorder::pushTrack(ComponentTrack&& r, const ComponentTrackLink& link) {
    if (!tracks_.emplace(r.track,data_->tracks.size()).second)
      throw cet::exception("ComponentCapture") << "Resumed/duplicate track is unsupported in schema 1";
    data_->tracks.push_back(std::move(r));
    links_->tracks.push_back(link);
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
    links_=std::make_unique<ComponentCaptureLinks>();
    links_->run=data_->run; links_->subrun=data_->subrun; links_->event=data_->event;
    buildManifest();
    links_->g4Version=G4Version; links_->manifest=manifest_; links_->manifestHash=manifestHash_;
  }

  // A child of an in-component interaction that the navigator places outside the envelope exits at birth.
  bool ComponentRecorder::stackingActionCut(const G4Track* t) {
    if (!data_ || t->GetParentID()==0) return false;
    const auto it=pending_.find(t);
    if (it==pending_.end() || !data_->steps.at(it->second).preInside || inside(t->GetTouchable())) return false;
    birthExits_.insert(t);
    return true;
  }

  void ComponentRecorder::classifiedTrack(const G4Track* t, bool killed) {
    if (!data_) throw cet::exception("ComponentCapture") << "Classification outside an event";
    if (data_->classifications.size()>=maxTracks_)
      throw cet::exception("ComponentCapture") << "Classification capacity exceeded; no truncation allowed";
    ComponentClassification c;
    c.track=t->GetTrackID(); c.parent=t->GetParentID(); c.pdg=t->GetDefinition()->GetPDGEncoding();
    c.status=static_cast<int>(t->GetTrackStatus()); c.killed=killed; c.state=state(t);
    const bool repeat=classified_.count(c.track)>0;
    std::int64_t creator=-1;
    if (c.parent && !repeat) {
      const auto it=pending_.find(t);
      if (it==pending_.end()) error("secondary_without_creator_step");
      else { creator=it->second; pending_.erase(it); }
    }
    const bool birthExit=birthExits_.erase(t)>0;
    data_->classifications.push_back(c);
    if (killed && !birthExit) error("classification_rejection");
    if (t->GetTrackID()<=0) error("classification_without_final_id");
    if (!classified_.insert(t->GetTrackID()).second) error("reclassification_unsupported");
    if (!repeat) creatorOf_.emplace(c.track,creator);
    if (!birthExit) return;
    if (!killed) error("exit_at_birth_not_killed");
    if (data_->tracks.size()>=maxTracks_) throw cet::exception("ComponentCapture") << "Track capacity exceeded";
    ComponentTrack r;
    r.track=c.track; r.parent=c.parent; r.pdg=c.pdg; r.begin=c.state; r.end=c.state; r.creator=c.state.process;
    r.ended=true; r.reason=4; r.status=static_cast<int>(fStopAndKill);
    ComponentTrackLink link;
    link.track=r.track; link.parent=r.parent; link.creatorStep=creator; link.exitAtBirth=true;
    link.bornOnSurface=onSurface(r.begin);
    pushTrack(std::move(r),link);
  }

  void ComponentRecorder::beginTrack(const G4Track* t) {
    if (!data_) throw cet::exception("ComponentCapture") << "Tracking outside an event";
    if (data_->tracks.size()>=maxTracks_) throw cet::exception("ComponentCapture") << "Track capacity exceeded";
    ComponentTrack r;
    r.track=t->GetTrackID(); r.parent=t->GetParentID(); r.pdg=t->GetDefinition()->GetPDGEncoding(); r.begin=state(t);
    r.creator=r.begin.process;
    if (t->GetTrackID()<=0 || !classified_.count(t->GetTrackID())) error("track_without_classification");
    if (t->GetWeight()!=1.) error("biased_track_weight");
    if (inside(t->GetTouchable())) { r.entered=true; r.entrance=r.begin; }
    ComponentTrackLink link;
    link.track=r.track; link.parent=r.parent;
    const auto it=creatorOf_.find(r.track);
    if (it!=creatorOf_.end()) link.creatorStep=it->second;
    if (r.parent) {
      // Ownership follows the creating step and the navigator's starting volume, never position alone.
      const bool creatorInside=link.creatorStep>=0 && data_->steps.at(link.creatorStep).preInside;
      if (!r.entered) error("secondary_born_outside_envelope");
      else if (!creatorInside) error("secondary_inside_from_outside_interaction");
      link.bornOnSurface=onSurface(r.begin);
    } else if (r.entered) error("primary_must_start_in_vacuum_upstream");
    pushTrack(std::move(r),link);
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
    const auto index=static_cast<std::int64_t>(data_->steps.size()-1);
    if (const auto* children=step->GetSecondaryInCurrentStep())
      for (const auto* child : *children)
        if (!pending_.emplace(child,index).second) error("duplicate_secondary_pointer");
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
      // A track stopped by its own energy loss on a boundary-limited step is a natural stop, not a resource kill.
      if (t->GetStep() && t->GetTrackStatus()==fStopAndKill && t->GetKineticEnergy()>0. &&
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
    if (!pending_.empty()) error("unclassified_secondary");
    data_->complete=data_->errors.empty();
    event.put(std::move(data_),"componentCapture");
    event.put(std::move(links_),"componentCaptureLinks");
    tracks_.clear(); classified_.clear(); pending_.clear(); creatorOf_.clear(); birthExits_.clear();
  }
}
DEFINE_ART_CLASS_TOOL(mu2e::ComponentRecorder)
