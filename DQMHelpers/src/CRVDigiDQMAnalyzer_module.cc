// Thin art analyzer that constructs, books, and fills CRVDigiDQM.
//
// Original Author: R. Mina

#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/DataProducts/inc/CRSScintillatorBarIndex.hh"
#include "Offline/DataProducts/inc/CRVId.hh"
#include "Offline/DQMHelpers/inc/CRVDigiDQM.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/GeometryService/inc/GeometryService.hh"
#include "Offline/RecoDataProducts/inc/CrvDigi.hh"
#include "Offline/RecoDataProducts/inc/CrvStatus.hh"

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art_root_io/TFileDirectory.h"
#include "art_root_io/TFileService.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/TableFragment.h"
#include "fhiclcpp/types/Table.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace mu2e {

class CRVDigiDQMAnalyzer : public art::EDAnalyzer {
public:
  struct Config {
    using Name = fhicl::Name;
    using Comment = fhicl::Comment;

    fhicl::Atom<art::InputTag> crvDigiTag{
        Name("crvDigiTag"),
        Comment("CRV digi producer"),
        art::InputTag{"CrvDigi"}};
    fhicl::Atom<art::InputTag> crvStatusTag{
        Name("crvStatusTag"),
        Comment("CRV status producer"),
        art::InputTag{"CrvDigi"}};
    fhicl::Atom<std::string> outputTag{
        Name("outputTag"),
        Comment("TFileService subdirectory; empty books in the module directory"),
        ""};
    fhicl::Atom<int> diagLevel{Name("diagLevel"), Comment("Diagnostic level"), 0};

    // Every parameter the helper itself takes, spliced in at this level so
    // the FCL stays flat and the atoms and their defaults live once, beside
    // the Config they fill (CRVDigiDQMFhicl, in the helper header).
    fhicl::TableFragment<CRVDigiDQMFhicl> helper;

    fhicl::Atom<bool> fillSectorOccupancy{
        Name("fillSectorOccupancy"),
        Comment("Fill per-sector crvDigisPerChannelAndEvent_* using GeometryService"),
        false};
    fhicl::Atom<int> histDigisBins{
        Name("histDigisBins"),
        Comment("Bins for crvDigisPerChannelAndEvent_CRVsector*"),
        200};
    fhicl::Atom<double> histDigisStart{
        Name("histDigisStart"),
        Comment("Low edge for crvDigisPerChannelAndEvent_CRVsector*"),
        0.0};
    fhicl::Atom<double> histDigisEnd{
        Name("histDigisEnd"),
        Comment("High edge for crvDigisPerChannelAndEvent_CRVsector*"),
        0.1};
  };

  using Parameters = art::EDAnalyzer::Table<Config>;

  explicit CRVDigiDQMAnalyzer(const Parameters& conf);

  void beginJob() override;
  void beginRun(const art::Run& run) override;
  void beginSubRun(const art::SubRun& subrun) override;
  void endSubRun(const art::SubRun& subrun) override;
  void analyze(const art::Event& event) override;
  void endJob() override;

private:
  static CRVDigiDQM::Config makeHelperConfig(const Config& conf);

  art::InputTag crvDigiTag_;
  art::InputTag crvStatusTag_;
  std::string outputTag_;
  int diagLevel_;
  bool fillSectorOccupancy_;
  int histDigisBins_;
  double histDigisStart_;
  double histDigisEnd_;
  CRVDigiDQM dqm_;
  bool sectorMapSent_{false};
  bool warnedMissingDigi_{false};
  bool warnedMissingStatus_{false};
};

CRVDigiDQM::Config CRVDigiDQMAnalyzer::makeHelperConfig(const Config& conf)
{
  return toConfig(conf.helper());
}

CRVDigiDQMAnalyzer::CRVDigiDQMAnalyzer(const Parameters& conf) :
    art::EDAnalyzer{conf},
    crvDigiTag_(conf().crvDigiTag()),
    crvStatusTag_(conf().crvStatusTag()),
    outputTag_(conf().outputTag()),
    diagLevel_(conf().diagLevel()),
    fillSectorOccupancy_(conf().fillSectorOccupancy()),
    histDigisBins_(std::max(conf().histDigisBins(), 1)),
    histDigisStart_(conf().histDigisStart()),
    histDigisEnd_(conf().histDigisEnd()),
    dqm_(makeHelperConfig(conf()))
{}

void CRVDigiDQMAnalyzer::beginJob()
{
  art::ServiceHandle<art::TFileService> tfs;
  if (outputTag_.empty()) {
    // TFileService already gives each module its own directory; book into it.
    art::TFileDirectory dir = *tfs;
    dqm_.Book(dir);
  } else {
    dqm_.Book(tfs->mkdir(outputTag_));
  }
}

