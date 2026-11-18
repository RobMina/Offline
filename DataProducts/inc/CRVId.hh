#ifndef DataProducts_CRVId_hh
#define DataProducts_CRVId_hh
//
// Fixed CRV numbering constants.
// The numbers of bars, layers and offline channels are not fixed:
// they come from the CRV geometry (offline channel = bar index * nChanPerBar + SiPM).

#include <cstddef>

namespace mu2e {

  class CRVId{
  public:

    constexpr static std::size_t nChanPerBar = 4;
    constexpr static std::size_t nSidesPerBar = 2;
    // not all the implied possible channels below are active
    // these are the dimensions of sparse containers
    constexpr static std::size_t nChanPerFEB = 64;
    constexpr static std::size_t nFEBPerROC = 24;
    constexpr static std::size_t nROC = 18;
    constexpr static std::size_t nROCPerDTC = 6;

    constexpr static std::size_t nChanPerFPGA = 16;
    constexpr static std::size_t nFPGAPerFEB = 4;
  };
}
#endif /* DataProducts_CRVId_hh */
