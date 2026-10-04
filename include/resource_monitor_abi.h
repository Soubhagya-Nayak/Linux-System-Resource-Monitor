#ifndef RESOURCE_MONITOR_ABI_H
#define RESOURCE_MONITOR_ABI_H

/* Shared fixed-width interface between the kernel module and user space. */
#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __u32 rm_u32;
typedef __u64 rm_u64;
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint32_t rm_u32;
typedef uint64_t rm_u64;
#endif

#define RM_ABI_VERSION 1U
#define RM_DEVICE_PATH "/dev/resource_monitor"

struct rm_kernel_stats {
    rm_u32 abi_version;
    rm_u32 struct_size;
    rm_u64 uptime_seconds;
    rm_u64 total_memory_bytes;
    rm_u64 free_memory_bytes;
};

#define RM_IOCTL_GET_STATS _IOR('R', 1, struct rm_kernel_stats)

#endif
