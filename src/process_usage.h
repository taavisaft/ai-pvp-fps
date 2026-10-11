#pragma once

// Client process usage. CPU is summed across threads: 100% = one CPU core.
// RAM is resident memory (working set on Windows), not reserved address space.
struct ProcessUsage {
    float cpuPercent = -1.0f;
    float residentMiB = -1.0f;
    void update(float elapsedSeconds);
private:
    float untilSample = 0.0f;
    double previousWall = 0.0;
    double previousCpu = -1.0;
};
