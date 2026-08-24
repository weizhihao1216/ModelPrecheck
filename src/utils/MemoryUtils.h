#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <cstddef>
#include <cstdint>

struct ProcessMemoryStats {
    size_t workingSetBytes;   // Physical RAM currently used by process
    size_t privateUsageBytes; // Private committed virtual memory
    size_t peakWorkingSet;    // Peak Working Set
};

class MemoryUtils {
public:
    static ProcessMemoryStats GetCurrentProcessMemory();
    static double BytesToMB(size_t bytes);
    static double BytesToKB(size_t bytes);
};

#endif // MEMORY_UTILS_H
