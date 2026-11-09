// Constructed end-kinematics contracts only; no engine, physical transport or response-bank donors.
#include "Offline/GeneralUtilities/inc/sqrtOrThrow.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4UserHelpers.hh"
#include "Offline/Mu2eG4/inc/Mu2eG4UserTrackInformation.hh"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "CLHEP/Vector/LorentzVector.h"
#include "Geant4/G4DynamicParticle.hh"
#include "Geant4/G4Electron.hh"
#include "Geant4/G4Gamma.hh"
#include "Geant4/G4ParticleDefinition.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4Track.hh"
#include "TTree.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

namespace mu2e {
namespace {
constexpr std::size_t caseCount = 16;
enum class Info { absent, defaults, recorded };
struct Case {
  char const* name;
  bool electron;
  double rawKE;
  Info info;
  double recordedKE;
  bool stepPresent;
  int nSteps;
  double trackLength;
  double massOverride;
};

std::array<Case, caseCount> const cases{{
  {"zero_gamma_default_step",         false, 10., Info::defaults,  0., true,  0, 0., 0.},
  {"zero_gamma_absent_no_step",       false, 10., Info::absent,    0., false, 0, 0., 0.},
  {"zero_gamma_stale_no_step",        false, 10., Info::recorded,  9., false, 0, 0., 0.},
  {"zero_electron_absent_step",       true,  10., Info::absent,    0., true,  0, 0., 0.},
  {"zero_electron_default_no_step",   true,  10., Info::defaults,  0., false, 0, 0., 0.},
  {"zero_electron_stale_step",        true,  10., Info::recorded,  9., true,  0, 0., 0.},
  {"zero_electron_at_rest_step",      true,   0., Info::defaults,  0., true,  0, 0., 0.},
  {"zero_electron_dynamic_mass",      true,  10., Info::recorded,  9., false, 0, 0., 2.5},
  {"one_gamma_raw_zero_recorded_7",   false,  0., Info::recorded,  7., true,  1, 0., 0.},
  {"one_electron_raw_zero_recorded_7",true,   0., Info::recorded,  7., true,  1, 0., 0.},
  {"multi_gamma_raw_3_recorded_7",    false,  3., Info::recorded,  7., true,  3, 4., 0.},
  {"multi_electron_raw_3_recorded_7", true,   3., Info::recorded,  7., true,  4, 5., 0.},
  {"one_gamma_default_info",         false,  5., Info::defaults,  0., true,  1, 0., 0.},
  {"one_electron_default_info",      true,   5., Info::defaults,  0., true,  1, 0., 0.},
  {"multi_electron_recorded_zero",   true,   5., Info::recorded,  0., true,  2, 2., 0.},
  {"multi_electron_dynamic_mass",    true,   3., Info::recorded,  7., true,  2, 2., 2.5}
}};

void check(bool value, std::string const& message) {
  if (!value) throw cet::exception("ComponentEndKinematicsFixture") << message << '\n';
}

double missing() { return std::numeric_limits<double>::quiet_NaN(); }
struct Kinematics {
  double ke{missing()}, px{missing()}, py{missing()}, pz{missing()}, energy{missing()};
};
Kinematics state(double ke, CLHEP::HepLorentzVector const& p) {
  return {ke, p.px(), p.py(), p.pz(), p.e()};
}
bool exactDouble(double a, double b) {
  return std::isfinite(a) && std::isfinite(b) && std::memcmp(&a, &b, sizeof(double)) == 0;
}
bool exact(Kinematics const& a, Kinematics const& b) {
  return exactDouble(a.ke, b.ke) && exactDouble(a.px, b.px) && exactDouble(a.py, b.py)
    && exactDouble(a.pz, b.pz) && exactDouble(a.energy, b.energy);
}

// Preserve the legacy operation order, epsilon, sqrtOrThrow and definition mass exactly.
CLHEP::HepLorentzVector legacyMomentum(G4Track const& track, Mu2eG4UserTrackInformation const& info) {
  auto const& pdir = info.GetMomentumDirection();
  double ke = info.GetKineticEnergy();
  double mass = track.GetParticleDefinition()->GetPDGMass();
  double e = 0, p = 0;
  if (mass > std::numeric_limits<double>::epsilon()) {
    e = ke + mass;
    p = sqrtOrThrow<double>(e*e-mass*mass, std::numeric_limits<double>::epsilon());
  } else {
    e = ke;
    p = ke;
  }
  return CLHEP::HepLorentzVector(p*pdir, e);
}

struct OwnedObjects {
  std::unique_ptr<G4Step> step;
  std::unique_ptr<G4Track> track;
  ~OwnedObjects() {
    // G4Track owns its dynamic particle and user information, but not its step.
    if (track) track->SetStep(nullptr);
    if (step) step->SetTrack(nullptr);
  }
};

struct Row {
  std::int64_t run{}, subrun{}, event{};
  int caseIndex{}, pdg{}, nSteps{};
  std::string caseName, infoKind, error;
  bool diagnosticOnly{true}, bankAdmissible{false}, realEngine{false};
  bool stepPresent{}, infoPresent{}, helperKEPresent{}, helperMomentumPresent{}, pass{};
  double dynamicMass{missing()}, definitionMass{missing()}, trackLength{missing()};
  double rawDirX{missing()}, rawDirY{missing()}, rawDirZ{missing()};
  double infoKE{missing()}, infoDirX{missing()}, infoDirY{missing()}, infoDirZ{missing()};
  Kinematics raw, actual, expected;
};

void branch(TTree* tree, std::string const& name, std::int64_t& value) {
  static_assert(sizeof(std::int64_t) == sizeof(Long64_t), "64-bit ROOT identity required");
  tree->Branch(name.c_str(), &value, (name + "/L").c_str());
}
void branch(TTree* tree, std::string const& name, int& value) {
  tree->Branch(name.c_str(), &value, (name + "/I").c_str());
}
void branch(TTree* tree, std::string const& name, bool& value) {
  tree->Branch(name.c_str(), &value, (name + "/O").c_str());
}
void branch(TTree* tree, std::string const& name, double& value) {
  tree->Branch(name.c_str(), &value, (name + "/D").c_str());
}
void branch(TTree* tree, std::string const& name, std::string& value) {
  tree->Branch(name.c_str(), &value);
}
void stateBranches(TTree* tree, std::string const& prefix, Kinematics& value) {
  branch(tree, prefix + "KE", value.ke);
  branch(tree, prefix + "Px", value.px);
  branch(tree, prefix + "Py", value.py);
  branch(tree, prefix + "Pz", value.pz);
  branch(tree, prefix + "E", value.energy);
}
std::string describe(Kinematics const& value) {
  std::ostringstream text;
  text << std::setprecision(17) << "KE=" << value.ke << " p=(" << value.px << ','
       << value.py << ',' << value.pz << ") E=" << value.energy;
  return text.str();
}
} // namespace

class ComponentEndKinematicsFixture : public art::EDAnalyzer {
 public:
  struct Config {
    fhicl::Atom<bool> diagnosticOnly{fhicl::Name("diagnosticOnly")};
    fhicl::Atom<unsigned> expectedRun{fhicl::Name("expectedRun"), 16u};
  };
  using Parameters = art::EDAnalyzer::Table<Config>;
  explicit ComponentEndKinematicsFixture(Parameters const& p)
    : art::EDAnalyzer(p), expectedRun_(p().expectedRun()) {
    check(p().diagnosticOnly(), "diagnosticOnly must be true; this is not transport");
    check(expectedRun_ == 16u, "The fixed fixture requires expectedRun=16");
  }
  void beginJob() override;
  void analyze(art::Event const&) override;
  void endJob() override;

