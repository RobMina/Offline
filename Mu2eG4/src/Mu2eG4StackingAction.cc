// Andrei Gaponenko, 2015

#include "Offline/Mu2eG4/inc/Mu2eG4StackingAction.hh"
#include "Offline/Mu2eG4/inc/IMu2eG4Observer.hh"

#include <vector>
#include <string>
#include <iostream>

namespace mu2e {

  Mu2eG4StackingAction::Mu2eG4StackingAction(IMu2eG4Cut& stackingCuts,
                                             IMu2eG4Cut& commonCuts)
    : stackingCuts_(&stackingCuts)
    , commonCuts_(&commonCuts)
  {}

  G4ClassificationOfNewTrack Mu2eG4StackingAction::ClassifyNewTrack(const G4Track* trk){
    const bool killed = stackingCuts_->stackingActionCut(trk) || commonCuts_->stackingActionCut(trk);
    if (auto* observer = dynamic_cast<IMu2eG4Observer*>(commonCuts_))
      observer->classifiedTrack(trk, killed);
    return killed ? fKill : fUrgent;
  }



} // end namespace mu2e
