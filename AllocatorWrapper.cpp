#include "AllocatorWrapper.hpp"
#include "ZAllocators.hpp"

template<class A>
AllocatorWrapper<A>::AllocatorWrapper(void* initial_pool, size_t pool_size, int lazyThreshold, bool startFull) 
    : allocator(initial_pool, pool_size, lazyThreshold, startFull) {}

template<class A>
void AllocatorWrapper<A>::reset() {
    allocator.reset();
}

template<class A>
void *AllocatorWrapper<A>::allocate(size_t size) {
    return allocator.allocate(size);
}

template<class A>
void AllocatorWrapper<A>::free(void* ptr) {
    allocator.free(ptr,0);
}

template<class A>
void AllocatorWrapper<A>::free(void* ptr, size_t block_size) {
    allocator.free(ptr, block_size);
}

template<class A>
void AllocatorWrapper<A>::free_range(void* start_ptr, size_t block_size) {
    allocator.free_range(start_ptr, block_size);
}

template<class A>
void AllocatorWrapper<A>::aggregate() {
    allocator.aggregate();
}

template<class A>
AllocatorWrapper<A>::~AllocatorWrapper(){}

template class AllocatorWrapper<ZTLSFAllocator>;
// template class AllocatorWrapper<ZBuddyAllocator>;
template class AllocatorWrapper<ZinaryBuddyAllocator>;



ZAllocatorWrapper::ZAllocatorWrapper(void* initial_pool, size_t pool_size, int lazyThreshold, bool startFull, bool useBinaryBuddyAllocator) 
  : useBinaryBuddyAllocator(useBinaryBuddyAllocator) {
    if(useBinaryBuddyAllocator) {
      binaryBuddyAllocator = new AllocatorWrapper<ZinaryBuddyAllocator>(initial_pool, pool_size, lazyThreshold, startFull);
    } else {
      tlsfAllocator = new AllocatorWrapper<ZTLSFAllocator>(initial_pool, pool_size, lazyThreshold, startFull);
    }
}

void ZAllocatorWrapper::reset() {
  if(useBinaryBuddyAllocator) {
    binaryBuddyAllocator->reset();
  } else {
    tlsfAllocator->reset();
  }
}

void *ZAllocatorWrapper::allocate(size_t size) {
  if(useBinaryBuddyAllocator) {
    return binaryBuddyAllocator->allocate(size);
  } else {
    return tlsfAllocator->allocate(size);
  }
}

void ZAllocatorWrapper::free(void *ptr) {
  if(useBinaryBuddyAllocator) {
    binaryBuddyAllocator->free(ptr);
  } else {
    tlsfAllocator->free(ptr);
  }
}

void ZAllocatorWrapper::free(void *ptr, size_t block_size) {
  if(useBinaryBuddyAllocator) {
    binaryBuddyAllocator->free(ptr, block_size);
  } else {
    tlsfAllocator->free(ptr, block_size);
  }
}

void ZAllocatorWrapper::free_range(void *start_ptr, size_t size) {
  if(useBinaryBuddyAllocator) {
    binaryBuddyAllocator->free_range(start_ptr, size);
  } else {
    tlsfAllocator->free_range(start_ptr, size);
  }
}

void ZAllocatorWrapper::aggregate() {
  if(useBinaryBuddyAllocator) {
    binaryBuddyAllocator->aggregate();
  } else {
    tlsfAllocator->aggregate();
  }
}

ZAllocatorWrapper::~ZAllocatorWrapper() {
  if(useBinaryBuddyAllocator) {
    delete binaryBuddyAllocator;
  } else {
    delete tlsfAllocator;
  }
}