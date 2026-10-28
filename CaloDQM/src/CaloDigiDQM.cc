// Calorimeter digi DQM client.

#include "Offline/CaloDQM/inc/CaloDigiDQM.hh"

#include <algorithm>

namespace mu2e {

CaloDigiDQM::CaloDigiDQM(const DQMHistSet::Config& hists) :
    DQMClient("CaloDigiDQM", kBinningVersion, hists) {}

void CaloDigiDQM::book() {
  h_nDigis_ = hists().book1<TH1F>("nDigis", "Calo digis / event;N digis;Events", kNDigis);
  h_t0_ = hists().book1<TH1F>("t0", "Digi t0;t0 [5 ns ticks];Digis", kT0);
  h_sipmOccupancy_ =
      hists().book1<TH1F>("sipmOccupancy", "Digis per SiPM;Offline SiPM ID;Digis", kSiPM);
  h_peakAdcVsSiPM_ = hists().book2<TH2F>(
      "peakAdcVsSiPM", "Largest waveform sample per SiPM;Offline SiPM ID;Peak ADC;Digis", kSiPM,
      kPeakAdc);
}

void CaloDigiDQM::Fill(const CaloDigiCollection& digis) {
  beginEvent();
  h_nDigis_.Fill(digis.size());
  for (const auto& digi : digis) {
    const int sipm = digi.SiPMID();
    if (sipm < 0 || sipm >= CaloConst::_nChannel) {
      diag().Count("sipmIdOutOfRange",
                   "a CaloDigi SiPM ID is not an offline ID in [0, CaloConst::_nChannel); it is "
                   "not histogrammed");
      continue;
    }
    h_t0_.Fill(digi.t0());
    h_sipmOccupancy_.Fill(sipm);
    const auto& waveform = digi.waveform();
    if (waveform.empty()) {
      diag().Count("emptyWaveform", "a CaloDigi has no waveform samples");
      continue;
    }
    h_peakAdcVsSiPM_.Fill(sipm, *std::max_element(waveform.begin(), waveform.end()));
  }
}

} // namespace mu2e
