//
// Calorimeter fast-simulation model: replaces the shower of a particle entering a calorimeter disk through its
// front face or its inner edge by the CaloShowerSteps of a Geant4 donor from a per-entrant shower library
// (mu2e-fastsim calofastsim.showers.entrants), placed at the entry (see Mu2eG4/doc/fastSimulation.md).
//
// Envelopes: the CaloDisk_<n> volumes. A track triggers the model only at its entry into the envelope (on the
// envelope surface, moving inward) through the front face or the inner edge; tracks born inside the disk, entering
// through the back or the outer edge, of an unsupported species, outside the library bins, without a donor cell close
// enough, or whose donor energy differs too much stay with full Geant4.
//
// Placement is the library's: the entry state is taken in the disk frame of the geometry service (entry azimuth,
// radius or depth, face cosine, total energy), a donor is drawn in that cell, and each donor step is turned by the
// query azimuth about the disk axis and translated to the query entry. Energies are never rescaled. Each placed step
// becomes a fast hit of the entrant (Mu2eG4FastHitMaker), recorded by CaloCrystalSD if it falls in a crystal.
//

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <memory>
#include <string>
#include <vector>

#include "art/Utilities/ToolConfigTable.h"
#include "art/Utilities/ToolMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "Geant4/G4AffineTransform.hh"
#include "Geant4/G4FastStep.hh"
#include "Geant4/G4FastTrack.hh"
#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4Region.hh"
#include "Geant4/G4Track.hh"
#include "Geant4/G4VFastSimulationModel.hh"
#include "Geant4/G4VPhysicalVolume.hh"
#include "Geant4/G4VSolid.hh"
#include "Geant4/Randomize.hh"

#include "Offline/CalorimeterGeom/inc/DiskCalorimeter.hh"
#include "Offline/ConfigTools/inc/ConfigFileLookupPolicy.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/GeometryService/inc/WorldG4.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4FastSimModel.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4CaloEntrantLibrary.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4FastHit.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4FastHitMaker.hh"

namespace mu2e {

  namespace {

    using Library = Mu2eG4CaloEntrantLibrary;

    // A disk frame in Geant4 world coordinates: origin and the disk x, y, z axes
    struct DiskFrame {
      G4ThreeVector origin, ex, ey, ez;
      G4ThreeVector toLocalPoint(const G4ThreeVector& world) const {
        const G4ThreeVector v = world - origin;
        return {v.dot(ex), v.dot(ey), v.dot(ez)};
      }
      G4ThreeVector toLocalVector(const G4ThreeVector& v) const { return {v.dot(ex), v.dot(ey), v.dot(ez)}; }
      G4ThreeVector toWorldVector(const G4ThreeVector& v) const { return v.x() * ex + v.y() * ey + v.z() * ez; }
    };

    struct Settings {
      std::shared_ptr<const Library> library;
      std::vector<DiskFrame> disks;   // indexed by disk id
      double minKineticEnergy = 0.;
      bool nearest = true;
      std::size_t nearestK = 1;
      double maxEntryDistance = 20.;
      int maxCellDistance2 = 0;
      double maxEnergyDeviation = -1.;
      double faceTolerance = 0.9;
      bool diagnostics = false;
    };

    class CaloEntrantLibraryModel : public G4VFastSimulationModel {
    public:
      CaloEntrantLibraryModel(const std::string& name, G4Region* envelope, const Settings& settings)
        : G4VFastSimulationModel(name, envelope)
        , s_(settings)
      {}

      // particles are chosen by physics.fastSim.models[].particles; species without library entrants never trigger
      G4bool IsApplicable(const G4ParticleDefinition&) override { return true; }

