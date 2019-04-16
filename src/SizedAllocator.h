#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <Windows.h>
#include <vector>

#include "TUtils.h"

#define TMALLOC_IN_USE 0
#define TMALLOC_FREE 1

#define MAX_PRINT_SIZE (16 * 1024)

//If this is defined the Memory page will keep extra information and try to give back memory to the os occasionally when Free is called.
//This mode will reduce memory usage but lower performance
#define TM_RETURN_MEMORY

//The size of commited memory after a call to FreeAll()
//0 means the block should maintain its current (potentally very large) size
#if defined(TM_RETURN_MEMORY) /*|| true*/ //manual override for when TM_RETURN_MEMORY is off
	#define	TM_MEMORY_ON_FREE_ALL
	#define TM_SIZE_AFTER_FREE_ALL (1024 * 1024)
#endif
#define ALLOC_LOCATION_FULL UINT64_MAX
#define FREE_LIST_ELEMENT_BITS (sizeof(uint64_t) * CHAR_BIT)
#define CHUNKS_PER_LIST_ELEMENT (sizeof(uint64_t) * CHAR_BIT)

#ifdef TM_DEBUG
	//#define SHOW_ALL_CHANGES
#endif

class SizedAllocator {

public:
	SizedAllocator() {}//Default constructor does nothing

	void Init(uint64_t allocSize, uint64_t startingSize, uint64_t maxCapacity) {
		this->m_AllocSize = allocSize;
		this->m_Size = startingSize;
		this->m_MaxCapacity = maxCapacity;
		if (FreeListSize() == 0) return;
		m_Block = (uint8_t*) TUtils::OSAllocVMemory(maxCapacity);//Reserve the address space
		TUtils::OSAllocRMemory(m_Block, startingSize);//Map the inital capacity

		m_FreeList = (uint64_t*) TUtils::OSAllocHeap(FreeListSize());
		memset(m_FreeList, 0xFF, FreeListSize());//Set all bits to 1's (indicates that each chunk is open)
		if (m_Block == nullptr) m_NextAllocLocation = ALLOC_LOCATION_FULL;
		else m_NextAllocLocation = 0;
	}

	//Allocates a buffer that starts at the returned pointer and is ALLOC_SIZE bytes long. Returns null if there are no free chunks
	void* Allocate() {
		if (m_NextAllocLocation == ALLOC_LOCATION_FULL) {
#ifdef SHOW_ALL_CHANGES
			PrintPage();
			printf("No chunks avilable ^\n");
#endif
			return nullptr;
		} else {//We have memory to spare
			void* result = ChunkIndexToAddress(m_NextAllocLocation);
			ReserveChunk(m_NextAllocLocation);
			//Update m_NextAllocLocation to point to a different un-allocated chunk or ALLOC_LOCATION_FULL if no memory is avilable
			if (IsIndexAvilable(m_NextAllocLocation + 1)) {//The next index is avilable
				m_NextAllocLocation++;
#ifdef TM_RETURN_MEMORY
				m_ChunksInUse++;
#endif
#ifdef SHOW_ALL_CHANGES
				PrintPage({ m_NextAllocLocation - 1, m_NextAllocLocation }, { FOREGROUND_BLUE, FOREGROUND_GREEN });
#endif
			} else {//We must try to find another index somewhere else
				for (int i = 0; i < FreeListElements(); i++) {
					uint64_t value = m_FreeList[i];
					if (value) {//There is a free chunk somewhere in these 64 chunks
#ifdef SHOW_ALL_CHANGES	
						uint64_t oldLocation = m_NextAllocLocation;
#endif
						m_NextAllocLocation = MakeChunkAddress(i, TUtils::GetMinBitPosition(value));
#ifdef SHOW_ALL_CHANGES
						PrintPage({ oldLocation, m_NextAllocLocation }, { FOREGROUND_BLUE, FOREGROUND_RED });
#endif
#ifdef TM_RETURN_MEMORY
						m_ChunksInUse++;
#endif
						return result;//Return so we dont set the next location to out of memory
					}
				}
#ifdef SHOW_ALL_CHANGES
				PrintPage({ m_NextAllocLocation }, { FOREGROUND_RED });
#endif
				m_NextAllocLocation++;//We know the next one is avilable
				uint64_t newSize = Size() / AllocSize();
				if (Size() < (512 * 1024)) {
					newSize = Size() * 4;//Be greedy at the start
				} else if(Size() < (16 * 1024 * 1024)) {
					newSize = Size() * 3;
				} else if (Size() < (128 * 1024 * 1024)) {
					newSize = Size() * 2;
				} else if (Size() < (1024 * 1024 * 1024)) {
					newSize = Size() * 3 / 2;//*1.5
				} else {
					newSize = Size() * 9 / 8;//*1.125
				}
				Resize(newSize);
#ifdef TM_RETURN_MEMORY
				if (m_NextAllocLocation != ALLOC_LOCATION_FULL) 
					m_ChunksInUse++;
#endif
				//if this fails m_NextAllocLocation will be set accordingly
			}
			return result;
		}
	}

