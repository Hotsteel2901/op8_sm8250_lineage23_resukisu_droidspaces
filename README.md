# OnePlus 8 (Instantnoodle) Kernel — LineageOS 23.2 + ReSukiSU + Droidspaces

Linux 4.19.325 kernel for OnePlus 8 series (SM8250/Kona) with
ReSukiSU root and Droidspaces container support.

## Supported Devices

| Device | Codename |
|--------|----------|
| OnePlus 8 | instantnoodle |
| OnePlus 8 Pro | instantnoodlep |
| OnePlus 8T | kebab |
| OnePlus 9R | lemonades |

## Features

- **ReSukiSU** — KernelSU non-GKI fork, manual hook mode (4 hooks)
- **Droidspaces** — Full container support (namespaces, cgroups, overlayfs, networking)
- **OPlus vendor drivers** — Charging, touch, fingerprint, display, camera

## Kernel Config Highlights

| Config | Status | Purpose |
|--------|--------|---------|
| `CONFIG_KSU` | y | ReSukiSU |
| `CONFIG_KSU_MANUAL_HOOK` | y | Manual hook mode |
| `CONFIG_USER_NS` | y | Container user namespaces |
| `CONFIG_DEVTMPFS` | y | Container /dev access |
| `CONFIG_AUTOFS4_FS` | y | systemd containers |
| `CONFIG_OVERLAY_FS` | y | Container volatile mode |
| `CONFIG_PID_NS / IPC_NS / UTS_NS / NET_NS` | y | Full namespace isolation |
| `CONFIG_CGROUP_DEVICE / CONFIG_CGROUP_PIDS` | y | Container cgroup limits |
| `CONFIG_SECCOMP` | y | Container security |
| `CONFIG_VETH / CONFIG_BRIDGE` | y | Container NAT networking |
| `CONFIG_FHANDLE` | y | Android storage |
| `CONFIG_PSTORE` | y | Crash logs |

## Build Guide

See [ReSukiSU-SUSFS-ONEPLUS-SM8250-BUILD-GUIDE.md](ReSukiSU-SUSFS-ONEPLUS-SM8250-BUILD-GUIDE.md)

## Flash

1. TWRP → Install → AnyKernel3 zip → swipe to flash
2. Reboot

**Do not** `fastboot flash boot Image` — this overwrites DTB and ramdisk.

## Droidspaces Usage

After flashing, run `droidspaces check` to verify all requirements pass.
Use a container image compatible with Linux 4.19 (Debian 11 / Ubuntu 20.04 or similar).

## Related Projects

- [KernelSU](https://github.com/tiann/KernelSU)
- [ReSukiSU](https://github.com/ReSukiSU/ReSukiSU)
- [Droidspaces](https://t.me/Droidspaces)
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3)
