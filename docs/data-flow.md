# Data flow

1. The terminal loop asks `ResourceCollector` for one snapshot.
2. The collector samples Linux proc files, counts all numeric `/proc` entries, samples each readable process's CPU ticks and start time, and queries the root filesystem with `statvfs`.
3. `DriverClient` attempts to open `/dev/resource_monitor` and call `RM_IOCTL_GET_STATS`.
4. A successful response is version-checked and included in the snapshot; a missing device leaves `/proc` and filesystem (`statvfs`) collection intact.
5. The UI formats all fields for one-shot output or refreshes the display.

The collector ranks up to eight readable processes by CPU ticks accumulated since the previous refresh. A process without a comparable prior sample has CPU shown as `n/a`; ties and the initial sample use descending PID as a simple fallback that often surfaces recently created processes.

The displayed `CPU%/core` is relative to one logical CPU core: 100% means one fully utilized core. A multithreaded process can legitimately exceed 100% when it uses multiple cores.

System CPU sampling reports `n/a` when there is no prior sample or any cumulative `/proc/stat` counter decreases. The current counters are saved as the new baseline so the following comparable interval can produce a percentage.
