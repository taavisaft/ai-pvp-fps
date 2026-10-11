#include "process_usage.h"
#include <chrono>
#include <cstdint>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#ifdef __APPLE__
#include <mach/mach.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#endif
#endif

namespace {
double cpuSeconds() {
#ifdef _WIN32
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return -1;
    auto ticks = [](FILETIME t) {
        return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return double(ticks(kernel) + ticks(user))*1e-7;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1;
    return double(usage.ru_utime.tv_sec) + usage.ru_utime.tv_usec*1e-6 +
           double(usage.ru_stime.tv_sec) + usage.ru_stime.tv_usec*1e-6;
#endif
}
float residentMiB() {
    constexpr double mib = 1024.0*1024.0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) return -1;
    return float(memory.WorkingSetSize/mib);
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) return -1;
    return float(info.resident_size/mib);
#else
    // Unbuffered /proc read avoids stdio's lazily allocated file buffer.
    int fd = open("/proc/self/statm", O_RDONLY);
    if (fd < 0) return -1;
    char text[128]{};
    ssize_t size = read(fd, text, sizeof(text)-1);
    close(fd);
    unsigned long long total = 0, resident = 0;
    long page = sysconf(_SC_PAGESIZE);
    if (size <= 0 || page <= 0 || std::sscanf(text, "%llu %llu", &total, &resident) != 2) return -1;
    return float(double(resident)*page/mib);
#endif
}
}

void ProcessUsage::update(float elapsedSeconds) {
    untilSample -= elapsedSeconds;
    if (untilSample > 0.0f) return;
    untilSample = 1.0f;
    double wall = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    double cpu = cpuSeconds();
    cpuPercent = -1.0f;
    if (previousCpu >= 0 && cpu >= previousCpu && wall > previousWall)
        cpuPercent = float(100.0*(cpu-previousCpu)/(wall-previousWall));
    previousWall = wall;
    previousCpu = cpu;
    residentMiB = ::residentMiB();
}
