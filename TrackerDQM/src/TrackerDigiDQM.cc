// Tracker digi DQM client.

#include "Offline/TrackerDQM/inc/TrackerDigiDQM.hh"

#include "Offline/DataProducts/inc/StrawEnd.hh"

namespace mu2e {

TrackerDigiDQM::TrackerDigiDQM(const DQMHistSet::Config& hists) :
    DQMClient("TrackerDigiDQM", kBinningVersion, hists) {}

void TrackerDigiDQM::book() {
  h_nDigis_ = hists().book1<TH1F>("nDigis", "Straw digis / event;N digis;Events", kNDigis);
  h_tdcCal_ = hists().book1<TH1F>("tdcCal", "Digi TDC, cal end;TDC;Digis", kTdc);
  h_panelOccupancy_ = hists().book1<TH1F>(
      "panelOccupancy", "Digis per panel;Unique panel (6*plane + panel);Digis", kPanel);
  h_strawOccupancy_ = hists().book2<TH2F>(
      "strawOccupancy", "Digis per straw;Unique panel (6*plane + panel);Straw;Digis", kPanel,
      kStraw);
}

void TrackerDigiDQM::Fill(const StrawDigiCollection& digis) {
  beginEvent();
  h_nDigis_.Fill(digis.size());
  for (const auto& digi : digis) {
    const StrawId sid = digi.strawId();
    if (!sid.valid()) {
      diag().Count("invalidStrawId", "a StrawDigi has an invalid StrawId; it is not histogrammed");
      continue;
    }
    h_tdcCal_.Fill(digi.TDC(StrawEnd::cal));
    h_panelOccupancy_.Fill(sid.uniquePanel());
    h_strawOccupancy_.Fill(sid.uniquePanel(), sid.getStraw());
  }
}

} // namespace mu2e
