#ifndef DRIVER_CLIENT_HPP
#define DRIVER_CLIENT_HPP
#include "resource_monitor_abi.h"
namespace monitor {
class DriverClient {
public:
    bool read_stats(rm_kernel_stats &stats) const;
};
}
#endif
