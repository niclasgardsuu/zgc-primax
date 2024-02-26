/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "gc/shared/gc_globals.hpp"
#include "gc/z/zGeneration.inline.hpp"
#include "gc/z/zList.inline.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zPhysicalMemory.inline.hpp"
#include "gc/z/zRememberedSet.inline.hpp"
#include "gc/z/zVirtualMemory.inline.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/growableArray.hpp"
#include <string>
#include <sstream>

#define MINIMUM_DISTANCE_BETWEEN_OBJECTS_TO_CONSIDER_FREEING_RANGE_BETWEEN_THEM__LONG_NAME__I_KNOW___ILL_CHANGE_IT_LATER 1024

ZPage::ZPage(ZPageType type, const ZVirtualMemory& vmem, const ZPhysicalMemory& pmem)
  : _type(type),
    _generation_id(ZGenerationId::young),
    _age(ZPageAge::eden),
    _numa_id((uint8_t)-1),
    _seqnum(0),
    _seqnum_other(0),
    _virtual(vmem),
    _top(to_zoffset_end(start())),
    _livemap(object_max_count()),
    _remembered_set(),
    _last_used(0),
    _physical(pmem),
    _node(),
    _allocator(nullptr),
    _recycling_seqnum(-1) {
  assert(!_virtual.is_null(), "Should not be null");
  assert(!_physical.is_null(), "Should not be null");
  assert(_virtual.size() == _physical.size(), "Virtual/Physical size mismatch");
  assert((_type == ZPageType::small && size() == ZPageSizeSmall) ||
         (_type == ZPageType::medium && size() == ZPageSizeMedium) ||
         (_type == ZPageType::large && is_aligned(size(), ZGranuleSize)),
         "Page type/size mismatch");
}

ZPage* ZPage::clone_limited() const {
  // Only copy type and memory layouts. Let the rest be lazily reconstructed when needed.
  return new ZPage(_type, _virtual, _physical);
}

ZPage* ZPage::clone_limited_promote_flipped() const {
  ZPage* const page = new ZPage(_type, _virtual, _physical);

  // The page is still filled with the same objects, need to retain the top pointer.
  page->_top = _top;

  return page;
}

ZGeneration* ZPage::generation() {
  return ZGeneration::generation(_generation_id);
}

const ZGeneration* ZPage::generation() const {
  return ZGeneration::generation(_generation_id);
}

void ZPage::reset_seqnum() {
  Atomic::store(&_seqnum, generation()->seqnum());
  Atomic::store(&_seqnum_other, ZGeneration::generation(_generation_id == ZGenerationId::young ? ZGenerationId::old : ZGenerationId::young)->seqnum());
}

void ZPage::remset_clear() {
  _remembered_set.clear_all();
}

void ZPage::verify_remset_after_reset(ZPageAge prev_age, ZPageResetType type) {
  // Young-to-old reset
  if (prev_age != ZPageAge::old) {
    verify_remset_cleared_previous();
    verify_remset_cleared_current();
    return;
  }

  // Old-to-old reset
  switch (type) {
  case ZPageResetType::Splitting:
    // Page is on the way to be destroyed or reused, delay
    // clearing until the page is reset for Allocation.
    break;

  case ZPageResetType::InPlaceRelocation:
    // Relocation failed and page is being compacted in-place.
    // The remset bits are flipped each young mark start, so
    // the verification code below needs to use the right remset.
    if (ZGeneration::old()->active_remset_is_current()) {
      verify_remset_cleared_previous();
    } else {
      verify_remset_cleared_current();
    }
    break;

  case ZPageResetType::FlipAging:
    fatal("Should not have called this for old-to-old flipping");
    break;

  case ZPageResetType::Allocation:
    verify_remset_cleared_previous();
    verify_remset_cleared_current();
    break;
  };
}

void ZPage::reset_remembered_set() {
  if (is_young()) {
    // Remset not needed
    return;
  }

  // Clearing of remsets is done when freeing a page, so this code only
  // needs to ensure the remset is initialized the first time a page
  // becomes old.
  if (!_remembered_set.is_initialized()) {
    _remembered_set.initialize(size());
  }
}

void ZPage::reset(ZPageAge age, ZPageResetType type) {
  const ZPageAge prev_age = _age;
  _age = age;
  _last_used = 0;

  _generation_id = age == ZPageAge::old
      ? ZGenerationId::old
      : ZGenerationId::young;

  reset_seqnum();

  // Flip aged pages are still filled with the same objects, need to retain the top pointer.
  if (type != ZPageResetType::FlipAging) {
    _top = to_zoffset_end(start());
  }

  reset_remembered_set();
  verify_remset_after_reset(prev_age, type);

  if (type != ZPageResetType::InPlaceRelocation || (prev_age != ZPageAge::old && age == ZPageAge::old)) {
    // Promoted in-place relocations reset the live map,
    // because they clone the page.
    _livemap.reset();
  }
}

