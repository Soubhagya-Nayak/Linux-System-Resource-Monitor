# Software architecture

The C++ application uses three boundaries:

1. `main.cpp` formats snapshots and manages the terminal refresh loop.
2. `monitor.cpp` gathers `/proc` and filesystem (`statvfs`) values into value types.
3. `driver_client.cpp` owns opening the device and issuing the ioctl.

The shared ABI header defines the contract without sharing implementation. Sampling remains in user space for portability. Driver data supplements this collection and proves the application-to-device path when available.
