// #ifndef SHARE_GC_Z_ZPAGERECYCLER_HPP
// #define SHARE_GC_Z_ZPAGERECYCLER_HPP

// #include "gc/z/zAddress.hpp"
// #include "gc/z/zAllocationFlags.hpp"
// #include "gc/z/zPageAge.hpp"
// #include "gc/z/zPageType.hpp"
// #include "gc/z/zValue.hpp"
// #include "gc/z/shared/z_shared_globals.hpp"

// class ZPageRecycler {
// public:
//   zaddress alloc_object(size_t size);
//   void add_page(ZPage* page);
//   void reset();

// private:
//   ZArray<ZPage*> _targets[ZPageAgeMax + 1];
//   ZArray<size_t> _ntargets[ZPageAgeMax + 1];

//   void remove_page(ZPage* page);
// }

// #endif