	bool Free(void* address) {
		if (address == nullptr) return false;
		if ((uint64_t) address < (uint64_t) m_Block) return false;// Bad free, this is not our address
		uint64_t index = ((uint64_t) address - (uint64_t) m_Block) / AllocSize();
		if (index >= ChunkCount()) return false;//Not our address
#ifdef TM_RETURN_MEMORY
		if(UnReserveChunk(index))
			m_ChunksInUse--;
#else
		UnReserveChunk(index)
#endif
#ifdef SHOW_ALL_CHANGES
		PrintPage({ index }, { FOREGROUND_BLUE });
#endif
		if (m_NextAllocLocation == ALLOC_LOCATION_FULL) m_NextAllocLocation = index;
		return true;
	}

	void Resize(uint64_t newSize) {
		uint64_t oldSize = Size(), oldFreeListSize = FreeListSize();
		uint64_t* oldFreeList = m_FreeList;
		if (newSize >= MaxCapacity()) {
			if (Size() >= MaxCapacity()) {//Last time we went over the max size to allign to a page boundary. So we are entierly out of space
				m_NextAllocLocation = ALLOC_LOCATION_FULL;
				return;
			} else {
				newSize = MaxCapacity();//Last time we were below the max capacity. This will be the last call to commit more memory to this address space
			}
		}
		uint64_t addedBytes = newSize - oldSize;
		if (addedBytes < AllocSize()) {
			addedBytes = AllocSize();
		}
		newSize = addedBytes + oldSize;
		newSize = TUtils::RoundUp(newSize, TUtils::GetPageSize());//Make sure the new size is a mutiple of the page size
//#ifdef SHOW_ALL_CHANGES
		printf("Resizing up %.1lf%% to %s\n", (double) newSize / Size() * 100.0, TUtils::BytesToString(newSize).c_str());
//#endif

		m_Size = newSize;//Re-assign Capacity, FreeListSize, and all the other accessors will return the new values
		void* startingSection = TUtils::OSAllocRMemory((uint8_t*) m_Block + oldSize, addedBytes);
		if (startingSection == nullptr) {
			m_NextAllocLocation = ALLOC_LOCATION_FULL;
			printf("Unable to resize block to %ull bytes. Error: %d", newSize, GetLastError());
			DebugBreak();
		}

		m_FreeList = (uint64_t*) TUtils::OSAllocHeap(FreeListSize());
		if (m_Block == nullptr || m_FreeList == nullptr) {
			m_NextAllocLocation = ALLOC_LOCATION_FULL;
			return;
		}

		memcpy(m_FreeList, oldFreeList, oldFreeListSize);
		memset((uint8_t*) m_FreeList + oldFreeListSize, 0xFF, FreeListSize() - oldFreeListSize);//Set all new bits to 1's
		TUtils::OSFreeHeap(oldFreeList);
	}

	void PrintPage(std::vector<uint64_t> bitIndices = std::vector<uint64_t>(), std::vector<uint32_t> color = std::vector<uint32_t>()) {
		uint64_t used = 0;
		for (int i = 0; i < FreeListElements(); i++) {
			used += TUtils::CountZeroBits(m_FreeList[i]);
		}
		if(bitIndices.size() == 0) printf("\nMemory Page Max Capacity: %s, In Use: %s, Chunk Size: %llu bytes, Total Chunks: %llu, Free List Elements: %llu\n", 
			TUtils::BytesToString(MaxCapacity(), 2).c_str(), TUtils::BytesToString(Size()).c_str(), AllocSize(), ChunkCount(), FreeListElements());
		if (bitIndices.size() == 0) printf("%d chunks currently used out of %d chunks. I = in use, . = free\n", used, ChunkCount());
		if (Size() > MAX_PRINT_SIZE) {
			printf("Too many chunks to print...\n");
			return;
		}
		printf("<");
		for (int i = 0; i < FreeListElements(); i++) {
			uint64_t value = m_FreeList[i];
			for (int j = 0; j < 64; j++) {
				int k = 0;
				bool resetNeeded = false;
				for (auto it = bitIndices.begin(); it != bitIndices.end(); it++, k++) {
					if (MakeChunkAddress(i, j) == *it) {
						SetConsoleColor(color[k]);
						resetNeeded = true;
					}
				}
				if ((value >> j) & 0x1 == TMALLOC_FREE) printf(".");
				else printf("I");

				if (resetNeeded) ResetConsoleColor();
				if ((j % 8 == 0) && j != 0) printf(" ");
			}
			printf("\n");
		}
		puts(">");
	}

	void FreeAll() {
#ifdef TM_MEMORY_ON_FREE_ALL
		if (TM_SIZE_AFTER_FREE_ALL != 0) {//If we are decommiting memory...
	#ifdef TM_RETURN_MEMORY
			m_ChunksInUse = 0;
			m_BytesFreedSinceMemReleaseCheck = 0;//0 since we are releasing the memory
	#endif
			TUtils::OSFreeRMemory(m_Block + TM_SIZE_AFTER_FREE_ALL, Size() - TM_SIZE_AFTER_FREE_ALL);
			m_Size = TM_SIZE_AFTER_FREE_ALL;
		}
		//printf("List: %p, size: %llu\n", m_FreeList, FreeListSize());
		memset(m_FreeList, 0xFF, FreeListSize());
#endif
	}

