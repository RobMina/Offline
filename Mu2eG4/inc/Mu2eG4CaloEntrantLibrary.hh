#ifndef Mu2eG4_Mu2eG4CaloEntrantLibrary_hh
#define Mu2eG4_Mu2eG4CaloEntrantLibrary_hh
//
// Read-only per-entrant calorimeter shower library, as written by mu2e-fastsim
// calofastsim.showers.entrants.export_root (see Mu2eG4/doc/fastSimulation.md).
//
// One row per entrant (a particle whose first crossing into a calorimeter disk precedes its ancestors'), with
// its cell (species, face, disk, energy, face cosine, radius or depth) and its steps: every CaloShowerStep of its
// subtree, as positions relative to the entry point in the deposit's disk frame turned to the entry azimuth,
// times relative to the entry time, and energies (never rescaled).
//
// The library is loaded once per file and shared by all Geant4 threads (load() is thread safe; the object is const).
//

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mu2e {

  class Mu2eG4CaloEntrantLibrary {
  public:
    enum Face : int { front = 0, inner = 1, back = 2, outer = 3 };
    enum Species : int { electron = 0, positron = 1, gamma = 2, other = 3 };

    using Cell = std::array<int, 3>;   // energy, cosine and position (radius or depth) bin indices

    struct Entry {
      int species = other, face = front, disk = 0;
      double energy = 0., cosine = 0., key = 0., azimuth = 0., time = 0.;
      Cell cell{};
      std::int64_t stepBegin = 0, stepEnd = 0;
    };

    struct Step {
      double x = 0., y = 0., z = 0., t = 0.;   // relative to the entry: mm (turned disk frame), ns
      double energyG4 = 0., energyBirks = 0.;  // MeV
      int disk = 0;
      int crystal = 0;                         // donor crystal, for diagnostics only
    };

    // Shared instance for a file, read on first use
    static std::shared_ptr<const Mu2eG4CaloEntrantLibrary> load(const std::string& path);

    explicit Mu2eG4CaloEntrantLibrary(const std::string& path);

    static int speciesCode(int pdgId);

    // Bin indices of an entry (clipped to the edges) and whether any feature lies outside the edges
    // (same rule as calofastsim.showers.entrants.cells)
    bool locate(int face, double energy, double cosine, double key, Cell& cell) const;

    // Donor row for a query cell: the query's own cell if occupied, otherwise the closest occupied cell (squared
    // bin-index distance, ties to the lowest cell) of the same species, face and disk if its distance is at most
    // maxDistance2; uniform in [0,1) picks the entrant inside the cell. Returns -1 if there is none.
    // distance2 is set to the squared distance of the cell used.
    std::int64_t select(int species, int face, int disk, const Cell& cell, double uniform,
                        int maxDistance2, int& distance2) const;

    // Donor row by entry-point distance: among the entrants of the same species, face, disk, energy bin and cosine
    // bin, one of the k closest to the query entry, uniform in [0,1) picking which. The distance counts a donor and
    // its image turned by 180 degrees about the disk axis (a symmetry of the crystal lattice), so placement turns a
    // donor by about 0 or 180 degrees and moves it by about that distance: front face, the distance between the entry
    // points on the face; inner edge, sqrt(dz^2 + (rho * dphi)^2) with the query radius. Returns -1 if the group is
    // empty; distance is set to the chosen donor's distance (mm).
    std::int64_t selectNearest(int species, int face, int disk, int energyBin, int cosineBin,
                               double key, double azimuth, double rho, std::size_t k, double uniform,
                               double& distance) const;

    const Entry& entry(std::int64_t row) const { return entries_.at(row); }
    const Step& step(std::int64_t row) const { return steps_[row]; }
    std::size_t nEntries() const { return entries_.size(); }
    std::size_t nSteps() const { return steps_.size(); }
    const std::string& contentHash() const { return contentHash_; }
    const std::string& path() const { return path_; }

  private:
    struct Occupied {
      Cell cell;
      std::vector<std::int64_t> rows;   // ascending library rows
    };
    using Group = std::array<int, 3>;   // species, face, disk

    std::string path_;
    std::string contentHash_;
    std::vector<Entry> entries_;
    std::vector<Step> steps_;
    std::array<std::vector<double>, 4> edges_;   // energy, cosine, front radius, inner depth
    std::map<Group, std::vector<Occupied>> groups_;  // occupied cells per group, sorted by cell
    std::map<std::array<int, 5>, std::vector<std::int64_t>> nearestGroups_;  // species, face, disk, energy, cosine bins
  };

} // namespace mu2e

#endif /* Mu2eG4_Mu2eG4CaloEntrantLibrary_hh */
