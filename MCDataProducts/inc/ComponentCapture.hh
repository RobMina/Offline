#ifndef MCDataProducts_ComponentCapture_hh
#define MCDataProducts_ComponentCapture_hh

#include <cstdint>
#include <string>
#include <vector>

namespace mu2e {

  // Schema 1 is diagnostic capture, not a certified response-library format.
  struct ComponentState {
    double x=0, y=0, z=0, px=0, py=0, pz=0;
    double lx=0, ly=0, lz=0, lpx=0, lpy=0, lpz=0;
    double energy=0, kinetic=0, mass=0, charge=0, time=0, properTime=0, weight=1;
    std::int64_t volume=-1;
    std::string placement, material, process;
    int stepStatus=-1;
  };

  struct ComponentClassification {
    std::int64_t track=0, parent=0;
    int pdg=0, status=-1;
    bool killed=false;
    ComponentState state;
  };

  struct ComponentTrack {
    std::int64_t track=0, parent=0, steps=0, secondaries=0;
    int pdg=0, status=-1, reason=0;
    bool entered=false, ended=false;
    std::string creator, stopProcess;
    ComponentState begin, end, entrance;
  };

  struct ComponentStep {
    std::int64_t track=0, number=0, secondaries=0;
    double deposit=0, nonIonizingDeposit=0, length=0;
    int trackStatus=-1;
    bool preInside=false, postInside=false, firstExit=false;
    ComponentState pre, post;
  };

  struct ComponentVolume {
    std::int64_t id=-1, copy=-1;
    std::string name, logical, motherLogical, material, solid;
    double density=0;
    std::vector<double> translation, rotation;
  };

  struct ComponentCapture {
    int schema=1;
    std::int64_t run=0, subrun=0, event=0;
    bool complete=false, bankAdmissible=false;
    std::string geometryTag, physicsTag, envelope;
    std::vector<double> translation, rotation, halfSize;
    std::vector<std::string> errors;
    std::vector<ComponentVolume> volumes;
    std::vector<ComponentClassification> classifications;
    std::vector<ComponentTrack> tracks;
    std::vector<ComponentStep> steps;
  };

  // Per-track ancestry link for a ComponentCapture; creatorStep indexes ComponentCapture::steps, -1 for the primary.
  struct ComponentTrackLink {
    std::int64_t track=0, parent=0, creatorStep=-1;
    bool bornOnSurface=false, exitAtBirth=false;
  };

  // Companion of ComponentCapture with ancestry links and the physics/material/cut manifest; never bank admissible.
  struct ComponentCaptureLinks {
    int schema=1;
    std::int64_t run=0, subrun=0, event=0;
    std::string g4Version, manifest, manifestHash;
    std::vector<ComponentTrackLink> tracks;
  };

}
#endif
