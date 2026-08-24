#include "MemoryUtils.h"
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

ProcessMemoryStats MemoryUtils::GetCurrentProcessMemory() {
    ProcessMemoryStats stats = { 0, 0, 0 };
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        stats.workingSetBytes = pmc.WorkingSetSize;
        stats.privateUsageBytes = pmc.PrivateUsage;
        stats.peakWorkingSet = pmc.PeakWorkingSetSize;
    }
    return stats;
}

double MemoryUtils::BytesToMB(size_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

double MemoryUtils::BytesToKB(size_t bytes) {
    return static_cast<double>(bytes) / 1024.0;
}
