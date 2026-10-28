#ifndef CaloDQM_inc_CaloDigiDQM_hh
#define CaloDQM_inc_CaloDigiDQM_hh
// Calorimeter digi DQM client: the histograms the otsdaq online monitor and
// the offline DQM module fill from CaloDigis. This is the starting set, one
// histogram per kind of axis (a count, a raw value, a detector index, a 2D
// map); the calorimeter group replaces it with its own. Every axis is a
// constant here, every histogram is booked in book(), and a change to either
// bumps kBinningVersion and the catalogue in data/ (Offline/DQMHelpers/README.md).
//
// Original Author: R. Mina

#include "Offline/DQMHelpers/inc/DQMClient.hh"
#include "Offline/DataProducts/inc/CaloConst.hh"
#include "Offline/RecoDataProducts/inc/CaloDigi.hh"

#include "TH1F.h"
#include "TH2F.h"

namespace mu2e {

class CaloDigiDQM : public DQMClient {
public:
  static constexpr int kBinningVersion = 1;

  static constexpr DQMAxis kNDigis = DQMAxis::Counts(0, 499);
  static constexpr DQMAxis kT0{400, 0., 4000.};  //digitizer ticks (CaloConst::_digitizationPeriod)
  static constexpr DQMAxis kSiPM = DQMAxis::Counts(0, CaloConst::_nChannel - 1);
  static constexpr DQMAxis kPeakAdc{64, 0., 4096.};

  explicit CaloDigiDQM(const DQMHistSet::Config& hists = {});

  void Fill(const CaloDigiCollection& digis);

  TH1F* nDigis() const { return h_nDigis_; }  //digis per event
  TH1F* t0() const { return h_t0_; }  //waveform start time
  TH1F* sipmOccupancy() const { return h_sipmOccupancy_; }  //digis per offline SiPM ID
  TH2F* peakAdcVsSiPM() const { return h_peakAdcVsSiPM_; }  //largest sample, by SiPM

private:
  void book() override;

  DQMH1<TH1F> h_nDigis_;
  DQMH1<TH1F> h_t0_;
  DQMH1<TH1F> h_sipmOccupancy_;
  DQMH2<TH2F> h_peakAdcVsSiPM_;
};

} // namespace mu2e

#endif /* CaloDQM_inc_CaloDigiDQM_hh */