void ZPage::finalize_reset_for_in_place_relocation() {
  // Now we're done iterating over the livemaps
  _livemap.reset();
}

void ZPage::reset_type_and_size(ZPageType type) {
  _type = type;
  _livemap.resize(object_max_count());
  _remembered_set.resize(size());
}

ZPage* ZPage::retype(ZPageType type) {
  assert(_type != type, "Invalid retype");
  reset_type_and_size(type);
  return this;
}

ZPage* ZPage::split(size_t split_of_size) {
  return split(type_from_size(split_of_size), split_of_size);
}

ZPage* ZPage::split_with_pmem(ZPageType type, const ZPhysicalMemory& pmem) {
  // Resize this page
  const ZVirtualMemory vmem = _virtual.split(pmem.size());

  reset_type_and_size(type_from_size(_virtual.size()));
  reset(_age, ZPageResetType::Splitting);

  assert(vmem.end() == _virtual.start(), "Should be consecutive");

  log_trace(gc, page)("Split page [" PTR_FORMAT ", " PTR_FORMAT ", " PTR_FORMAT "]",
      untype(vmem.start()),
      untype(vmem.end()),
      untype(_virtual.end()));

  // Create new page
  return new ZPage(type, vmem, pmem);
}

ZPage* ZPage::split(ZPageType type, size_t split_of_size) {
  assert(_virtual.size() > split_of_size, "Invalid split");

  const ZPhysicalMemory pmem = _physical.split(split_of_size);

  return split_with_pmem(type, pmem);
}

ZPage* ZPage::split_committed() {
  // Split any committed part of this page into a separate page,
  // leaving this page with only uncommitted physical memory.
  const ZPhysicalMemory pmem = _physical.split_committed();
  if (pmem.is_null()) {
    // Nothing committed
    return nullptr;
  }

  assert(!_physical.is_null(), "Should not be null");

  return split_with_pmem(type_from_size(pmem.size()), pmem);
}

class ZFindBaseOopClosure : public ObjectClosure {
private:
  volatile zpointer* _p;
  oop _result;

public:
  ZFindBaseOopClosure(volatile zpointer* p)
    : _p(p),
      _result(nullptr) {}

  virtual void do_object(oop obj) {
    const uintptr_t p_int = reinterpret_cast<uintptr_t>(_p);
    const uintptr_t base_int = cast_from_oop<uintptr_t>(obj);
    const uintptr_t end_int = base_int + wordSize * obj->size();
    if (p_int >= base_int && p_int < end_int) {
      _result = obj;
    }
  }

  oop result() const { return _result; }
};

bool ZPage::is_remset_cleared_current() const {
  return _remembered_set.is_cleared_current();
}

bool ZPage::is_remset_cleared_previous() const {
  return _remembered_set.is_cleared_previous();
}

void ZPage::verify_remset_cleared_current() const {
  if (ZVerifyRemembered && !is_remset_cleared_current()) {
    fatal_msg(" current remset bits should be cleared");
  }
}

void ZPage::verify_remset_cleared_previous() const {
  if (ZVerifyRemembered && !is_remset_cleared_previous()) {
    fatal_msg(" previous remset bits should be cleared");
  }
}

void ZPage::clear_remset_current() {
 _remembered_set.clear_current();
}

void ZPage::clear_remset_previous() {
 _remembered_set.clear_previous();
}

void ZPage::swap_remset_bitmaps() {
  _remembered_set.swap_remset_bitmaps();
}

void* ZPage::remset_current() {
  return _remembered_set.current();
}

void ZPage::print_on_msg(outputStream* out, const char* msg) const {
  out->print_cr(" %-6s  " PTR_FORMAT " " PTR_FORMAT " " PTR_FORMAT " %s/%-4u %s%s%s",
                type_to_string(), untype(start()), untype(top()), untype(end()),
                is_young() ? "Y" : "O",
                seqnum(),
                is_allocating()  ? " Allocating " : "",
                is_relocatable() ? " Relocatable" : "",
                msg == nullptr ? "" : msg);
}

void ZPage::print_on(outputStream* out) const {
  print_on_msg(out, nullptr);
}

void ZPage::print() const {
  print_on(tty);
}

void ZPage::verify_live(uint32_t live_objects, size_t live_bytes, bool in_place) const {
  if (!in_place) {
    // In-place relocation has changed the page to allocating
    assert_zpage_mark_state();
  }
  guarantee(live_objects == _livemap.live_objects(), "Invalid number of live objects");
  guarantee(live_bytes == _livemap.live_bytes(), "Invalid number of live bytes");
}

void ZPage::fatal_msg(const char* msg) const {
  stringStream ss;
  print_on_msg(&ss, msg);
  fatal("%s", ss.base());
}

