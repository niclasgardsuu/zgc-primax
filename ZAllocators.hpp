#ifndef AAZ_ALLOCATORS
#define AAZ_ALLOCATORS

#include "gc/z/JSMalloc.hpp"
// #include "gc/z/ibuddy.hpp"
#include "gc/z/btbuddy.hpp"
// #include "gc/z/bbuddy.hpp"
#include "gc/z/buddy_config.hpp"

// class ZBuddyAllocator : public IBuddyAllocator<ZConfig> {
// public:
//   ZBuddyAllocator(void* start, size_t size, int lazyThreshold, bool startFull) 
//     : IBuddyAllocator(start, lazyThreshold, startFull) {} 

//   void reset() {fill();}
//   // void* allocate(size_t size) {return allocate(size);} already exists in super class
//   void free(void* ptr) {deallocate(ptr);}
//   void free(void* ptr, size_t size) {deallocate(ptr, size);}
//   void free_range(void* ptr, size_t size) {deallocate_range(ptr,size);} 
//   void aggregate() {empty_lazy_list();}
// };

class ZinaryBuddyAllocator : public BTBuddyAllocator<ZConfig> {
public:
  ZinaryBuddyAllocator(void* start, size_t size, int lazyThreshold, bool startFull) 
    : BTBuddyAllocator(start, lazyThreshold, startFull) {} 

  void reset() {fill();}
  // void* allocate(size_t size) {return allocate(size);} already exists in super class
  void free(void* ptr) {deallocate(ptr);}
  void free(void* ptr, size_t size) {deallocate(ptr, size);}
  void free_range(void* ptr, size_t size) {deallocate_range(ptr,size);} 
  void aggregate() {empty_lazy_list();}
};

class ZTLSFAllocator : public JSMallocZ {
public:
  ZTLSFAllocator(void* start, size_t size, int lazyThreshold, bool startFull) 
    : JSMallocZ(start, size, startFull) {}

  // void reset() {reset();}
  // void* allocate(size_t size) {return allocate(size);} already exists in super class
  // void free(void* ptr) {free(ptr);}
  // void free(void* ptr, size_t size) {free(ptr, size);}
  // void free_range(void* ptr, size_t size) {free_range(ptr,size);} 
  // void aggregate() {aggregate();}
};

#endif