 private:
  void runCase(std::size_t index, art::Event const& event);
  unsigned const expectedRun_;
  unsigned seen_{};
  std::array<unsigned, caseCount> census_{};
  TTree* tree_{};
  Row row_;
};

void ComponentEndKinematicsFixture::beginJob() {
  art::ServiceHandle<art::TFileService> fs;
  tree_ = fs->make<TTree>("endKinematics", "Constructed helper contracts; diagnostic only, no transport");
  branch(tree_, "run", row_.run);
  branch(tree_, "subrun", row_.subrun);
  branch(tree_, "event", row_.event);
  branch(tree_, "caseIndex", row_.caseIndex);
  branch(tree_, "caseName", row_.caseName);
  branch(tree_, "infoKind", row_.infoKind);
  branch(tree_, "diagnosticOnly", row_.diagnosticOnly);
  branch(tree_, "bankAdmissible", row_.bankAdmissible);
  branch(tree_, "realEngine", row_.realEngine);
  branch(tree_, "pdg", row_.pdg);
  branch(tree_, "nSteps", row_.nSteps);
  branch(tree_, "stepPresent", row_.stepPresent);
  branch(tree_, "infoPresent", row_.infoPresent);
  branch(tree_, "dynamicMass", row_.dynamicMass);
  branch(tree_, "definitionMass", row_.definitionMass);
  branch(tree_, "trackLength", row_.trackLength);
  branch(tree_, "rawDirX", row_.rawDirX);
  branch(tree_, "rawDirY", row_.rawDirY);
  branch(tree_, "rawDirZ", row_.rawDirZ);
  branch(tree_, "infoKE", row_.infoKE);
  branch(tree_, "infoDirX", row_.infoDirX);
  branch(tree_, "infoDirY", row_.infoDirY);
  branch(tree_, "infoDirZ", row_.infoDirZ);
  branch(tree_, "helperKEPresent", row_.helperKEPresent);
  branch(tree_, "helperMomentumPresent", row_.helperMomentumPresent);
  stateBranches(tree_, "raw", row_.raw);
  stateBranches(tree_, "actual", row_.actual);
  stateBranches(tree_, "expected", row_.expected);
  branch(tree_, "pass", row_.pass);
  branch(tree_, "error", row_.error);
}

void ComponentEndKinematicsFixture::runCase(std::size_t index, art::Event const& event) {
  auto const& spec = cases.at(index);
  // Assignment resets values without changing the addresses used by ROOT branches.
  row_ = Row{};
  row_.run = event.id().run();
  row_.subrun = event.id().subRun();
  row_.event = event.id().event();
  row_.caseIndex = static_cast<int>(index + 1);
  row_.caseName = spec.name;
  row_.infoKind = spec.info == Info::absent ? "absent" : spec.info == Info::defaults ? "default" : "recorded";
  try {
    check(census_.at(index) == 0, "Duplicate fixed case");
    G4ParticleDefinition* definition = nullptr;
    if (spec.electron) definition = G4Electron::ElectronDefinition();
    else definition = G4Gamma::GammaDefinition();
    auto dynamic = std::make_unique<G4DynamicParticle>(definition, G4ThreeVector(0., 0., 1.), spec.rawKE);
    if (spec.massOverride > 0.) dynamic->SetMass(spec.massOverride);
    OwnedObjects owned;
    owned.track = std::make_unique<G4Track>(dynamic.get(), 0., G4ThreeVector{});
    dynamic.release();
    auto& track = *owned.track;
    Mu2eG4UserTrackInformation* info = nullptr;
    if (spec.info != Info::absent) {
      auto constructedInfo = std::make_unique<Mu2eG4UserTrackInformation>();
      if (spec.info == Info::recorded) {
        constructedInfo->SetKineticEnergy(spec.recordedKE);
        constructedInfo->SetMomentumDirection(G4ThreeVector(0.6, 0.8, 0.));
      }
      info = constructedInfo.get();
      track.SetUserInformation(info);
      constructedInfo.release();
    }
    if (spec.stepPresent) {
      owned.step = std::make_unique<G4Step>();
      owned.step->SetTrack(&track);
      track.SetStep(owned.step.get());
    }
    for (int i = 0; i < spec.nSteps; ++i) track.IncrementCurrentStepNumber();
    track.AddTrackLength(spec.trackLength);

    row_.pdg = track.GetParticleDefinition()->GetPDGEncoding();
    row_.nSteps = track.GetCurrentStepNumber();
    row_.stepPresent = track.GetStep() != nullptr;
    row_.infoPresent = track.GetUserInformation() != nullptr;
    row_.dynamicMass = track.GetDynamicParticle()->GetMass();
    row_.definitionMass = track.GetParticleDefinition()->GetPDGMass();
    row_.trackLength = track.GetTrackLength();
    row_.raw = state(track.GetKineticEnergy(), CLHEP::HepLorentzVector(track.GetMomentum(), track.GetTotalEnergy()));
    row_.rawDirX = track.GetMomentumDirection().x();
    row_.rawDirY = track.GetMomentumDirection().y();
    row_.rawDirZ = track.GetMomentumDirection().z();
    if (info) {
      row_.infoKE = info->GetKineticEnergy();
      row_.infoDirX = info->GetMomentumDirection().x();
      row_.infoDirY = info->GetMomentumDirection().y();
      row_.infoDirZ = info->GetMomentumDirection().z();
    }
    check(row_.pdg == (spec.electron ? 11 : 22) && row_.raw.ke == spec.rawKE,
          "Constructed particle identity or raw KE differs from fixed case");
    check(row_.nSteps == spec.nSteps && row_.trackLength == spec.trackLength,
          "Constructed actual step count/track length differs from fixed case");
    check(row_.stepPresent == spec.stepPresent && track.GetStep() == owned.step.get(), "Step attachment differs");
    check(!owned.step || owned.step->GetTrack() == &track, "Step back-reference differs");
    check(row_.infoPresent == (spec.info != Info::absent) && track.GetUserInformation() == info,
          "User-information attachment differs");
    check(row_.rawDirX == 0. && row_.rawDirY == 0. && row_.rawDirZ == 1., "Raw direction differs");
    check(row_.dynamicMass == (spec.massOverride > 0. ? spec.massOverride : row_.definitionMass),
          "Constructed dynamic mass differs");
    if (spec.massOverride > 0.) check(row_.dynamicMass != row_.definitionMass, "Mass override is not distinct");
    if (info) {
      check(row_.infoKE == spec.recordedKE, "Recorded/default KE differs");
      auto const expectedDirection = spec.info == Info::recorded ? G4ThreeVector(0.6, 0.8, 0.) : G4ThreeVector{};
      check(info->GetMomentumDirection() == expectedDirection, "Recorded/default direction differs");
    }
    if (row_.nSteps == 0) {
      row_.expected = row_.raw;
    } else {
      check(info != nullptr, "Stepped cases require Mu2e user information");
      row_.expected = state(info->GetKineticEnergy(), legacyMomentum(track, *info));
    }
    row_.actual.ke = Mu2eG4UserHelpers::getEndKE(&track);
    row_.helperKEPresent = true;
    auto const momentum = Mu2eG4UserHelpers::getEndMomentum(&track);
    row_.actual = state(row_.actual.ke, momentum);
    row_.helperMomentumPresent = true;
    row_.pass = exact(row_.actual, row_.expected);
    if (!row_.pass) row_.error = "Exact end-KE/four-momentum comparison failed";
  } catch (std::exception const& ex) {
    row_.pass = false;
    row_.error = ex.what();
  } catch (...) {
    row_.pass = false;
    row_.error = "Non-standard exception while constructing/checking helper case";
  }
  // Checkpoint the failing case before throwing; this is not crash-durability protection.
  auto const filled = tree_->Fill();
  auto const saved = tree_->AutoSave("SaveSelf");
  check(filled >= 0 && saved > 0, "ROOT Fill/AutoSave failed at case " + row_.caseName);
  if (!row_.pass) {
    throw cet::exception("ComponentEndKinematicsFixture")
      << std::setprecision(17) << "Case " << row_.caseIndex << '/' << caseCount << " " << row_.caseName
      << " at " << row_.run << ':' << row_.subrun << ':' << row_.event << ": " << row_.error
      << "\npdg=" << row_.pdg << " nSteps=" << row_.nSteps << " stepPresent=" << row_.stepPresent
      << " infoPresent=" << row_.infoPresent << " trackLength=" << row_.trackLength
      << " dynamicMass=" << row_.dynamicMass << " definitionMass=" << row_.definitionMass
      << " infoKE=" << row_.infoKE << " infoDirection=(" << row_.infoDirX << ',' << row_.infoDirY << ',' << row_.infoDirZ
      << ")\nraw: " << describe(row_.raw) << "\nactual: " << describe(row_.actual)
      << "\nexpected: " << describe(row_.expected) << '\n';
  }
  ++census_.at(index);
}

void ComponentEndKinematicsFixture::analyze(art::Event const& event) {
  check(seen_ == 0 && event.id().run() == expectedRun_ && event.id().subRun() == 0 && event.id().event() == 1,
        "Exactly one source event, run 16 subrun 0 event 1, is required; received " + std::to_string(event.id().run())
        + ":" + std::to_string(event.id().subRun()) + ":" + std::to_string(event.id().event())
        + " after " + std::to_string(seen_) + " event(s)");
  ++seen_;
  for (std::size_t i = 0; i < caseCount; ++i) runCase(i, event);
}

void ComponentEndKinematicsFixture::endJob() {
  check(seen_ == 1, "endJob requires exactly one source event; saw " + std::to_string(seen_));
  for (std::size_t i = 0; i < caseCount; ++i) {
    check(census_[i] == 1, "endJob requires exactly one passing case " + std::to_string(i + 1)
          + " " + cases[i].name + "; saw " + std::to_string(census_[i]));
  }
  check(tree_ && tree_->GetEntries() == static_cast<Long64_t>(caseCount), "endJob requires exactly 16 diagnostic rows");
}
} // namespace mu2e
DEFINE_ART_MODULE(mu2e::ComponentEndKinematicsFixture)
