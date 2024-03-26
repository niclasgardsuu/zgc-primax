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