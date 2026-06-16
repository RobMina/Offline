//
// Create KinKalGeom objects. These depend on other objects served by GeometryService so this must be external to KinKalGeom itself
// Original author: Dave Brown (LBNL) 4/2026
//
#include "Offline/GeometryService/inc/KinKalGeomMaker.hh"
#include "Offline/TrackerGeom/inc/Tracker.hh"
#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/KinKalGeom/inc/KinKalGeom.hh"
#include "Offline/KinKalGeom/inc/Tracker.hh"
#include "Offline/KinKalGeom/inc/DetectorSolenoid.hh"
#include "Offline/KinKalGeom/inc/StoppingTarget.hh"
#include "Offline/KinKalGeom/inc/CRV.hh"
#include "Offline/BeamlineGeom/inc/Beamline.hh"
#include "Offline/GeometryService/inc/DetectorSystem.hh"
#include "Offline/DetectorSolenoidGeom/inc/DetectorSolenoid.hh"
#include "Offline/ConfigTools/inc/SimpleConfig.hh"
#include "CLHEP/Vector/ThreeVector.h"
#include "cetlib_except/exception.h"
#include <cmath>
#include <algorithm>
#include <string>

namespace mu2e {
  using KinKal::VEC3;
  using KinKal::Cylinder;
  using KinKal::Disk;
  using KinKal::Surface;
  using KinKal::Frustrum;
  using KinKal::Annulus;
  using KinKal::Rectangle;
  using CylPtr = std::shared_ptr<KinKal::Cylinder>;
  using DiskPtr = std::shared_ptr<KinKal::Disk>;
  using RecPtr = std::shared_ptr<KinKal::Rectangle>;
  using AnnPtr = std::shared_ptr<KinKal::Annulus>;
  using FruPtr = std::shared_ptr<KinKal::Frustrum>;
  using SurfacePtr = std::shared_ptr<KinKal::Surface>;
  using KKGMap = std::multimap<SurfaceId,SurfacePtr>;
  using mu2e::KKGeom::KKCRVSector;


  std::unique_ptr<KinKalGeom>& KinKalGeomMaker::makeKKG() {
    kkg_ = std::make_unique<KinKalGeom>();
    makeTracker();
    makeDS();
    makeTarget();
    makeCRV();
    makePassiveMaterials();
    return kkg_;
  }

  // sort by transverse distance
  struct sortCRVSectors {
    bool operator () (KKCRVSector const& sect1, KKCRVSector const& sect2) {
      return sect1.sector_->center().Rho() > sect2.sector_->center().Rho(); // put largest distance first as cosmic rays (generally) go outside-in (downwards)
    }
  }crvsectorsort;

