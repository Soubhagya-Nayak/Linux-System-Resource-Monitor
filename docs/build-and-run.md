# Build and execution

`make app` invokes G++ in C++17 mode and writes `build/resource-monitor`. `make driver` delegates to the kernel's Kbuild tree at `/lib/modules/$(uname -r)/build`, overridable with `KDIR`. The module target checks for the build-tree Makefile and prints a targeted diagnostic if headers are missing. Run with `--once` for a single snapshot or without arguments for a two-second live display. `make clean` removes generated artifacts.
