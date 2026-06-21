// predicate to extrapolate to CRV
#ifndef Mu2eKinKal_ExtrapolateCRV_hh
#define Mu2eKinKal_ExtrapolateCRV_hh
#include "Offline/Mu2eKinKal/inc/KKTrack.hh"
#include "KinKal/General/TimeDir.hh"
#include "KinKal/General/TimeRange.hh"
#include "KinKal/Geometry/Intersection.hh"
#include "KinKal/Geometry/Rectangle.hh"
#include "KinKal/Geometry/ParticleTrajectoryIntersect.hh"
#include "Offline/KinKalGeom/inc/CRV.hh"
#include "cetlib_except/exception.h"
#include <memory>
#include <vector>
#include <limits>
namespace mu2e {
  using KinKal::TimeDir;
  using KinKal::TimeRange;
  using KKGeom::CRV;
  using KinKal::Rectangle;
  using KinKal::Intersection;
  class ExtrapolateCRV {
    public:
      using CRVSV = std::vector<KKGeom::KKCRVSector>;
      struct CRVI {
        Intersection inter_;
        double whw_ = 0;
        int isect_ = -1;
        CRVI(){}
        CRVI(Intersection const& inter,double whw, int isect) : inter_(inter),whw_(whw),isect_(isect) {}
      };
      using CRVIV = std::vector<CRVI>;

      struct sortIntersections { // sort in time direction
        TimeDir tdir_;
        bool operator () (CRVI const& inter1, CRVI const& inter2) {
          return tdir_ == TimeDir::forwards ? inter1.inter_.time_ < inter2.inter_.time_ : inter1.inter_.time_ > inter2.inter_.time_;
        }
        sortIntersections(TimeDir tdir) : tdir_(tdir) {}
      };

      ExtrapolateCRV(double maxdt, double maxdtstep, double dptol, double intertol, double minv, double minvlong, CRV const& crv, int debug=0) :
        maxDt_(maxdt), maxDtStep_(maxdtstep), dptol_(dptol), intertol_(intertol), minvnorm_(minv), minvlong_(minvlong), debug_(debug), sectors_(crv.sectors()) {
        // Per-plane grazing floor. End-cap planes whose normal lies along the DS axis (z) are crossed
        // head-on only by genuine beam-axis muons; soft transverse cosmics merely graze them, producing
        // fake upstream/downstream crossings. Require a higher normal velocity (minvlong) for these
        // z-normal planes, while the top/side planes (x/y-normal), which legitimately see oblique
        // cosmics, keep the looser minv.
        sectorminv_.reserve(sectors_.size());
        for(auto const& sector : sectors_)
          sectorminv_.push_back( fabs(sector.sector_->normal().Z()) > 0.9 ? minvlong_ : minvnorm_ );
      }
      // interface for extrapolation
      double maxDt() const { return maxDt_; }
      double maxDtStep() const { return maxDtStep_; }
      double dpTolerance() const { return dptol_; }
      double interTolerance() const { return intertol_; }
      // CRV specific
      auto const& sectors() const { return sectors_; }
      auto const& sector(size_t isect) const { return sectors_[isect]; }
      auto const& intersections() const { return inters_; }
      int debug() const { return debug_; }
      void reset() const { inters_.clear(); }
      // extrapolation predicate: the track will be extrapolated until this predicate returns false, subject to the maximum time
      template <class KTRAJ> bool needsExtrapolation(KinKal::ParticleTrajectory<KTRAJ> const& fittraj, TimeDir tdir) const;
    private:
      double maxDt_ = -1; // maximum extrapolation time
      double maxDtStep_ = -1; // maximum extrapolation time step in a single iteration
      double dptol_ = 1e10; // fractional momentum tolerance
      double intertol_ = 1e10; // intersection tolerance (mm)
      double minvnorm_ = 1e-5; // minimum vel normal (outwards) to plane (top/side planes)
      double minvlong_ = 1e-5; // minimum vel normal to z-normal end-cap planes (U/D/E)
      int debug_ = 0; // debug level
      CRVSV sectors_; // sectors cache
      std::vector<double> sectorminv_; // per-sector minimum normal velocity (minvnorm_ or minvlong_ by orientation)
      mutable CRVIV inters_; // cache of most recent intersections
  };