void CRVDigiDQMAnalyzer::beginRun(const art::Run&)
{
  if (!fillSectorOccupancy_ || sectorMapSent_) {
    return;
  }
  sectorMapSent_ = true;

  GeomHandle<CosmicRayShield> CRS;
  auto const& crvSectors = CRS->getCRSScintillatorShields();
  std::vector<std::string> sectorNames;
  sectorNames.reserve(crvSectors.size());
  for (std::size_t i = 0; i < crvSectors.size(); ++i) {
    sectorNames.emplace_back(crvSectors.at(i).name(""));
  }

  auto const& crvCounters = CRS->getAllCRSScintillatorBars();
  std::vector<int> channelToSector(crvCounters.size() * CRVId::nChanPerBar, -1);
  for (std::size_t channel = 0; channel < channelToSector.size(); ++channel) {
    CRSScintillatorBarIndex barIndex(
        static_cast<int>(channel / CRVId::nChanPerBar));
    channelToSector[channel] = CRS->getBar(barIndex).id().getShieldNumber();
  }

  dqm_.BookSectorOccupancy(sectorNames, channelToSector,
                           histDigisBins_, histDigisStart_, histDigisEnd_);
}

void CRVDigiDQMAnalyzer::beginSubRun(const art::SubRun& subrun)
{
  dqm_.BeginSubRun(static_cast<int>(subrun.run()),
                   static_cast<int>(subrun.subRun()));
}

void CRVDigiDQMAnalyzer::endSubRun(const art::SubRun&)
{
  dqm_.EndSubRun();
}

void CRVDigiDQMAnalyzer::analyze(const art::Event& event)
{
  art::Handle<CrvDigiCollection> digiHandle;
  event.getByLabel(crvDigiTag_, digiHandle);
  if (!digiHandle.isValid() || digiHandle.product() == nullptr) {
    if (!warnedMissingDigi_) {
      warnedMissingDigi_ = true;
      mf::LogWarning("CRVDigiDQMAnalyzer")
          << "No CrvDigiCollection at " << crvDigiTag_
          << ". Event skipped. Reported once per job.";
    }
    return;
  }

  art::Handle<CrvStatusCollection> statusHandle;
  event.getByLabel(crvStatusTag_, statusHandle);
  const bool haveStatus =
      statusHandle.isValid() && statusHandle.product() != nullptr;
  if (!haveStatus && !warnedMissingStatus_) {
    warnedMissingStatus_ = true;
    mf::LogWarning("CRVDigiDQMAnalyzer")
        << "No CrvStatusCollection at " << crvStatusTag_
        << " (empty collection used; occupancy/ADC/TDC still fill). "
        << "Reported once per job.";
  }
  const CrvStatusCollection emptyStatus;
  const CrvStatusCollection& status = haveStatus ? *statusHandle : emptyStatus;

  dqm_.Fill(*digiHandle, status);
}

void CRVDigiDQMAnalyzer::endJob()
{
  dqm_.WriteGraphs();

  if (diagLevel_ > 0) {
    std::cout << "[CRVDigiDQMAnalyzer] Total events: " << dqm_.nEvents()
              << std::endl;
    std::cout << "[CRVDigiDQMAnalyzer] Total digis: " << dqm_.nDigis()
              << std::endl;
    std::cout << "[CRVDigiDQMAnalyzer] Active FEBs: " << dqm_.activeFEBs().size()
              << std::endl;
    for (const auto& [roc, febs] : dqm_.rocFEBMap()) {
      std::cout << "[CRVDigiDQMAnalyzer] ROC " << static_cast<int>(roc) << " has "
                << febs.size() << " FEBs:";
      for (auto feb : febs) {
        std::cout << " " << static_cast<int>(feb);
      }
      std::cout << std::endl;
    }
    if (dqm_.nFebIdOutOfAxis() > 0) {
      std::cout << "[CRVDigiDQMAnalyzer] Off-axis FEB ids: "
                << dqm_.nFebIdOutOfAxis() << " (max globalFebId "
                << dqm_.maxFebIdSeen() << ")" << std::endl;
    }
    if (dqm_.nCrvIdOutOfRange() > 0) {
      std::cout << "[CRVDigiDQMAnalyzer] CRVId-range skips (crvDigiRates*): "
                << dqm_.nCrvIdOutOfRange() << std::endl;
    }
  }
}

} // namespace mu2e

DEFINE_ART_MODULE(mu2e::CRVDigiDQMAnalyzer)
