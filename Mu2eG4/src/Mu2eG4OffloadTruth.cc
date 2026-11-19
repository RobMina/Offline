#include "Offline/Mu2eG4/inc/Mu2eG4OffloadTruth.hh"

#include <cstdlib>

namespace mu2e {

  Mu2eG4OffloadTruth& Mu2eG4OffloadTruth::instance() {
    static thread_local Mu2eG4OffloadTruth truth;
    return truth;
  }

  void Mu2eG4OffloadTruth::beginEvent() {
    next_ = firstTrackId;
    particles_.clear();
  }

  ProcessCode Mu2eG4OffloadTruth::inferCreationCode(int parentPdgId, int pdgId) {
    const bool parentLepton = std::abs(parentPdgId) == 11;
    if ( parentLepton && pdgId == 11 ) return ProcessCode(ProcessCode::eIoni);
    if ( parentLepton && pdgId == 22 ) return ProcessCode(ProcessCode::eBrem);
    if ( parentPdgId == 22 && std::abs(pdgId) == 11 ) return ProcessCode(ProcessCode::mu2eGpuPhotonSecondary);
    return ProcessCode(ProcessCode::mu2eGpuSecondary);
  }

} // namespace mu2e
