#ifndef TrackerDQM_inc_TrackerDigiDQM_hh
#define TrackerDQM_inc_TrackerDigiDQM_hh
// Tracker digi DQM client: the histograms the otsdaq online monitor and the
// offline DQM module fill from StrawDigis. This is the starting set, one
// histogram per kind of axis (a count, a raw value, a detector index, a 2D
// map); the tracker group replaces it with its own. Every axis is a constant
// here, every histogram is booked in book(), and a change to either bumps
// kBinningVersion and the catalogue in data/ (Offline/DQMHelpers/README.md).

#include "Offline/DQMHelpers/inc/DQMClient.hh"
#include "Offline/DataProducts/inc/StrawId.hh"
#include "Offline/RecoDataProducts/inc/StrawDigi.hh"

#include "TH1F.h"
#include "TH2F.h"

namespace mu2e {

class TrackerDigiDQM : public DQMClient {
public:
  static constexpr int kBinningVersion = 1;

  static constexpr DQMAxis kNDigis = DQMAxis::Counts(0, 199);
  static constexpr DQMAxis kTdc{100, 0., 80.e3};
  static constexpr DQMAxis kPanel = DQMAxis::Counts(0, StrawId::_nupanels - 1);
  static constexpr DQMAxis kStraw = DQMAxis::Counts(0, StrawId::_nstraws - 1);

  explicit TrackerDigiDQM(const DQMHistSet::Config& hists = {});

  void Fill(const StrawDigiCollection& digis);

  TH1F* nDigis() const { return h_nDigis_; }  //digis per event
  TH1F* tdcCal() const { return h_tdcCal_; }  //TDC at the cal end
  TH1F* panelOccupancy() const { return h_panelOccupancy_; }  //digis per unique panel
  TH2F* strawOccupancy() const { return h_strawOccupancy_; }  //digis per straw, by panel

private:
  void book() override;

  DQMH1<TH1F> h_nDigis_;
  DQMH1<TH1F> h_tdcCal_;
  DQMH1<TH1F> h_panelOccupancy_;
  DQMH2<TH2F> h_strawOccupancy_;
};

} // namespace mu2e

#endif /* TrackerDQM_inc_TrackerDigiDQM_hh */
