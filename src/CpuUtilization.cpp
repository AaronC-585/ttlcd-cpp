// CpuUtilization.cpp
#include "CpuUtilization.hpp"
#include <sstream>

CpuUtilizationTracker::CpuTimes CpuUtilizationTracker::read_cpu_times() {
    CpuTimes t;
    std::ifstream f("/proc/stat");
    if (!f.is_open()) return t;

    std::string line;
    if (!std::getline(f, line) || line.rfind("cpu ", 0) != 0) return t;

    std::istringstream iss(line.substr(4));
    iss >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
    return t;
}

double CpuUtilizationTracker::update_and_get_percent() {
    CpuTimes curr = read_cpu_times();
    if (!initialized_) {
        prev_ = curr;
        initialized_ = true;
        return 0.0;
    }

    auto total_diff = curr.total() - prev_.total();
    auto idle_diff = curr.idle_time() - prev_.idle_time();

    if (total_diff == 0) return 0.0;

    double util = 100.0 * (total_diff - idle_diff) / total_diff;
    prev_ = curr;
    return util;
}