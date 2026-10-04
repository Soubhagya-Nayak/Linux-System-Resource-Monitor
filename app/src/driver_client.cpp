#include "driver_client.hpp"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace monitor {
bool DriverClient::read_stats(rm_kernel_stats &stats) const
{
    const int fd = open(RM_DEVICE_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    const bool success = ioctl(fd, RM_IOCTL_GET_STATS, &stats) == 0 &&
                         stats.abi_version == RM_ABI_VERSION &&
                         stats.struct_size >= sizeof(rm_kernel_stats);
    close(fd);
    return success;
}
}
