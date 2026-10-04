// SPDX-License-Identifier: GPL-2.0
/* A small read-only character device exposing a stable kernel snapshot ABI. */
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/ktime.h>
#include <linux/device.h>

#include "../include/resource_monitor_abi.h"

#define DEVICE_NAME "resource_monitor"

static dev_t rm_dev;
static struct cdev rm_cdev;
static struct class *rm_class;

static void rm_collect_stats(struct rm_kernel_stats *stats)
{
    const unsigned int shift = PAGE_SHIFT;

    memset(stats, 0, sizeof(*stats));
    stats->abi_version = RM_ABI_VERSION;
    stats->struct_size = sizeof(*stats);
    stats->uptime_seconds = ktime_get_boottime_seconds();
    /* totalram_pages() and global_page_state() are page counts. */
    stats->total_memory_bytes = (rm_u64)totalram_pages() << shift;
    stats->free_memory_bytes = (rm_u64)global_zone_page_state(NR_FREE_PAGES) << shift;
}

static ssize_t rm_read(struct file *file, char __user *buffer, size_t count,
                       loff_t *position)
{
    char snapshot[192];
    struct rm_kernel_stats stats;
    int length;

    rm_collect_stats(&stats);
    length = scnprintf(snapshot, sizeof(snapshot),
                       "abi_version=%u\nuptime_seconds=%llu\n"
                       "total_memory_bytes=%llu\nfree_memory_bytes=%llu\n",
                       stats.abi_version,
                       (unsigned long long)stats.uptime_seconds,
                       (unsigned long long)stats.total_memory_bytes,
                       (unsigned long long)stats.free_memory_bytes);
    return simple_read_from_buffer(buffer, count, position, snapshot, length);
}

static long rm_ioctl(struct file *file, unsigned int command, unsigned long argument)
{
    struct rm_kernel_stats stats;

    if (command != RM_IOCTL_GET_STATS)
        return -ENOTTY;

    rm_collect_stats(&stats);
    if (copy_to_user((void __user *)argument, &stats, sizeof(stats)))
        return -EFAULT;
    return 0;
}

static int rm_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int rm_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations rm_fops = {
    .owner = THIS_MODULE,
    .open = rm_open,
    .read = rm_read,
    .unlocked_ioctl = rm_ioctl,
    .release = rm_release,
    .llseek = no_llseek,
};

static int __init rm_init(void)
{
    struct device *device;
    int result;

    result = alloc_chrdev_region(&rm_dev, 0, 1, DEVICE_NAME);
    if (result)
        return result;

    cdev_init(&rm_cdev, &rm_fops);
    rm_cdev.owner = THIS_MODULE;
    result = cdev_add(&rm_cdev, rm_dev, 1);
    if (result)
        goto unregister_number;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    rm_class = class_create(DEVICE_NAME);
#else
    rm_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(rm_class)) {
        result = PTR_ERR(rm_class);
        goto delete_cdev;
    }

    device = device_create(rm_class, NULL, rm_dev, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        result = PTR_ERR(device);
        goto destroy_class;
    }

    pr_info(DEVICE_NAME ": registered major=%u minor=%u\n",
            MAJOR(rm_dev), MINOR(rm_dev));
    return 0;

destroy_class:
    class_destroy(rm_class);
delete_cdev:
    cdev_del(&rm_cdev);
unregister_number:
    unregister_chrdev_region(rm_dev, 1);
    return result;
}

static void __exit rm_exit(void)
{
    device_destroy(rm_class, rm_dev);
    class_destroy(rm_class);
    cdev_del(&rm_cdev);
    unregister_chrdev_region(rm_dev, 1);
    pr_info(DEVICE_NAME ": unregistered\n");
}

module_init(rm_init);
module_exit(rm_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux System Resource Monitor project");
MODULE_DESCRIPTION("Character device exposing system resource snapshots");
MODULE_VERSION("1.0");
