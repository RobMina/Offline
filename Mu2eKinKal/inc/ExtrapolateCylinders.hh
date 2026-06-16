// Predicate to extrapolate to the next intersection with passive cylindrical material shells.
#ifndef Mu2eKinKal_ExtrapolateCylinders_hh
#define Mu2eKinKal_ExtrapolateCylinders_hh

#include "KinKal/General/TimeDir.hh"
#include "KinKal/General/TimeRange.hh"
#include "KinKal/Geometry/Intersection.hh"
#include "KinKal/Geometry/ParticleTrajectoryIntersect.hh"
#include "Offline/KinKalGeom/inc/DetectorSolenoid.hh"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace mu2e {
  using KinKal::Intersection;
  using KinKal::TimeDir;
  using KinKal::TimeRange;
  using KinKal::timeDirSign;

  class ExtrapolateCylinders {
    public:
      using MaterialCylinder = KKGeom::DetectorSolenoid::MaterialCylinder;
      using MaterialCylinderCollection = KKGeom::DetectorSolenoid::MaterialCylinderCollection;
      struct CylinderIntersection {
        Intersection inter_;
        MaterialCylinder const* cylinder_ = nullptr;
        CylinderIntersection() {}
        CylinderIntersection(Intersection const& inter, MaterialCylinder const& cylinder) :
            inter_(inter), cylinder_(&cylinder) {}
      };
      using CylinderIntersectionCollection = std::vector<CylinderIntersection>;

      struct SortIntersections {
        TimeDir tdir_;
        bool operator () (CylinderIntersection const& lhs, CylinderIntersection const& rhs) {
          return tdir_ == TimeDir::forwards ? lhs.inter_.time_ < rhs.inter_.time_ : lhs.inter_.time_ > rhs.inter_.time_;
        }
        SortIntersections(TimeDir tdir) : tdir_(tdir) {}
      };

      ExtrapolateCylinders(double maxdt, double maxdtstep, double dptol, double intertol,
          double minv, MaterialCylinderCollection const& cylinders, int debug=0) :
        maxDt_(maxdt), maxDtStep_(maxdtstep), dptol_(dptol), intertol_(intertol), minvnorm_(minv), cylinders_(cylinders), debug_(debug) {
        for(auto const& cylinder : cylinders_) {
          maxRadius_ = std::max(maxRadius_, cylinder.surface_->radius() + 0.5*cylinder.thickness_);
        }
      }

      double maxDt() const { return maxDt_; }
      double maxDtStep() const { return maxDtStep_; }
      double dpTolerance() const { return dptol_; }
      double interTolerance() const { return intertol_; }
      auto const& intersections() const { return inters_; }
      int debug() const { return debug_; }
      void reset() const { inters_.clear(); }

      template <class KTRAJ> bool needsExtrapolation(KinKal::ParticleTrajectory<KTRAJ> const& fittraj, TimeDir tdir) const;

    private:
      double maxDt_ = -1;
      double maxDtStep_ = -1;
      double dptol_ = 1e10;
      double intertol_ = 1e10;
      double minvnorm_ = 1e-5;
      double minShellCosine_ = 0.1;
      MaterialCylinderCollection const& cylinders_;
      int debug_ = 0;
      double maxRadius_ = 0.0;
      mutable CylinderIntersectionCollection inters_;
  };

  template <class KTRAJ> bool ExtrapolateCylinders::needsExtrapolation(KinKal::ParticleTrajectory<KTRAJ> const& fittraj, TimeDir tdir) const {
    reset();
    if(cylinders_.empty()) return false;
    auto const& ktraj = tdir == TimeDir::forwards ? fittraj.back() : fittraj.front();
    static const double epsilon(1.0e-6);
    if(ktraj.range().range() <= epsilon) return true;
    auto stime = tdir == TimeDir::forwards ? ktraj.range().begin()+epsilon : ktraj.range().end()-epsilon;
    auto etime = tdir == TimeDir::forwards ? ktraj.range().end() : ktraj.range().begin();
    TimeRange trange(stime,etime,false);
    for(auto const& cylinder : cylinders_) {
      auto newinter = KinKal::intersect(fittraj,*cylinder.surface_,trange,intertol_,tdir);
      if(debug_ > 3)std::cout << cylinder.sid_ << " " << newinter << std::endl;
      if(newinter.good()) {
        double const normvel = std::fabs(newinter.norm_.Dot(newinter.pdir_));
        // The DS materials are represented as thin cylindrical shells.  For near-tangent
        // crossings that approximation gives an unbounded path length through a finite shell.
        if(normvel > std::max(minvnorm_,minShellCosine_)) inters_.emplace_back(newinter,cylinder);
        else if(debug_ > 1) std::cout << "Skipping grazing DS material intersection " << cylinder.sid_
          << " normvel " << normvel << std::endl;
      } else if(newinter.onsurface_) {
        return trange.beyond(newinter.time_,tdir);
      }
    }
    SortIntersections isort(tdir);
    std::sort(inters_.begin(),inters_.end(),isort);
    if(!inters_.empty()) return false;

    auto pos = ktraj.position3(etime);
    auto vel = ktraj.velocity(etime);
    double rho = std::sqrt(pos.X()*pos.X() + pos.Y()*pos.Y());
    double vrho = rho > 0.0 ? (pos.X()*vel.X() + pos.Y()*vel.Y())/rho : 0.0;
    if(debug_ > 4)std::cout << "Cylinder material extrap rho " << rho << " vrho " << vrho << " maxRadius " << maxRadius_ << std::endl;
    return rho < maxRadius_ || vrho*timeDirSign(tdir) < 0.0;
  }
}

#endif
