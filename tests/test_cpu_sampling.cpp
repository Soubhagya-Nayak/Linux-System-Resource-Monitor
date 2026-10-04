#include "monitor.hpp"

#include <cmath>
#include <iostream>

namespace {
bool same_counters(const monitor::CpuCounters &left, const monitor::CpuCounters &right)
{
    return left.user == right.user && left.nice == right.nice &&
           left.system == right.system && left.idle == right.idle &&
           left.iowait == right.iowait && left.irq == right.irq &&
           left.softirq == right.softirq && left.steal == right.steal;
}

bool expect(bool condition, const char *message)
{
    if (!condition) std::cerr << "CPU sampling test failed: " << message << '\n';
    return condition;
}
}

int main()
{
    monitor::CpuCounters previous{};
    bool have_previous = false;
    double usage = -1.0;
    monitor::CpuCounters current{100, 0, 100, 800, 50, 0, 0, 0};

    if (!expect(!monitor::update_cpu_usage(current, previous, have_previous, usage),
                "first sample must be unavailable") ||
        !expect(same_counters(previous, current), "first sample must set the baseline"))
        return 1;

    current.user += 10;
    current.idle += 90;
    if (!expect(monitor::update_cpu_usage(current, previous, have_previous, usage),
                "increasing counters must produce a sample") ||
        !expect(std::abs(usage - 10.0) < 0.001, "normal delta percentage is incorrect"))
        return 1;

    current.user += 10;
    current.idle -= 1;
    if (!expect(!monitor::update_cpu_usage(current, previous, have_previous, usage),
                "decreasing idle counter must invalidate sample") ||
        !expect(std::abs(usage - 10.0) < 0.001, "invalid sample must not overwrite prior usage") ||
        !expect(same_counters(previous, current), "idle regression must refresh baseline"))
        return 1;

    current.user += 10;
    current.idle += 90;
    if (!expect(monitor::update_cpu_usage(current, previous, have_previous, usage),
                "sampling must recover after idle regression"))
        return 1;

    current.user += 10;
    current.iowait -= 1;
    if (!expect(!monitor::update_cpu_usage(current, previous, have_previous, usage),
                "decreasing iowait counter must invalidate sample") ||
        !expect(same_counters(previous, current), "iowait regression must refresh baseline"))
        return 1;

    current.user += 10;
    current.idle += 90;
    if (!expect(monitor::update_cpu_usage(current, previous, have_previous, usage),
                "sampling must recover after iowait regression"))
        return 1;

    std::cout << "CPU counter sampling test passed\n";
    return 0;
}
