// Isolated source-faithful CRV module for Mu2eStudyWorld, not a full-detector partition.
#include "Offline/ConfigTools/inc/SimpleConfig.hh"
#include "Offline/CosmicRayShieldGeom/inc/CosmicRayShield.hh"
#include "Offline/GeometryService/inc/CosmicRayShieldMaker.hh"
#include "Offline/Mu2eG4/inc/InitEnvToolBase.hh"
#include "Offline/Mu2eG4/inc/checkForOverlaps.hh"
#include "Offline/Mu2eG4/inc/constructCRVElectronicsMaterials.hh"
#include "Offline/Mu2eG4/inc/findMaterialOrThrow.hh"
#include "Offline/Mu2eG4Helper/inc/Mu2eG4Helper.hh"
#include "Offline/Mu2eG4Helper/inc/VolumeInfo.hh"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Utilities/ToolMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "Geant4/G4Box.hh"
#include "Geant4/G4GeometryTolerance.hh"
#include "Geant4/G4LogicalVolume.hh"
#include "Geant4/G4Material.hh"
#include "Geant4/G4PVPlacement.hh"
#include "Geant4/G4RotationMatrix.hh"
#include "Geant4/G4ThreeVector.hh"
#include "Geant4/G4ios.hh"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip>
#include <string>
#include <vector>

namespace mu2e {
namespace {
  using Vec = G4ThreeVector;
  using Half = std::vector<double>;

  void validateBox(Vec const& p, Half const& h) {
    if (h.size() != 3) throw cet::exception("GEOM") << "CRV box needs three half lengths\n";
    for (unsigned i = 0; i < 3; ++i)
      if (!std::isfinite(p[i]) || !std::isfinite(h[i]) || h[i] <= 0. ||
          !std::isfinite(p[i]-h[i]) || !std::isfinite(p[i]+h[i]))
        throw cet::exception("GEOM") << "Nonfinite or nonpositive CRV box extent\n";
  }

  void contains(Vec const& p, Half const& h, Vec const& child, Half const& ch, double tol) {
    validateBox(child, ch);
    for (unsigned i = 0; i < 3; ++i)
      if (std::abs(child[i]-p[i]) + ch[i] > h[i] + tol)
        throw cet::exception("GEOM") << "CRV bar/CMB extends outside its source layer\n";
  }

