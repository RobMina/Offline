// Andrei Gaponenko, 2012

#include "Offline/BFieldGeom/inc/BFCacheManager.hh"

namespace mu2e {

    BFCacheManager::BFCacheManager():
    innerForLastInner(0),
    innerForLastOuter(0),
    counter(0)
    {
        outerCache.insert(std::make_pair(static_cast<const BFMap*>(nullptr), CacheElement(nullptr, MapList())));
        CacheType::const_iterator p = outerCache.find(nullptr);
        assert(p != outerCache.end());
        innerForLastOuter = &p->second;
    }

    void BFCacheManager::setMaps(const MapContainerType& innerMaps,
                                 const MapContainerType& outerMaps) {
        typedef MapContainerType::const_iterator Iter;

        // All inner maps in the input order
        MapList defaultInnerList;
        for (Iter i = innerMaps.begin(); i != innerMaps.end(); ++i) {
            defaultInnerList.push_back(*i);
        }

        // The fixed-order outer map list
        for (Iter i = outerMaps.begin(); i != outerMaps.end(); ++i) {
            this->outer.push_back(*i);
        }

        // Now populate the cache lookup structures

        for (Iter i = innerMaps.begin(); i != innerMaps.end(); ++i) {
            // or can assign a dedicated innerList for this map, e.g. using hints from FHICL
            innerCache.insert(std::make_pair(i->get(), CacheElement(*i, defaultInnerList)));
        }

        for (Iter i = outerMaps.begin(); i != outerMaps.end(); ++i) {
            // or can assign a dedicated innerList for this map, e.g. using hints from FHICL
            outerCache.insert(std::make_pair(i->get(), CacheElement(*i, defaultInnerList)));
        }

        CacheType::iterator p = outerCache.find(nullptr);
        assert(p != outerCache.end());
        p->second = CacheElement(nullptr, defaultInnerList);
        innerForLastOuter = &p->second;
    }
}  // namespace mu2e
