// #include "gz/z/zPageRecycler.hpp"

// #include "gc/z/zAddress.hpp"
// #include "gc/z/zAllocationFlags.hpp"
// #include "gc/z/zPageAge.hpp"
// #include "gc/z/zPageType.hpp"
// #include "gc/z/zValue.hpp"
// #include "gc/z/shared/z_shared_globals.hpp"

// zaddress ZPageRecycler::alloc_object(size_t size, ZPageAge to_age) {
//   zaddress dest;
//   for(ZPage* const page: _targets[static_cast<uint>(to_age)]) {
//     dest = page->alloc_object_free_list(size);
//     if(dest != zaddress::null) {
//       return dest;
//     }
//   }
//   return zaddress::null;
// }

// void ZPageRecycler::add_page(ZPage* page) {
//   _targets[static_cast<uint>(page->age())].append(page);
// }

// void ZPageRecycler::reset() {
//   for (uint i = 0; i <= ZPageAgeMax; ++i) {
//     _targets[i].clear();
//   }
// }

// void ZPageRecycler::remove_page(ZPage* page) {
//   return; //TODO 
// }