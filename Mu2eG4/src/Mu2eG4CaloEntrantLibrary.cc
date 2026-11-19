//
// Read-only per-entrant calorimeter shower library (see Mu2eG4/inc/Mu2eG4CaloEntrantLibrary.hh)
//

#include "Offline/Mu2eG4/inc/Mu2eG4CaloEntrantLibrary.hh"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <mutex>

#include "cetlib_except/exception.h"

#include "TFile.h"
#include "TObjString.h"
#include "TTree.h"

namespace mu2e {

  namespace {

    std::unique_ptr<TFile> open(const std::string& path) {
      std::unique_ptr<TFile> file(TFile::Open(path.c_str(), "READ"));
      if (!file || file->IsZombie()) {
        throw cet::exception("CONFIG") << "Mu2eG4CaloEntrantLibrary: cannot open " << path << "\n";
      }
      return file;
    }

    TTree* tree(TFile& file, const char* name) {
      auto t = file.Get<TTree>(name);
      if (t == nullptr) {
        throw cet::exception("CONFIG") << "Mu2eG4CaloEntrantLibrary: " << file.GetName() << " has no tree " << name
                                       << " (not a calofastsim.showers.entrants ROOT export?)\n";
      }
      return t;
    }

    template <typename T>
    void bind(TTree* t, const char* branch, T* value) {
      if (t->GetBranch(branch) == nullptr) {
        throw cet::exception("CONFIG") << "Mu2eG4CaloEntrantLibrary: tree " << t->GetName() << " has no branch "
                                       << branch << "\n";
      }
      t->SetBranchAddress(branch, value);
    }

    std::vector<double> edges(TFile& file, const char* name) {
      TTree* t = tree(file, name);
      double edge = 0.;
      bind(t, "edge", &edge);
      std::vector<double> out;
      for (Long64_t i = 0; i < t->GetEntries(); ++i) {
        t->GetEntry(i);
        out.push_back(edge);
      }
      if (out.size() < 2 || !std::is_sorted(out.begin(), out.end(), std::less_equal<double>())) {
        throw cet::exception("CONFIG") << "Mu2eG4CaloEntrantLibrary: " << name << " edges must be strictly increasing\n";
      }
      return out;
    }

    // Python _locate: clipped bin index, and whether the value lies outside the edges
    int locateOne(const std::vector<double>& e, double value, bool& outside) {
      outside = outside || value < e.front() || value > e.back();
      const long index = std::upper_bound(e.begin(), e.end(), value) - e.begin() - 1;
      return static_cast<int>(std::clamp<long>(index, 0, static_cast<long>(e.size()) - 2));
    }

  } // namespace

  std::shared_ptr<const Mu2eG4CaloEntrantLibrary> Mu2eG4CaloEntrantLibrary::load(const std::string& path) {
    static std::mutex mutex;
    static std::map<std::string, std::weak_ptr<const Mu2eG4CaloEntrantLibrary>> cache;
    std::lock_guard<std::mutex> lock(mutex);
    if (auto shared = cache[path].lock()) return shared;
    auto library = std::make_shared<const Mu2eG4CaloEntrantLibrary>(path);
    cache[path] = library;
    return library;
  }

