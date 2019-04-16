#include "TUtils.h"

#include <sstream>
#include <iomanip>

#ifdef TM_WINDOWS
	#include <Windows.h>
	#include <intrin.h>
#else
	#error Only Windows is supported for now!
#endif

#ifdef TM_WINDOWS

uint32_t TUtils::GetMinBitPosition(uint64_t value) {
	DWORD result;
	_BitScanForward64(&result, value);//TODO provide other implementation for non Windows platforms
	return result;
}

uint64_t TUtils::CountBits(uint64_t value) {
	return _mm_popcnt_u64(value);//TODO other implementation for non X86 
}

uint64_t TUtils::RoundUp(uint64_t value, uint64_t multiple) {
	if (multiple == 0)
		return value;

	uint64_t remainder = value % multiple;
	if (remainder == 0)
		return value;

	return value + multiple - remainder;
}

void* TUtils::OSAllocVMemory(uint64_t bytes) {
	return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
}

void* TUtils::OSAllocRMemory(void* ptr, uint64_t bytes) {
	return VirtualAlloc(ptr, bytes, MEM_COMMIT, PAGE_READWRITE);
}

void TUtils::OSFreeVMemory(void* ptr) {
	VirtualFree(ptr, 0, MEM_RELEASE);
}

void TUtils::OSFreeRMemory(void* ptr, uint64_t bytes) {
	VirtualFree(ptr, bytes, MEM_DECOMMIT);
}

void* TUtils::OSAllocHeap(uint64_t bytes) {
	HANDLE heap = GetProcessHeap();
	printf("allocating %llu heap bytes\n", bytes);
	return HeapAlloc(heap, 0, bytes);
}

void TUtils::OSFreeHeap(void* ptr) {
	HeapFree(GetProcessHeap(), 0, ptr);
}

uint64_t TUtils::GetPageSize() {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
}

uint64_t TUtils::AllocationGranularity() {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwAllocationGranularity;
}


uint64_t TUtils::LogFloor(uint64_t value) {
	const static uint64_t tab64[64] = {
	63,  0, 58,  1, 59, 47, 53,  2,
	60, 39, 48, 27, 54, 33, 42,  3,
	61, 51, 37, 40, 49, 18, 28, 20,
	55, 30, 34, 11, 43, 14, 22,  4,
	62, 57, 46, 52, 38, 26, 32, 41,
	50, 36, 17, 19, 29, 10, 13, 21,
	56, 45, 25, 31, 35, 16,  9, 12,
	44, 24, 15,  8, 23,  7,  6,  5 };

	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
	return tab64[((uint64_t)((value - (value >> 1ULL)) * 0x07EDD5E59A4E28C2)) >> 58ULL];
}

std::string TUtils::BytesToString(uint64_t bytes, uint32_t percision) {
	if (bytes == 0) return "0 bytes";
	std::stringstream ss;
	static const char sizes[] = { '?', 'K', 'M' , 'G' , 'T' , 'P' , 'E' , 'Z' , 'Y' };
	int i;
	if (bytes < 1024) {
		i = 0;
		ss << bytes;
	} else {
		i = 1;//We have at least 1KiB
		while (bytes / 1024 / 1024 > 0) {
			bytes /= 1024;
			i++;
		}
		if (percision == 0) {
			ss << bytes / 1024; //Do the last division here not caring about the fractional part
		} else {
			ss << std::fixed << std::setprecision(percision) << (bytes / 1024.0);
		}
	}
	ss << ' ';
	if (i == 0) {
		ss << "bytes";
	} else if (i >= sizeof(sizes)) {
		ss << "many many bytes...";
	} else {
		ss << sizes[i] << 'i' << 'B';
	}

	return ss.str();
}
#else 
uint64_t TUtils::LogFloor(uint64_t value) {
	const static uint64_t tab64[64] = {
	63,  0, 58,  1, 59, 47, 53,  2,
	60, 39, 48, 27, 54, 33, 42,  3,
	61, 51, 37, 40, 49, 18, 28, 20,
	55, 30, 34, 11, 43, 14, 22,  4,
	62, 57, 46, 52, 38, 26, 32, 41,
	50, 36, 17, 19, 29, 10, 13, 21,
	56, 45, 25, 31, 35, 16,  9, 12,
	44, 24, 15,  8, 23,  7,  6,  5 };

	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
	return tab64[((uint64_t)((value - (value >> 1ULL)) * 0x07EDD5E59A4E28C2)) >> 58ULL];
}
#endif

