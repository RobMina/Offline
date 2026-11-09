// Finite smoke-test component for the pinned Mu2eStudyWorld, not a CRV module.
#include "Offline/ConfigTools/inc/SimpleConfig.hh"
#include "Offline/Mu2eG4/inc/InitEnvToolBase.hh"
#include "Offline/Mu2eG4/inc/findMaterialOrThrow.hh"
#include "Offline/Mu2eG4/inc/nestBox.hh"
#include "Offline/Mu2eG4Helper/inc/Mu2eG4Helper.hh"
#include "Offline/Mu2eG4Helper/inc/VolumeInfo.hh"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Utilities/ToolMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "Geant4/G4GeometryTolerance.hh"
#include "Geant4/G4RotationMatrix.hh"
#include "Geant4/G4String.hh"
#include "Geant4/G4ThreeVector.hh"
#include <cmath>
#include <vector>

namespace mu2e {

  class ConstructCaptureBox : public InitEnvToolBase {
  public:
    explicit ConstructCaptureBox(fhicl::ParameterSet const&) {}
    int construct(VolumeInfo const& parent, SimpleConfig const& config) override;
  };

  int ConstructCaptureBox::construct(VolumeInfo const& parent, SimpleConfig const& config) {
    // Geometry parameters use the study world's SimpleConfig interface and mm units.
    std::vector<double> half, translation, matrix;
    config.getVectorDouble("captureBox.halfLengths", half, {50., 50., 50.}, 3);
    config.getVectorDouble("captureBox.translation", translation, {0., 0., 0.}, 3);
    config.getVectorDouble("captureBox.localToWorld", matrix,
                           {1., 0., 0., 0., 1., 0., 0., 0., 1.}, 9);
    for (double value : half) {
      if (!std::isfinite(value) || value <= 0.)
        throw cet::exception("GEOM") << "captureBox.halfLengths must be finite and positive\n";
    }
    for (double value : translation) {
      if (!std::isfinite(value))
        throw cet::exception("GEOM") << "captureBox.translation must be finite\n";
    }
    for (double value : matrix) {
      if (!std::isfinite(value))
        throw cet::exception("GEOM") << "captureBox.localToWorld must be finite\n";
    }

    // Row-major R maps column vectors by x_world = R*x_local + translation.
    G4ThreeVector const x(matrix[0], matrix[3], matrix[6]);
    G4ThreeVector const y(matrix[1], matrix[4], matrix[7]);
    G4ThreeVector const z(matrix[2], matrix[5], matrix[8]);
    constexpr double rotationTolerance = 1.e-10;
    if (std::abs(x.mag2() - 1.) > rotationTolerance ||
        std::abs(y.mag2() - 1.) > rotationTolerance ||
        std::abs(z.mag2() - 1.) > rotationTolerance ||
        std::abs(x.dot(y)) > rotationTolerance ||
        std::abs(x.dot(z)) > rotationTolerance ||
        std::abs(y.dot(z)) > rotationTolerance ||
        std::abs(x.dot(y.cross(z)) - 1.) > rotationTolerance) {
      throw cet::exception("GEOM") << "captureBox.localToWorld must be orthonormal with determinant +1\n";
    }

    // This tool assumes the unrotated, origin-centred parent made by Mu2eStudyWorld.
    if (parent.name != "BoxInTheWorld" || parent.centerInWorld.mag2() != 0. ||
        config.getString("world.materialName") != "G4_Galactic") {
      throw cet::exception("GEOM") << "ConstructCaptureBox requires the vacuum Mu2eStudyWorld parent\n";
    }
    double const worldHalf = config.getDouble("world.halfLength");
    double const clearance = 10. * G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
    if (!std::isfinite(worldHalf) || worldHalf <= 0.)
      throw cet::exception("GEOM") << "world.halfLength must be finite and positive\n";
    for (unsigned row = 0; row != 3; ++row) {
      double extent = std::abs(translation[row]);
      for (unsigned col = 0; col != 3; ++col)
        extent += std::abs(matrix[3 * row + col]) * half[col];
      if (!std::isfinite(extent) || extent + clearance >= worldHalf)
        throw cet::exception("GEOM") << "CaptureEnvelope must fit strictly inside the vacuum parent\n";
    }

    G4RotationMatrix const localToWorld(x, y, z);
    // nestBox uses G4PVPlacement's passive frame rotation; keep its inverse alive.
    auto* passiveRotation = art::ServiceHandle<Mu2eG4Helper>()->antiLeakRegistry().add(localToWorld.inverse());
    auto* material = findMaterialOrThrow(config.getString("captureBox.material", "G4_POLYSTYRENE"));
    nestBox("CaptureEnvelope", half, material, passiveRotation,
            G4ThreeVector(translation[0], translation[1], translation[2]), parent, 2,
            true, G4Colour::Yellow(), false,
            config.getBool("g4.forceAuxEdgeVisible", true), true,
            config.getBool("g4.doSurfaceCheck", true));
    // Mu2eStudyWorld installs neither a magnetic field nor sensitive detectors.
    return 0;
  }

}

DEFINE_ART_CLASS_TOOL(mu2e::ConstructCaptureBox)