  void KinKalGeomMaker::makeTracker() {
      // surfaces need to match with virtual detectors. The following is extracted from VirtualDetectorMaker and needs to be updated if that changes.
    // Note that these are placed at the center of the VDs, which have half-thickness of 0.01mm. Since the VD hits are recorded where the SimParticle
    // enters the volume, the reco track will be sampled at a different position depending on the track direction by that amount. This is a fundamental
    // discrepancy between reco and sim data
    auto const& tracker = *(GeomHandle<mu2e::Tracker>());
    auto const& g4tmom = tracker.g4Tracker()->mother();
    auto const& ds = *(GeomHandle<DetectorSolenoid>());
    double vdHL(0.01); // hardcoded in VirtualDetectorMaker line 56
    // below are from VirtualDetectorMaker lnes 241-244
    double zFrontGlobal = g4tmom.position().z()-g4tmom.tubsParams().zHalfLength()-vdHL;
    double zBackGlobal  = g4tmom.position().z()+g4tmom.tubsParams().zHalfLength()+vdHL;
    // the 0.4 below comes from offsets in the mother volume nesting.
    double zFrontLocal  = zFrontGlobal - tracker.g4Tracker()->z0() + 0.4;
    double zBackLocal   = zBackGlobal  - tracker.g4Tracker()->z0() - 0.4;
    double zMidLocal = 10.1; // 10.1 is hard-coded in VirtualDetectorMaker line 224
    double halfLen = 0.5*(zBackLocal-zFrontLocal);
    double orvd = g4tmom.tubsParams().outerRadius();
    double irvd = tracker.g4Tracker()->getInnerTrackerEnvelopeParams().innerRadius();
    double irds = ds.rIn1();
    // cylinders are defined by TT_outer (_inner) virtual detectors
    // Disks are defined to match TT_front (mid, back) virtual detectors
    auto outer = std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,zMidLocal),orvd,halfLen);
    auto inner = std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,zMidLocal),irvd,halfLen);
    // expand the disk radii to the DS
    auto front = std::make_shared<Disk>(VEC3(0.0,0.0,1.0),VEC3(1.0,0.0,0.0),VEC3(0.0,0.0,zFrontLocal),irds);
    auto mid = std::make_shared<Disk>(VEC3(0.0,0.0,1.0),VEC3(1.0,0.0,0.0),VEC3(0.0,0.0,zMidLocal),irds);
    auto back = std::make_shared<Disk>(VEC3(0.0,0.0,1.0),VEC3(1.0,0.0,0.0),VEC3(0.0,0.0,zBackLocal),irds);
    // add all these to the map
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::TT_Front),std::static_pointer_cast<Surface>(front)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::TT_Mid),std::static_pointer_cast<Surface>(mid)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::TT_Back),std::static_pointer_cast<Surface>(back)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::TT_Inner),std::static_pointer_cast<Surface>(inner)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::TT_Outer),std::static_pointer_cast<Surface>(outer)));
    // construct the tracker object
    kkg_->tracker_ = std::make_unique<KKGeom::Tracker>(outer,inner,front,mid,back);
  }

  void KinKalGeomMaker::makeDS() {
    GeomHandle<DetectorSystem> det;
    GeomHandle<DetectorSolenoid> ds;
//    std::cout << "DS Cryo or " << ds->rOut1() << "," << ds->rOut2() << " ir " << ds->rIn1()<<","<< ds->rIn2() << " halfl " << ds->halfLength()
//      << " zpos " << ds->position().z() << " material " << ds->material() << std::endl;
//    std::cout << "DS shield or " << ds->shield_rOut1() << "," << ds->shield_rOut2() << " ir " << ds->shield_rIn1()<<","<< ds->shield_rIn2() << " halfl " << ds->shield_halfLength() << " material " << ds->shield_material() << std::endl;
//    std::cout << "DS ncoils " << ds->nCoils() << std::endl;
//    for(size_t icoil = 0; icoil < static_cast<size_t>(ds->nCoils()); icoil++){
//      std::cout << "DS coil ir " << ds->coil_rIn() << " or " << ds->coil_rOut()[icoil] << " length " << ds->coil_zLength()[icoil] << " zpos " << ds->coil_zPosition()[icoil]
//        << " material " << ds->coil_materials()[icoil] << std::endl;
//    }
    //DS Cryo or 1303,1328 ir 950,969.05 halfl 5450 zpos 8689 material StainlessSteel
    //DS shield or 1237.3,1250 ir 1010,1022.7 halfl 5287.7 material G4_Al
    //DS ncoils 11
    //DS coil ir 1050 or 1091 length 419.75 zpos 3539 material DS1CoilMix
    //DS coil ir 1050 or 1091 length 419.75 zpos 3964 material DS1CoilMix
    //DS coil ir 1050 or 1091 length 419.75 zpos 4389 material DS1CoilMix
    //DS coil ir 1050 or 1091 length 419.75 zpos 5042 material DS1CoilMix
    //DS coil ir 1050 or 1091 length 362.25 zpos 5699 material DS1CoilMix
    //DS coil ir 1050 or 1091 length 362.25 zpos 6369 material DS1CoilMix
    //DS coil ir 1050 or 1091 length 362.25 zpos 7176 material DS1CoilMix
    //DS coil ir 1050 or 1070.5 length 1777.5 zpos 7949 material DS2CoilMix
    //DS coil ir 1050 or 1070.5 length 1777.5 zpos 9761 material DS2CoilMix
    //DS coil ir 1050 or 1070.5 length 1777.5 zpos 11544 material DS2CoilMix
    //DS coil ir 1050 or 1091 length 362.25 zpos 13326 material DS1CoilMix
    //
    //
    //
    //
    auto inner= std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,-1482),ds->rIn1(),ds->halfLength());
    auto outer= std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,-1482),ds->rOut2(),ds->halfLength()); // bounding surfaces
    auto front= std::make_shared<Disk>(outer->frontDisk());
    auto back= std::make_shared<Disk>(outer->backDisk());
    KKGeom::DetectorSolenoid::MaterialCylinderCollection materialCylinders;
    auto toDetectorZ = [&det](double zmu2e) {
      return VEC3(det->toDetector(CLHEP::Hep3Vector(0.0,0.0,zmu2e))).Z();
    };
    auto addMaterialCylinder = [this,&materialCylinders,&toDetectorZ](SurfaceId const& sid,
        double rin, double rout, double zcenter, double halfLength, std::string const& material) {
      auto cylinder = std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),
          VEC3(0.0,0.0,toDetectorZ(zcenter)),0.5*(rin+rout),halfLength);
      materialCylinders.emplace_back(sid,cylinder,material,rout-rin);
      kkg_->map_.emplace(std::make_pair(sid,std::static_pointer_cast<Surface>(cylinder)));
    };
    addMaterialCylinder(SurfaceId(SurfaceIdEnum::DS_CryoInner),ds->rIn1(),ds->rIn2(),ds->position().z(),ds->halfLength(),ds->material());
    addMaterialCylinder(SurfaceId(SurfaceIdEnum::DS_CryoOuter),ds->rOut1(),ds->rOut2(),ds->position().z(),ds->halfLength(),ds->material());
    addMaterialCylinder(SurfaceId(SurfaceIdEnum::DS_ShieldInner),ds->shield_rIn1(),ds->shield_rIn2(),ds->position().z(),ds->shield_halfLength(),ds->shield_material());
    addMaterialCylinder(SurfaceId(SurfaceIdEnum::DS_ShieldOuter),ds->shield_rOut1(),ds->shield_rOut2(),ds->position().z(),ds->shield_halfLength(),ds->shield_material());
    for(size_t icoil = 0; icoil < static_cast<size_t>(ds->nCoils()); icoil++){
      auto material = ds->coilVersion() == 1 ? ds->coil_material() : ds->coil_materials().at(icoil);
      double halflen = 0.5*ds->coil_zLength().at(icoil);
      addMaterialCylinder(SurfaceId(SurfaceIdEnum::DS_Coil,static_cast<int>(icoil)),
          ds->coil_rIn(),ds->coil_rOut().at(icoil),ds->coil_zPosition().at(icoil)+halflen,halflen,material);
    }


    // hard-coded for now
    auto ipa= std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,-2770),300.0,500.0);
    auto ipafront= std::make_shared<Disk>(ipa->frontDisk());
    auto ipaback= std::make_shared<Disk>(ipa->backDisk());
    auto opa= std::make_shared<Frustrum>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,-3766),454.0,728.4,2125.0); // inner surface
    auto tsda= std::make_shared<Annulus>(VEC3(0.0,0.0,1.0),VEC3(1.0,0.0,0.0),VEC3(0.0,0.0,-5967),235.0,525.0); // back surface

    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::DS_Front),std::static_pointer_cast<Surface>(front)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::DS_Back),std::static_pointer_cast<Surface>(back)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::DS_Inner),std::static_pointer_cast<Surface>(inner)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::DS_Outer),std::static_pointer_cast<Surface>(outer)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::IPA),std::static_pointer_cast<Surface>(ipa)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::IPA_Front),std::static_pointer_cast<Surface>(ipafront)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::IPA_Back),std::static_pointer_cast<Surface>(ipaback)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::OPA),std::static_pointer_cast<Surface>(opa)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::TSDA),std::static_pointer_cast<Surface>(tsda)));

    kkg_->ds_ = std::make_unique<KKGeom::DetectorSolenoid>(inner, outer, front, back, ipa, ipafront, ipaback, opa, tsda, materialCylinders);
  }

  void KinKalGeomMaker::makeTarget() {
    // currently use hard-coded geometry
    auto outer = std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,-4300),75,400.0);
    auto inner= std::make_shared<Cylinder>(VEC3(0.0,0.0,1.0),VEC3(0.0,0.0,-4300),21.5,400.0);
    auto front= std::make_shared<Disk>(outer->frontDisk());
    auto back= std::make_shared<Disk>(outer->backDisk());
    double startz = -4700;
    double endz = -3900;
    double dz = (endz-startz)/36.0;

    std::vector<AnnPtr> foils;
    for(int ifoil=0;ifoil < 37; ++ifoil){
      double zpos = startz + ifoil*dz;
      foils.push_back(std::make_shared<Annulus>(VEC3(0.0,0.0,1.0),VEC3(1.0,0.0,0.0),VEC3(0.0,0.0,zpos),21.5,75));
    }

    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::ST_Front),std::static_pointer_cast<Surface>(front)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::ST_Back),std::static_pointer_cast<Surface>(back)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::ST_Inner),std::static_pointer_cast<Surface>(inner)));
    kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::ST_Outer),std::static_pointer_cast<Surface>(outer)));
    for(size_t ifoil=0;ifoil < foils.size();++ifoil){
      kkg_->map_.emplace(std::make_pair(SurfaceId(SurfaceIdEnum::ST_Foils,ifoil),std::static_pointer_cast<Surface>(foils[ifoil])));
    }

    kkg_->st_ = std::make_unique<KKGeom::StoppingTarget>(outer,inner,front,back,foils);
  }

  void KinKalGeomMaker::makeCRV() {
    GeomHandle<CosmicRayShield> CRS;
    GeomHandle<DetectorSystem> det;
    auto const& shields = CRS->getCRSScintillatorShields();
    std::vector<KKCRVSector> sectors;
    // loop over the shields (= sectors)
    for (auto const& shield : shields) {
      //
      // First find this shield's orientation; the first bar is enough for that
      //
      auto const& firstbar = shield.getFirstBar();
      auto fbarpos = VEC3(det->toDetector(firstbar.getPosition())); // convert to detector (tracker) coordinates and root vectors
      auto bardet = firstbar.getBarDetail();
      // normal (w) direction is the thickness direction. Make sure it points away from the tracker
      VEC3 wdir;
      switch(bardet.getThicknessDirection()) {
        case 0:
          wdir = VEC3(copysign(1.0,fbarpos.X()),0.0,0.0);
          break;
        case 1:
          wdir = VEC3(0.0,copysign(1.0,fbarpos.Y()),0.0);
          break;
        case 2:
          wdir = VEC3(0.0,0.0,copysign(1.0,fbarpos.Z()));
          break;
        default:
          throw cet::exception("Service")<<"invalid direction "<< bardet.getThicknessDirection() << std::endl;
          break;
      }
      // u direction points along the bars (length direction). Sign is unimportant.
      VEC3 udir;
      switch(bardet.getLengthDirection()) {
        case 0:
          udir = VEC3(1.0,0.0,0.0);
          break;
        case 1:
          udir = VEC3(0.0,1.0,0.0);
          break;
        case 2:
          udir = VEC3(0.0,0.0,1.0);
          break;
        default:
          throw cet::exception("Service")<<"invalid direction "<< bardet.getLengthDirection() << std::endl;
          break;
      }
      // v points along bar width
      VEC3 vdir;
      switch(bardet.getWidthDirection()) {
        case 0:
          vdir = VEC3(1.0,0.0,0.0);
          break;
        case 1:
          vdir = VEC3(0.0,1.0,0.0);
          break;
        case 2:
          vdir = VEC3(0.0,0.0,1.0);
          break;
        default:
          throw cet::exception("Service")<<"invalid direction "<< firstbar.getBarDetail().getWidthDirection() << std::endl;
          break;
      }
      // next compute the average position. All the bars have the same position along their length
      double upos = fbarpos.Dot(udir);
      double uhw = firstbar.getHalfLength();
      // average first and last layers to get the w position and half-width
      auto const& firstmod = shield.getModule(0);
      auto flaypos = VEC3(det->toDetector(firstmod.getLayer(0).getPosition()));
      auto llaypos = VEC3(det->toDetector(firstmod.getLayer(firstmod.nLayers()-1).getPosition()));
      double wpos = 0.5*(flaypos+llaypos).Dot(wdir);
      // wdir can point opposite the layer-stacking order, depending on the sector.
      double whw = 0.5*std::abs((llaypos-flaypos).Dot(wdir)) + firstbar.getHalfThickness();
      // include the layer stagger when computing the position and width perp to the bars
      auto nlay = firstmod.nLayers();
      auto nbar = firstmod.getLayer(0).nBars();
      auto const& lastmod = shield.getModule(shield.nModules()-1);
      auto vf0 = VEC3(det->toDetector(firstmod.getLayer(0).getBar(0).getPosition())).Dot(vdir);
      auto vf3 = VEC3(det->toDetector(firstmod.getLayer(nlay-1).getBar(0).getPosition())).Dot(vdir);
      auto vl0 = VEC3(det->toDetector(lastmod.getLayer(0).getBar(nbar-1).getPosition())).Dot(vdir);
      auto vl3 = VEC3(det->toDetector(lastmod.getLayer(nlay-1).getBar(nbar-1).getPosition())).Dot(vdir);
      double vpos = 0.25*(vf0+vf3+vl0+vl3);
      double vmin = std::min({vf0,vf3,vl0,vl3});
      double vmax = std::max({vf0,vf3,vl0,vl3});
      double vhw = 0.5*(vmax-vmin)+ firstbar.getHalfWidth();
      VEC3 midpoint = upos*udir + vpos*vdir + wpos*wdir;
      // create the rectangle
      KKCRVSector sector;
      sector.sname_ = shield.getName();
      sector.sector_ = std::make_shared<KinKal::Rectangle>(wdir,udir,midpoint,uhw,vhw);
      sector.whw_ = whw;
      sectors.push_back(sector);
    }
    // sort the sectors according to their transverse distance (largest first), to optimize searching for downward going tracks.
    std::sort(sectors.begin(),sectors.end(),crvsectorsort);
    if(debug_ > 0){
      for(auto const& sector : sectors){
        std::cout << "CRV sector " <<  sector.sname_;
        auto const& sectptr = sector.sector_;
        std::cout << " midpoint " << sectptr->center() << " wdir " << sectptr->normal() << " udir " << sectptr->uDirection() << " vdir " << sectptr->vDirection()
          << " uhw " << sectptr->uHalfLength() << " vhw " << sectptr->vHalfLength() <<  " whw " << sector.whw_ << std::endl;
      }
    }

    kkg_->crv_ = std::make_unique<KKGeom::CRV>(sectors);
    // fill map
    unsigned isect(0);
    for(auto const& sector : kkg_->crv_->sectors()){
      kkg_->map_.emplace(std::make_pair(SurfaceId(sector.sname_),std::static_pointer_cast<Surface>(sector.sector_)));
      isect++;
    }
  }

  void KinKalGeomMaker::makePassiveMaterials() {
    GeomHandle<DetectorSystem> det;
    double const invSqrt3 = 1.0/std::sqrt(3.0);
    // Model the ExtShieldDownstream Type 2 regular concrete roof T-blocks.
    // These blocks sit between the DS outer cryostat (y~1328 mm) and the CRV top
    // sectors T1/T2 (y~2653 mm), and are the dominant passive material for
    // tracker-to-CRV-top extrapolation.
    //
    // With orientations "010"/"012" (both map U->z, V->y, W->x):
    //   U (±uhw_outer mm) -> ±z footprint of the wide crossbar per block
    //   V range [vmin, vmax]  -> y; vshoulder separates crossbar from stem
    //   W (length/2 = xhw mm) -> x (transverse, same for both components)
    //
    // The T cross-section is decomposed into two non-overlapping rectangles:
    //   Crossbar: V in [vmin, vshoulder], U in [-uhw_outer, +uhw_outer]
    //   Stem    : V in [vshoulder, vmax], U in [-uhw_inner, +uhw_inner]
    // Each is modelled with two Gauss-point planes so tracks through the crossbar-
    // only region (the ~2/3 of z-width outside the stem) correctly see half the
    // concrete compared to tracks through the full T height.
    int const nType2 = config_.getInt("ExtShieldDownstream.nBoxType2");
    if(nType2 > 0) {
      std::vector<double> uVerts, vVerts;
      config_.getVectorDouble("ExtShieldDownstream.outlineType2UVerts", uVerts);
      config_.getVectorDouble("ExtShieldDownstream.outlineType2VVerts", vVerts);
      if(uVerts.empty() || vVerts.empty()) return;

      auto const uExt = std::minmax_element(uVerts.begin(), uVerts.end());
      auto const vExt = std::minmax_element(vVerts.begin(), vVerts.end());
      double const xhw       = 0.5*config_.getDouble("ExtShieldDownstream.lengthType2");
      auto const material    = config_.getString("ExtShieldDownstream.materialType2");

      double const vmin_t    = *vExt.first;   // = -452.2 mm (bottom of crossbar)
      double const vmax_t    = *vExt.second;  // = +452.2 mm (top of stem)
      double const uhw_outer = *uExt.second;  // = 680.8 mm  (crossbar z half-width per block)

      // Inner U half-width (stem): smallest positive U vertex that differs from uhw_outer
      double uhw_inner = uhw_outer;
      for(double u : uVerts)
        if(u > 0 && u < uhw_outer - 1.0) uhw_inner = std::min(uhw_inner, u);

      // Shoulder V (where T width steps inward): V value strictly between vmin and vmax
      double vshoulder = 0.0;
      for(double v : vVerts)
        if(v > vmin_t + 1.0 && v < vmax_t - 1.0) { vshoulder = v; break; }

      // Crossbar component: V in [vmin_t, vshoulder]
      double const yhalf_cb     = 0.5*(vshoulder - vmin_t);   // = 223.6 mm
      double const vcenter_cb   = 0.5*(vmin_t + vshoulder);   // = -228.6 mm (local V offset)

      // Stem component: V in [vshoulder, vmax_t]
      double const yhalf_stem   = 0.5*(vmax_t - vshoulder);   // = 228.6 mm
      double const vcenter_stem = 0.5*(vshoulder + vmax_t);   // = +223.6 mm (local V offset)

      // Accumulate x/y center (common to both components) and z bounding boxes
      // (different per component because stem is narrower in z than crossbar).
      double xcenter = 0.0, ycenter = 0.0;
      double zmin_cb = 1.0e9, zmax_cb = -1.0e9;
      double zmin_st = 1.0e9, zmax_st = -1.0e9;
      int nfound = 0;
      for(int ibox = 1; ibox <= nType2; ++ibox) {
        std::string const key = "ExtShieldDownstream.centerType2Box" + std::to_string(ibox);
        if(!config_.hasName(key)) continue;
        std::vector<double> ctr;
        config_.getVectorDouble(key, ctr);
        xcenter += ctr[0];
        ycenter += ctr[1];
        zmin_cb = std::min(zmin_cb, ctr[2] - uhw_outer);
        zmax_cb = std::max(zmax_cb, ctr[2] + uhw_outer);
        zmin_st = std::min(zmin_st, ctr[2] - uhw_inner);
        zmax_st = std::max(zmax_st, ctr[2] + uhw_inner);
        ++nfound;
      }
      if(nfound == 0) return;
      xcenter /= nfound;
      ycenter /= nfound;

      // Global Mu2e Y centers for each T component (ycenter = mean box center = 2006.1 mm)
      double const ycenter_cb   = ycenter + vcenter_cb;    // ≈ 1777.5 mm
      double const ycenter_stem = ycenter + vcenter_stem;  // ≈ 2229.7 mm

      double const zcenters[2] = { 0.5*(zmin_cb + zmax_cb), 0.5*(zmin_st + zmax_st) };
      double const zhws[2]     = { 0.5*(zmax_cb - zmin_cb), 0.5*(zmax_st - zmin_st) };
      double const yctrs[2]    = { ycenter_cb,   ycenter_stem  };
      double const yhalfs[2]   = { yhalf_cb,     yhalf_stem    };

      // Four planes total: two Gauss-point planes for each T component.
      for(int icomp = 0; icomp < 2; ++icomp) {
        for(int iplane = 0; iplane < 2; ++iplane) {
          double const ysign = iplane == 0 ? -1.0 : 1.0;
          auto const center = VEC3(det->toDetector(
              CLHEP::Hep3Vector(xcenter, yctrs[icomp] + ysign*yhalfs[icomp]*invSqrt3, zcenters[icomp])));
          auto const plane = std::make_shared<Rectangle>(
              VEC3(0.0,1.0,0.0), VEC3(1.0,0.0,0.0), center, xhw, zhws[icomp]);
          SurfaceId sid(SurfaceIdEnum::DS_HatchConcrete,
                        static_cast<int>(kkg_->passiveMaterialPlanes_.size()));
          kkg_->passiveMaterialPlanes_.emplace_back(sid, plane, material, yhalfs[icomp]);
          kkg_->map_.emplace(std::make_pair(sid, std::static_pointer_cast<Surface>(plane)));
        }
      }
    }
  }
}