      G4bool ModelTrigger(const G4FastTrack& fastTrack) override {
        const G4Track* track = fastTrack.GetPrimaryTrack();
        if (track->GetKineticEnergy() < s_.minKineticEnergy) return false;
        const int species = Library::speciesCode(track->GetDefinition()->GetPDGEncoding());
        if (species == Library::other) return false;

        // only at the entry into the envelope
        const G4ThreeVector localPosition = fastTrack.GetPrimaryTrackLocalPosition();
        const G4VSolid* solid = fastTrack.GetEnvelopeSolid();
        if (solid->Inside(localPosition) != kSurface) return false;
        const G4ThreeVector localNormal = solid->SurfaceNormal(localPosition);   // outward
        if (fastTrack.GetPrimaryTrackLocalDirection().dot(localNormal) >= 0.) return false;

        const G4ThreeVector position = track->GetPosition();
        const int disk = nearestDisk(position);
        const DiskFrame& frame = s_.disks[disk];
        const G4ThreeVector p = frame.toLocalPoint(position);
        const G4ThreeVector momentum = frame.toLocalVector(track->GetMomentum());
        const G4ThreeVector normal =
          frame.toLocalVector(fastTrack.GetInverseAffineTransformation()->TransformAxis(localNormal));

        const double rho = std::hypot(p.x(), p.y());
        const double phi = std::atan2(p.y(), p.x());
        const G4ThreeVector radial(std::cos(phi), std::sin(phi), 0.);
        int face = -1;
        if (normal.z() < -s_.faceTolerance) face = Library::front;                // front face: outward normal -z
        else if (normal.dot(radial) < -s_.faceTolerance) face = Library::inner;   // inner edge: outward normal to the axis
        auto reject = [&](const char* why) {
          if (s_.diagnostics) {
            mf::LogInfo("Mu2eG4FastSimDiag") << "caloEntrant reject " << why << " pdg " << track->GetDefinition()->GetPDGEncoding()
                                             << " face " << face << " disk " << disk << " rho " << rho << " z " << p.z()
                                             << " E " << track->GetTotalEnergy();
          }
          return false;
        };
        if (face < 0) return reject("face");

        const double pmag = momentum.mag();
        if (!(pmag > 0.)) return reject("momentum");
        const double cosine = std::clamp(face == Library::inner ? momentum.dot(radial) / pmag : momentum.z() / pmag, 0., 1.);
        const double key = face == Library::inner ? p.z() : rho;
        const double energy = track->GetTotalEnergy();

        Library::Cell cell;
        if (s_.library->locate(face, energy, cosine, key, cell)) return reject("bins");
        double match = 0.;   // entry-point distance (nearest) or squared cell distance (cell)
        std::int64_t donor = -1;
        if (s_.nearest) {
          donor = s_.library->selectNearest(species, face, disk, cell[0], cell[1], key, phi, rho, s_.nearestK,
                                            G4UniformRand(), match);
          if (donor < 0) return reject("cell");
          if (match > s_.maxEntryDistance) return reject("distance");
        } else {
          int distance2 = 0;
          donor = s_.library->select(species, face, disk, cell, G4UniformRand(), s_.maxCellDistance2, distance2);
          if (donor < 0) return reject("cell");
          match = distance2;
        }
        if (s_.maxEnergyDeviation >= 0. &&
            std::abs(s_.library->entry(donor).energy - energy) > s_.maxEnergyDeviation * energy) return reject("energy");

        pending_ = Pending{donor, position, phi, track->GetGlobalTime(), face, disk, rho, p.z(), cosine, energy, match};
        return true;
      }

