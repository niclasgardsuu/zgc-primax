#ifndef ALLOCATOR_WRAPPER
#define ALLOCATOR_WRAPPER

#include "memory/allocation.hpp"
#include "ZAllocators.hpp"

// #include ""

template<class A>
class AllocatorWrapper : public CHeapObj<mtGC>{
private:
  A allocator;
public:
  AllocatorWrapper(void* initial_pool, size_t pool_size, int lazyThreshold, bool startFull);
  void reset();
  void *allocate(size_t size);
  void free(void *ptr);
  void free(void *ptr, size_t block_size);
  void free_range(void *start_ptr, size_t size);
  void aggregate();
  ~AllocatorWrapper();
};

class ZAllocatorWrapper : public CHeapObj<mtGC>{
private:
  AllocatorWrapper<ZTLSFAllocator>*       tlsfAllocator;
  AllocatorWrapper<ZinaryBuddyAllocator>* binaryBuddyAllocator;
  bool useBinaryBuddyAllocator;
public:
  ZAllocatorWrapper(void* initial_pool, size_t pool_size, int lazyThreshold, bool startFull, bool useBinaryBuddyAllocator);
  void reset();
  void *allocate(size_t size);
  void free(void *ptr);
  void free(void *ptr, size_t block_size);
  void free_range(void *start_ptr, size_t size);
  void aggregate();
  ~ZAllocatorWrapper();
};

#endif