  G4LogicalVolume* box(std::string const& name, Half const& h, G4Material* material) {
    return new G4LogicalVolume(new G4Box(name, h[0], h[1], h[2]), material, name);
  }
}

class ConstructCaptureCRVModule : public InitEnvToolBase {
public:
  explicit ConstructCaptureCRVModule(fhicl::ParameterSet const&) {}
  int construct(VolumeInfo const& parent, SimpleConfig const& config) override;
};

int ConstructCaptureCRVModule::construct(VolumeInfo const& parent, SimpleConfig const& config) {
  if (!config.getBool("hasCosmicRayShield", false) || !config.getBool("g4.doSurfaceCheck"))
    throw cet::exception("GEOM") << "Capture CRV requires hasCosmicRayShield and g4.doSurfaceCheck=true\n";
  auto const* parentBox = dynamic_cast<G4Box const*>(parent.solid);
  if (parent.name != "BoxInTheWorld" || parent.centerInWorld.mag2() != 0. ||
      !parentBox || !parent.logical || !parent.physical ||
      config.getString("world.materialName") != "G4_Galactic" ||
      parent.logical->GetMaterial()->GetName() != "G4_Galactic" ||
      parent.physical->GetTranslation().mag2() != 0. ||
      parent.physical->GetObjectRotationValue() != G4RotationMatrix())
    throw cet::exception("GEOM") << "Capture CRV requires the identity vacuum Mu2eStudyWorld parent\n";

  std::string const sector = config.getString("captureCRV.sector");
  int const moduleNumber = config.getInt("captureCRV.module");
  if (sector.empty() || moduleNumber < 0)
    throw cet::exception("GEOM") << "Invalid captureCRV sector/module selection\n";
  Half translation, matrix;
  config.getVectorDouble("captureCRV.translation", translation, {0., 0., 0.}, 3);
  config.getVectorDouble("captureCRV.localToWorld", matrix, {1., 0., 0., 0., 1., 0., 0., 0., 1.}, 9);
  for (double v : translation)
    if (!std::isfinite(v)) throw cet::exception("GEOM") << "Nonfinite captureCRV.translation\n";
  for (double v : matrix)
    if (!std::isfinite(v)) throw cet::exception("GEOM") << "Nonfinite captureCRV.localToWorld\n";
  Vec const x(matrix[0], matrix[3], matrix[6]), y(matrix[1], matrix[4], matrix[7]);
  Vec const z(matrix[2], matrix[5], matrix[8]);
  constexpr double rotationTolerance = 1.e-10;
  if (std::abs(x.mag2()-1.) > rotationTolerance || std::abs(y.mag2()-1.) > rotationTolerance ||
      std::abs(z.mag2()-1.) > rotationTolerance || std::abs(x.dot(y)) > rotationTolerance ||
      std::abs(x.dot(z)) > rotationTolerance || std::abs(y.dot(z)) > rotationTolerance ||
      std::abs(x.dot(y.cross(z))-1.) > rotationTolerance)
    throw cet::exception("GEOM") << "captureCRV.localToWorld must be a proper orthonormal rotation\n";
  double const tolerance = G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
  double const clearance = config.getDouble("captureCRV.clearance", 0.001);
  if (!std::isfinite(clearance) || clearance < 10.*tolerance)
    throw cet::exception("GEOM") << "captureCRV.clearance must be finite and >=10 surface tolerances\n";

  // Build all source data without altering source geometry or consulting study-mode GeomHandles.
  CosmicRayShieldMaker maker(config, 0.);
  auto const geometry = maker.getCosmicRayShieldPtr();
  CRSScintillatorShield const* shield = nullptr;
  for (auto const& candidate : geometry->getCRSScintillatorShields()) {
    if (candidate.getName() != sector) continue;
    if (shield) throw cet::exception("GEOM") << "Ambiguous CRV sector " << sector << '\n';
    shield = &candidate;
  }
  if (!shield || static_cast<std::size_t>(moduleNumber) >= shield->getCRSScintillatorModules().size())
    throw cet::exception("GEOM") << "No CRV module " << sector << '/' << moduleNumber << '\n';
  auto const& module = shield->getModule(moduleNumber);
  auto const& layers = module.getLayers();
  if (layers.size() < 2 || config.getBool("crs.countersOnly"+sector.substr(4), false) ||
      module.getAbsorberLayers().size()+1 != layers.size() || module.getAluminumSheets().size() != 2)
    throw cet::exception("GEOM") << "Capture CRV requires a multilayer module with absorbers and two sheets\n";
  bool const hideCMBs = config.getBool("crs.hideCRVCMBs");
  bool const forMARS = config.getBool("crs.forMARS", false);
  G4cout << "CaptureCRV source=" << geometry->getName() << " sector=" << sector
         << " module=" << moduleNumber << " crs.hideCRVCMBs=" << hideCMBs
         << " (source omission preserved; layer bounds unchanged)" << G4endl;

  Vec low, high;
  for (unsigned i = 0; i < 3; ++i) {
    low[i] = std::numeric_limits<double>::infinity();
    high[i] = -low[i];
  }
  auto bound = [&](auto const& item) {
    auto const& p = item.getPosition();
    auto const& h = item.getHalfLengths();
    validateBox(p, h);
    for (unsigned i = 0; i < 3; ++i) {
      low[i] = std::min(low[i], p[i]-h[i]);
      high[i] = std::max(high[i], p[i]+h[i]);
    }
  };
  std::size_t nBars = 0, nCMBs = 0, omittedCMBs = 0;
  for (auto const& layer : layers) {
    bound(layer);
    if (layer.getBars().empty()) throw cet::exception("GEOM") << "Empty CRV layer\n";
    for (auto const& ptr : layer.getBars()) {
      if (!ptr) throw cet::exception("GEOM") << "Null CRV bar\n";
      auto const& bar = *ptr;
      int const index = bar.index().asInt();
      if (index < 0 || index > (std::numeric_limits<int>::max()-1)/2)
        throw cet::exception("GEOM") << "Invalid CRV bar/CMB copy index\n";
      contains(layer.getPosition(), layer.getHalfLengths(), bar.getPosition(), bar.getHalfLengths(), tolerance);
      ++nBars;
      for (int side = 0; side < 2; ++side) if (bar.hasCMB(side)) {
        if (hideCMBs) { ++omittedCMBs; continue; }
        contains(layer.getPosition(), layer.getHalfLengths(), bar.getCMBPosition(side), bar.getCMBHalfLengths(), tolerance);
        ++nCMBs;
      }
    }
  }
  for (auto const& item : module.getAbsorberLayers()) bound(item);
  for (auto const& item : module.getAluminumSheets()) bound(item);
  for (auto const& item : module.getFEBs()) bound(item);
  Vec const origin = 0.5*low + 0.5*high;
  Half half(3);
  for (unsigned i = 0; i < 3; ++i) half[i] = 0.5*high[i]-0.5*low[i]+clearance;
  validateBox(origin, half);

  // The entire cleared envelope in the source frame must exclude every global shared support.
  for (auto const& support : geometry->getSupportStructures()) {
    auto const& p = support.getPosition();
    auto const& h = support.getHalfLengths();
    validateBox(p, h);
    bool overlap = true;
    for (unsigned i = 0; i < 3; ++i)
      overlap = overlap && origin[i]-half[i] < p[i]+h[i] && p[i]-h[i] < origin[i]+half[i];
    if (overlap) throw cet::exception("GEOM") << "CaptureEnvelope intersects global shared support "
                                            << support.getName() << "; ownership is undefined\n";
  }
  double const worldHalf = config.getDouble("world.halfLength");
  Half const parentHalf{parentBox->GetXHalfLength(), parentBox->GetYHalfLength(), parentBox->GetZHalfLength()};
  if (!std::isfinite(worldHalf) || worldHalf <= 0.)
    throw cet::exception("GEOM") << "Invalid world.halfLength\n";
  for (unsigned row = 0; row < 3; ++row) {
    double extent = std::abs(translation[row]);
    for (unsigned col = 0; col < 3; ++col) extent += std::abs(matrix[3*row+col])*half[col];
    if (!std::isfinite(extent) || extent+10.*tolerance >= std::min(worldHalf, parentHalf[row]))
      throw cet::exception("GEOM") << "CaptureEnvelope must fit strictly inside the vacuum study parent\n";
  }

  mu2e::constructCRVElectronicsMaterials();
  auto* air = findMaterialOrThrow(config.getString("hall.insideMaterialName"));
  auto& helper = *art::ServiceHandle<Mu2eG4Helper>();
  // x_world = R*(x_source-origin)+translation; only the envelope receives the passive inverse.
  G4RotationMatrix const localToWorld(x, y, z);
  auto* inverse = helper.antiLeakRegistry().add(localToWorld.inverse());
  auto place = [&](std::string const& name, G4LogicalVolume* lv, Vec const& offset,
                   G4LogicalVolume* mother, int copy, G4RotationMatrix* rotation = nullptr) {
    auto* pv = new G4PVPlacement(rotation, offset, lv, name, mother, false, copy, false);
    if (checkForOverlaps(pv, config)) throw cet::exception("GEOM") << "CRV overlap at " << name << '\n';
    auto const* solid = dynamic_cast<G4Box const*>(lv->GetSolid());
    auto const precision = G4cout.precision();
    G4cout << std::setprecision(17) << "CaptureCRV volume=" << name << " mother=" << mother->GetName()
           << " copy=" << copy << " offset=" << offset << " half="
           << Vec(solid->GetXHalfLength(), solid->GetYHalfLength(), solid->GetZHalfLength())
           << " material=" << lv->GetMaterial()->GetName() << G4endl;
    G4cout.precision(precision);
    return pv;
  };
  Vec const displacement(translation[0], translation[1], translation[2]);
  auto* envelope = box("CaptureEnvelope", half, air);
  auto* envelopePV = place("CaptureEnvelope", envelope, displacement, parent.logical, 2, inverse);
  VolumeInfo info("CaptureEnvelope", displacement, parent.centerInWorld);
  info.solid = envelope->GetSolid(); info.logical = envelope; info.physical = envelopePV;
  helper.addVolInfo(info);

  auto const& detail = shield->getCRSScintillatorBarDetail();
  auto* barLogical = box("CRSscintBar_"+sector, detail.getHalfLengths(), findMaterialOrThrow(detail.getMaterialName()));
  G4LogicalVolume* cmbLogical = nullptr;
  if (nCMBs) cmbLogical = box("CRSCMB_"+sector, detail.getCMBHalfLengths(), findMaterialOrThrow(detail.getCMBMaterialName()));
  std::size_t localID = 0;
  for (auto const& layer : layers) {
    auto const name = layer.name("CRSscintLayer_");
    auto* layerLogical = box(name, layer.getHalfLengths(), air);
    place(name, layerLogical, layer.getPosition()-origin, envelope, 0);
    for (auto const& ptr : layer.getBars()) {
      auto const& bar = *ptr;
      place(bar.name("CRSScintillatorBar_"), barLogical, bar.getPosition()-layer.getPosition(), layerLogical, bar.index().asInt());
      for (int side = 0; side < 2; ++side) if (!hideCMBs && bar.hasCMB(side)) {
        auto const cmbName = bar.name(side == 0 ? "CRSCMB0_" : "CRSCMB1_");
        auto* lv = forMARS ? new G4LogicalVolume(cmbLogical->GetSolid(), cmbLogical->GetMaterial(), cmbName) : cmbLogical;
        place(cmbName, lv, bar.getCMBPosition(side)-layer.getPosition(), layerLogical, 2*bar.index().asInt()+side);
      }
      G4cout << "CaptureCRV barLocalID=" << localID++ << " sector=" << sector
             << " shield=" << bar.id().getShieldNumber() << " module=" << bar.id().getModuleNumber()
             << " layer=" << bar.id().getLayerNumber() << " bar=" << bar.id().getBarNumber()
             << " sourceCopy=" << bar.index().asInt() << " name=" << bar.name("CRSScintillatorBar_") << G4endl;
    }
  }
  auto placeParts = [&](auto const& parts, std::string const& base, std::string const& material) {
    auto* mat = findMaterialOrThrow(material);
    for (std::size_t i = 0; i < parts.size(); ++i) {
      auto const name = module.name(base)+"_"+std::to_string(i);
      auto* lv = box(name, parts[i].getHalfLengths(), mat);
      place(name, lv, parts[i].getPosition()-origin, envelope, 0);
    }
  };
  placeParts(module.getAbsorberLayers(), "CRSAbsorber_", shield->getAbsorberMaterialName());
  // Preserve the pinned aluminum-sheet accessor, including its absorber-material alias.
  placeParts(module.getAluminumSheets(), "CRSAluminumSheet_", shield->getAluminumSheetMaterialName());
  placeParts(module.getFEBs(), "CRSFEB_", shield->getFEBMaterialName());
  G4cout << "CaptureCRV inventory sector=" << sector << " module=" << moduleNumber
         << " layers=" << layers.size() << " bars=" << nBars << " CMBs=" << nCMBs << " omittedCMBs=" << omittedCMBs
         << " absorbers=" << module.getAbsorberLayers().size() << " aluminumSheets=" << module.getAluminumSheets().size()
         << " FEBs=" << module.getFEBs().size() << " sharedSupportsChecked=" << geometry->getSupportStructures().size()
         << " sourceMin=" << low << " sourceMax=" << high << " sourceOrigin=" << origin
         << " envelopeHalf=" << Vec(half[0], half[1], half[2]) << " clearance=" << clearance
         << " fill=" << air->GetName() << G4endl;
  return 0;
}
}

DEFINE_ART_CLASS_TOOL(mu2e::ConstructCaptureCRVModule)
