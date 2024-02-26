#ifndef AAZ_ALLOCATORS
#define AAZ_ALLOCATORS

#include "gc/z/TLSF.hpp"
#include "gc/z/ibuddy.hpp"

using ZConfig = IBuddyConfig<4, 18, 8, true, 4>;
class ZBuddyAllocator : public IBuddyAllocator<ZConfig> {
public:
  ZBuddyAllocator(void* start, size_t size, allocation_size_func size_func, int lazyThreshold, bool startFull) 
    : IBuddyAllocator(start, lazyThreshold, startFull) {} 

  void reset() {fill();}
  // void* allocate(size_t size) {return allocate(size);} already exists in super class
  void free(void* ptr) {deallocate(ptr);}
  void free(void* ptr, size_t size) {deallocate(ptr, size);}
  void free_range(void* ptr, size_t size) {deallocate_range(ptr,size);} 
  void aggregate() {empty_lazy_list();}
};

class ZTLSFAllocator : public ZPageOptimizedTLSF {
public:
  ZTLSFAllocator(void* start, size_t size, allocation_size_func size_func, int lazyThreshold, bool startFull) 
    : ZPageOptimizedTLSF(start, size, size_func, startFull) {}

  // void reset() {reset();}
  // void* allocate(size_t size) {return allocate(size);} already exists in super class
  // void free(void* ptr) {free(ptr);}
  // void free(void* ptr, size_t size) {free(ptr, size);}
  // void free_range(void* ptr, size_t size) {free_range(ptr,size);} 
  // void aggregate() {aggregate();}
};

#endif