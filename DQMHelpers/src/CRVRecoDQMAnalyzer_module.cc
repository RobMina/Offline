// Thin art analyzer that constructs, books, and fills CRVRecoDQM.
//
// Original Author: R. Mina

#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/DataProducts/inc/CRSScintillatorBarIndex.hh"
#include "Offline/DataProducts/inc/CRVId.hh"
#include "Offline/DQMHelpers/inc/CRVRecoDQM.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/GeometryService/inc/GeometryService.hh"
#include "Offline/RecoDataProducts/inc/CrvCoincidenceCluster.hh"
#include "Offline/RecoDataProducts/inc/CrvRecoPulse.hh"

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

class CRVRecoDQMAnalyzer : public art::EDAnalyzer {
public:
  struct Config {
    using Name = fhicl::Name;
    using Comment = fhicl::Comment;

    fhicl::Atom<art::InputTag> crvCoincidenceClusterTag{
        Name("crvCoincidenceClusterTag"),
        Comment("CRV coincidence cluster finder"),
        art::InputTag{"CrvCoincidenceClusterFinder"}};
    fhicl::Atom<art::InputTag> crvRecoPulseTag{
        Name("crvRecoPulseTag"),
        Comment("CRV reco pulse producer; empty skips the inclusive pulse plots"),
        art::InputTag{"CrvRecoPulses"}};
    fhicl::Atom<std::string> outputTag{
        Name("outputTag"),
        Comment("TFileService subdirectory; empty books in the module directory"),
        ""};
    fhicl::Atom<int> diagLevel{Name("diagLevel"), Comment("Diagnostic level"), 0};

    // Every parameter the helper itself takes, spliced in at this level so
    // the FCL stays flat and the atoms and their defaults live once, beside
    // the Config they fill (CRVRecoDQMFhicl, in the helper header).
    fhicl::TableFragment<CRVRecoDQMFhicl> helper;

    fhicl::Atom<bool> fillSectorMPV{
        Name("fillSectorMPV"),
        Comment("Fill per-sector crvPEsMPV_CRVsector* using GeometryService"),
        false};
  };

  using Parameters = art::EDAnalyzer::Table<Config>;

  explicit CRVRecoDQMAnalyzer(const Parameters& conf);

  void beginJob() override;
  void beginRun(const art::Run& run) override;
  void beginSubRun(const art::SubRun& subrun) override;
  void endSubRun(const art::SubRun& subrun) override;
  void analyze(const art::Event& event) override;
  void endJob() override;

private:
  static CRVRecoDQM::Config makeHelperConfig(const Config& conf);

  art::InputTag crvCoincidenceClusterTag_;
  art::InputTag crvRecoPulseTag_;
  std::string outputTag_;
  int diagLevel_;
  bool fillSectorMPV_;
  CRVRecoDQM dqm_;
  bool sectorMapSent_{false};
  bool warnedMissingClusters_{false};
  bool warnedMissingPulses_{false};
};

CRVRecoDQM::Config CRVRecoDQMAnalyzer::makeHelperConfig(const Config& conf)
{
  return toConfig(conf.helper());
}

CRVRecoDQMAnalyzer::CRVRecoDQMAnalyzer(const Parameters& conf) :
    art::EDAnalyzer{conf},
    crvCoincidenceClusterTag_(conf().crvCoincidenceClusterTag()),
    crvRecoPulseTag_(conf().crvRecoPulseTag()),
    outputTag_(conf().outputTag()),
    diagLevel_(conf().diagLevel()),
    fillSectorMPV_(conf().fillSectorMPV()),
    dqm_(makeHelperConfig(conf()))
{}

void CRVRecoDQMAnalyzer::beginJob()
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

void CRVRecoDQMAnalyzer::beginSubRun(const art::SubRun& subrun)
{
  dqm_.BeginSubRun(static_cast<int>(subrun.run()),
                   static_cast<int>(subrun.subRun()));
}

void CRVRecoDQMAnalyzer::endSubRun(const art::SubRun&)
{
  dqm_.EndSubRun();
}

