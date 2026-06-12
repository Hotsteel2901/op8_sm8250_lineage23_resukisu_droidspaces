# OnePlus 8 (SM8250) Kernel Build Guide

## ReSukiSU + Droidspaces — LineageOS 23.2 — Linux 4.19.325

This guide covers building a fully-featured custom kernel for the OnePlus 8 series
(instantnoodle / instantnoodlep / kebab / lemonades) with ReSukiSU root and
Droidspaces container support.

---

## Table of Contents

1.  [Prerequisites](#prerequisites)
2.  [Clone the Repository](#clone-the-repository)
3.  [Toolchain Setup](#toolchain-setup)
4.  [Kernel Configuration](#kernel-configuration)
5.  [Build the Kernel](#build-the-kernel)
6.  [Package with AnyKernel3](#package-with-anykernel3)
7.  [Flash the Kernel](#flash-the-kernel)
8.  [Verify Everything Works](#verify-everything-works)
9.  [Updating ReSukiSU](#updating-resukisu)
10. [Applying Droidspaces Patches](#applying-droidspaces-patches)
11. [Troubleshooting](#troubleshooting)
12. [Reference: Manual Hooks](#reference-manual-hooks)

---

## Prerequisites

- **Linux build environment** (Ubuntu 20.04+ or WSL2 recommended)
- **Git** with at least 15 GB free disk space
- **GCC 9.3.0 Buildroot cross-compiler** for aarch64
  ([prebuilts download](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-a/downloads) or use the AOSP prebuilt)
- **AnyKernel3** template from [osm0sis/AnyKernel3](https://github.com/osm0sis/AnyKernel3)
- Required host packages:
  ```bash
  sudo apt install build-essential flex bison libssl-dev bc cpio zip
  ```

## Clone the Repository

```bash
git clone https://github.com/Hotsteel2901/op8_sm8250_lineage23_resukisu_droidspaces.git
cd op8_sm8250_lineage23_resukisu_droidspaces
git checkout lineage-23.2
```

> **Note:** The repo includes the full ReSukiSU kernel source under `KernelSU/` as a
> git-tracked directory (not a submodule). A `.git` file at `KernelSU/.git` points
> to the main repository so that `KSU_VERSION` is auto-calculated from the commit
> count at build time.

## Toolchain Setup

Set the following environment variables before building:

```bash
export ARCH=arm64
export CROSS_COMPILE=/path/to/aarch64-buildroot-linux-gnu-
export PATH="/path/to/toolchain/bin:$PATH"
```

### Recommended Toolchain

| Compiler | Version | Notes |
|----------|---------|-------|
| **GCC Buildroot** | 9.3.0 | Tested, stable. AOSP prebuilt: `android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3` |
| Clang / LLVM | 12+ | Untested. The kernel Makefile has Clang-specific flags but GCC is the recommended path. |

## Kernel Configuration

The kernel uses two config fragments that are merged at build time:

| Fragment | Path | Purpose |
|----------|------|---------|
| Base defconfig | `arch/arm64/configs/vendor/kona-perf_defconfig` | Qualcomm Kona platform base |
| OPlus vendor | `arch/arm64/configs/vendor/oplus.config` | OnePlus device drivers |

### Key Config Options (Built-In)

These are already enabled in the committed `.config` — no manual changes needed:

**ReSukiSU:**
```
CONFIG_KSU=y                           # KernelSU core
CONFIG_KSU_MANUAL_HOOK=y               # Manual hook mode (non-GKI)
CONFIG_KSU_MANUAL_HOOK_AUTO_SETUID_HOOK=y  # LSM auto-hook for setuid
CONFIG_KSU_MANUAL_HOOK_AUTO_INITRC_HOOK=y  # LSM auto-hook for init.rc
CONFIG_KSU_MANUAL_HOOK_AUTO_INPUT_HOOK=y   # Input handler auto-hook
CONFIG_KALLSYMS_ALL=y                  # Required by ReSukiSU (avoids SELinux symbol exports)
```

**Droidspaces (Container Support):**
```
CONFIG_USER_NS=y        CONFIG_PID_NS=y         CONFIG_IPC_NS=y
CONFIG_UTS_NS=y         CONFIG_NET_NS=y         CONFIG_CGROUP_DEVICE=y
CONFIG_CGROUP_PIDS=y    CONFIG_CGROUP_FREEZER=y CONFIG_OVERLAY_FS=y
CONFIG_VETH=y           CONFIG_BRIDGE=y         CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y CONFIG_AUTOFS4_FS=y     CONFIG_FANOTIFY=y
CONFIG_SECCOMP=y        CONFIG_SECCOMP_FILTER=y CONFIG_FHANDLE=y
CONFIG_SYSVIPC=y        CONFIG_POSIX_MQUEUE=y   CONFIG_CHECKPOINT_RESTORE=y
CONFIG_FW_LOADER_COMPRESS=y                     CONFIG_SECURITYFS=y
```

**OPlus Vendor:**
```
CONFIG_OPLUS_CHG_BASIC=y               # Charging driver
CONFIG_OPLUS_TP_BASIC=y                # Touch panel
CONFIG_OPLUS_FINGERPRINT=y             # Fingerprint sensor
# ... plus ~70 more vendor options defined in oplus.config
```

### Working with Config Changes

If you need to add or remove config options, edit `.config` directly and run:

```bash
make olddefconfig   # resolves dependencies, fills in defaults
```

Then verify the resulting `.config` before building.

## Build the Kernel

```bash
# Step 1: Refresh the config (resolves any dependency changes)
make olddefconfig

# Step 2: Build with parallel jobs
make -j$(nproc)
```

The build produces:

| File | Location | Size (approx.) |
|------|----------|-----------------|
| **Image** (kernel) | `arch/arm64/boot/Image` | ~55 MB |
| Device tree blobs | `arch/arm64/boot/dts/vendor/oplus/*.dtb` | ~470 KB each |
| DTBO overlays | `arch/arm64/boot/dts/vendor/oplus/*-overlay.dtbo` | ~200 KB each |

> **Optimization:** The kernel is compiled with `-O2` (CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE=y).
> To switch to size-optimized builds (`-Os`), change to `CONFIG_CC_OPTIMIZE_FOR_SIZE=y`.

## Package with AnyKernel3

AnyKernel3 creates a flashable ZIP that injects the kernel Image and DTBs into
the existing boot partition *without* overwriting the ramdisk or DTB layout.

```bash
# Clone AK3 template (one-time)
git clone https://github.com/osm0sis/AnyKernel3.git ~/AnyKernel3

# Copy built artifacts
cp arch/arm64/boot/Image                          ~/AnyKernel3/
cp arch/arm64/boot/dts/vendor/oplus/*.dtb          ~/AnyKernel3/
cp arch/arm64/boot/dts/vendor/oplus/*-overlay.dtbo ~/AnyKernel3/dtbo/

# Update kernel string in anykernel.sh (optional)
# kernel.string=ReSukiSU v4.1.0 - Linux 4.19.325 for OnePlus 8 Series

# Create the flashable zip
cd ~/AnyKernel3
zip -r ../ReSukiSU-kona-4.19.325-$(date +%Y%m%d).zip . -x ".git*" "*.ko"
```

The resulting ZIP (~25 MB) is ready to flash.

## Flash the Kernel

### Method 1: TWRP / Recovery (Recommended)

1.  Reboot to TWRP recovery
2.  Tap **Install** → select the AnyKernel3 ZIP
3.  Swipe to confirm flash
4.  **Reboot to system**

### Method 2: Kernel Flasher App

Apps like Kernel Flasher or Franco Kernel Manager can flash AnyKernel3 ZIPs
directly from Android without recovery.

### ⚠️ Do NOT use fastboot

```bash
# WRONG — this overwrites DTB and ramdisk!
fastboot flash boot Image
```

The OnePlus 8 has `BOARD_INCLUDE_DTB_IN_BOOTIMG := true` and a separated DTBO
partition. Always use AnyKernel3 or a proper boot image tool (`mkbootimg`).

## Verify Everything Works

### Check ReSukiSU

Open the **ReSukiSU Manager** app (install from [ReSukiSU releases](https://github.com/ReSukiSU/ReSukiSU/releases)):

- **Home page** should show "Working" (not "Not installed" or "Incompatible")
- **Version code**: 34967 (KSU_VERSION = 30000 + 4267 commits + 700)
- **Version name**: `v4.1.0-xxxxxxxx-hotsteel@ReSukiSU`

From a root shell:
```bash
su -c dmesg | grep -i ksu
```

### Check Droidspaces

From a root shell:
```bash
su -c droidspaces check
```

All green checkmarks = fully supported. Yellow warnings are optional features
(e.g., OverlayFS for volatile mode). The kernel passes all critical checks:

| Check | Status |
|-------|--------|
| PID / MNT / UTS / IPC namespaces | ✅ |
| Network namespace | ✅ |
| Cgroup device | ✅ |
| devtmpfs | ✅ |
| VETH / Bridge (NAT mode) | ✅ |
| OverlayFS (volatile mode) | ✅ |
| Seccomp | ✅ |

## Updating ReSukiSU

When a new ReSukiSU version is released:

```bash
# 1. Remove the old KernelSU directory
rm -rf KernelSU

# 2. Clone the latest ReSukiSU
git clone --branch main https://github.com/ReSukiSU/ReSukiSU.git KernelSU

# 3. Unshallow to get full commit history (required for KSU_VERSION)
git -C KernelSU fetch --unshallow

# 4. Apply the -hotsteel suffix customization
sed -i 's/-dirty/-hotsteel/' KernelSU/kernel/Kbuild

# 5. Rebuild
make olddefconfig
make -j$(nproc)
```

> The `KSU_VERSION` is calculated automatically as `30000 + commit_count + 700`.
> Do NOT hardcode it unless the git history is unavailable.

## Applying Droidspaces Patches

The Droidspaces [non-GKI guide](https://t.me/Droidspaces) provides two patches
for 4.19 kernels. Their status in this repo:

| Patch | Status | Notes |
|-------|--------|-------|
| `01.fix_kernel_panic_in_xt_qtaguid` | **N/A** | `xt_qtaguid.c` does not exist in this 4.19.325 tree |
| `02.fix_restore cgroup file prefix handling` | **Applied** | Patched in `kernel/cgroup/cgroup.c` — adds symlink for cgroup files when `CGRP_ROOT_NOPREFIX` is set |

If you need to re-apply patch 02 after updating the kernel base:

```bash
patch -p1 < ~/droidspaces/02.fix_restore\ cgroup\ file\ prefix\ handling\ .patch
```

## Troubleshooting

### Build fails: "Can't find ReSukiSU git submodule"

The `KernelSU/.git` file must exist and point to the main repository:

```bash
echo "gitdir: ../.git" > KernelSU/.git
```

### Build fails: "You should use ReSukiSU as a git submodule"

The Kbuild checks for `KernelSU/../.git`. Ensure the `.git` file exists as
described above. If you copied the KernelSU source instead of cloning it,
this check will fail.

### Manager reports "version too low"

The KSU_VERSION is too low. Ensure the KernelSU directory was cloned (not
copied) and that `git fetch --unshallow` was run. The Kbuild at build time
will auto-calculate the version from the commit count.

### Container fails to boot (exit 255)

Modern systemd (≥ 250) requires Linux 5.3+ syscalls (`pidfd_open`, `close_range`)
that are unavailable on 4.19. Use a container image compatible with Linux 4.19:

- **Working:** Debian 11, Ubuntu 20.04, Alpine 3.16
- **Broken:** Arch Linux ARM, Ubuntu 24.04, Fedora 38+

### "section mismatch" warnings during build

Two benign section mismatch warnings appear during modpost. These are harmless
and do not affect kernel stability. They exist in the upstream LineageOS kernel
and are not related to ReSukiSU or Droidspaces patches.

## Reference: Manual Hooks

ReSukiSU uses four manual syscall hooks for non-GKI kernels. They are already
applied in this repository:

| Hook | File | Function | Kernel Version |
|------|------|----------|----------------|
| `ksu_handle_execveat` | `fs/exec.c` | `do_execve()` + `compat_do_execve()` | 3.14+ |
| `ksu_handle_stat` | `fs/stat.c` | `newfstatat()` + `fstatat64()` + compat | all |
| `ksu_handle_newfstat_ret` | `fs/stat.c` | `newfstat()` | all |
| `ksu_handle_fstat64_ret` | `fs/stat.c` | `fstat64()` (32-bit compat) | all |
| `ksu_handle_faccessat` | `fs/open.c` | `faccessat()` (4.19+ variant) | 4.19+ |
| `ksu_handle_sys_reboot` | `kernel/reboot.c` | `SYSCALL_DEFINE4(reboot...)` | 3.12+ |

Three additional hooks are handled automatically via LSM (`CONFIG_KSU_MANUAL_HOOK_AUTO_*`):

- **setuid** → `security_operations.task_fix_setuid` → `kernel/sys.c`: `__sys_setresuid`
- **initrc** → `security_operations.file_permission` → `fs/read_write.c`: `SYSCALL_DEFINE3(read...)`
- **input** → `input_register_handler` → `drivers/input/input.c`: `input_event`

These LSM hooks are viable on all kernels < 6.8 and require no manual code changes.

The full integration reference is at:
https://resukisu.github.io/guide/manual-integrate.html

---

*Guide last updated: 2026-06-12. Kernel version: 4.19.325. ReSukiSU: v4.1.0 (commit 0dbd28e).*
