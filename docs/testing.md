# Testing strategy

`make test` runs the compiled program once and checks that its core output fields exist. This exercises proc parsing and root filesystem collection in the current host. When matching kernel headers are available, `make driver` checks module compilation. Module insertion, ioctl, read, and removal should be exercised in a disposable Linux VM matching the module build kernel; module loading is intentionally not part of the automated smoke test because it changes the running kernel.
