#ifndef GeometryService_KinKalGeomMaker_hh
#define GeometryService_KinKalGeomMaker_hh
//
// Create KinKalGeom objects. These depend on other objects served by GeometryService so this must be external to KinKalGeom itself
// Original author: Dave Brown (LBNL) 4/2026
//
#include "Offline/KinKalGeom/inc/KinKalGeom.hh"
#include "Offline/KinKalGeom/inc/KKMaterial.hh"
namespace mu2e {
  class SimpleConfig;
  class KinKalGeomMaker {
    public:
      KinKalGeomMaker(SimpleConfig const& config, int debug) : config_(config), debug_(debug) {}
      std::unique_ptr<KinKalGeom>& makeKKG();
    private:
      void makeTracker();
      void makeDS();
      void makeTarget();
      void makeCRV();
      void makePassiveMaterials();
      SimpleConfig const& config_;
      std::unique_ptr<KinKalGeom> kkg_;
      int debug_ = 0;
  };
}
#endif
