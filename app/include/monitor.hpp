#ifndef MONITOR_HPP
#define MONITOR_HPP

#include "resource_monitor_abi.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace monitor {
struct CpuStats { double usage_percent{}; bool available{}; };
struct CpuCounters {
    std::uint64_t user{}, nice{}, system{}, idle{}, iowait{}, irq{}, softirq{}, steal{};
};
bool update_cpu_usage(const CpuCounters &current, CpuCounters &previous,
                      bool &have_previous, double &usage_percent);
struct MemoryStats { std::uint64_t total{}, available{}, used{}; double usage_percent{}; };
struct DiskStats { std::uint64_t total{}, used{}, available{}; double usage_percent{}; std::string mount_point; };
struct ProcessInfo {
    int pid{};
    std::string name;
    double cpu_percent{};
    bool cpu_available{};
};
struct ProcessCpuSample {
    std::uint64_t cpu_ticks{};
    std::uint64_t start_time_ticks{};
};
struct Snapshot {
    CpuStats cpu;
    MemoryStats memory;
    DiskStats disk;
    double uptime_seconds{};
    std::size_t process_count{};
    std::vector<ProcessInfo> processes;
    bool driver_available{};
    rm_kernel_stats driver_stats{};
};

class ResourceCollector {
public:
    Snapshot collect(std::size_t process_limit = 8) const;
private:
    mutable CpuCounters previous_cpu_counters_{};
    mutable bool have_cpu_sample_{false};
    mutable std::unordered_map<int, ProcessCpuSample> previous_process_samples_;
    mutable std::chrono::steady_clock::time_point previous_process_sample_time_{};
    mutable bool have_process_sample_{false};
};
}
#endif
