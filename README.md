# Linux System Resource Monitor with Custom Character Device Driver

A modular terminal system monitor written in C++ with a companion Linux character device driver written in C. The application reads standard Linux interfaces for a portable monitoring view and uses the driver ioctl when the module is installed.

## Problem statement

Linux exposes resource information through several interfaces. This project combines those interfaces into one small terminal application and demonstrates how a custom kernel character device can provide a controlled, versioned interface to user space.

## Objectives

- Display CPU, RAM, root filesystem, uptime, and process information.
- Separate user interface, collection, and driver communication responsibilities.
- Demonstrate character-device registration, file operations, safe user copying, and ioctl.
- Build with Make and document supported and unsupported validation environments accurately.

## Features

- Live terminal refresh every two seconds, or one-shot output with `--once`.
- CPU utilization from deltas in `/proc/stat`; memory from `/proc/meminfo`.
- Root filesystem capacity from `statvfs(2)`, uptime from `/proc/uptime`, and process count/sample names from `/proc`.
- A total count over numeric `/proc` entries and up to eight sample processes ranked by measured per-process CPU use.
- Optional driver data through `/dev/resource_monitor` and `RM_IOCTL_GET_STATS`.
- Continues in fallback mode if the device is absent or inaccessible.

## Technologies

C++17, C (Linux kernel module), Linux `/proc`, `statvfs(2)`, Linux character-device APIs, ioctl, and GNU Make. No external libraries are required.

## Architecture

```text
Terminal UI (app/src/main.cpp)
              |
Resource collection (app/src/monitor.cpp)
       |                     |
/proc + statvfs          Driver client (ioctl)
                             |
Shared ABI (include/resource_monitor_abi.h)
                             |
Character device (/dev/resource_monitor)
                             |
Linux kernel
```

The collector owns Linux resource sampling, the driver client handles only the device ABI, and the UI formats a snapshot. This keeps the user-space application useful without module privileges or an installed module.

## Directory structure

```text
app/include/       C++ interfaces
app/src/           C++ application and collectors
driver/            Kernel character device module
include/           Shared ioctl ABI
docs/              Architecture and workflow notes
Makefile           Application, driver, test, and clean targets
```

## How the driver works

The module reserves a device number, adds a `cdev`, creates a device class and `/dev/resource_monitor` node through sysfs/udev, and removes each resource in reverse order on unload. Its read operation returns a small textual snapshot. Its ioctl returns a fixed-width, versioned structure with kernel uptime and total/free RAM page counts converted to bytes. `copy_to_user` is used for the kernel-to-user transfer; unknown ioctl commands return `ENOTTY`. The driver is read-only and does not accept writes because the monitor has no kernel configuration to change.

The device node is normally created by udev. If it is absent after module load, inspect `dmesg` and `/sys/class/resource_monitor/`; create a node using the major number printed by the module only as a troubleshooting step, then remove it when unloading.

## User-space communication

`DriverClient` opens `/dev/resource_monitor` read-only and requests `RM_IOCTL_GET_STATS`. It checks both ABI version and structure size. If open or ioctl fails, the application continues using `/proc` and `statvfs(2)` for its resource view and reports that the driver is unavailable. The ABI is shared by kernel C and user-space C++ using fixed-width integer types.

## Build

Build the application:

```sh
make app
```

Build the kernel module when matching kernel headers are installed:

```sh
make driver
# or: make driver KDIR=/path/to/kernel/build
```

`make` builds the application by default. `make clean` removes generated application and module build artifacts.

## Run

```sh
./build/resource-monitor
./build/resource-monitor --once
```

The live view refreshes every two seconds. CPU use on the first sample is 0 until a prior sample exists; the live view will show a measured delta on its next refresh.

## Load and unload the driver

On a supported Linux host, build against the running kernel and load/unload with administrator privileges:

```sh
make driver
sudo insmod driver/resource_monitor.ko
ls -l /dev/resource_monitor
./build/resource-monitor --once
sudo rmmod resource_monitor
```

Loading a module changes the host kernel and requires explicit administrator action. Do not run the load commands on a system where you do not control kernel modules.

## Testing

```sh
make test
```

The user-space application was built and its smoke test and one-shot demo were run successfully in Ubuntu 24.04 under WSL2. `make test` runs a deterministic CPU-counter test and a one-shot application smoke check for CPU, RAM, disk, process count, and process table fields. The custom character-device driver is implemented, but it was not built or loaded in this environment: the running `6.6.87.2-microsoft-standard-WSL2` kernel has no matching build tree or headers at `/lib/modules/$(uname -r)/build`. The driver has therefore not been kernel-compile- or load-tested here.

To test the driver, use a native Linux system or disposable VM with kernel development headers matching the exact running kernel and support for loading modules. Run `make driver`, load with `sudo insmod driver/resource_monitor.ko`, verify `/dev/resource_monitor`, run the app to exercise its ioctl path, and unload with `sudo rmmod resource_monitor`. These module operations require suitable privileges. On systems without the device, the application's `/proc` and filesystem (`statvfs`) fallback remains available.

## Example output

```text
Linux System Resource Monitor
CPU usage:       n/a (waiting for comparable samples)
RAM usage:       37.8% (5.9 / 15.6 GiB; 9.7 GiB available)
Root disk:       42.1% (20.2 / 48.0 GiB)
System uptime:   86400 seconds
Processes:       176
Driver interface: unavailable (proc/filesystem fallback active)

Sample processes (highest PID until CPU samples are available):
      PID  CPU%/core  NAME
      432      n/a  bash
      420      n/a  systemd
```

Values vary by machine. The example shows the initial/one-shot sample. The process count includes every numeric `/proc` directory seen during enumeration, even if that process exits before its details can be read. The sample shows up to eight processes, ranked by CPU ticks consumed between monitor refreshes. Per-process CPU is `n/a` on the first sample, for newly observed processes, or when counters cannot be compared; equal or unavailable CPU values are ordered by descending PID. This makes high PIDs a useful initial fallback, though PID order only approximates process recency. `CPU%/core` is relative to one logical CPU core: 100% means one fully utilized core, and a multithreaded process can legitimately exceed 100% when it uses multiple cores.

## Limitations

- CPU is system-wide and sampled as a delta; the first sample and a counter-reset interval are shown as `n/a` until comparable samples are available again.
- Per-process CPU percentages require two comparable `/proc/<pid>/stat` samples; they appear as `n/a` for the initial sample and newly observed processes.
- Disk figures describe the filesystem containing `/`, not every mounted filesystem.
- The process count may vary during enumeration and is limited by `/proc` visibility.
- The module's free memory is the kernel free-page count, not `MemAvailable`.
- The module has no compatibility guarantee across arbitrary kernel internals; build against the exact target kernel headers.
- This is an educational monitor, not a privileged performance or security tool.

## Future improvements

Add per-core CPU and per-process memory sampling, selectable mount points, signal-safe terminal shutdown, and kernel integration testing in a disposable VM. Any ABI expansion should increment or otherwise explicitly manage its version.

## Linux and kernel compatibility

The user-space program targets Linux and uses standard Linux interfaces. The module uses normal out-of-tree Kbuild conventions and handles the `class_create` signature change introduced in Linux 6.4. Kernel APIs can change, so the module must be built against matching headers. In the WSL2 development environment used for this project, the Microsoft kernel has no matching build tree/header package in the Ubuntu guest; user-space build and tests work, while module build and load testing require a compatible native Linux or VM kernel with matching headers.

## License

GPL-2.0-only. See [LICENSE](LICENSE).
