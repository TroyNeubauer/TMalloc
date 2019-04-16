
#include <stdint.h>
#include "SizedAllocator.h"
#include "TUtils.h"

constexpr uint64_t Compile_Log2Floor(uint64_t n) {
	return ((n < 2) ? 0 : 1 + Compile_Log2Floor(n / 2));
}

#define MAX_ALLOCATOR_SIZE (512ull * 1024ull * 1024ull * 1024ull)//512 GB
//If defined then allocations bigger than MAX_ALLOC will use a HeapAlloc, HeapFree pair
#define ENABLE_ABOVE_MAX_ALLOCS

template<uint64_t MIN_ALLOC, uint64_t MAX_ALLOC, 
	uint64_t MIN_ALLOC_LOG2 = Compile_Log2Floor(MIN_ALLOC), uint64_t MAX_ALLOC_LOG2 = Compile_Log2Floor(MAX_ALLOC),
	uint64_t ELEMENTS = MAX_ALLOC_LOG2 - MIN_ALLOC_LOG2 + 1>
class TAllocator {
public:
	TAllocator() {
		uint64_t allocSize = MIN_ALLOC;
		printf("Min alloc %llu, Max alloc %llu, Min alloc log2 %llu, max alloc log2 %llu, elements: %llu\n", MIN_ALLOC, MAX_ALLOC, MIN_ALLOC_LOG2, MAX_ALLOC_LOG2, ELEMENTS);
		printf("log2(1)=%llu, log2(2)=%llu, log2(3)=%llu, log2(4)=%llu, log2(7)=%llu, log2(8)=%llu, log2(15)=%llu\n", Compile_Log2Floor(1), Compile_Log2Floor(2), Compile_Log2Floor(3), Compile_Log2Floor(4), Compile_Log2Floor(7), Compile_Log2Floor(8), Compile_Log2Floor(15));
		fflush(stdout);
		for (int i = 0; i < ELEMENTS; i++) {
			allocators[i].Init(allocSize, allocSize * 64, MAX_ALLOCATOR_SIZE);
			allocSize *= 2;
		}
	}

	void* Allocate(uint64_t bytes) {
		if (bytes <= MAX_ALLOC) {
			return allocators[AllocSizeToIndex(bytes)].Allocate();
		}
#ifdef ENABLE_ABOVE_MAX_ALLOCS
		return TUtils::OSAllocHeap(bytes);
#else
		return nullptr;
#endif
	}

	void Free(void* ptr, size_t size = 0) {
		if (ptr == nullptr) return;
		if (size == 0) {//We dont know the size
			for (int i = 0; i < ELEMENTS; i++) {//Try each allocator
				if (allocators[i].Free(ptr)) {
					return;// One of the allocators freed it so were done here
				}
			}
#ifdef ENABLE_ABOVE_MAX_ALLOCS
			TUtils::OSFreeHeap(ptr);//This was not allocated by any allocator so it must have been from the heap
#endif
		} else {
#ifdef ENABLE_ABOVE_MAX_ALLOCS
			if (size > MAX_ALLOC) {
				TUtils::OSFreeHeap(ptr);
			} else {
				allocators[AllocSizeToIndex(size)].Free(ptr);
			}
#else
			allocators[AllocSizeToIndex(size)].Free(ptr);
#endif
		}
	}

	void FreeAll() {
		for (int i = 0; i < ELEMENTS; i++) {//Free everything from each allocator
			allocators[i].FreeAll();
		}
#ifdef ENABLE_ABOVE_MAX_ALLOCS//Were kinda screwed. How do we track all the allocations to HeapAlloc()?

#endif
	}

	inline uint64_t AllocSizeToIndex(uint64_t bytes) {
		if (bytes < MIN_ALLOC)
			return 0;//Round up to the first allocator
		return TUtils::Log2Celi(bytes) - MIN_ALLOC_LOG2;
	}

	SizedAllocator allocators[ELEMENTS];
private:

};