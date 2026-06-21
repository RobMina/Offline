#include "Offline/MCDataProducts/inc/SurfaceStep.hh"
#include "Offline/MCDataProducts/inc/CrvStep.hh"
namespace mu2e {
  SurfaceStep::SurfaceStep(SurfaceId sid, StepPointMC const& spmc, GeomHandle<DetectorSystem>const& det) : sid_(sid),
  edep_(spmc.totalEDep()), time_(spmc.time()),
  mom_(spmc.momentum()),simp_(spmc.simParticle()){
    startpos_ = det->toDetector(spmc.position());
    endpos_ = det->toDetector(spmc.postPosition());
  }

  SurfaceStep::SurfaceStep(SurfaceId sid, CrvStep const& cs, GeomHandle<DetectorSystem>const& det) : sid_(sid),
  edep_(cs.visibleEDep()), time_(cs.startTime()),
  mom_(cs.startMom()),simp_(cs.simParticle()){
    // CrvStep positions are in Mu2e coordinates; SurfaceStep stores detector coordinates
    startpos_ = XYZVectorF(det->toDetector(cs.startPosition()));
    endpos_ = XYZVectorF(det->toDetector(cs.endPosition()));
  }

  void SurfaceStep::addStep(StepPointMC const& spmc, GeomHandle<DetectorSystem>const& det) {
    if(spmc.simParticle() != simp_) throw cet::exception("Simulation")<<"SurfaceStep: SimParticles don't match" << std::endl;
    if(time_ > spmc.time())throw cet::exception("Simulation")<<"SurfaceStep: times out-of-order" << std::endl;
    edep_ += spmc.totalEDep();
    endpos_ = XYZVectorF(det->toDetector(spmc.postPosition()));
  }

  void SurfaceStep::addStep(CrvStep const& cs, GeomHandle<DetectorSystem>const& det) {
    if(cs.simParticle() != simp_) throw cet::exception("Simulation")<<"SurfaceStep: SimParticles don't match" << std::endl;
    if(time_ > cs.startTime())throw cet::exception("Simulation")<<"SurfaceStep: times out-of-order" << std::endl;
    edep_ += cs.visibleEDep();
    endpos_ = XYZVectorF(det->toDetector(cs.endPosition()));
  }

  std::ostream& operator<<( std::ostream& ost, SurfaceStep const& ss){
    ost << "SurfaceStep in surface " << ss.surfaceId().name()
    << " Energy Loss " << ss.energyDeposit() << " path length " << ss.pathLength() << " time " << ss.time()
    << " Particle momentum " << ss.momentum().R() << " PDG " << ss.simParticle()->pdgId();
    return ost;
  }

}
