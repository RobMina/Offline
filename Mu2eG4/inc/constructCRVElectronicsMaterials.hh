#ifndef Mu2eG4_constructCRVElectronicsMaterials_hh
#define Mu2eG4_constructCRVElectronicsMaterials_hh

namespace mu2e {
  // These factories reject duplicate names rather than substituting another recipe.
  void constructCRVPolyethylene();
  void constructCRVElectronicsMaterials(bool includePolyethylene = true);
}

#endif
