#ifndef ValCrvBarAxis_HH_
#define ValCrvBarAxis_HH_
//
// Upper edge of the CRV bar-index axes of the Validation histograms: the stock 5504-bar range for every
// stock layout, extended to the geometry bar count when the CRV has more bars (layouts with more layers or narrower
// counters). Without a GeometryService (or without a CRV in it) the stock range is used.
//

#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/GeometryService/inc/GeometryService.hh"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Registry/ServiceRegistry.h"

#include <algorithm>
#include <cstddef>

namespace mu2e {

inline double valCrvBarAxisMax() {
  std::size_t nBars = 5504;
  if (art::ServiceRegistry::isAvailable<GeometryService>() &&
      art::ServiceHandle<GeometryService>()->hasElement<CosmicRayShield>()) {
    nBars = std::max(nBars, GeomHandle<CosmicRayShield>()->getAllCRSScintillatorBars().size());
  }
  return nBars - 0.5;
}

}  // namespace mu2e

#endif
