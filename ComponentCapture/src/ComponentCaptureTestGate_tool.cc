#include "Offline/Mu2eG4/inc/IMu2eG4Cut.hh"
#include "art/Framework/Principal/Event.h"
#include "art/Utilities/ToolMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include <algorithm>
#include <vector>

namespace mu2e {
  // Diagnostic stacking-only gate; the common cut remains ComponentRecorder.
  class ComponentCaptureTestGate : public IMu2eG4Cut {
  public:
    explicit ComponentCaptureTestGate(const fhicl::ParameterSet& p)
      : rejectEvents_(p.get<std::vector<unsigned>>("rejectEvents")) {
      if (!p.get<bool>("diagnosticOnly"))
        throw cet::exception("ComponentCaptureTestGate") << "diagnosticOnly must be true";
      if (rejectEvents_.empty() ||
          !std::is_sorted(rejectEvents_.begin(), rejectEvents_.end()) ||
          rejectEvents_.front()==0 ||
          std::adjacent_find(rejectEvents_.begin(), rejectEvents_.end())!=rejectEvents_.end())
        throw cet::exception("ComponentCaptureTestGate")
          << "rejectEvents must be nonempty, positive, unique and sorted";
    }

    void declareProducts(art::ProducesCollector&, art::ConsumesCollector&) override {}
    void finishConstruction(const CLHEP::Hep3Vector&) override {}
    void beginEvent(const art::Event& event, const SimParticleHelper&) override {
      const auto eventNumber=event.id().event();
      killed_=std::binary_search(rejectEvents_.begin(), rejectEvents_.end(), eventNumber);
    }
    bool stackingActionCut(const G4Track*) override { return killed_; }
    bool steppingActionCut(const G4Step*) override { return false; }
    void put(art::Event&) override {}
    void deleteCutsData() override { killed_=false; }

  private:
    const std::vector<unsigned> rejectEvents_;
    bool killed_=false;
  };
}
DEFINE_ART_CLASS_TOOL(mu2e::ComponentCaptureTestGate)
