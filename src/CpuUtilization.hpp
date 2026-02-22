#pragma once
#include <fstream>
#include <string>

class CpuUtilizationTracker {
private:
    struct CpuTimes {
        unsigned long long user = 0, nice = 0, system = 0, idle = 0;
        unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;

        unsigned long long total() const { return user + nice + system + idle + iowait + irq + softirq + steal; }
        unsigned long long idle_time() const { return idle + iowait; }
    };

    CpuTimes prev_;
    bool initialized_ = false;

public:
    double update_and_get_percent();
private:
    CpuTimes read_cpu_times();
};