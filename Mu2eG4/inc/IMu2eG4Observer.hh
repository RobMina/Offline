#ifndef Mu2eG4_IMu2eG4Observer_hh
#define Mu2eG4_IMu2eG4Observer_hh

class G4Track;

namespace mu2e {
  // Optional lifecycle observation for a top-level common cut; callbacks never change classification.
  class IMu2eG4Observer {
  public:
    virtual ~IMu2eG4Observer() = default;
    virtual void classifiedTrack(const G4Track*, bool killed) = 0;
    virtual void beginTrack(const G4Track*) = 0;
    virtual void endTrack(const G4Track*) = 0;
  };
}
#endif
