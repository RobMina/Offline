#include "Offline/CRVConditions/inc/CRVLightPropagationMaker.hh"
#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/GeometryService/inc/GeometryService.hh"
#include "cetlib_except/exception.h"
#include <iomanip>
#include <iostream>

using namespace std;

namespace mu2e {

//***************************************************

CRVLightPropagation::ptr_t CRVLightPropagationMaker::fromFcl() {
  if (_config.verbose()) {
    cout << "CRV light propagation ratios set to 1 for all bars, because database is not used.\n";
  }

  size_t nBars = GeomHandle<CosmicRayShield>()->getAllCRSScintillatorBars().size();
  CRVLightPropagation::RatioVec ones(nBars, 1.0);
  return make_shared<CRVLightPropagation>(ones, ones);

}  // end fromFcl

//***************************************************

CRVLightPropagation::ptr_t CRVLightPropagationMaker::fromDb(CRVPropagation::cptr_t tab_p) {
  if (_config.verbose()) {
    cout << "CRVLightPropagationMaker::fromDb making CRVLightPropagation\n";
  }

  size_t nBars = GeomHandle<CosmicRayShield>()->getAllCRSScintillatorBars().size();

  // require the db table is the same length as geometry
  if (tab_p->nrow() != nBars) {
    throw cet::exception("CRVLIGHTPROPAGATIONMAKER_BAD_N_BARS")
        << "CRVLightPropagationMaker::fromDb bad bar counts: "
        << "  geometry: " << nBars << "  CRVPropagation: " << tab_p->nrow() << "\n";
  }

  CRVLightPropagation::RatioVec speed(nBars, 1.0), attenuation(nBars, 1.0);

  for (auto const& row : tab_p->rows()) {
    speed[row.bar()] = row.signalSpeedRatio();
    attenuation[row.bar()] = row.attenuationLengthRatio();
    if (_config.verbose() > 1 || (_config.verbose() && (row.bar() < 5 || nBars - row.bar() <= 5))) {
      cout << setw(10) << row.bar() << fixed << setprecision(4) << setw(10)
           << row.signalSpeedRatio() << setw(10) << row.attenuationLengthRatio() << "\n";
    }
  }

  return make_shared<CRVLightPropagation>(speed, attenuation);

}  // end fromDb

}  // namespace mu2e
