// This class implements cache logic that works for overlapping maps.
//
// Andrei Gaponenko, 2012
//
// Modifed by Brian Pollack to use shared_ptrs to BFMaps for consistent use across classes.

#ifndef BFCacheManager_hh
#define BFCacheManager_hh

#include <cassert>
#include <map>
#include <memory>
#include <vector>

#include "CLHEP/Vector/ThreeVector.h"

#include "Offline/BFieldGeom/inc/BFMap.hh"

namespace mu2e {

    // There are two classes of magnetic field maps in Mu2e: "Inner" and "Outer" maps.
    // No overlaps are allowed among any of the maps in the "Inner" set.
    // The maps in the "Outer" set may overlap with the "Inner" maps, and among themselves.
    // If a space point belongs to an "Inner" map, that map will be used to
    // compute the field value.   If a point is outside of the "Inner" map set,
    // then the "Outer" map list will be consulted in order, and the first map
    // that contains the point will be used.
    //

    class BFCacheManager {

        typedef std::vector<std::shared_ptr<const BFMap>> MapContainerType;

        struct MapList : public MapContainerType {
            // return the first matching map or nullptr
            const BFMap* findMap(const CLHEP::Hep3Vector& x) const {
                for (const auto& m : *this) {
                    if (m->isValid(x)) {
                        return m.get();
                    }
                }
                return nullptr;
            }
        };

        // An instance per (any) map, allows to optimize the lookup order of "inner" maps
        struct CacheElement {
            std::shared_ptr<const BFMap> myMap;  // the map this instance is attached to
            // A list of "inner" maps optimized for the "my" map
            // If "my" map is an inner map, it is not in the list.
            MapList inner;

            CacheElement(std::shared_ptr<const BFMap> const&  my, const MapList& in)
                : myMap(my), inner(in) {}
        };

        // Inner map lists optimized for the last used map
        mutable const CacheElement* innerForLastInner;
        mutable const CacheElement* innerForLastOuter;  // never null

        // Outer maps in the user-specified order
        MapList outer;

        typedef std::map<const BFMap*, CacheElement> CacheType;
        CacheType innerCache;  // keys are all inner maps
        CacheType outerCache;  // keys are outer maps and nullptr
        int counter;

       public:
        BFCacheManager();

        void setMaps(const MapContainerType& innerMaps, const MapContainerType& outerMaps);

        // Returns the field map for x, or nullptr; a raw pointer, as a shared_ptr copy per call contends across threads
        const BFMap* findMap(const CLHEP::Hep3Vector& x) const {
            // First try to find if the point belong to any of the inner maps

            if (innerForLastInner) {  // we were in an inner map last time

                if (innerForLastInner->myMap->isValid(x)) {
                    // Cache update not needed, we are still in the same inner map
                    return innerForLastInner->myMap.get();
                }

                // The lookup order here is optimized
                const BFMap* newinner = innerForLastInner->inner.findMap(x);
                if (newinner) {  // Update cache
                    CacheType::const_iterator p = innerCache.find(newinner);
                    assert(p != innerCache.end());
                    innerForLastInner = &p->second;
                    return newinner;
                }
            } else {  // We were not in an inner map last time

                // innerForLastOuter is never null
                const BFMap* newinner = innerForLastOuter->inner.findMap(x);
                if (newinner) {  // Update cache
                    CacheType::const_iterator p = innerCache.find(newinner);
                    assert(p != innerCache.end());
                    innerForLastInner = &p->second;
                    return newinner;
                }
            }

            // The current point is not in any of the inner maps
            innerForLastInner = 0;

            // The lookup order of the outer maps is always the same
            const BFMap* newouter = outer.findMap(x);

            // Keep the inner map lookup optimized
            if (innerForLastOuter->myMap.get() != newouter) {
                CacheType::const_iterator p = outerCache.find(newouter);
                assert(p != outerCache.end());
                innerForLastOuter = &p->second;
            }

            return newouter;
        }
    };
}  // namespace mu2e

#endif /*BFCacheManager_hh*/
