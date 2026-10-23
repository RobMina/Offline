#ifndef CRVConditions_CRVLightPropagationCache_hh
#define CRVConditions_CRVLightPropagationCache_hh

#include "Offline/CRVConditions/inc/CRVLightPropagationMaker.hh"
#include "Offline/Mu2eInterfaces/inc/ProditionsCache.hh"

namespace mu2e {

class CRVLightPropagationCache : public ProditionsCache {
 public:
  CRVLightPropagationCache(CRVLightPropagationConfig const& config) :
      ProditionsCache(CRVLightPropagation::cxname, config.verbose()),
      _useDb(config.useDb()), _maker(config) {}

  void initialize() {
    if (_useDb) {
      _tab_p = std::make_unique<DbHandle<CRVPropagation>>();
    }
  }

  set_t makeSet(art::EventID const& eid) {
    ProditionsEntity::set_t cids;
    if (_useDb) {
      _tab_p->get(eid);
      cids.insert(_tab_p->cid());
    }
    return cids;
  }

  DbIoV makeIov(art::EventID const& eid) {
    DbIoV iov;
    iov.setMax();
    if (_useDb) {
      _tab_p->get(eid);
      iov.overlap(_tab_p->iov());
    }
    return iov;
  }

  ProditionsEntity::ptr makeEntity(art::EventID const& eid) {
    if (_useDb) {
      return _maker.fromDb(_tab_p->getPtr(eid));
    } else {
      return _maker.fromFcl();
    }
  }

 private:
  bool _useDb;
  CRVLightPropagationMaker _maker;

  // not default constructed, so the db can be completely turned off
  std::unique_ptr<DbHandle<CRVPropagation>> _tab_p;
};

}  // namespace mu2e

#endif