bool ZPage::init_free_list() {
  // log_debug(gc)("START INITIALIZING FREE LIST %p",(void*)start());
  if(_allocator){
    //reset the current allocator, and mark entire page as allocated
    _allocator->reset();
  } else {
    allocation_size_func size_func = [](void* addr){
      if(oopDesc::is_oop(cast_to_oop(to_zaddress((uintptr_t)addr)))) {
        return ZUtils::object_size(to_zaddress((uintptr_t) addr)); 
      } else {
        return (size_t)0;
      }
    };
    _allocator = new AllocatorWrapper<ZBuddyAllocator>((void*)ZOffset::address(start()), size(), size_func, 0, true);
  }
  
  if(_age == ZPageAge::old) {
    log_debug(gc)("OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOLD PAGE FOUND init");
    return false;
  } else {
    // log_debug(gc)("young");
  }

  if(live_objects() <= 0) {
    // log_debug(gc)("WTFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    _recycling_seqnum -= 1; 
    _top = to_zoffset_end(start());
    return false;
  }

  //Reconstruct the free list from the livemap
  //
  //Resetting a free list allocator assumes allocating all the
  //space available, and we are reconstructing it by freeing the
  //spaces inbetween live objects.
  uintptr_t curr = untype(ZOffset::address(start()));
  auto free_internal_range = [&](BitMap::idx_t index) -> bool {
    zaddress allocated_addr = ZOffset::address(offset_from_bit_index(index));
    uintptr_t allocated_addr_p = untype(allocated_addr);
    
    if(allocated_addr_p - curr > MINIMUM_DISTANCE_BETWEEN_OBJECTS_TO_CONSIDER_FREEING_RANGE_BETWEEN_THEM__LONG_NAME__I_KNOW___ILL_CHANGE_IT_LATER) {
      log_debug(gc)("initadr2 : %p\ninitsiz2 : %zu", (void*)curr, allocated_addr_p - curr);
      _allocator->free_range((void*)curr,allocated_addr_p - curr);
    } else if(allocated_addr_p - curr > 0) {
      log_debug(gc)("initadr : %p\ninitsiz : %zu", (void*)curr, allocated_addr_p - curr);
    }
    
    //Jump to beginning of next free block
    size_t allocated_size = align_up(ZUtils::object_size(allocated_addr), object_alignment());
    curr = allocated_addr_p + allocated_size;
    return true;
  };

  _livemap.iterate_forced(_generation_id, free_internal_range);
  //free final block after last object
  _allocator->free_range((void*)curr, (uintptr_t)ZOffset::address(to_zoffset(end()))-curr); 
  log_debug(gc)("initadr : %p\ninitsiz : %zu", (void*)curr, untype(ZOffset::address(to_zoffset(end())))-curr);
  // log_debug(gc)("FINISHED FREE LIST INITIALIZATION %p",(void*)start());
  return true;
}

// void ZPage::allocate_object_in_free_list(size_t size, uint32_t gc_cycle) {
//   if(!_allocator || generation()->seqnum() != _seqnum) {
//     //this is the first time relocation tries to use the free list
//     //allocator
//     // OR
//     //this is the first time during this cycle of relocation that
//     //someone uses this page as a to_page, therefore we have to 
//     //initialize the free list allocator
//     _seqnum = generation()->seqnum(); //TODO kommer detta förstöra nåt med concurrency?
//     init_free_list_allocator(size);
//   }
//   uintptr_t addr = _free_list_allocator.allocate(); //TODO ska den va i zaddress space så måste den convertas
//   _top = max(top, addr+size);
//   return addr;
// }

void ZPage::reset_recycling_seqnum() {
  //TODO kom på va som händer om flera threads försöker recyclea samma page.
  //just nu kan de inte hända, men i framtiden om ja får till concurrency.

  //också, se till att på nåt sätt sätta bump pointer på nåt konstigt ställa så att 
  //inga vanliga allocations kan göras. Men vet inte än hur ja ska göra för o 
  //skilja recycle allocation och vanlig allocation.
  _recycling_seqnum = generation()->seqnum();
}

void ZPage::print_live_addresses() {
  int age = 1;
  if(_age == ZPageAge::old) {
    age = 2;
    // log_debug(gc)("OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOLD PAGE FOUND");
  }
  // log_debug(gc)("seqnum-age: %d-%d", _seqnum,age);
  // log_debug(gc)("start     : %p",(void*)(start()));
  // log_debug(gc)("top       : %p", (void*)(top()));
  // log_debug(gc)("end       : %p",(void*)(end()));
  // int* a = (int*)_allocator->allocate(64);
  // int* b = (int*)_allocator->allocate(1);
  // int* c = (int*)_allocator->allocate(64);
  // _allocator->free(b);
  auto do_bit = [&](BitMap::idx_t index) -> bool {
    zoffset offset = offset_from_bit_index(index);
    const zaddress to_addr = ZOffset::address(offset);
    const size_t size = ZUtils::object_size(to_addr);

    log_debug(gc)("livealocc  : %p\nlivesizze  : %lu", (void*)offset, size);
    
    return true;
  };
  _livemap.iterate_forced(_generation_id, do_bit);
  log_debug(gc)("Done Printing Live Addresses");
}