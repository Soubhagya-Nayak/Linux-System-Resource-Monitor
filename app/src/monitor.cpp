#include "monitor.hpp"
#include "driver_client.hpp"

#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>

namespace monitor {
bool update_cpu_usage(const CpuCounters &current, CpuCounters &previous,
                      bool &have_previous, double &usage_percent)
{
    const bool counters_decreased = current.user < previous.user ||
        current.nice < previous.nice || current.system < previous.system ||
        current.idle < previous.idle || current.iowait < previous.iowait ||
        current.irq < previous.irq || current.softirq < previous.softirq ||
        current.steal < previous.steal;

    bool sample_available = false;
    if (have_previous && !counters_decreased) {
        const std::uint64_t previous_total = previous.user + previous.nice + previous.system +
            previous.idle + previous.iowait + previous.irq + previous.softirq + previous.steal;
        const std::uint64_t current_total = current.user + current.nice + current.system +
            current.idle + current.iowait + current.irq + current.softirq + current.steal;
        if (current_total > previous_total) {
            const std::uint64_t total_delta = current_total - previous_total;
            const std::uint64_t previous_idle = previous.idle + previous.iowait;
            const std::uint64_t current_idle = current.idle + current.iowait;
            const std::uint64_t idle_delta = current_idle - previous_idle;
            if (idle_delta <= total_delta) {
                usage_percent = 100.0 * (total_delta - idle_delta) / total_delta;
                sample_available = true;
            }
        }
    }

    // Even an incomparable sample becomes the baseline for the next interval.
    previous = current;
    have_previous = true;
    return sample_available;
}

namespace {
bool numeric_name(const char *name)
{
    if (!*name) return false;
    for (; *name; ++name) if (*name < '0' || *name > '9') return false;
    return true;
}

void read_cpu(CpuStats &cpu, CpuCounters &previous, bool &have_previous)
{
    std::ifstream input("/proc/stat");
    std::string label;
    CpuCounters current;
    if (!(input >> label >> current.user >> current.nice >> current.system >> current.idle
                 >> current.iowait >> current.irq >> current.softirq >> current.steal))
        return;
    cpu.available = update_cpu_usage(current, previous, have_previous, cpu.usage_percent);
}

void read_memory(MemoryStats &memory)
{
    std::ifstream input("/proc/meminfo");
    std::string key, unit;
    std::uint64_t value;
    std::uint64_t total_kb = 0, available_kb = 0, free_kb = 0, buffers_kb = 0, cached_kb = 0;
    while (input >> key >> value >> unit) {
        if (key == "MemTotal:") total_kb = value;
        else if (key == "MemAvailable:") available_kb = value;
        else if (key == "MemFree:") free_kb = value;
        else if (key == "Buffers:") buffers_kb = value;
        else if (key == "Cached:") cached_kb = value;
    }
    if (!available_kb) available_kb = free_kb + buffers_kb + cached_kb;
    memory.total = total_kb * 1024;
    memory.available = available_kb * 1024;
    memory.used = memory.total > memory.available ? memory.total - memory.available : 0;
    if (memory.total) memory.usage_percent = 100.0 * memory.used / memory.total;
}

bool read_process_stat(int pid, ProcessCpuSample &sample)
{
    std::ifstream input(std::string("/proc/") + std::to_string(pid) + "/stat");
    std::string line;
    if (!std::getline(input, line)) return false;
    // The command name is parenthesized and can itself contain spaces or ')'.
    const auto command_end = line.rfind(')');
    if (command_end == std::string::npos) return false;

    std::istringstream fields(line.substr(command_end + 1));
    std::string field;
    std::uint64_t user_ticks = 0, system_ticks = 0;
    bool have_user = false, have_system = false, have_start_time = false;
    for (std::size_t index = 0; index <= 19; ++index) {
        if (!(fields >> field)) return false;
        try {
            // After ')', index 0 is stat field 3 (state).
            if (index == 11) { user_ticks = std::stoull(field); have_user = true; }
            else if (index == 12) { system_ticks = std::stoull(field); have_system = true; }
            else if (index == 19) {
                sample.start_time_ticks = std::stoull(field);
                have_start_time = true;
            }
        } catch (...) {
            return false;
        }
    }
    if (!have_user || !have_system || !have_start_time) return false;
    sample.cpu_ticks = user_ticks + system_ticks;
    return true;
}

bool read_processes(Snapshot &snapshot, std::size_t limit,
                    const std::unordered_map<int, ProcessCpuSample> &previous_samples,
                    std::chrono::steady_clock::time_point previous_sample_time,
                    bool have_previous_sample,
                    std::unordered_map<int, ProcessCpuSample> &current_samples,
                    std::chrono::steady_clock::time_point &sample_time)
{
    DIR *proc = opendir("/proc");
    if (!proc) return false;
    while (dirent *entry = readdir(proc)) {
        if (!numeric_name(entry->d_name)) continue;
        ++snapshot.process_count;
        ProcessInfo process;
        try { process.pid = std::stoi(entry->d_name); } catch (...) { continue; }
        std::ifstream comm(std::string("/proc/") + entry->d_name + "/comm");
        std::getline(comm, process.name);
        if (process.name.empty()) continue;

        ProcessCpuSample current_sample;
        if (read_process_stat(process.pid, current_sample)) {
            current_samples.emplace(process.pid, current_sample);
            const auto previous = previous_samples.find(process.pid);
            if (have_previous_sample && previous != previous_samples.end() &&
                previous->second.start_time_ticks == current_sample.start_time_ticks &&
                current_sample.cpu_ticks >= previous->second.cpu_ticks)
                process.cpu_available = true;
        }
        snapshot.processes.push_back(std::move(process));
    }
    closedir(proc);

    sample_time = std::chrono::steady_clock::now();
    const double elapsed_seconds = have_previous_sample
        ? std::chrono::duration<double>(sample_time - previous_sample_time).count()
        : 0.0;
    const long clock_ticks_per_second = sysconf(_SC_CLK_TCK);
    if (elapsed_seconds > 0.0 && clock_ticks_per_second > 0) {
        for (auto &process : snapshot.processes) {
            const auto current = current_samples.find(process.pid);
            const auto previous = previous_samples.find(process.pid);
            if (current == current_samples.end() || previous == previous_samples.end() ||
                !process.cpu_available)
                continue;
            const auto tick_delta = current->second.cpu_ticks - previous->second.cpu_ticks;
            process.cpu_percent = 100.0 * static_cast<double>(tick_delta) /
                                  (static_cast<double>(clock_ticks_per_second) * elapsed_seconds);
        }
    } else {
        for (auto &process : snapshot.processes) process.cpu_available = false;
    }

    // Highest CPU first. Ties and unavailable samples use descending PID.
    std::sort(snapshot.processes.begin(), snapshot.processes.end(),
              [](const ProcessInfo &left, const ProcessInfo &right) {
                  const double left_cpu = left.cpu_available ? left.cpu_percent : 0.0;
                  const double right_cpu = right.cpu_available ? right.cpu_percent : 0.0;
                  if (left_cpu != right_cpu) return left_cpu > right_cpu;
                  return left.pid > right.pid;
              });
    if (snapshot.processes.size() > limit) snapshot.processes.resize(limit);
    return true;
}
}

Snapshot ResourceCollector::collect(std::size_t process_limit) const
{
    Snapshot result;
    read_cpu(result.cpu, previous_cpu_counters_, have_cpu_sample_);
    read_memory(result.memory);
    std::unordered_map<int, ProcessCpuSample> current_process_samples;
    std::chrono::steady_clock::time_point process_sample_time;
    if (read_processes(result, process_limit, previous_process_samples_,
                       previous_process_sample_time_, have_process_sample_,
                       current_process_samples, process_sample_time)) {
        previous_process_samples_ = std::move(current_process_samples);
        previous_process_sample_time_ = process_sample_time;
        have_process_sample_ = true;
    }

    std::ifstream uptime("/proc/uptime");
    uptime >> result.uptime_seconds;

    struct statvfs filesystem{};
    if (statvfs("/", &filesystem) == 0) {
        const std::uint64_t block_size = filesystem.f_frsize;
        result.disk.total = static_cast<std::uint64_t>(filesystem.f_blocks) * block_size;
        const std::uint64_t available_bytes = static_cast<std::uint64_t>(filesystem.f_bavail) * block_size;
        // Report space unavailable to this user as used, including reserved blocks.
        result.disk.available = available_bytes < result.disk.total ? available_bytes : result.disk.total;
        result.disk.used = result.disk.total - result.disk.available;
        if (result.disk.total) result.disk.usage_percent = 100.0 * result.disk.used / result.disk.total;
        result.disk.mount_point = "/";
    }

    DriverClient client;
    result.driver_available = client.read_stats(result.driver_stats);
    return result;
}
}