void CRVRecoDQMAnalyzer::beginRun(const art::Run&)
{
  if (!fillSectorMPV_ || sectorMapSent_) {
    return;
  }
  sectorMapSent_ = true;

  GeomHandle<CosmicRayShield> CRS;

  //cluster position axes from the CRV envelope, as CrvDQMcollector does. Gated
  //on fillSectorMPV because that is what says this job configured a geometry;
  //without it the helper falls back to the Config ranges on first fill.
  // Built from the counters, not CosmicRayShield::getSectorHalfLengths: that one
  // measures the aluminum sheets, and a countersOnly module (all of the extracted
  // geometry) has none, so it throws. getHalfLengths() is already indexed by world
  // axis -- see CRSScintillatorBarDetail::getHalfThickness(), which reads
  // _halfLengths[_localToWorld[0]] -- so no permutation is needed.
  std::vector<double> crvMin(3, 0.0), crvMax(3, 0.0);
  bool firstBar = true;
  for (const auto& bar : CRS->getAllCRSScintillatorBars()) {
    const std::vector<double>& hl = bar->getHalfLengths();
    if (hl.size() < 3) continue;
    const CLHEP::Hep3Vector& pos = bar->getPosition();
    for (int i = 0; i < 3; ++i) {
      const double lo = pos[i] - hl[i];
      const double hi = pos[i] + hl[i];
      if (firstBar) {
        crvMin[i] = lo;
        crvMax[i] = hi;
      } else {
        crvMin[i] = std::min(crvMin[i], lo);
        crvMax[i] = std::max(crvMax[i], hi);
      }
    }
    firstBar = false;
  }
  dqm_.BookPositionAxes(crvMin, crvMax);  //empty envelope falls back to fcl

  auto const& crvSectors = CRS->getCRSScintillatorShields();
  std::vector<std::string> sectorNames;
  sectorNames.reserve(crvSectors.size());
  for (std::size_t i = 0; i < crvSectors.size(); ++i) {
    sectorNames.emplace_back(crvSectors.at(i).name(""));
  }

  // No Proditions here, so unlike CrvDQMcollector this keeps notConnected
  // channels; they enter the sector MPV hists at zero.
  auto const& crvCounters = CRS->getAllCRSScintillatorBars();
  std::vector<int> channelToSector(crvCounters.size() * CRVId::nChanPerBar, -1);
  for (std::size_t channel = 0; channel < channelToSector.size(); ++channel) {
    CRSScintillatorBarIndex barIndex(
        static_cast<int>(channel / CRVId::nChanPerBar));
    channelToSector[channel] = CRS->getBar(barIndex).id().getShieldNumber();
  }

  dqm_.BookSectorMPV(sectorNames, channelToSector);
}

void CRVRecoDQMAnalyzer::analyze(const art::Event& event)
{
  art::Handle<CrvCoincidenceClusterCollection> clusterHandle;
  event.getByLabel(crvCoincidenceClusterTag_, clusterHandle);
  if (!clusterHandle.isValid() || clusterHandle.product() == nullptr) {
    if (!warnedMissingClusters_) {
      warnedMissingClusters_ = true;
      mf::LogWarning("CRVRecoDQMAnalyzer")
          << "No CrvCoincidenceClusterCollection at "
          << crvCoincidenceClusterTag_ << ". Event skipped. "
          << "Reported once per job.";
    }
    return;
  }

  if (crvRecoPulseTag_.empty()) {
    dqm_.Fill(*clusterHandle);
    return;
  }

  art::Handle<CrvRecoPulseCollection> pulseHandle;
  event.getByLabel(crvRecoPulseTag_, pulseHandle);
  if (!pulseHandle.isValid() || pulseHandle.product() == nullptr) {
    if (!warnedMissingPulses_) {
      warnedMissingPulses_ = true;
      mf::LogWarning("CRVRecoDQMAnalyzer")
          << "No CrvRecoPulseCollection at " << crvRecoPulseTag_
          << ". The inclusive reco-pulse plots stay empty; everything driven "
          << "by the clusters still fills. Reported once per job.";
    }
    dqm_.Fill(*clusterHandle);
    return;
  }

  dqm_.Fill(*clusterHandle, *pulseHandle);
}

void CRVRecoDQMAnalyzer::endJob()
{
  dqm_.WriteGraphs();

  if (diagLevel_ > 0) {
    std::cout << "[CRVRecoDQMAnalyzer] Total events: " << dqm_.nEvents()
              << std::endl;
    std::cout << "[CRVRecoDQMAnalyzer] Events with coincidence clusters: "
              << dqm_.nEventsWithClusters() << std::endl;
    std::cout << "[CRVRecoDQMAnalyzer] Coincidence clusters: "
              << dqm_.nClusters() << std::endl;
    std::cout << "[CRVRecoDQMAnalyzer] Reco pulses in clusters: "
              << dqm_.nRecoPulses() << " of " << dqm_.nInclusiveRecoPulses()
              << " reconstructed" << std::endl;
    std::cout << "[CRVRecoDQMAnalyzer] Channel fits: " << dqm_.nFitsSucceeded()
              << " of " << dqm_.nFits() << " attempted, mean chi2/ndf "
              << dqm_.meanFitChi2PerNdf() << std::endl;
    if (dqm_.nOnlineIdOutOfRange() > 0) {
      std::cout << "[CRVRecoDQMAnalyzer] CRVId-range skips (crvPEsMPV_ROC*): "
                << dqm_.nOnlineIdOutOfRange() << std::endl;
    }
    if (dqm_.nOfflineChannelOutOfRange() > 0) {
      std::cout << "[CRVRecoDQMAnalyzer] Offline-channel-range skips: "
                << dqm_.nOfflineChannelOutOfRange() << std::endl;
    }
    if (dqm_.nNullRecoPulsePtrs() > 0) {
      std::cout << "[CRVRecoDQMAnalyzer] Null CrvRecoPulse Ptrs: "
                << dqm_.nNullRecoPulsePtrs() << std::endl;
    }
  }
}

} // namespace mu2e

DEFINE_ART_MODULE(mu2e::CRVRecoDQMAnalyzer)
