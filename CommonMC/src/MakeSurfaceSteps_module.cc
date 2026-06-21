//
//  This module creates SurfaceStep objects from G4 StepPointMCs
//
//  Original author: David Brown (LBNL) 7/2024
//
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Handle.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Sequence.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "canvas/Utilities/InputTag.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "Offline/GlobalConstantsService/inc/ParticleData.hh"
#include "Offline/DataProducts/inc/SurfaceId.hh"
#include "Offline/MCDataProducts/inc/SurfaceStep.hh"
#include "Offline/MCDataProducts/inc/StepPointMC.hh"
#include "Offline/MCDataProducts/inc/CrvStep.hh"
#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/GeometryService/inc/DetectorSystem.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mu2e {

  class MakeSurfaceSteps : public art::EDProducer {
    public:
      using Name=fhicl::Name;
      using Comment=fhicl::Comment;
      struct Config {
        fhicl::Atom<int> debug{ Name("debugLevel"), Comment("Debug Level"), 0};
        fhicl::Atom<art::InputTag> vdstepmcs { Name("VDStepPointMCs"), Comment("Virtual Detector StepPointMC collection")};
        fhicl::Atom<art::InputTag> absstepmcs { Name("AbsorberStepPointMCs"), Comment("Absorber StepPointMC collection")};
        fhicl::Atom<art::InputTag> ststepmcs { Name("TargetStepPointMCs"), Comment("Stopping target StepPointMC collection")};
        fhicl::Atom<double> maxdgap{ Name("MaxDistGap"), Comment("Maximum dstance gap between aggregated StepPointMCs")};
        fhicl::Atom<double> maxtgap{ Name("MaxTimeGap"), Comment("Maximum time gap between aggregated StepPointMCs")};
        fhicl::Atom<double> maxseglen{ Name("MaxSegmentLength"),
          Comment("Maximum path length (mm) per fixed-Sid SurfaceStep; thick volumes (e.g. concrete) are split into sub-segments so the in-volume trajectory (multiple-scattering) is resolved. Large default disables splitting; thin volumes are unaffected."), 1.0e9 };
        fhicl::Sequence<art::InputTag> dsmatstepmcs { Name("DSMaterialStepPointMCs"),
          Comment("Per-volume StepPointMC collections for DS passive materials + concrete (sensitiveVolumes); each maps to one fixed SurfaceId"), std::vector<art::InputTag>{} };
        fhicl::Sequence<std::string> dsmatsids { Name("DSMaterialSurfaceIds"),
          Comment("SurfaceId name for each DSMaterialStepPointMCs entry (parallel list, same length)"), std::vector<std::string>{} };
        fhicl::OptionalAtom<art::InputTag> crvsteps { Name("CrvSteps"),
          Comment("CrvStep collection; if set, produce one SurfaceStep per CRV sector crossing") };
      };
      using Parameters = art::EDProducer::Table<Config>;
      explicit MakeSurfaceSteps(const Parameters& conf);
    private:
      void produce(art::Event& e) override;
      // aggregate adjacent same-SimParticle steps from one collection into SurfaceSteps with a fixed SurfaceId
      void aggregateFixedSid(StepPointMCCollection const& col, SurfaceId const& sid,
                             GeomHandle<DetectorSystem> const& det, SurfaceStepCollection& ssc) const;
      int debug_;
      double maxdgap_;
      double maxtgap_;
      double maxseglen_;
      art::ProductToken<StepPointMCCollection> vdstepmcs_, absstepmcs_, ststepmcs_;
      std::vector<std::pair<art::ProductToken<StepPointMCCollection>,SurfaceId>> dsmat_; // DS material/concrete collections -> SurfaceId
      std::optional<art::ProductToken<CrvStepCollection>> crvsteps_;
      std::map<VirtualDetectorId,SurfaceId> vdmap_; // map of VDIds to surfaceIds
  };

  MakeSurfaceSteps::MakeSurfaceSteps(const Parameters& config )  :
    art::EDProducer{config},
    debug_(config().debug()),
    maxdgap_(config().maxdgap()),
    maxtgap_(config().maxtgap()),
    maxseglen_(config().maxseglen()),
    vdstepmcs_{ consumes<StepPointMCCollection>(config().vdstepmcs())},
    absstepmcs_{ consumes<StepPointMCCollection>(config().absstepmcs())},
    ststepmcs_{ consumes<StepPointMCCollection>(config().ststepmcs())}
    {
      produces <SurfaceStepCollection>();
      // DS passive-material + concrete collections, each with a fixed SurfaceId (parallel lists)
      auto const& dsmattags = config().dsmatstepmcs();
      auto const& dsmatsids = config().dsmatsids();
      if(dsmattags.size() != dsmatsids.size())
        throw cet::exception("Configuration") << "MakeSurfaceSteps: DSMaterialStepPointMCs ("
          << dsmattags.size() << ") and DSMaterialSurfaceIds (" << dsmatsids.size() << ") must have the same length" << std::endl;
      for(size_t i=0; i<dsmattags.size(); ++i)
        dsmat_.emplace_back(consumes<StepPointMCCollection>(dsmattags[i]), SurfaceId(dsmatsids[i]));
      // optional CRV truth steps
      art::InputTag crvtag;
      if(config().crvsteps(crvtag)) crvsteps_ = consumes<CrvStepCollection>(crvtag);
      // build the VDId -> SId map by hand. This should come from a service TODO
      vdmap_[VirtualDetectorId(VirtualDetectorId::TT_FrontHollow)] = SurfaceId("TT_Front");
      vdmap_[VirtualDetectorId(VirtualDetectorId::TT_Mid)] = SurfaceId("TT_Mid");
      vdmap_[VirtualDetectorId(VirtualDetectorId::TT_MidInner)] = SurfaceId("TT_Mid");
      vdmap_[VirtualDetectorId(VirtualDetectorId::TT_Back)] = SurfaceId("TT_Back");
      vdmap_[VirtualDetectorId(VirtualDetectorId::TT_OutSurf)] = SurfaceId("TT_Outer");
      vdmap_[VirtualDetectorId(VirtualDetectorId::TT_InSurf)] = SurfaceId("TT_Inner");
    }

  void MakeSurfaceSteps::produce(art::Event& event) {
    GeomHandle<DetectorSystem> det;
    // create output
    std::unique_ptr<SurfaceStepCollection> ssc(new SurfaceStepCollection);
    // start with virtual detectors; these are copied directly
    auto const& vdspmccol_h = event.getValidHandle<StepPointMCCollection>(vdstepmcs_);
    auto const& vdspmccol = *vdspmccol_h;
    for(auto const& vdspmc : vdspmccol) {
      // only some VDs are kept
      auto isid = vdmap_.find(vdspmc.virtualDetectorId());
      if(isid != vdmap_.end())ssc->emplace_back(isid->second,vdspmc,det); // no aggregation of VD hits
    }
    auto nvdsteps = ssc->size();
    if(debug_ > 0)std::cout << "Added " << nvdsteps << " VD steps " << std::endl;
    // now absorbers: IPA, OPA, and TSDA
    // colate adjacent steppointMCs from the same particle. The following assumes the StepPointMCs from the same SimParticle are adjacent and (roughly) time-ordered. This is observationally true currently, but if G4 ever evolves so that its not, this code will need reworking
    auto const& absspmccol_h = event.getValidHandle<StepPointMCCollection>(absstepmcs_);
    auto const& absspmccol = *absspmccol_h;
    if(debug_ > 0)std::cout << "Absorber StepPointMC collection has " << absspmccol.size() << " Entries" << std::endl;
    // match steps by their sim particle. There is only 1 volume for IPA
    SurfaceStep absorberstep;
    for(auto const& spmc : absspmccol) {
      // convert to detector coordinates
      auto spmcpos = XYZVectorF(det->toDetector(spmc.position()));
      bool added(false);
      // decide if this step is contiguous to existing steps already aggregated
      // first, test SimParticle (surface is defined by the collection)
      if(absorberstep.simParticle() == spmc.simParticle()){
        if(debug_ > 2) std::cout << "Same SimParticle" << std::endl;
        // then time;
        auto tgap = fabs(absorberstep.time()-spmc.time());
        auto dgap = (absorberstep.endPosition() - spmcpos).R();
        if(spmc.time() < absorberstep.time())throw cet::exception("Simulation") << " StepPointMC times out-of-order" << std::endl;
        if(debug_ > 2) std::cout << "Time gap " << tgap << " Distance gap " << dgap << std::endl;
        if(tgap < maxtgap_ && dgap < maxdgap_){
          // accumulate this step into the existing surface step
          if(debug_ > 1)std::cout <<"Added step" << std::endl;
          absorberstep.addStep(spmc,det);
          added = true;
        }
      }
      if(!added){
        // if the existing step is valid, save it
        if(absorberstep.surfaceId() != SurfaceIdDetail::unknown && absorberstep.simParticle().isNonnull())ssc->push_back(absorberstep);
        // start a new SurfaceStep for this step
        // hack around the fact that the IPA, OPA, and TSDA are all in the same collection ('absorber')
        SurfaceId sid;
        if(spmcpos.Z() < -5960) // TSDA
          sid = SurfaceId(SurfaceIdDetail::TSDA);
        else if(spmcpos.Rho() > 400) // OPA
          sid = SurfaceId(SurfaceIdDetail::OPA);
        else
          sid = SurfaceId(SurfaceIdDetail::IPA);
        absorberstep = SurfaceStep(sid,spmc,det);
        if(debug_ > 1)std::cout <<"New step" << std::endl;
      }
    }
    // save last step
    if(absorberstep.surfaceId() != SurfaceIdDetail::unknown && absorberstep.simParticle().isNonnull())ssc->push_back(absorberstep);
    auto nabsorbersteps = ssc->size() - nvdsteps;
    if(debug_ > 0)std::cout << "Added " << nabsorbersteps << " IPA Steps "<< std::endl;
    // same for stopping target. Here, the foil number (= volumeid) matters
    auto const& stspmccol_h = event.getValidHandle<StepPointMCCollection>(ststepmcs_);
    auto const& stspmccol = *stspmccol_h;
    if(debug_ > 0)std::cout << "Target StepPointMC collection has " << stspmccol.size() << " Entries" << std::endl;
    // match steps by their sim particle. There is only 1 volume for IPA
    SurfaceStep ststep;
    for(auto const& spmc : stspmccol) {
      // convert to detector coordinates
      auto spmcpos = XYZVectorF(det->toDetector(spmc.position()));
      bool added(false);
      // decide if this step is contiguous to existing steps already aggregated
      // first, test SimParticle (surface is defined by the collection)
      if(ststep.simParticle() == spmc.simParticle() && ststep.surfaceId().index()== (int)spmc.volumeId()){
        if(debug_ > 2) std::cout << "Same SimParticle" << std::endl;
        // then time;
        auto tgap = fabs(ststep.time()-spmc.time());
        auto dgap = (ststep.endPosition() - spmcpos).R();
        if(spmc.time() < ststep.time())throw cet::exception("Simulation") << " StepPointMC times out-of-order" << std::endl;
        if(debug_ > 2) std::cout << "Time gap " << tgap << " Distance gap " << dgap << std::endl;
        if(tgap < maxtgap_ && dgap < maxdgap_){
          // accumulate this step into the existing e step
          if(debug_ > 1)std::cout <<"Added step" << std::endl;
          ststep.addStep(spmc,det);
          added = true;
        }
      }
      if(!added){
        // if the existing step is valid, save it
        if(ststep.surfaceId() != SurfaceIdDetail::unknown && ststep.simParticle().isNonnull())ssc->push_back(ststep);
        // start a new SurfaceStep for this step
        // hack around the problem that the ST wires are mixed in with the ST foils
        SurfaceId stid(SurfaceIdDetail::ST_Foils,spmc.volumeId());
        if(spmcpos.Rho() > 75.001)stid = SurfaceId(SurfaceIdDetail::ST_Wires,spmc.volumeId());
        ststep = SurfaceStep(stid,spmc,det);
        if(debug_ > 1)std::cout <<"New step" << std::endl;
      }
    }
    if(ststep.surfaceId() != SurfaceIdDetail::unknown && ststep.simParticle().isNonnull())ssc->push_back(ststep);
    auto nststeps = ssc->size() - nabsorbersteps - nvdsteps;
    if(debug_ > 0)std::cout << "Added " << nststeps << " stopping target Steps "<< std::endl;
    // DS passive materials + concrete: each collection corresponds to one volume -> one fixed SurfaceId.
    // These come from G4 SDConfig.sensitiveVolumes (full position+momentum truth at the crossing /
    // throughout the concrete). Aggregate adjacent same-SimParticle steps within each collection.
    auto npre = ssc->size();
    for(auto const& tagsid : dsmat_) {
      auto const& col_h = event.getValidHandle<StepPointMCCollection>(tagsid.first);
      aggregateFixedSid(*col_h, tagsid.second, det, *ssc);
    }
    if(debug_ > 0)std::cout << "Added " << (ssc->size()-npre) << " DS-material/concrete steps " << std::endl;
    // CRV: build one SurfaceStep per (SimParticle, CRV sector) from CrvSteps, mapping the
    // scintillator bar index to its sector's SurfaceId exactly as KinKalGeomMaker does.
    if(crvsteps_) {
      GeomHandle<CosmicRayShield> crs;
      auto const& crvcol_h = event.getValidHandle<CrvStepCollection>(*crvsteps_);
      auto const& crvcol = *crvcol_h;
      auto ncrvpre = ssc->size();
      SurfaceStep cur; int curshield = -1;
      for(auto const& cs : crvcol) {
        int shield = crs->getBar(cs.barIndex()).id().getShieldNumber();
        auto pos = XYZVectorF(det->toDetector(cs.startPosition()));
        bool added(false);
        if(cur.simParticle().isNonnull() && cur.simParticle() == cs.simParticle() && shield == curshield
           && cs.startTime() >= cur.time()
           && std::fabs(cur.time()-cs.startTime()) < maxtgap_
           && (cur.endPosition()-pos).R() < maxdgap_) {
          cur.addStep(cs,det); added = true;
        }
        if(!added) {
          if(cur.simParticle().isNonnull()) ssc->push_back(cur);
          SurfaceId sid(crs->getCRSScintillatorShields().at(shield).getName());
          cur = SurfaceStep(sid, cs, det);
          curshield = shield;
        }
      }
      if(cur.simParticle().isNonnull()) ssc->push_back(cur);
      if(debug_ > 0)std::cout << "Added " << (ssc->size()-ncrvpre) << " CRV sector steps " << std::endl;
    }
    // finish
    event.put(move(ssc));
  }

  void MakeSurfaceSteps::aggregateFixedSid(StepPointMCCollection const& col, SurfaceId const& sid,
      GeomHandle<DetectorSystem> const& det, SurfaceStepCollection& ssc) const {
    SurfaceStep cur;
    for(auto const& spmc : col) {
      auto pos = XYZVectorF(det->toDetector(spmc.position()));
      bool added(false);
      // aggregate only contiguous, time-ordered steps of the same SimParticle
      if(cur.simParticle().isNonnull() && cur.simParticle() == spmc.simParticle()
         && spmc.time() >= cur.time()
         && std::fabs(cur.time()-spmc.time()) < maxtgap_
         && (cur.endPosition()-pos).R() < maxdgap_
         && cur.pathLength() < maxseglen_) {  // split long crossings (e.g. concrete) into sub-segments
        cur.addStep(spmc,det); added = true;
      }
      if(!added) {
        if(cur.simParticle().isNonnull()) ssc.push_back(cur);
        cur = SurfaceStep(sid, spmc, det);
      }
    }
    if(cur.simParticle().isNonnull()) ssc.push_back(cur);
  }

}
DEFINE_ART_MODULE(mu2e::MakeSurfaceSteps)
