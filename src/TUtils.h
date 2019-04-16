#pragma once

#include <stdint.h>
#include <string>

class TUtils {
public:

	//Calls the relevant OS Heap Allocate function
	static void* OSAllocVMemory(uint64_t bytes);
	static void* OSAllocRMemory(void* ptr, uint64_t bytes);

	static void OSFreeVMemory(void* ptr);
	static void OSFreeRMemory(void* ptr, uint64_t bytes);

	static void* OSAllocHeap(uint64_t bytes);
	static void OSFreeHeap(void* ptr);

	static uint64_t GetPageSize();
	static uint64_t AllocationGranularity();


	//Returns the lowest bit in value 0x1 = 0, 0x100 = 2, etc.
	static uint32_t GetMinBitPosition(uint64_t value);
	static uint64_t CountBits(uint64_t value);
	static inline uint64_t CountZeroBits(uint64_t value) { return CountBits(~value); }

	static uint64_t RoundUp(uint64_t value, uint64_t multiple);

	static uint64_t LogFloor(uint64_t value);
	//Computes the log base 2 for the value and rounds up. Ex 2->1, 4->2, 5->3, 8->3
	//We need to make powers of two produce the largest value for a given input.
	//because LogFloor(5)->2, LogFloor(4) is still 2 and with a +1 we get the right anwser
	//LogFloor(4)->2, Log2Celi(3)->1+1->2
	static inline uint64_t Log2Celi(uint64_t value) { return LogFloor(value - 1) + 1; }

	//Returns a string like 8.5KiB
	static std::string BytesToString(uint64_t bytes, uint32_t percision = 2);

};
