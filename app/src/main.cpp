#include "monitor.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {
double gib(std::uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0); }
void print_snapshot(const monitor::Snapshot &s)
{
    std::cout << std::fixed << std::setprecision(1)
              << "Linux System Resource Monitor\n"
              << "CPU usage:       ";
    if (s.cpu.available)
        std::cout << std::fixed << std::setprecision(1) << s.cpu.usage_percent << "%\n";
    else
        std::cout << "n/a (waiting for comparable samples)\n";
    std::cout
              << "RAM usage:       " << s.memory.usage_percent << "% (" << gib(s.memory.used) << " / " << gib(s.memory.total) << " GiB; " << gib(s.memory.available) << " GiB available)\n"
              << "Root disk:       " << s.disk.usage_percent << "% (" << gib(s.disk.used) << " / " << gib(s.disk.total) << " GiB)\n"
              << "System uptime:   " << std::setprecision(0) << s.uptime_seconds << " seconds\n"
              << "Processes:       " << s.process_count << "\n"
              << "Driver interface: " << (s.driver_available ? "connected" : "unavailable (proc/filesystem fallback active)") << "\n";
    if (s.driver_available)
        std::cout << "Driver uptime:   " << s.driver_stats.uptime_seconds << " seconds\n"
                  << "Driver RAM:      " << gib(s.driver_stats.total_memory_bytes) << " GiB total, "
                  << gib(s.driver_stats.free_memory_bytes) << " GiB free\n";
    const bool have_process_cpu = std::any_of(s.processes.begin(), s.processes.end(),
        [](const monitor::ProcessInfo &process) { return process.cpu_available; });
    std::cout << "\nSample processes (" << (have_process_cpu
        ? "highest sampled CPU; ties by PID"
        : "highest PID until CPU samples are available") << "):\n"
              << "  " << std::setw(7) << "PID" << "  " << std::setw(11) << "CPU%/core" << "  NAME\n";
    for (const auto &p : s.processes) {
        std::cout << "  " << std::setw(7) << p.pid << "  ";
        if (p.cpu_available)
            std::cout << std::setw(6) << std::fixed << std::setprecision(1) << p.cpu_percent << "%";
        else
            std::cout << std::setw(7) << "n/a";
        std::cout << "  " << p.name << '\n';
    }
}
}

int main(int argc, char **argv)
{
    bool once = false;
    if (argc == 2 && std::string(argv[1]) == "--once") once = true;
    else if (argc > 1) {
        std::cerr << "Usage: " << argv[0] << " [--once]\n";
        return 2;
    }

    monitor::ResourceCollector collector;
    do {
        auto snapshot = collector.collect();
        if (once) print_snapshot(snapshot);
        else {
            std::cout << "\033[2J\033[H";
            print_snapshot(snapshot);
            std::cout << "\nRefreshing every 2 seconds. Press Ctrl+C to exit.\n" << std::flush;
            sleep(2);
        }
    } while (!once);
    return 0;
}