  template <class KTRAJ> bool ExtrapolateCRV::needsExtrapolation(KinKal::ParticleTrajectory<KTRAJ> const& fittraj, TimeDir tdir) const {
    // we are answering the question: did the segment last added to this extrapolated track cross a CRV sector or not?
    // if so, stop extrapolating (for now). If not, and if we're still heading towards at least 1 CRV sector , keep going.
    // cache the intersection if it's found, so it can be used without recomputing it.
    bool retval(false);
    reset();
    auto const& ktraj = tdir == TimeDir::forwards ? fittraj.back() : fittraj.front();
    // Make sure the range is positive definite
    static const double epsilon(1.0e-6);
    static const double coincidenttol(1.0e-3); // ns; CRV crossings closer than this in time are the same physical point (a sector seam) and must yield a single xing
    auto stime = tdir == TimeDir::forwards ? ktraj.range().begin()+epsilon : ktraj.range().end()-epsilon;
    auto etime = tdir == TimeDir::forwards ? ktraj.range().end() : ktraj.range().begin();
    TimeRange trange(stime,etime,false);
    // test from the start of the end piece
    auto pos = ktraj.position3(stime);
    auto vel = ktraj.velocity(stime);
    // time span of this piece: used to detect when a sector is beyond the piece (intersect returns nothing)
    double piece_span = fabs(stime - etime);
    if(debug_ > 4)std::cout << "CRV extrap tdir " << tdir << " stime " << stime << " etime " << etime << " vel " << vel << " pos " << pos << std::endl;
    for(size_t isect = 0; isect < sectors_.size(); ++isect ){
      auto const& sector = sectors_[isect];
      double normvel = vel.Dot(sector.sector_->normal())*timeDirSign(tdir); // sign by extrapolation direction
      // Decide if the sector plane is ahead using the normal-projected time-to-reach, NOT the
      // velocity-dot distance. The old measure ((center-pos).vel) projects onto the full velocity,
      // so a sector whose center is laterally offset from the (stale, material-edge) evaluation
      // point could compute as "behind" and be skipped even though the track crosses its plane
      // ahead. After the DS/concrete material steps pin the evaluation point at the concrete, that
      // mis-skip dropped the very top module containing the crossing. signed_perp/normvel is the
      // same quantity the backward branch below uses.
      double signed_perp = (sector.sector_->center()-pos).Dot(sector.sector_->normal());
      double time_to_sector = signed_perp / normvel; // extrap-time to reach the plane; <0 = plane behind
      if(debug_ > 4)std::cout << "CRV extrap normvel " << normvel << " time_to_sector " << time_to_sector << " signed_perp " << signed_perp << std::endl;
      // skip if grazing the plane (per-plane floor: stricter for z-normal end-caps), or the plane is behind
      if(fabs(normvel) < sectorminv_[isect] || time_to_sector < 0 )continue;
      // try to intersect
      auto newinter = KinKal::intersect(fittraj,*sector.sector_,trange,intertol_,tdir);
      if(debug_ > 3)std::cout << "CRV " << newinter  << std::endl;
      if(newinter.good()){
        // Deduplicate coincident crossings: a track crossing at a seam between two adjacent
        // coplanar sectors registers the SAME point/time in both. Adding an xing for each
        // produces a degenerate zero-time-gap second xing whose Kalman extrapolation fails
        // (thrown by KKTrack::addCRVXing). Keep only the first crossing at a given time.
        bool coincident(false);
        for(auto const& prev : inters_) if(fabs(prev.inter_.time_ - newinter.time_) < coincidenttol){ coincident = true; break; }
        if(!coincident){
          inters_.emplace_back(newinter,sector.whw_,(int)isect);
          if(debug_ > 1)std::cout << "Good CRV " <<  newinter << " sector " << sector.sname_ << std::endl;
        } else if(debug_ > 1){
          std::cout << "Skipping coincident CRV crossing (seam) sector " << sector.sname_ << " time " << newinter.time_ << std::endl;
        }
      } else if ( newinter.onsurface_ && newinter.inbounds_) {
        retval |= trange.beyond(newinter.time_,tdir);
        if(trange.beyond(newinter.time_,tdir) && debug_ > 2)std::cout << "Potential CRV " <<  newinter << std::endl;
      } else if(newinter.onsurface_ && debug_ > 3) {
        auto inter_pos = fittraj.position3(newinter.time_);
        std::cout << "CRV onsurface !good inbounds=" << newinter.inbounds_ << " t=" << newinter.time_
                  << " pos=" << inter_pos << " sector=" << sector.sname_ << std::endl;
      } else if(!newinter.onsurface_ && tdir == TimeDir::backwards) {
        // intersect completely failed (Newton-Raphson did not converge). In the backward
        // direction, concrete-plane scattering steps can create pieces too short for the
        // solver to reach the CRV surface. If the sector is geometrically beyond this piece
        // (perpendicular time-to-reach > piece span), keep extrapolating.
        if(time_to_sector > piece_span) retval = true;
      }
    }
    // sort intersections in the time direction
    sortIntersections isort(tdir);
    std::sort(inters_.begin(),inters_.end(),isort);
    // If this piece produced CRV crossing(s), stop and record them now: the extrapolateCRV loop
    // re-extrapolates past them to find any further crossings. Without this, a found crossing is
    // discarded by the next reset() when another sector keeps retval true (e.g. a non-converged
    // far sector triggering the backward branch above).
    if(!inters_.empty()) return false;
    return retval;
  }
}
#endif
