#ifndef DbTables_CRVPropagation_hh
#define DbTables_CRVPropagation_hh

#include "Offline/DataProducts/inc/CRVId.hh"
#include "Offline/DbTables/inc/DbTable.hh"
#include "cetlib_except/exception.h"
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace mu2e {

// light propagation along each CRV bar relative to the bar's lookup table (1 = as simulated in the table)
class CRVPropagation : public DbTable {
 public:
  typedef std::shared_ptr<CRVPropagation> ptr_t;
  typedef std::shared_ptr<const CRVPropagation> cptr_t;

  class Row {
   public:
    Row(std::uint16_t bar, float signalSpeedRatio, float attenuationLengthRatio) :
        _bar(bar), _signalSpeedRatio(signalSpeedRatio),
        _attenuationLengthRatio(attenuationLengthRatio) {}
    std::uint16_t bar() const { return _bar; }
    float signalSpeedRatio() const { return _signalSpeedRatio; }
    float attenuationLengthRatio() const { return _attenuationLengthRatio; }

   private:
    std::uint16_t _bar;
    float _signalSpeedRatio;
    float _attenuationLengthRatio;
  };

  constexpr static const char* cxname = "CRVPropagation";

  CRVPropagation() :
      DbTable(cxname, "crv.propagation", "bar,signalSpeedRatio,attenuationLengthRatio") {}
  const Row& rowAt(const std::size_t index) const { return _rows.at(index); }
  const Row& row(std::uint16_t bar) const { return _rows.at(bar); }
  std::vector<Row> const& rows() const { return _rows; }
  std::size_t nrow() const override { return _rows.size(); };
  std::size_t size() const override {
    return baseSize() + nrow() * sizeof(Row);
  };
  const std::string orderBy() const override { return std::string("bar"); }

  void addRow(const std::vector<std::string>& columns) override {
    std::uint16_t bar = std::stoul(columns[0]);
    // enforce order, so bar can be looked up by index
    if (bar >= CRVId::nBars || bar != _rows.size()) {
      throw cet::exception("CRVPROPAGATION_BAD_BAR")
          << "CRVPropagation::addRow bad bar, saw " << columns[0] << ", expected "
          << _rows.size() << "\n";
    }
    float speed = std::stof(columns[1]);
    float attenuation = std::stof(columns[2]);
    if (!(speed > 0) || !(attenuation > 0)) {
      throw cet::exception("CRVPROPAGATION_BAD_RATIO")
          << "CRVPropagation::addRow ratios must be positive, bar " << bar
          << ": " << columns[1] << ", " << columns[2] << "\n";
    }
    _rows.emplace_back(bar, speed, attenuation);
  }

  void rowToCsv(std::ostringstream& sstream, std::size_t irow) const override {
    Row const& r = _rows.at(irow);
    sstream << r.bar() << ",";
    sstream << std::fixed << std::setprecision(4);
    sstream << r.signalSpeedRatio() << ",";
    sstream << r.attenuationLengthRatio();
  }

  virtual void clear() override {
    baseClear();
    _rows.clear();
  }

 private:
  std::vector<Row> _rows;
};

}  // namespace mu2e
#endif
