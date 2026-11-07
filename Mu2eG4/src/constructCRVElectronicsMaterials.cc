#include "Offline/Mu2eG4/inc/constructCRVElectronicsMaterials.hh"
#include "Offline/Mu2eG4/inc/findMaterialOrThrow.hh"
#include "CLHEP/Units/SystemOfUnits.h"
#include "Geant4/G4Material.hh"
#include "Geant4/G4NistManager.hh"
#include "cetlib_except/exception.h"
#include <initializer_list>

namespace mu2e {
  namespace {
    void requireNew(const G4String& name) {
      if (G4Material::GetMaterial(name, false))
        throw cet::exception("GEOM") << "Material already defined: " << name;
    }
    G4Element* element(const G4String& name) {
      auto* result = G4NistManager::Instance()->FindOrBuildElement(name, true);
      if (!result) throw cet::exception("GEOM") << "Could not load G4 element: " << name;
      return result;
    }
  }

  void constructCRVPolyethylene() {
    requireNew("Polyethylene");
    auto* material = new G4Material("Polyethylene", 0.956*CLHEP::g/CLHEP::cm3, 2);
    material->AddElement(element("C"), 1);
    material->AddElement(element("H"), 2);
  }

  void constructCRVElectronicsMaterials(bool includePolyethylene) {
    if (includePolyethylene) constructCRVPolyethylene();
    requireNew("Electronics");
    auto* material = new G4Material("Electronics", 0.58*CLHEP::g/CLHEP::cm3, 5);
    material->AddElement(element("Si"), 0.103);
    material->AddElement(element("Al"), 0.034);
    material->AddElement(element("Au"), 0.034);
    material->AddElement(element("Cu"), 0.691);
    material->AddMaterial(findMaterialOrThrow("Polyethylene"), 0.138);
    for (const auto* name : {"ElectronicsFEB", "ElectronicsCMB"}) {
      requireNew(name);
      auto* alias = new G4Material(name, 0.58*CLHEP::g/CLHEP::cm3, 1);
      alias->AddMaterial(findMaterialOrThrow("Electronics"), 1.0);
    }
  }
}