      void DoIt(const G4FastTrack& fastTrack, G4FastStep& fastStep) override {
        const auto& entry = s_.library->entry(pending_.donor);
        const double c = std::cos(pending_.azimuth), sn = std::sin(pending_.azimuth);
        const G4AffineTransform* toEnvelope = fastTrack.GetAffineTransformation();
        double donorE = 0., placedE = 0.;
        std::map<std::string, double> dropped;
        for (auto i = entry.stepBegin; i < entry.stepEnd; ++i) {
          const auto& step = s_.library->step(i);
          if (step.disk < 0 || step.disk >= static_cast<int>(s_.disks.size())) continue;
          // turned disk frame of the deposit's disk -> that disk's frame -> world
          const G4ThreeVector turned(c * step.x - sn * step.y, sn * step.x + c * step.y, step.z);
          const G4ThreeVector world = pending_.entry + s_.disks[step.disk].toWorldVector(turned);
          Mu2eG4FastHit hit;
          hit.localPosition = toEnvelope->TransformPoint(world);
          hit.edep = step.energyG4;
          hit.visibleEdep = step.energyBirks;
          hit.time = pending_.time + step.t;
          const bool recorded = hitMaker_.make(hit, fastTrack);
          if (s_.diagnostics && hit.edep > 0.) {
            donorE += hit.edep;
            if (recorded) placedE += hit.edep;
            else {
              const G4VPhysicalVolume* volume = hitMaker_.lastVolume();
              dropped[volume ? std::string(volume->GetName()) : std::string("none")] += hit.edep;
            }
          }
        }
        if (s_.diagnostics) {
          std::ostringstream lost;
          for (const auto& [volume, e] : dropped) lost << " " << volume << ":" << e;
          mf::LogInfo("Mu2eG4FastSimDiag") << "caloEntrant place face " << pending_.face << " disk " << pending_.disk
                                           << " rho " << pending_.rho << " z " << pending_.z << " cos " << pending_.cosine
                                           << " E " << pending_.energy << " donor " << pending_.donor << " donorE "
                                           << entry.energy << " match " << pending_.match << " turn " << std::remainder(pending_.azimuth - entry.azimuth, 2. * M_PI)
                                           << " depE " << donorE
                                           << " placedE " << placedE << " dropped" << lost.str();
        }
        // no energy on the fast step itself: the hit maker records it, and an envelope SD would count it twice
        fastStep.KillPrimaryTrack();
        fastStep.ProposePrimaryTrackPathLength(0.);
      }

    private:
      struct Pending {
        std::int64_t donor = -1;
        G4ThreeVector entry;
        double azimuth = 0., time = 0.;
        int face = -1, disk = -1;                                 // diagnostics only
        double rho = 0., z = 0., cosine = 0., energy = 0.;
        double match = 0.;
      };

      int nearestDisk(const G4ThreeVector& world) const {
        int best = 0;
        double d2 = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < s_.disks.size(); ++i) {
          const double z = s_.disks[i].toLocalPoint(world).z();
          if (z * z < d2) {
            d2 = z * z;
            best = static_cast<int>(i);
          }
        }
        return best;
      }

