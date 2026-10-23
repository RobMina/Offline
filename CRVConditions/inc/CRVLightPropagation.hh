#ifndef CRVConditions_CRVLightPropagation_hh
#define CRVConditions_CRVLightPropagation_hh

//
// Holds the light propagation of CRV bars relative to their lookup tables
// (fiber signal speed ratio, attenuation length ratio; 1 = as simulated in the table)
//

#include "Offline/Mu2eInterfaces/inc/ProditionsEntity.hh"
#include "cetlib_except/exception.h"
#include <cstdint>
#include <vector>

namespace mu2e {

class CRVLightPropagation : virtual public ProditionsEntity {
 public:
  typedef std::shared_ptr<CRVLightPropagation> ptr_t;
  typedef std::shared_ptr<const CRVLightPropagation> cptr_t;
  constexpr static const char* cxname = {"CRVLightPropagation"};

  typedef std::vector<float> RatioVec;

  CRVLightPropagation(RatioVec const& speed, RatioVec const& attenuation) :
      ProditionsEntity(cxname), _speed(speed), _attenuation(attenuation) {}

  // fiber signal speed relative to the lookup table of the bar
  float signalSpeedRatio(std::uint16_t bar) const { return _speed.at(check(bar)); }
  // attenuation length relative to the lookup table of the bar
  float attenuationLengthRatio(std::uint16_t bar) const { return _attenuation.at(check(bar)); }

 private:
  std::uint16_t check(std::uint16_t bar) const {
    if (bar >= _speed.size()) {
      throw cet::exception("CRVLIGHTPROPAGATION_BAD_BAR")
          << "CRVLightPropagation bad bar requested: bar=" << bar << "\n";
    }
    return bar;
  }
  RatioVec _speed;
  RatioVec _attenuation;
};

}  // namespace mu2e

#endif
