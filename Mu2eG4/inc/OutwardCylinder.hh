#ifndef Mu2eG4_OutwardCylinder_hh
#define Mu2eG4_OutwardCylinder_hh

#include <array>
#include <cmath>

namespace mu2e {
  namespace Mu2eG4Cuts {

    struct OutwardCylinder {
      std::array<double, 2> axisXY;
      double radius;
      double zMin;
      double zMax;

      bool valid() const {
        return std::isfinite(axisXY[0]) && std::isfinite(axisXY[1])
          && std::isfinite(radius) && radius > 0.
          && std::isfinite(zMin) && std::isfinite(zMax) && zMin < zMax;
      }

      // Position and cylinder parameters are in the Mu2e frame.
      bool leaving(double x, double y, double z, double ux, double uy, double uz) const {
        const double dx = x - axisXY[0];
        const double dy = y - axisXY[1];
        return (std::hypot(dx, dy) > radius && dx*ux + dy*uy > 0.)
          || (z < zMin && uz < 0.)
          || (z > zMax && uz > 0.);
      }

      // World and Mu2e axes are parallel, so only the position is translated.
      template<class Vector>
      bool leavingWorld(const Vector& position, const Vector& direction, const Vector& origin) const {
        return leaving(position.x() - origin.x(), position.y() - origin.y(),
                       position.z() - origin.z(), direction.x(), direction.y(), direction.z());
      }
    };

  }
}

#endif