      const Settings& s_;
      Pending pending_;
      Mu2eG4FastHitMaker hitMaker_;
    };

  } // namespace

  class Mu2eG4FastSimCaloEntrantLibrary : public IMu2eG4FastSimModel {
  public:
    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;
      fhicl::Atom<std::string> libraryFile {Name("libraryFile"),
        Comment("ROOT export of a calofastsim.showers.entrants library: absolute path, or relative to FW_SEARCH_PATH")};
      fhicl::OptionalAtom<std::string> contentHash {Name("contentHash"),
        Comment("If given, the library's content_hash must equal it")};
      fhicl::Atom<double> minKineticEnergy {Name("minKineticEnergy"), Comment("Trigger threshold, MeV"), 0.};
      fhicl::Atom<std::string> donorSelection {Name("donorSelection"),
        Comment("nearest: one of the nearestK entrants of the query's species, face, disk, energy and cosine bins "
                "closest to the query entry point (counting the 180-degree lattice image), so a donor is turned by "
                "about 0 or 180 degrees and moved a few mm; cell: any entrant of the query's cell (the Python sampler)"),
        "nearest"};
      fhicl::Atom<unsigned> nearestK {Name("nearestK"), Comment("donorSelection nearest: candidates per query"), 1u};
      fhicl::Atom<double> maxEntryDistance {Name("maxEntryDistance"),
        Comment("donorSelection nearest: largest distance between the query and donor entry points, mm"), 20.};
      fhicl::Atom<int> maxCellDistance2 {Name("maxCellDistance2"),
        Comment("donorSelection cell: largest squared bin-index distance to a donor cell when the query's own cell is "
                "empty (0: own cell only; a large value reproduces the Python sampler's closest-cell fallback)"), 0};
      fhicl::Atom<double> maxEnergyDeviation {Name("maxEnergyDeviation"),
        Comment("Largest |E_donor - E| / E for a placed donor (energies are not rescaled); negative disables"), 0.1};
      fhicl::Atom<double> faceTolerance {Name("faceTolerance"),
        Comment("Smallest |cosine| between the envelope surface normal and the face normal to assign a face"), 0.9};
      fhicl::Atom<bool> diagnostics {Name("diagnostics"),
        Comment("Log each rejected entry and each placement (donor energy, recorded energy, dropped energy per volume)"), false};
    };
    using Parameters = art::ToolConfigTable<Config>;

    explicit Mu2eG4FastSimCaloEntrantLibrary(const Parameters& conf)
    {
      const std::string name = conf().libraryFile();
      const std::string path = name.empty() || name.front() == '/' ? name : ConfigFileLookupPolicy()(name);
      settings_.library = Library::load(path);
      std::string expected;
      if (conf().contentHash(expected) && expected != settings_.library->contentHash()) {
        throw cet::exception("CONFIG") << "Mu2eG4FastSimCaloEntrantLibrary: " << path << " has content hash "
                                       << settings_.library->contentHash() << ", expected " << expected << "\n";
      }
      settings_.minKineticEnergy = conf().minKineticEnergy();
      const std::string selection = conf().donorSelection();
      if (selection != "nearest" && selection != "cell") {
        throw cet::exception("CONFIG") << "Mu2eG4FastSimCaloEntrantLibrary: donorSelection must be nearest or cell, not "
                                       << selection << "\n";
      }
      settings_.nearest = selection == "nearest";
      settings_.nearestK = conf().nearestK();
      settings_.maxEntryDistance = conf().maxEntryDistance();
      settings_.maxCellDistance2 = conf().maxCellDistance2();
      settings_.maxEnergyDeviation = conf().maxEnergyDeviation();
      settings_.faceTolerance = conf().faceTolerance();
      settings_.diagnostics = conf().diagnostics();
      mf::LogInfo("Mu2eG4FastSim") << "Mu2eG4FastSimCaloEntrantLibrary: " << path << ", "
                                   << settings_.library->nEntries() << " entrants, " << settings_.library->nSteps()
                                   << " steps, content hash " << settings_.library->contentHash();
    }

    G4VFastSimulationModel* construct(const std::string& name, G4Region* envelope) override {
      if (settings_.disks.empty()) {
        // disk frames from the geometry service, in Geant4 world coordinates (the library's frame convention)
        const DiskCalorimeter& cal = *GeomHandle<DiskCalorimeter>();
        const CLHEP::Hep3Vector offset = GeomHandle<WorldG4>()->mu2eOriginInWorld();
        for (unsigned d = 0; d < cal.nDisks(); ++d) {
          const CLHEP::Hep3Vector origin = cal.diskToMu2e(d, CLHEP::Hep3Vector(0, 0, 0));
          settings_.disks.push_back({origin + offset,
                                     (cal.diskToMu2e(d, CLHEP::Hep3Vector(1, 0, 0)) - origin).unit(),
                                     (cal.diskToMu2e(d, CLHEP::Hep3Vector(0, 1, 0)) - origin).unit(),
                                     (cal.diskToMu2e(d, CLHEP::Hep3Vector(0, 0, 1)) - origin).unit()});
        }
        if (settings_.disks.empty()) {
          throw cet::exception("CONFIG") << "Mu2eG4FastSimCaloEntrantLibrary: the geometry has no calorimeter disks\n";
        }
      }
      models_.push_back(std::make_unique<CaloEntrantLibraryModel>(name, envelope, settings_));
      return models_.back().get();
    }

  private:
    Settings settings_;
    std::vector<std::unique_ptr<CaloEntrantLibraryModel>> models_;
  };

} // namespace mu2e

DEFINE_ART_CLASS_TOOL(mu2e::Mu2eG4FastSimCaloEntrantLibrary)
