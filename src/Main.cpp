
#include <stdio.h>
#include <ctime>
#include <vector>
#include <algorithm>

#include "SizedAllocator.h"
#include "TAllocator.h"
#include "PlatformUtils.h"

#define COUNT (1024 * 1024)

int main() {
	PlatformUtils::Init();
	uint64_t pageSize = TUtils::GetPageSize();
	int j = 1;
	TAllocator<4, 4 * COUNT> alloc;
	
	void** ptrs = new void*[COUNT];
	while (true) {
		printf("\nBeginning new test %d\n", j++);
		for (int i = 0; i < COUNT; i++) {
			int* ptr = (int*) alloc.Allocate(i);
			ptrs[i] = ptr;
		}
		printf("Peak Memory usage: Virtual %s, real: %s\n", TUtils::BytesToString(PlatformUtils::GetProcessVirtualMemoryUsage()).c_str(), TUtils::BytesToString(PlatformUtils::GetProcessPhysicalMemoryUsage()).c_str());
		for (int i = 0; i < COUNT; i++) {
			alloc.Free(ptrs[i]);
		}
		printf("Min Memory usage: Virtual %s, real: %s\n", TUtils::BytesToString(PlatformUtils::GetProcessVirtualMemoryUsage()).c_str(), TUtils::BytesToString(PlatformUtils::GetProcessPhysicalMemoryUsage()).c_str());
		alloc.FreeAll();
	}
	system("PAUSE");
}
