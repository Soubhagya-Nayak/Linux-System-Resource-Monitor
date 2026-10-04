# Driver architecture

`resource_monitor.c` reserves one major/minor number, associates it with `file_operations`, adds a `cdev`, and asks the device model to create a class and device. Open/release are stateless. Read uses `simple_read_from_buffer`; ioctl validates the command, collects a snapshot, and transfers it using `copy_to_user`. Initialization unwinds partial failures; exit destroys the device, class, cdev, and number in reverse order.

The ABI carries a version and size so clients can reject incompatible layouts. It exposes only uptime and memory page counters and does not allow writes or arbitrary kernel memory access.