	void Release() {
		if (m_Block != nullptr) {
			TUtils::OSFreeVMemory(m_Block);
			m_Block = nullptr;
		}
		if (m_FreeList != nullptr) {
			TUtils::OSFreeHeap(m_FreeList);
			m_FreeList = nullptr;
		}
	}

	~SizedAllocator() {
		Release();
	}

	uint64_t ChunkCount() { return m_Size / m_AllocSize; }
	uint64_t FreeListElements() { return ChunkCount() / CHUNKS_PER_LIST_ELEMENT; }
	uint64_t FreeListSize() { return FreeListElements() * sizeof(uint64_t); }
	uint64_t MaxCapacity() { return m_MaxCapacity; }
	uint64_t Size() { return m_Size; }
	uint64_t AllocSize() { return m_AllocSize; }
	uint8_t* Block() { return m_Block; }
	uint64_t ChunksInUse() {
#ifdef TM_RETURN_MEMORY
		return m_ChunksInUse;
#else
		return 0;
#endif
	}

	uint64_t BytesFreedSinceMemReleaseCheck() {
#ifdef TM_RETURN_MEMORY
		return m_BytesFreedSinceMemReleaseCheck;
#else
		return 0;
#endif
	}
private:
	inline void* ChunkIndexToAddress(uint64_t chunkIndex) {
		return m_Block + chunkIndex * AllocSize();
	}

	inline bool IsIndexAvilable(uint64_t chunkIndex) {
		if (chunkIndex >= ChunkCount()) return false;//Nice bounds check - handles all cases
		return m_FreeList[GetFreeListIndex(chunkIndex)] & GetFreeListBit(chunkIndex);
	}

	inline bool IsIndexAllocated(uint64_t chunkIndex) { return !IsIndexAvilable(chunkIndex); }

	inline void ReserveChunk(uint64_t chunkIndex) {
		uint64_t temp = ~GetFreeListBit(chunkIndex);
		m_FreeList[GetFreeListIndex(chunkIndex)] &= temp;
	}//Turn the bit off

	//Un reserves a chunk in the free list and returns true if it was previously allocated, false otherwise
	bool UnReserveChunk(uint64_t chunkIndex) {
		bool changed = (m_FreeList[GetFreeListIndex(chunkIndex)] & GetFreeListBit(chunkIndex)) == 0;//Will be true if this chunk was reserved before
		m_FreeList[GetFreeListIndex(chunkIndex)] |= GetFreeListBit(chunkIndex);
		return changed;
	}//Turn the bit on

	//Returns the chunk address given an index and a bit
	//This does not do bounds checking
	inline uint64_t MakeChunkAddress(uint64_t index, uint64_t bit) { return index * CHUNKS_PER_LIST_ELEMENT + bit; }

	//Returns the bit in which the info for the desired chunk resides
	//FREE_LIST_ELEMENT_BITS is a power of 2 so the compiler will optomize this
	inline uint64_t GetFreeListBit(uint64_t chunkIndex) { return 1ULL << (chunkIndex % FREE_LIST_ELEMENT_BITS); }

	//Returns the index in the free list array where the given chunk's info resizes
	inline uint64_t GetFreeListIndex(uint64_t chunkIndex) { 
		return chunkIndex / FREE_LIST_ELEMENT_BITS;
	}

private:
	uint8_t* m_Block;//The pointer to the pages of memory given to us by the OS
	uint64_t* m_FreeList;//for each bit, a 0 means this block is in use, 1 means avilable for allocation
	uint64_t m_AllocSize;// The number of bytes in a chunk
	uint64_t m_MaxCapacity;//The number of bytes of virtual memory m_Block is allocated to hold and the absloule largest size it can be before the OS complains
	uint64_t m_Size;// The amount of bytes currently commited for this process starting at m_Block
	uint64_t m_NextAllocLocation;//The index where the next allocation will be stored. Will be ALLOC_LOCATION_FULL if no memory is avilable
#ifdef TM_RETURN_MEMORY
	uint64_t m_BytesFreedSinceMemReleaseCheck = 0;//The number of bytes freed since the last check for decommiting memory
	uint64_t m_ChunksInUse = 0;//A quick counter for the number of chunks currently allocated. This could also be computed by looking at the bits in m_FreeList
#endif

private://Win32 utils for printing
	uint16_t atts;
	void SetConsoleColor(uint32_t color) {
		CONSOLE_SCREEN_BUFFER_INFO info;
		HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
		GetConsoleScreenBufferInfo(out, &info);
		atts = info.wAttributes;
		SetConsoleTextAttribute(out, color);
	}

	void ResetConsoleColor() {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), atts);
	}

};
