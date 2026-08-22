//
// Count CRV response-chain products for debugging digitization/reconstruction.
//

#include "Offline/MCDataProducts/inc/CrvDigiMC.hh"
#include "Offline/MCDataProducts/inc/CrvPhotons.hh"
#include "Offline/MCDataProducts/inc/CrvSiPMCharges.hh"
#include "Offline/MCDataProducts/inc/CrvStep.hh"
#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/RecoDataProducts/inc/CrvDigi.hh"

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/types/Atom.h"

#include <iomanip>
#include <iostream>

namespace mu2e {
  class CrvResponseSummary : public art::EDAnalyzer {
   public:
    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;

      fhicl::Atom<art::InputTag> crvStepsTag{
          Name("crvStepsTag"), Comment("input CrvStep collection"), art::InputTag{}};
      fhicl::Atom<art::InputTag> crvPhotonsTag{
          Name("crvPhotonsTag"), Comment("input CrvPhotons collection"), art::InputTag{}};
      fhicl::Atom<art::InputTag> crvSiPMChargesTag{
          Name("crvSiPMChargesTag"), Comment("input CrvSiPMCharges collection"), art::InputTag{}};
      fhicl::Atom<art::InputTag> crvWaveformsTag{
          Name("crvWaveformsTag"), Comment("input CrvDigiMC waveform collection"), art::InputTag{}};
      fhicl::Atom<art::InputTag> crvDigiTag{
          Name("crvDigiTag"), Comment("input CrvDigi collection"), art::InputTag{}};
    };
    using Parameters = art::EDAnalyzer::Table<Config>;

    explicit CrvResponseSummary(Parameters const& conf);
    void beginJob() override;
    void analyze(art::Event const& event) override;

   private:
    art::InputTag _crvStepsTag;
    art::InputTag _crvPhotonsTag;
    art::InputTag _crvSiPMChargesTag;
    art::InputTag _crvWaveformsTag;
    art::InputTag _crvDigiTag;
  };
}

mu2e::CrvResponseSummary::CrvResponseSummary(Parameters const& conf)
    : art::EDAnalyzer(conf),
      _crvStepsTag(conf().crvStepsTag()),
      _crvPhotonsTag(conf().crvPhotonsTag()),
      _crvSiPMChargesTag(conf().crvSiPMChargesTag()),
      _crvWaveformsTag(conf().crvWaveformsTag()),
      _crvDigiTag(conf().crvDigiTag()) {}

void mu2e::CrvResponseSummary::beginJob() {
  std::cout
      << "CRV_RESPONSE_SUMMARY_COLUMNS "
      << "run,subrun,event,steps,step_visible_edep,photon_channels,photons,"
      << "charge_channels,charges,charge_pe,waveform_digimc,waveform_steps,digis"
      << std::endl;
  std::cout
      << "CRV_RESPONSE_STEP_COLUMNS "
      << "run,subrun,event,bar_index,shield,module,layer,bar,visible_edep,start_time,"
      << "local_start_x,local_start_y,local_start_z,local_end_x,local_end_y,local_end_z"
      << std::endl;
}

void mu2e::CrvResponseSummary::analyze(art::Event const& event) {
  long steps = -1;
  double stepVisibleEDep = 0.0;
  if (!_crvStepsTag.label().empty()) {
    auto handle = event.getHandle<CrvStepCollection>(_crvStepsTag);
    if (handle.isValid()) {
      steps = handle->size();
      GeomHandle<CosmicRayShield> CRS;
      for (auto const& step : *handle) {
        stepVisibleEDep += step.visibleEDep();
        auto const& crsBar = CRS->getBar(step.barIndex());
        auto const& id = crsBar.id();
        auto const localStart = crsBar.toLocal(step.startPosition());
        auto const localEnd = crsBar.toLocal(step.endPosition());
        std::cout << "CRV_RESPONSE_STEP " << event.run() << "," << event.subRun()
                  << "," << event.event() << "," << step.barIndex().asUint() << ","
                  << id.getShieldNumber() << "," << id.getModuleNumber() << ","
                  << id.getLayerNumber() << "," << id.getBarNumber() << ","
                  << std::fixed << std::setprecision(6) << step.visibleEDep() << ","
                  << step.startTime() << "," << localStart.x() << "," << localStart.y()
                  << "," << localStart.z() << "," << localEnd.x() << ","
                  << localEnd.y() << "," << localEnd.z() << std::endl;
      }
    }
  }

  long photonChannels = -1;
  long photons = -1;
  if (!_crvPhotonsTag.label().empty()) {
    auto handle = event.getHandle<CrvPhotonsCollection>(_crvPhotonsTag);
    if (handle.isValid()) {
      photonChannels = handle->size();
      photons = 0;
      for (auto const& channel : *handle) {
        photons += channel.GetPhotons().size();
      }
    }
  }

  long chargeChannels = -1;
  long charges = -1;
  double chargePE = 0.0;
  if (!_crvSiPMChargesTag.label().empty()) {
    auto handle = event.getHandle<CrvSiPMChargesCollection>(_crvSiPMChargesTag);
    if (handle.isValid()) {
      chargeChannels = handle->size();
      charges = 0;
      for (auto const& channel : *handle) {
        charges += channel.GetCharges().size();
        for (auto const& charge : channel.GetCharges()) {
          chargePE += charge._chargeInPEs;
        }
      }
    }
  }

  long waveformDigiMC = -1;
  long waveformSteps = -1;
  if (!_crvWaveformsTag.label().empty()) {
    auto handle = event.getHandle<CrvDigiMCCollection>(_crvWaveformsTag);
    if (handle.isValid()) {
      waveformDigiMC = handle->size();
      waveformSteps = 0;
      for (auto const& digiMC : *handle) {
        waveformSteps += digiMC.GetCrvSteps().size();
      }
    }
  }

  long digis = -1;
  if (!_crvDigiTag.label().empty()) {
    auto handle = event.getHandle<CrvDigiCollection>(_crvDigiTag);
    if (handle.isValid()) {
      digis = handle->size();
    }
  }

  std::cout << "CRV_RESPONSE_SUMMARY " << event.run() << "," << event.subRun()
            << "," << event.event() << "," << steps << "," << std::fixed
            << std::setprecision(6) << stepVisibleEDep << "," << photonChannels
            << "," << photons << "," << chargeChannels << "," << charges << ","
            << chargePE << "," << waveformDigiMC << "," << waveformSteps << ","
            << digis << std::endl;
}

DEFINE_ART_MODULE(mu2e::CrvResponseSummary)
