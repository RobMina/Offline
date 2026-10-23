#ifndef CRVConditions_CRVLightPropagationMaker_hh
#define CRVConditions_CRVLightPropagationMaker_hh

//
// construct a CRVLightPropagation proditions entity
// (light propagation of CRV bars relative to their lookup tables)
// from fcl (all ratios 1) or database
//

#include "Offline/CRVConditions/inc/CRVLightPropagation.hh"
#include "Offline/CRVConfig/inc/CRVLightPropagationConfig.hh"
#include "Offline/DbTables/inc/CRVPropagation.hh"

namespace mu2e {

class CRVLightPropagationMaker {
 public:
  CRVLightPropagationMaker(CRVLightPropagationConfig const& config) : _config(config) {}

  CRVLightPropagation::ptr_t fromFcl();
  CRVLightPropagation::ptr_t fromDb(CRVPropagation::cptr_t tab_p);

 private:
  // this object needs to be thread safe,
  // _config should only be initialized once
  const CRVLightPropagationConfig _config;
};
}  // namespace mu2e

#endif