  Mu2eG4CaloEntrantLibrary::Mu2eG4CaloEntrantLibrary(const std::string& path)
    : path_(path)
  {
    auto file = open(path);

    if (auto hash = file->Get<TObjString>("content_hash")) contentHash_ = hash->GetString().Data();

    edges_ = {edges(*file, "bins_energy"), edges(*file, "bins_cosine"),
              edges(*file, "bins_front_radius"), edges(*file, "bins_inner_depth")};

    TTree* s = tree(*file, "steps");
    Step step;
    Long64_t sdisk = 0, scrystal = 0;
    bind(s, "x", &step.x); bind(s, "y", &step.y); bind(s, "z", &step.z); bind(s, "t", &step.t);
    bind(s, "energy_g4", &step.energyG4); bind(s, "energy_birks", &step.energyBirks);
    bind(s, "disk", &sdisk); bind(s, "crystal", &scrystal);
    steps_.reserve(s->GetEntries());
    for (Long64_t i = 0; i < s->GetEntries(); ++i) {
      s->GetEntry(i);
      step.disk = static_cast<int>(sdisk);
      step.crystal = static_cast<int>(scrystal);
      steps_.push_back(step);
    }

    TTree* e = tree(*file, "entries");
    Entry entry;
    Long64_t species = 0, face = 0, disk = 0, ce = 0, cc = 0, ck = 0, begin = 0, end = 0;
    bind(e, "species", &species); bind(e, "face", &face); bind(e, "disk", &disk);
    bind(e, "energy", &entry.energy); bind(e, "cosine", &entry.cosine); bind(e, "key", &entry.key);
    bind(e, "azimuth", &entry.azimuth); bind(e, "time", &entry.time);
    bind(e, "cell_energy", &ce); bind(e, "cell_cosine", &cc); bind(e, "cell_key", &ck);
    bind(e, "step_begin", &begin); bind(e, "step_end", &end);
    entries_.reserve(e->GetEntries());
    for (Long64_t i = 0; i < e->GetEntries(); ++i) {
      e->GetEntry(i);
      entry.species = static_cast<int>(species);
      entry.face = static_cast<int>(face);
      entry.disk = static_cast<int>(disk);
      entry.cell = {static_cast<int>(ce), static_cast<int>(cc), static_cast<int>(ck)};
      entry.stepBegin = begin;
      entry.stepEnd = end;
      if (begin < 0 || end < begin || end > static_cast<Long64_t>(steps_.size())) {
        throw cet::exception("CONFIG") << "Mu2eG4CaloEntrantLibrary: " << path << " entry " << i
                                       << " has an invalid step range\n";
      }
      entries_.push_back(entry);
    }

    // occupied cells per (species, face, disk), sorted by cell, each with its rows in library order
    std::map<Group, std::map<Cell, std::vector<std::int64_t>>> cells;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      const auto& en = entries_[i];
      cells[{en.species, en.face, en.disk}][en.cell].push_back(static_cast<std::int64_t>(i));
    }
    for (auto& [group, byCell] : cells) {
      auto& occupied = groups_[group];
      for (auto& [cell, rows] : byCell) occupied.push_back({cell, std::move(rows)});
    }
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      const auto& en = entries_[i];
      nearestGroups_[{en.species, en.face, en.disk, en.cell[0], en.cell[1]}].push_back(static_cast<std::int64_t>(i));
    }
  }

  std::int64_t Mu2eG4CaloEntrantLibrary::selectNearest(int species, int face, int disk, int energyBin, int cosineBin,
                                                       double key, double azimuth, double rho, std::size_t k,
                                                       double uniform, double& distance) const {
    distance = std::numeric_limits<double>::max();
    auto group = nearestGroups_.find({species, face, disk, energyBin, cosineBin});
    if (group == nearestGroups_.end() || group->second.empty() || k == 0) return -1;
    const auto& rows = group->second;
    std::vector<std::pair<double, std::int64_t>> d2;
    d2.reserve(rows.size());
    for (auto row : rows) {
      const auto& en = entries_[row];
      // azimuth difference folded by the 180-degree lattice symmetry into [-pi/2, pi/2]
      const double dphi = std::remainder(azimuth - en.azimuth, M_PI);
      double value;
      if (face == inner) {
        const double dz = key - en.key, arc = rho * dphi;
        value = dz * dz + arc * arc;
      } else {
        value = key * key + en.key * en.key - 2. * key * en.key * std::cos(dphi);
      }
      d2.emplace_back(std::max(value, 0.), row);
    }
    const std::size_t n = std::min(k, d2.size());
    std::partial_sort(d2.begin(), d2.begin() + n, d2.end());
    const auto pick = std::min<std::size_t>(static_cast<std::size_t>(uniform * n), n - 1);
    distance = std::sqrt(d2[pick].first);
    return d2[pick].second;
  }

  int Mu2eG4CaloEntrantLibrary::speciesCode(int pdgId) {
    switch (pdgId) {
    case 11: return electron;
    case -11: return positron;
    case 22: return gamma;
    default: return other;
    }
  }

  bool Mu2eG4CaloEntrantLibrary::locate(int face, double energy, double cosine, double key, Cell& cell) const {
    bool outside = !(std::isfinite(energy) && std::isfinite(cosine) && std::isfinite(key));
    if (outside) return true;
    cell[0] = locateOne(edges_[0], energy, outside);
    cell[1] = locateOne(edges_[1], cosine, outside);
    cell[2] = locateOne(edges_[face == inner ? 3 : 2], key, outside);
    return outside;
  }

  std::int64_t Mu2eG4CaloEntrantLibrary::select(int species, int face, int disk, const Cell& cell, double uniform,
                                                int maxDistance2, int& distance2) const {
    distance2 = std::numeric_limits<int>::max();
    auto group = groups_.find({species, face, disk});
    if (group == groups_.end()) return -1;
    const Occupied* best = nullptr;
    for (const auto& occupied : group->second) {
      int d2 = 0;
      for (int k = 0; k < 3; ++k) d2 += (occupied.cell[k] - cell[k]) * (occupied.cell[k] - cell[k]);
      if (d2 < distance2) {
        distance2 = d2;
        best = &occupied;
      }
    }
    if (best == nullptr || distance2 > maxDistance2) return -1;
    const auto n = static_cast<std::int64_t>(best->rows.size());
    const auto pick = std::min<std::int64_t>(static_cast<std::int64_t>(uniform * n), n - 1);
    return best->rows[pick];
  }

} // namespace mu2e
