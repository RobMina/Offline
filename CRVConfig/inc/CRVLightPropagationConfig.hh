#ifndef CRVConfig_CRVLightPropagationConfig_hh
#define CRVConfig_CRVLightPropagationConfig_hh
//
// Fcl stanza for the CRV light propagation (fiber signal speed and attenuation length per bar) prodition
//
#include "fhiclcpp/types/Atom.h"
#include <string>

namespace mu2e {

struct CRVLightPropagationConfig {
  using Name = fhicl::Name;
  using Comment = fhicl::Comment;

  fhicl::Atom<int> verbose{Name("verbose"), Comment("verbosity: 0, 1 or 2")};
  fhicl::Atom<bool> useDb{Name("useDb"), Comment("use database (CRVPropagation table) or all ratios 1")};
};

}  // namespace mu2e

#endif
