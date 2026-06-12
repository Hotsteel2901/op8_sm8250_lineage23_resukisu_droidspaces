# OnePlus 8 (SM8250) Kernel Build Guide

## The Complete, Excruciatingly Detailed Walkthrough

**Linux 4.19.325 · ReSukiSU v4.1.0 · Droidspaces · LineageOS 23.2 · GCC 9.3.0 · AnyKernel3**

This guide will take you from a blank Ubuntu terminal to a flashable kernel ZIP, explaining
every single command, every flag, every expected output, and every pitfall along the way.
No prior kernel building experience is assumed.

---

## Table of Contents

- [0. What Are We Building and Why?](#0-what-are-we-building-and-why)
- [1. Prepare Your Build Machine](#1-prepare-your-build-machine)
  - [1.1 Operating System](#11-operating-system)
  - [1.2 Install Required Packages](#12-install-required-packages)
  - [1.3 Check Disk Space](#13-check-disk-space)
  - [1.4 Configure Git](#14-configure-git)
- [2. Get the Toolchain](#2-get-the-toolchain)
  - [2.1 What Is a Cross-Compiler?](#21-what-is-a-cross-compiler)
  - [2.2 Install the AOSP GCC 9.3.0 Prebuilt](#22-install-the-aosp-gcc-930-prebuilt)
  - [2.3 Verify the Toolchain Works](#23-verify-the-toolchain-works)
- [3. Clone the Kernel Source](#3-clone-the-kernel-source)
  - [3.1 Clone the Repository](#31-clone-the-repository)
  - [3.2 What's Inside?](#32-whats-inside)
  - [3.3 Verify the Clone](#33-verify-the-clone)
- [4. Understand the Kernel Configuration](#4-understand-the-kernel-configuration)
  - [4.1 How Linux Kernel Configuration Works](#41-how-linux-kernel-configuration-works)
  - [4.2 The Three Layers of Our Config](#42-the-three-layers-of-our-config)
  - [4.3 Every Config Option Explained](#43-every-config-option-explained)
- [5. Build the Kernel](#5-build-the-kernel)
  - [5.1 Set Environment Variables](#51-set-environment-variables)
  - [5.2 Refresh the Config](#52-refresh-the-config)
  - [5.3 Start the Build](#53-start-the-build)
  - [5.4 Understand the Build Output](#54-understand-the-build-output)
  - [5.5 Verify the Build Artifacts](#55-verify-the-build-artifacts)
- [6. Package with AnyKernel3](#6-package-with-anykernel3)
  - [6.1 What Is AnyKernel3?](#61-what-is-anykernel3)
  - [6.2 Get the AnyKernel3 Template](#62-get-the-anykernel3-template)
  - [6.3 Understand the AnyKernel3 Structure](#63-understand-the-anykernel3-structure)
  - [6.4 Copy the Kernel Artifacts](#64-copy-the-kernel-artifacts)
  - [6.5 Customize the Installer](#65-customize-the-installer)
  - [6.6 Create the Flashable ZIP](#66-create-the-flashable-zip)
- [7. Flash the Kernel](#7-flash-the-kernel)
  - [7.1 Method A: TWRP Recovery](#71-method-a-twrp-recovery)
  - [7.2 Method B: Kernel Flasher App](#72-method-b-kernel-flasher-app)
  - [7.3 Method C: Manual (ADB Shell)](#73-method-c-manual-adb-shell)
  - [7.4 What NOT to Do](#74-what-not-to-do)
- [8. Verify the Installation](#8-verify-the-installation)
  - [8.1 Check ReSukiSU Is Working](#81-check-resukisu-is-working)
  - [8.2 Check Droidspaces Requirements](#82-check-droidspaces-requirements)
  - [8.3 Check Kernel Version and Uptime](#83-check-kernel-version-and-uptime)
- [9. How to Update ReSukiSU](#9-how-to-update-resukisu)
- [10. Troubleshooting Every Possible Problem](#10-troubleshooting-every-possible-problem)
- [11. Technical Reference](#11-technical-reference)

---

## 0. What Are We Building and Why?

We are compiling a **Linux 4.19.325 kernel** for the **Qualcomm Snapdragon 865 (SM8250 / "Kona")**
platform, specifically for OnePlus 8 series phones. This is a **non-GKI** (Generic Kernel Image)
kernel, meaning it is custom-built with exactly the drivers and features these phones need.

When flashed, this kernel adds two major capabilities to the phone:

| Feature | What It Does |
|---------|---------------|
| **ReSukiSU** | A fork of KernelSU that provides root access to Android apps and modules. It works by hooking kernel syscalls (`execve`, `stat`, `faccessat`, `reboot`) to grant superuser permissions without modifying `/system`. |
| **Droidspaces** | Enables running full Linux distributions (Debian, Ubuntu, Alpine) inside containers on Android. Requires kernel support for namespaces, cgroups, overlayfs, and virtual networking. |

The build produces a single binary (`Image`, ~55 MB) plus device-tree blobs that describe the
hardware. These are packaged into a TWRP-flashable ZIP using the AnyKernel3 framework, which
injects the new kernel into the existing boot partition without touching the ramdisk or system
partition.

---

## 1. Prepare Your Build Machine

### 1.1 Operating System

You need a 64-bit Linux environment. Any of these will work:

| OS | Notes |
|----|-------|
| **Ubuntu 20.04 LTS** | Recommended. Most tested. |
| Ubuntu 22.04 / 24.04 LTS | Works fine. Slightly newer packages. |
| Debian 11 / 12 | Works. May need to enable non-free repos. |
| WSL2 (Windows Subsystem for Linux) | Works, but compilation is ~20% slower due to filesystem overhead. Use a native ext4 mount if possible (see [WSL tips](#wsl2-users)). |
| Fedora / Arch | Works. Package names differ (`dnf` instead of `apt`). |

**Absolute minimum:** a 64-bit Linux kernel (the host kernel, not the target).

> #### WSL2 Users
> If you're on Windows using WSL2, create your working directory inside the WSL filesystem
> (e.g., `/home/yourname/`), **not** on a Windows drive mounted at `/mnt/c/`. Building on
> `/mnt/c/` will be painfully slow and may cause filename case-sensitivity issues.
> ```bash
> # Good:
> cd ~
> # Bad (slow):
> cd /mnt/c/Users/You/
> ```

### 1.2 Install Required Packages

Open a terminal and run this command. It installs everything the build system needs:

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    flex \
    bison \
    libssl-dev \
    bc \
    cpio \
    zip \
    git \
    wget \
    curl
```

What each package does:

| Package | Why We Need It |
|---------|-----------------|
| `build-essential` | gcc, g++, make — the host C/C++ compiler and build system |
| `flex`, `bison` | Parser generators used during kernel config processing (`lex`/`yacc`) |
| `libssl-dev` | OpenSSL headers — needed for kernel module signing and crypto |
| `bc` | Arbitrary-precision calculator — used in Kbuild math expressions |
| `cpio` | Archive tool — used during initramfs generation |
| `zip` | Creates the final flashable ZIP file |
| `git` | Version control — to clone and manage the kernel source |
| `wget`, `curl` | Download tools — for fetching toolchains and files |

Verify everything installed correctly:

```bash
which gcc make git bc zip && echo "All tools found."
```

Expected output:
```
/usr/bin/gcc
/usr/bin/make
/usr/bin/git
/usr/bin/bc
/usr/bin/zip
All tools found.
```

### 1.3 Check Disk Space

The build needs significant space. Check with:

```bash
df -h ~
```

Required space breakdown:

| Item | Space Needed |
|------|--------------|
| Kernel source (cloned repo) | ~3 GB |
| Toolchain (extracted) | ~400 MB |
| Build artifacts (.o files, vmlinux, Image) | ~4 GB |
| AnyKernel3 template + final ZIP | ~80 MB |
| **Total requirement** | **~8 GB free** |
| **Comfortable headroom** | **15 GB** |

If you have less than 8 GB free, clean up some space before proceeding. The build will fail
midway if the disk fills up — and there's no graceful recovery.

### 1.4 Configure Git

Set your identity so Git commits are properly attributed:

```bash
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

Verify:

```bash
git config --global --list | grep user
```

Expected output:
```
user.name=Your Name
user.email=your.email@example.com
```

---

## 2. Get the Toolchain

### 2.1 What Is a Cross-Compiler?

Your build machine has an **x86_64** CPU (Intel/AMD). The OnePlus 8 has an **aarch64** (ARM64)
CPU. A cross-compiler runs on x86_64 but produces code that runs on aarch64.

The toolchain includes:
- **`aarch64-linux-gnu-gcc`** — the C compiler
- **`aarch64-linux-gnu-as`** — the assembler
- **`aarch64-linux-gnu-ld`** — the linker
- **`aarch64-linux-gnu-objcopy`** — converts ELF to raw binary (produces `Image`)

We use **GCC 9.3.0** because it's the version this kernel was validated against. Newer GCC
versions (10, 11, 12) may produce warnings that become errors due to the kernel's strict
compiler flags.

### 2.2 Install the AOSP GCC 9.3.0 Prebuilt

The easiest way to get the right toolchain is from AOSP's prebuilt repository:

```bash
# Navigate to your home directory
cd ~

# Clone the AOSP GCC 9.3.0 prebuilt for ARM64
git clone https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu-9.3 \
    android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3
```

> This is a ~400 MB download. If Google's servers are slow, there's a mirror at:
> `https://github.com/arter97/arm64-gcc` (GCC 9.2, compatible but untested with this kernel).

After cloning, the toolchain lives at:
```
~/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3/
```

### 2.3 Verify the Toolchain Works

```bash
# Set the path to the toolchain binaries
TC_PATH=~/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3/bin

# List the binaries (you should see ~20 files)
ls $TC_PATH/

# Check the compiler version
$TC_PATH/aarch64-buildroot-linux-gnu-gcc --version
```

Expected output (second command):
```
aarch64-buildroot-linux-gnu-gcc.br_real (Buildroot 2020.08) 9.3.0
Copyright (C) 2019 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

If you see `9.3.0` in the output, the toolchain is ready. If you get "No such file or directory",
re-check that the clone completed successfully.

---

## 3. Clone the Kernel Source

### 3.1 Clone the Repository

```bash
# Navigate to where you want the source (any path works, ~ is fine)
cd ~

# Clone the kernel source
git clone https://github.com/Hotsteel2901/op8_sm8250_lineage23_resukisu_droidspaces.git

# Enter the source directory
cd op8_sm8250_lineage23_resukisu_droidspaces

# Switch to the correct branch (should already be the default)
git checkout lineage-23.2
```

The clone takes 2-5 minutes depending on your internet speed (~3 GB transfer).

### 3.2 What's Inside?

After cloning, run `ls` and `git log --oneline -5` to see what you have:

```
$ ls
AndroidKernel.mk   KernelSU/          arch/             drivers/    init/      kernel/    net/      scripts/   techpack/  virt/
AndroidKernel.mk~  Makefile           block/            firmware/   ipc/       lib/       samples/  security/  tools/
COPYING            README.md          build.config.aarch64  fs/      Kbuild     mm/        scripts/  sound/     usr/
CREDITS            ReSukiSU-SUSFS-ONEPLUS-SM8250-BUILD-GUIDE.md  include/  Kconfig  modules.builtin  security/  techpack/
```

Key directories:

| Path | Contents |
|------|----------|
| `KernelSU/kernel/` | **ReSukiSU kernel module** — the root solution code |
| `arch/arm64/` | ARM64 architecture-specific code (boot, DTS, memory management) |
| `arch/arm64/boot/dts/vendor/oplus/` | Device tree source and compiled blobs for OnePlus 8 |
| `arch/arm64/configs/vendor/` | Kernel defconfig files |
| `drivers/kernelsu/` | Symlink to `KernelSU/kernel/` (what Kbuild actually compiles) |
| `drivers/soc/oplus/` | OnePlus-specific SoC drivers (charging, fingerprint, etc.) |
| `net/` | Network stack — includes Droidspaces-required netfilter modules |
| `fs/` | Filesystem code — includes ReSukiSU manual hooks |
| `.config` | The full kernel configuration (4,600+ options) |

> **Important:** The `KernelSU/` directory is **not a git submodule**. It is a full git clone
> of the ReSukiSU repository, tracked inside this repo. A `.git` file at `KernelSU/.git` was
> created to point to the parent repo's `.git`, which allows the build system to count commits
> for automatic version calculation. If you delete it, replace it with:
> ```bash
> echo "gitdir: ../.git" > KernelSU/.git
> ```

### 3.3 Verify the Clone

```bash
# Check the branch
git branch

# Check the latest commits
git log --oneline -3

# Check remote repositories
git remote -v
```

Expected output:
```
* lineage-23.2
```
```
a095f5672bf2 docs: add comprehensive build guide in English
af4b129f9adf ReSukiSU: update to v4.1.0, remove KPM, fix hooks and optimize
7c4eaf1d48b8 OnePlus 8 kernel: LineageOS 23.2 + ReSukiSU + Droidspaces container support
```
```
lineage https://github.com/LineageOS/android_kernel_oneplus_sm8250.git (fetch)
lineage https://github.com/LineageOS/android_kernel_oneplus_sm8250.git (push)
origin  https://github.com/Hotsteel2901/op8_sm8250_lineage23_resukisu_droidspaces.git (fetch)
origin  https://github.com/Hotsteel2901/op8_sm8250_lineage23_resukisu_droidspaces.git (push)
```

> `lineage` is the LineageOS upstream (read-only reference). `origin` is the customized
> repository with ReSukiSU and Droidspaces already integrated.

---

## 4. Understand the Kernel Configuration

### 4.1 How Linux Kernel Configuration Works

The Linux kernel has **thousands** of compile-time options. They are stored in a file called
`.config` in the source root. Each line looks like:

```
CONFIG_SOME_FEATURE=y        # Feature is built INTO the kernel image
# CONFIG_SOME_FEATURE is not set   # Feature is DISABLED
CONFIG_SOME_FEATURE=m        # Feature is built as a loadable MODULE (.ko file)
```

The `.config` in this repo already has everything enabled that you need. You do **not**
need to run `make menuconfig` or manually edit anything. The following sections explain
what's in there so you understand the kernel you're building.

### 4.2 The Three Layers of Our Config

The final `.config` is built from three sources merged together:

| Layer | Source | What It Provides |
|-------|--------|------------------|
| **1. Platform base** | `arch/arm64/configs/vendor/kona-perf_defconfig` | Qualcomm SM8250 SoC support, ARM64 architecture, Android-required features, performance tuning |
| **2. OPlus vendor** | `arch/arm64/configs/vendor/oplus.config` | OnePlus-specific hardware drivers: charging (OPLUS_CHG), touchscreen, fingerprint, display, camera, audio, WiFi |
| **3. Custom additions** | Manually added to `.config` | ReSukiSU root access (CONFIG_KSU + hooks), Droidspaces container support (namespaces, cgroups, networking) |

These were already merged and committed. If you ever need to regenerate the config from
scratch (e.g., after a kernel version upgrade), do:

```bash
make kona-perf_defconfig                          # generates .config from layer 1
./scripts/kconfig/merge_config.sh \
    .config \
    arch/arm64/configs/vendor/oplus.config        # merges layer 2
# Then manually add layer 3 options and run:
make olddefconfig                                  # fills in defaults
```

### 4.3 Every Config Option Explained

#### 4.3.1 ReSukiSU Root Solution

These options enable and configure ReSukiSU (KernelSU fork):

```
CONFIG_KSU=y
```
**Turns on the whole KernelSU subsystem.** Without this, there's no root solution at all.
This causes the `drivers/kernelsu/` directory to be compiled and linked into the kernel.

```
CONFIG_KSU_MANUAL_HOOK=y
```
**Selects the manual syscall hook method.** Non-GKI kernels like 4.19 cannot use tracepoint
hooks (which require GKI 2.0). Manual hooks work by inserting function calls at specific
points in the kernel source code (`fs/exec.c`, `fs/stat.c`, `fs/open.c`, `kernel/reboot.c`).

```
CONFIG_KSU_MANUAL_HOOK_AUTO_SETUID_HOOK=y
```
**Auto-hooks the `setresuid` syscall via LSM (Linux Security Module).** When an app requests
UID changes (e.g., `su` command), the LSM callback intercepts it and grants root. This works
on all kernels < 6.8 and avoids manual code changes in `kernel/sys.c`.

```
CONFIG_KSU_MANUAL_HOOK_AUTO_INITRC_HOOK=y
```
**Auto-hooks the `read` syscall via LSM.** When Android's init process reads service
configuration files, ReSukiSU can inject its own service definitions. Works on kernels < 6.8.

```
CONFIG_KSU_MANUAL_HOOK_AUTO_INPUT_HOOK=y
```
**Auto-hooks input events via the kernel's `input_handler` mechanism.** Allows ReSukiSU to
detect key combinations (e.g., volume-down + power) for features like Safe Mode.

```
CONFIG_KALLSYMS_ALL=y
```
**Exports ALL kernel symbols** (function names and addresses) to modules and kernel code.
ReSukiSU needs this to look up SELinux functions at runtime. Without it, you'd need to
manually un-static 9+ variables in the SELinux subsystem — a tedious and error-prone process.

```
CONFIG_SECURITY_SELINUX=y
CONFIG_SECURITY_SELINUX_DEVELOP=y
```
Standard SELinux support. `DEVELOP` mode is less restrictive and helps with debugging.

#### 4.3.2 Droidspaces Container Support

These options enable running full Linux distributions in containers:

**Namespaces (Container Isolation):**
```
CONFIG_NAMESPACES=y          # Master switch for all namespace types
CONFIG_USER_NS=y             # User namespace — allows unprivileged containers
CONFIG_PID_NS=y              # PID namespace — container gets its own PID 1
CONFIG_IPC_NS=y              # IPC namespace — isolates System V IPC and POSIX message queues
CONFIG_UTS_NS=y              # UTS namespace — container can have its own hostname
CONFIG_NET_NS=y              # Network namespace — container gets its own network stack
```
Without any one of these, containers will fail to start. `USER_NS` is the most commonly
missing one — it's often disabled for "security" on stock kernels.

**Control Groups (Resource Limits):**
```
CONFIG_CGROUPS=y             # Master switch for control groups
CONFIG_CGROUP_DEVICE=y       # Device cgroup — controls which /dev nodes a container can access
CONFIG_CGROUP_PIDS=y         # PID cgroup — limits how many processes a container can create
CONFIG_CGROUP_FREEZER=y      # Freezer cgroup — can pause/resume all processes in a container
CONFIG_MEMCG=y               # Memory cgroup — limits container memory usage
CONFIG_CGROUP_SCHED=y        # CPU cgroup — limits container CPU usage
CONFIG_FAIR_GROUP_SCHED=y    # Fair scheduler — ensures fair CPU distribution among containers
CONFIG_CGROUP_NET_PRIO=y     # Network priority cgroup
```

**Filesystem Support:**
```
CONFIG_DEVTMPFS=y            # Automatically populates /dev with device nodes at boot
CONFIG_DEVTMPFS_MOUNT=y      # Mounts devtmpfs at /dev during early boot
CONFIG_OVERLAY_FS=y          # Overlay filesystem — enables Docker/podman "volatile" containers
CONFIG_AUTOFS4_FS=y          # Automounter — systemd containers need this
CONFIG_FUSE_FS=y             # Filesystem in Userspace — some container runtimes use this
CONFIG_FHANDLE=y             # File handles — Android's sdcardfs needs this
CONFIG_TMPFS_POSIX_ACL=y     # POSIX ACLs on tmpfs — needed for NixOS containers
CONFIG_TMPFS_XATTR=y         # Extended attributes on tmpfs
```

**Networking (NAT / Bridge Mode):**
```
CONFIG_VETH=y                                          # Virtual Ethernet pair — connects container to host
CONFIG_BRIDGE=y                                        # Ethernet bridge — connects multiple containers
CONFIG_DUMMY=y                                         # Dummy network interface
CONFIG_NETFILTER=y                                     # Netfilter — firewall/NAT framework
CONFIG_NETFILTER_ADVANCED=y                            # Advanced netfilter options
CONFIG_BRIDGE_NETFILTER=y                              # Bridge netfilter — NAT through bridges
CONFIG_NF_CONNTRACK=y                                  # Connection tracking
CONFIG_NF_NAT=y                                        # Network Address Translation
CONFIG_NF_TABLES=y                                     # nftables — modern firewall (not in 4.19 kernel)
CONFIG_IP_NF_IPTABLES=y                                # iptables — legacy firewall
CONFIG_IP_NF_FILTER=y                                  # Packet filter
CONFIG_IP_NF_NAT=y                                     # IPv4 NAT
CONFIG_IP_NF_TARGET_MASQUERADE=y                       # MASQUERADE target (source NAT for containers)
CONFIG_NETFILTER_XT_TARGET_MASQUERADE=y                # xt_MASQUERADE module
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE=y                   # Address type matching
CONFIG_NETFILTER_XT_TARGET_TCPMSS=y                    # TCP MSS clamping (fixes MTU issues)
CONFIG_NF_CONNTRACK_NETLINK=y                          # Conntrack netlink interface
CONFIG_NF_NAT_REDIRECT=y                               # NAT redirection
CONFIG_IP_ADVANCED_ROUTER=y                            # Advanced routing features
CONFIG_IP_MULTIPLE_TABLES=y                            # Multiple routing tables
CONFIG_NF_NAT_IPV4=y                                   # Legacy IPv4 NAT (for older iptables)
```

**Security:**
```
CONFIG_SECCOMP=y             # Secure Computing — restricts syscalls a container can make
CONFIG_SECCOMP_FILTER=y      # BPF-based seccomp filters — Docker/podman require this
CONFIG_SECURITYFS=y          # Security filesystem — exports security module info
```

**IPC (Inter-Process Communication):**
```
CONFIG_SYSVIPC=y             # System V IPC — semaphores, shared memory, message queues
CONFIG_POSIX_MQUEUE=y        # POSIX message queues — some container runtimes need this
```

**Checkpoint/Restore (CRIU):**
```
CONFIG_CHECKPOINT_RESTORE=y  # Checkpoint/restore — enables container live migration (optional but nice)
```

**Firmware Loading:**
```
CONFIG_FW_LOADER=y           # Firmware loader
CONFIG_FW_LOADER_USER_HELPER=y  # Userspace firmware loader helper
CONFIG_FW_LOADER_COMPRESS=y  # Compressed firmware support
```

**Crash Logging:**
```
CONFIG_PSTORE=y              # Persistent storage — saves crash logs across reboots
CONFIG_PSTORE_RAM=y          # RAM-backed persistent storage
CONFIG_PSTORE_CONSOLE=y      # Console output to pstore
CONFIG_PSTORE_PMSG=y         # Userspace messages to pstore
```

#### 4.3.3 Build Optimization

```
CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE=y   # Compile with -O2 (balance of speed and size)
# CONFIG_CC_OPTIMIZE_FOR_SIZE is not set   # -Os is NOT used
```
`-O2` is recommended for daily use. If you have limited boot partition space, switch to
`-Os` (it saves ~5 MB on the Image but slightly reduces runtime performance).

```
CONFIG_LOCALVERSION="-perf"            # Appended to kernel name: 4.19.325-perf
```
The kernel version string is modified by `scripts/setlocalversion` which appends `-hotsteel`
when there are uncommitted changes (instead of the stock `-dirty`).

---

## 5. Build the Kernel

### 5.1 Set Environment Variables

**These must be set in every new terminal session before you build.** They tell `make` what
architecture to target and where the cross-compiler is.

```bash
# Set the target architecture
export ARCH=arm64

# Tell make where to find the cross-compiler
# The trailing dash (-) is REQUIRED — make appends 'gcc', 'ld', 'objcopy', etc.
export CROSS_COMPILE=~/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3/bin/aarch64-buildroot-linux-gnu-

# Make the toolchain binaries available in PATH (so 'git' commands inside Kbuild find them)
export PATH=~/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3/bin:$PATH

# Verify the setup
echo "ARCH=$ARCH"
echo "CROSS_COMPILE=$CROSS_COMPILE"
which aarch64-buildroot-linux-gnu-gcc
```

> **Pro tip:** Add these three `export` lines to `~/.bashrc` if you plan to build frequently.
> Add this block at the end:
> ```bash
> alias ksetup='export ARCH=arm64 && export CROSS_COMPILE=~/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3/bin/aarch64-buildroot-linux-gnu- && export PATH=~/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-gnu-9.3/bin:$PATH'
> ```
> Then just type `ksetup` in any new terminal to set everything up.

### 5.2 Refresh the Config

Before building, run `olddefconfig` to resolve any dependency changes between the `.config`
and the Kconfig files:

```bash
make olddefconfig
```

Expected output:
```
scripts/kconfig/conf  --olddefconfig Kconfig
#
# configuration written to .config
#
```

This is fast (2-3 seconds). It reads `.config`, checks every option against the Kconfig rules,
enables any options that are required as dependencies, and writes the resolved config back.
If there are conflicting options, `olddefconfig` picks the default.

> If `olddefconfig` ever produces warnings about unknown symbols, that means a config option
> was removed from the Kconfig files but still exists in `.config`. Delete the stale line
> from `.config` and re-run `olddefconfig`.

### 5.3 Start the Build

```bash
# Build with all CPU cores minus 2 (leaves some CPU for other tasks)
make -j$(($(nproc) - 2))

# Or use all cores if you're not doing anything else:
make -j$(nproc)
```

The `-j` flag sets the number of parallel compilation jobs. On a 32-core machine, `-j30`
completes the build in about 4-6 minutes. On a 4-core laptop, expect 30-45 minutes.

**You will see a waterfall of compiler commands.** This is normal. Here's what a healthy
build looks like at various stages:

```
# Stage 1: Kconfig sync (~2 seconds)
scripts/kconfig/conf  --syncconfig Kconfig
  UPD     include/config/kernel.release
-- ReSukiSU version code: 34967
-- ReSukiSU version name: v4.1.0-0dbd28ea-hotsteel@ReSukiSU
-- KERNEL_VERSION: 4.19
-- KERNEL_TYPE: Non-GKI
-- ReSukiSU/compat: current_sid found
-- ReSukiSU/compat: selinux_state found
-- ReSukiSU/compat: strncpy found
... (20+ compat detection lines) ...
-- ReSukiSU: using Manual Hook
-- ReSukiSU/manual_hook: ksu_handle_execveat found
-- ReSukiSU/manual_hook: ksu_handle_faccessat found
-- ReSukiSU/manual_hook: ksu_handle_stat found
-- ReSukiSU/manual_hook: ksu_handle_newfstat_ret found
-- ReSukiSU/manual_hook: ksu_handle_fstat64_ret found
-- ReSukiSU/manual_hook: ksu_handle_sys_reboot found
-- ReSukiSU/manual_hook: You are using LSM hooks for setuid hooks.
-- ReSukiSU/manual_hook: You are using LSM hooks for init rc hooks.
-- ReSukiSU/manual_hook: You are using input_handler for input hooks.
  UPD     include/generated/utsrelease.h

# Stage 2: Compilation (~3-5 minutes with -j30)
  CC      kernel/bounds.s
  CC      arch/arm64/kernel/asm-offsets.s
  CC      init/main.o
  CC      arch/arm64/mm/init.o
  ... (thousands of CC lines) ...

# Stage 3: Linking (~1 minute)
  AR      drivers/built-in.a
  AR      built-in.a
  MODPOST vmlinux.o
  KSYM    .tmp_kallsyms1.o
  KSYM    .tmp_kallsyms2.o
  LD      vmlinux
  SORTEX  vmlinux
  SYSMAP  System.map
  OBJCOPY arch/arm64/boot/Image

# Stage 4: Device trees (~30 seconds)
  DTC     arch/arm64/boot/dts/vendor/oplus/kona.dtb
  DTC     arch/arm64/boot/dts/vendor/oplus/kona-v2.dtb
  DTC     arch/arm64/boot/dts/vendor/oplus/kona-v2.1.dtb
  DTC     arch/arm64/boot/dts/vendor/oplus/kona-instantnoodle-overlay.dtbo
  DTC     arch/arm64/boot/dts/vendor/oplus/kona-instantnoodlep-overlay.dtbo
  DTC     arch/arm64/boot/dts/vendor/oplus/kona-kebab-overlay.dtbo
  DTC     arch/arm64/boot/dts/vendor/oplus/kona-lemonades-overlay.dtbo
```

**Key things to watch for in the output:**

| Line | Meaning |
|------|---------|
| `-- ReSukiSU version code: 34967` | The version the manager app checks. **Must be > 34759** for modern managers. |
| `-- ReSukiSU: using Manual Hook` | Confirms we're in manual hook mode (correct for non-GKI). |
| `-- ReSukiSU/manual_hook: ksu_handle_* found` | Each of the 6 manual hooks was detected in the source. If any says "You lost X hook", the build would error out. |
| `LD vmlinux` | The raw kernel ELF binary was linked successfully. |
| `OBJCOPY arch/arm64/boot/Image` | The bootable kernel image was created. |

### 5.4 Understand the Build Output

If anything goes wrong, the build error will look like one of these patterns:

```
error: 'something' undeclared (first use in this function)
    → A C code error. Usually a missing #include or a syntax mistake.

make[1]: *** [scripts/Makefile.build:XXX: path/to/file.o] Error 1
    → Compilation failed for file.o. Read the lines above this for the actual error.

make: *** [Makefile:1442: drivers/modules.builtin] Error 2
    → The build stopped. Fix the first error you see (scroll up).
```

**Warnings that are safe to ignore:**
```
WARNING: modpost: Found 2 section mismatch(es).
```
These are present in the upstream LineageOS kernel. They relate to GSPCA webcam driver code
that's incorrectly annotated but never loaded on a phone. They do not affect stability.

### 5.5 Verify the Build Artifacts

```bash
# Check the kernel image exists and has reasonable size
ls -lh arch/arm64/boot/Image

# Check device tree blobs
ls -lh arch/arm64/boot/dts/vendor/oplus/*.dtb

# Check DTBO overlay files
ls -lh arch/arm64/boot/dts/vendor/oplus/*-overlay.dtbo
```

Expected output:
```
-rw-r--r-- 1 user user 55M arch/arm64/boot/Image

-rw-r--r-- 1 user user 472K arch/arm64/boot/dts/vendor/oplus/kona.dtb
-rw-r--r-- 1 user user 479K arch/arm64/boot/dts/vendor/oplus/kona-v2.dtb
-rw-r--r-- 1 user user 479K arch/arm64/boot/dts/vendor/oplus/kona-v2.1.dtb

-rw-r--r-- 1 user user 192K arch/arm64/boot/dts/vendor/oplus/kona-instantnoodle-overlay.dtbo
-rw-r--r-- 1 user user 292K arch/arm64/boot/dts/vendor/oplus/kona-instantnoodlep-overlay.dtbo
-rw-r--r-- 1 user user 207K arch/arm64/boot/dts/vendor/oplus/kona-kebab-overlay.dtbo
-rw-r--r-- 1 user user 202K arch/arm64/boot/dts/vendor/oplus/kona-lemonades-overlay.dtbo
```

If `Image` is smaller than 40 MB, something was probably disabled that shouldn't be.
If `Image` is missing entirely, scroll up the build log and find the error.

**Verify the ReSukiSU version embedded in the image:**

```bash
strings arch/arm64/boot/Image | grep ReSukiSU
```

Expected output:
```
v4.1.0-0dbd28ea-hotsteel@ReSukiSU
```

The hash (`0dbd28ea`) will change with each ReSukiSU commit. The `-hotsteel` suffix
confirms the custom branding was applied.

---

## 6. Package with AnyKernel3

### 6.1 What Is AnyKernel3?

AnyKernel3 is a framework that creates a recovery-flashable ZIP which **unpacks your
existing boot partition, replaces only the kernel Image and DTB files, then repacks
the boot image**. This is important because:

- The ramdisk (init scripts, SELinux policy, fstab) is preserved as-is
- The kernel command line parameters are preserved
- The boot image header and signature are preserved
- It works even if your boot partition layout changes between ROM updates

Without AnyKernel3, you'd need to manually create a boot image with `mkbootimg` using
the exact same base address, pagesize, cmdline, and ramdisk as the current ROM — which
is fragile and ROM-specific.

### 6.2 Get the AnyKernel3 Template

```bash
cd ~
git clone https://github.com/osm0sis/AnyKernel3.git
cd AnyKernel3
ls
```

Expected output:
```
Image           # Placeholder — you'll replace this with the built kernel
META-INF/       # Recovery scripts (update-binary, updater-script)
anykernel.sh    # Configuration — device names, kernel string, flash options
dtbo/           # DTBO overlay images — placed in the dtbo partition
modules/        # Kernel modules (.ko files) — placed in vendor/lib/modules
patch/          # Empty — reserved for future use
ramdisk/        # Optional ramdisk overlays — empty by default
tools/          # AK3 binaries — busybox, magiskboot, lptools, etc.
```

### 6.3 Understand the AnyKernel3 Structure

Read `anykernel.sh` to understand the configuration:

```bash
cat anykernel.sh
```

Key lines explained:

```bash
kernel.string=ReSukiSU v4.1.0-hotsteel - Linux 4.19.325 for OnePlus 8 Series
# ^ This is displayed in TWRP during flash. Change it to whatever you want.

do.devicecheck=0
# ^ 0 = skip device name check (flash on any device — DANGEROUS!)
#   1 = only flash if device.name1-5 matches the actual device
#   For safety, set to 1 and list your device codenames below.

device.name1=instantnoodle    # OnePlus 8
device.name2=instantnoodlep   # OnePlus 8 Pro
device.name3=kebab            # OnePlus 8T
device.name4=lemonades        # OnePlus 9R (uses SM8250 too)

BLOCK=boot;                   # The partition to flash — always "boot" for kernel
IS_SLOT_DEVICE=1;             # A/B partition support (OnePlus 8 has slots)
RAMDISK_COMPRESSION=auto;     # Auto-detect ramdisk compression (lz4 or gzip)
```

### 6.4 Copy the Kernel Artifacts

Now bring the compiled kernel and device tree files into the AnyKernel3 directory:

```bash
# Navigate back to the kernel source
cd ~/op8_sm8250_lineage23_resukisu_droidspaces

# Copy the kernel Image (the main event)
cp arch/arm64/boot/Image ~/AnyKernel3/Image
echo "Image copied: $(du -h ~/AnyKernel3/Image | cut -f1)"

# Copy the base device tree blobs (the hardware description files)
# These go in the root of the AK3 directory — AK3 appends them to the boot image.
cp arch/arm64/boot/dts/vendor/oplus/kona.dtb       ~/AnyKernel3/
cp arch/arm64/boot/dts/vendor/oplus/kona-v2.dtb     ~/AnyKernel3/
cp arch/arm64/boot/dts/vendor/oplus/kona-v2.1.dtb   ~/AnyKernel3/
echo "DTBs copied."

# Copy the DTBO overlay files (these go to the separate dtbo partition)
cp arch/arm64/boot/dts/vendor/oplus/kona-instantnoodle-overlay.dtbo    ~/AnyKernel3/dtbo/
cp arch/arm64/boot/dts/vendor/oplus/kona-instantnoodlep-overlay.dtbo   ~/AnyKernel3/dtbo/
cp arch/arm64/boot/dts/vendor/oplus/kona-kebab-overlay.dtbo            ~/AnyKernel3/dtbo/
cp arch/arm64/boot/dts/vendor/oplus/kona-lemonades-overlay.dtbo        ~/AnyKernel3/dtbo/
echo "DTBOs copied."

# Verify everything is in place
echo "=== AnyKernel3 contents ==="
ls -lh ~/AnyKernel3/Image
ls -lh ~/AnyKernel3/*.dtb
ls -lh ~/AnyKernel3/dtbo/*.dtbo
```

Expected output:
```
Image copied: 55M
DTBs copied.
DTBOs copied.
=== AnyKernel3 contents ===
-rw-r--r-- 1 user user 55M ~/AnyKernel3/Image
-rw-r--r-- 1 user user 472K ~/AnyKernel3/kona.dtb
-rw-r--r-- 1 user user 479K ~/AnyKernel3/kona-v2.dtb
-rw-r--r-- 1 user user 479K ~/AnyKernel3/kona-v2.1.dtb
-rw-r--r-- 1 user user 192K ~/AnyKernel3/dtbo/kona-instantnoodle-overlay.dtbo
-rw-r--r-- 1 user user 292K ~/AnyKernel3/dtbo/kona-instantnoodlep-overlay.dtbo
-rw-r--r-- 1 user user 207K ~/AnyKernel3/dtbo/kona-kebab-overlay.dtbo
-rw-r--r-- 1 user user 202K ~/AnyKernel3/dtbo/kona-lemonades-overlay.dtbo
```

### 6.5 Customize the Installer

Edit `anykernel.sh` to set a nice kernel string that shows in TWRP:

```bash
nano ~/AnyKernel3/anykernel.sh
```

Change the `kernel.string` line to:
```bash
kernel.string=ReSukiSU v4.1.0-hotsteel - Linux 4.19.325 - OnePlus 8 Series
```

(Or whatever version/date/branding you prefer. This is purely cosmetic.)

### 6.6 Create the Flashable ZIP

```bash
cd ~/AnyKernel3

# Create a timestamped filename
ZIPNAME="ReSukiSU-kona-4.19.325-$(date +%Y%m%d)-hotsteel.zip"

# Build the zip
# The '.' means "zip everything in the current directory"
# The -x flag excludes files we don't want in the final zip
zip -r "../$ZIPNAME" . -x ".git*" "*.ko" "README.md"

# Show the result
ls -lh "../$ZIPNAME"
```

Expected output (after zip output listing all added files):
```
-rw-r--r-- 1 user user 25M /home/user/ReSukiSU-kona-4.19.325-20260612-hotsteel.zip
```

The ZIP is about 25 MB because:
- The 55 MB Image compresses to ~22 MB with deflate
- The three DTB files (~1.4 MB total) compress to ~250 KB
- The four DTBO files (~900 KB total) compress to ~160 KB
- The AK3 tools and scripts add ~2 MB

Transfer this ZIP to your phone however you prefer:

```bash
# Option A: ADB push (phone connected via USB with USB debugging enabled)
adb push ~/ReSukiSU-kona-4.19.325-20260612-hotsteel.zip /sdcard/

# Option B: Copy via MTP/File Manager (drag and drop in Windows Explorer)

# Option C: Cloud upload (Google Drive, Telegram Saved Messages, etc.)
```

---

## 7. Flash the Kernel

### 7.1 Method A: TWRP Recovery

This is the recommended method. Works with official TWRP, Orangefox, or PBRP.

**Step-by-step:**

1. **Reboot to recovery.** With the phone powered on and USB debugging enabled:
   ```bash
   adb reboot recovery
   ```
   Or manually: power off, then hold **Volume Down + Power** until the recovery screen appears.

2. **Navigate to Install.** In TWRP, tap the **Install** button.

3. **Select the ZIP.** Browse to `/sdcard/` (or wherever you transferred the file),
   find `ReSukiSU-kona-4.19.325-*-hotsteel.zip`, and tap it.

4. **Check the output.** Before swiping, TWRP shows:
   ```
   ReSukiSU v4.1.0-hotsteel - Linux 4.19.325 - OnePlus 8 Series
   ```
   Verify this looks correct.

5. **Swipe to confirm flash.** The flash takes about 3-5 seconds. You'll see:
   ```
   Unpacking boot image...
   Splitting boot image...
   Replacing kernel...
   Repacking boot image...
   Flashing boot image...
   Flashing dtbo image...
   Done!
   ```

6. **Reboot to system.** Tap **Reboot System**. The first boot may take slightly longer
   than usual (30-60 seconds) as the new kernel initializes.

> **If you get stuck in a bootloop:** Don't panic. Reboot to TWRP, flash your previous
> kernel ZIP (or dirty-flash your ROM), and the phone will be back to normal.
> AnyKernel3 only touches the boot partition — your data is safe.

### 7.2 Method B: Kernel Flasher App

If you don't have TWRP installed (e.g., you're using LineageOS recovery), use a kernel
flasher app:

1. Install **[Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)**
   or **Franco Kernel Manager** from F-Droid / GitHub / Play Store.

2. Open the app and grant root access.

3. Tap **Flash** → select the AnyKernel3 ZIP.

4. The app shows a preview of what will be flashed. Verify it says "Boot partition"
   and "DTBO partition".

5. Tap **Flash** and wait for completion. The app will prompt you to reboot.

### 7.3 Method C: Manual (ADB Shell)

If you're comfortable with the command line and have root ADB access:

```bash
# Push the ZIP to the phone
adb push ReSukiSU-kona-4.19.325-20260612-hotsteel.zip /tmp/

# Open a root shell
adb shell
su

# Extract the AnyKernel3 zip to a temp directory
mkdir -p /data/local/tmp/ak3
cd /data/local/tmp/ak3
unzip /tmp/ReSukiSU-kona-4.19.325-20260612-hotsteel.zip

# Flash using the AK3 tools directly
chmod +x tools/magiskboot
./tools/magiskboot unpack /dev/block/by-name/boot$(getprop ro.boot.slot_suffix)
cp Image kernel
./tools/magiskboot repack /dev/block/by-name/boot$(getprop ro.boot.slot_suffix)

# Flash the DTBO
dd if=dtbo/kona-instantnoodle-overlay.dtbo of=/dev/block/by-name/dtbo$(getprop ro.boot.slot_suffix)

# Reboot
reboot
```

**This is advanced and risky.** If you mistype a partition name, you could brick the phone.
Stick to TWRP or Kernel Flasher unless you know exactly what you're doing.

### 7.4 What NOT to Do

```bash
# ⛔ NEVER DO THIS:
fastboot flash boot arch/arm64/boot/Image
```

This command writes the raw kernel binary to the boot partition **without** the DTB,
ramdisk, or proper boot image header. The phone will not boot. You'll need to reflash
the ROM's boot image to recover.

```bash
# ⛔ ALSO DANGEROUS:
fastboot flash dtbo arch/arm64/boot/dts/vendor/oplus/kona-instantnoodle-overlay.dtbo
```

This flashes only ONE DTBO overlay. The OnePlus 8 needs ALL overlays packaged together
or a proper `dtbo.img` created by `mkdtimg`. AnyKernel3 handles this correctly by flashing
individual DTBO files through the recovery's `dtbo` partition handling.

---

## 8. Verify the Installation

### 8.1 Check ReSukiSU Is Working

**Method 1: ReSukiSU Manager App**

Download and install the latest ReSukiSU Manager APK from:
https://github.com/ReSukiSU/ReSukiSU/releases

When you open the app:

| What You See | What It Means |
|--------------|---------------|
| **"Working"** with a checkmark | ✅ Everything is fine. Root is active. |
| **"Not installed"** or **"Incompatible"** | ❌ The kernel doesn't have ReSukiSU, or the KSU_VERSION is too old. |
| **"Disabled"** | ⚠️ ReSukiSU is compiled in but the user has disabled it. |

Tap the settings icon (gear) and look at the **About** section:
- **Version code:** should show `34967`
- **Version name:** should show `v4.1.0-xxxxxxxx-hotsteel@ReSukiSU`

**Method 2: Terminal (ADB Shell or Local Terminal App)**

```bash
# Open a terminal and run:
su

# If you get a root prompt (#), ReSukiSU is working. Then check the kernel log:
dmesg | grep -i ksu
```

Expected output (your exact numbers may differ):
```
[    2.345678] ReSukiSU: version: v4.1.0-0dbd28ea-hotsteel@ReSukiSU
[    2.345690] ReSukiSU: version code: 34967
[    2.345700] ReSukiSU: kernel version: 4.19.325
[    2.345710] ReSukiSU: hook type: manual
```

**Method 3: Check the `/proc/version` string**

```bash
cat /proc/version
```

Expected output:
```
Linux version 4.19.325-perf-g0dbd28ea-hotsteel (user@hostname) (gcc version 9.3.0 ...)
```

The `-g0dbd28ea-hotsteel` suffix confirms you're running the right kernel.

### 8.2 Check Droidspaces Requirements

Install the Droidspaces app and run the built-in requirements check:

```bash
# Or from terminal:
su -c droidspaces check
```

Expected output (all green checkmarks):

```
✅ Root access detected
✅ Kernel version: 4.19.325 (minimum: 3.18)
✅ PID namespace supported
✅ MNT namespace supported
✅ UTS namespace supported
✅ IPC namespace supported
✅ Network namespace supported
✅ Cgroup device controller available
✅ devtmpfs is mounted
✅ OverlayFS available (volatile mode)
✅ VETH driver loaded
✅ Bridge driver loaded
✅ Seccomp with BPF filter supported
⚠️  Cgroup namespace not available (optional, for modern cgroup isolation)
```

The cgroup namespace warning is normal for 4.19 — it's only available on 4.6+ kernels
and is optional (modern containers use cgroup v2 which doesn't need cgroup NS isolation).

### 8.3 Check Kernel Version and Uptime

```bash
uname -a
```

Expected output:
```
Linux localhost 4.19.325-perf-g0dbd28ea-hotsteel #1 SMP PREEMPT Wed Jun 12 21:46:00 CST 2026 aarch64
```

Check that the phone has been stable:
```bash
uptime
cat /proc/uptime
```

If the uptime is low (< 2 minutes) and you didn't just reboot, the phone may have
crashed and rebooted on its own — investigate with `dmesg` or check `/sys/fs/pstore/`
for crash logs.

---

## 9. How to Update ReSukiSU

When the ReSukiSU team releases a new version, follow these steps to update the kernel:

```bash
# Enter the kernel source directory
cd ~/op8_sm8250_lineage23_resukisu_droidspaces

# Step 1: Remove the old ReSukiSU source
rm -rf KernelSU

# Step 2: Clone the latest ReSukiSU from GitHub
git clone --branch main https://github.com/ReSukiSU/ReSukiSU.git KernelSU

# Step 3: Get the FULL commit history (--depth 1 only gives 1 commit, which breaks version calculation)
cd KernelSU
git fetch --unshallow
# Expected: fetches ~4200 commits and ~50 tags (v2.0_beta through v4.1.0)
cd ..

# Step 4: Apply the -hotsteel custom branding
sed -i 's/KSU_COMMIT_SHA  := $(KSU_COMMIT_SHA)-dirty/KSU_COMMIT_SHA  := $(KSU_COMMIT_SHA)-hotsteel/' KernelSU/kernel/Kbuild

# Step 5: Verify no KPM leftovers (KPM was removed from ReSukiSU upstream)
ls KernelSU/kernel/kpm/ 2>/dev/null && echo "WARNING: KPM directory exists!" || echo "OK: no KPM"
# Expected: "OK: no KPM"

# Step 6: Check the new version that will be calculated
KSU_COMMITS=$(git -C KernelSU rev-list --count HEAD)
KSU_VERSION=$((30000 + KSU_COMMITS + 700))
echo "New KSU_VERSION will be: $KSU_VERSION (based on $KSU_COMMITS commits)"

# Step 7: Rebuild
make olddefconfig
make -j$(nproc)

# Step 8: Repackage with AnyKernel3 (repeat Section 6 above)
```

> **Critical warning:** Do NOT use `--depth 1` when cloning KernelSU. Shallow clones only
> have 1 commit in their history, which makes `KSU_VERSION = 30000 + 1 + 700 = 30701`.
> The manager requires `>= 34759`. You MUST run `git fetch --unshallow` to get all ~4200
> commits, giving the correct version of ~34967.

---

## 10. Troubleshooting Every Possible Problem

### 10.1 Build Errors

#### `Can't find ReSukiSU git submodule!`
```
-- Can't find ReSukiSU git submodule!
-- If you are using bazel to build this kernel,
-- Please go to Kbuild and change KSU_SRC to the absolute path of the KSU kernel folder.
drivers/kernelsu/Kbuild:67: *** You should use ReSukiSU as a git submodule instead of copying code directly.  Stop.
```
**Cause:** The `KernelSU/.git` file is missing or broken. The Kbuild at
`KernelSU/kernel/Kbuild` line 61 checks `test -e $(KSU_SRC)/../.git`.
**Fix:**
```bash
echo "gitdir: ../.git" > KernelSU/.git
```

#### `You should use ReSukiSU as a git submodule` (but you cloned it)
**Cause:** You copied the ReSukiSU kernel source manually instead of using `git clone`.
**Fix:** Remove KernelSU and clone it properly:
```bash
rm -rf KernelSU
git clone --branch main https://github.com/ReSukiSU/ReSukiSU.git KernelSU
git -C KernelSU fetch --unshallow
```

#### `error: 'something' undeclared`
**Cause:** A C compilation error in a kernel source file. Usually from a mismatched
header or a missing `#include`.
**Fix:** Read the error carefully — it tells you the file and line number. Common causes:
- Missing backport: some 4.19 APIs differ from newer kernels
- GCC version incompatibility: GCC 10+ may reject code that GCC 9.3 accepts
- Modified file with a typo: run `git diff` to check what you changed

#### `error: Unsupported hook method`
**Cause:** Neither `CONFIG_KSU_TRACEPOINT_HOOK`, `CONFIG_KSU_MANUAL_HOOK`, nor
`CONFIG_KSU_SUSFS` is enabled. You need at least one.
**Fix:** Check your `.config`:
```bash
grep "CONFIG_KSU_MANUAL_HOOK\|CONFIG_KSU_TRACEPOINT\|CONFIG_KSU_SUSFS" .config
```
Only one should be `=y`. For this kernel, it should be `CONFIG_KSU_MANUAL_HOOK=y`.

#### `TP hooks are incompatible with Non-GKI/GKI 1.0 kernels.`
**Cause:** `CONFIG_KSU_TRACEPOINT_HOOK` was accidentally enabled. Tracepoint hooks only
work on GKI 2.0 kernels (5.10+).
**Fix:**
```bash
sed -i 's/CONFIG_KSU_TRACEPOINT_HOOK=y/# CONFIG_KSU_TRACEPOINT_HOOK is not set/' .config
make olddefconfig
```

#### Build hangs or crashes with no error (OOM kill)
**Cause:** Your machine ran out of memory during compilation.
**Fix:** Reduce parallelism. On a machine with 8 GB RAM:
```bash
make -j4   # 4 jobs instead of $(nproc)
```
Or add swap space temporarily:
```bash
sudo fallocate -l 8G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### 10.2 Flash / Boot Problems

#### Phone bootloops after flashing
**Step 1: Don't panic.** Your data is safe. The bootloop means the kernel crashes during
init, but the system partition (apps, settings, files) is untouched.

**Step 2: Reboot to recovery.** Hold Volume Down + Power for 10 seconds to force-reboot,
then immediately hold Volume Down to enter recovery.

**Step 3: Restore.** Flash your ROM's stock boot image, or a previously-working kernel ZIP.

**Step 4: Diagnose.** Pull the crash log:
```bash
adb shell cat /sys/fs/pstore/console-ramoops
```
This shows the kernel panic message. Common causes:
- Missing driver: a hardware driver (touchscreen, charging, display) wasn't compiled in
- Wrong DTB: the device tree doesn't match the hardware revision
- SELinux denial: the kernel can't load a required policy

#### Manager says "Incompatible" or "Version too low"
**Cause:** The KSU_VERSION embedded in the kernel is below what the manager requires.
**Check:** What version did the build produce?
```bash
grep "version code" build.log
# Should show: -- ReSukiSU version code: 34967
```
If it shows `< 34759`, the KernelSU clone was shallow. Run:
```bash
git -C KernelSU fetch --unshallow
make -j$(nproc)   # rebuild
```

#### Droidspaces container exits with code 255 immediately
**Cause:** The container's init system (usually systemd) tried to use a syscall that
doesn't exist in Linux 4.19.

| Container Image | systemd Version | Works on 4.19? |
|-----------------|-----------------|----------------|
| Debian 11 (bullseye) | 247 | ✅ Yes |
| Ubuntu 20.04 (focal) | 245 | ✅ Yes |
| Alpine 3.16 | OpenRC (not systemd) | ✅ Yes |
| Debian 12 (bookworm) | 252 | ⚠️ May work with workarounds |
| Ubuntu 22.04 (jammy) | 249 | ⚠️ May work |
| Arch Linux ARM | 260+ | ❌ No — requires Linux 5.3+ |
| Ubuntu 24.04 (noble) | 255 | ❌ No — requires Linux 5.3+ |
| Fedora 38+ | 253+ | ❌ No — requires Linux 5.3+ |

The specific missing syscalls are `pidfd_open()` (added in 5.3) and `close_range()`
(added in 5.4). systemd ≥ 250 uses both unconditionally.

### 10.3 Runtime Problems

#### Root works but modules don't load
**Cause:** Kernel modules (.ko files) are not signed or don't match the kernel version.
**Check:**
```bash
uname -r                     # shows kernel version
ls /vendor/lib/modules/      # should have .ko files
modprobe some_module         # try loading manually
dmesg | tail -20             # check for module loading errors
```
Most OnePlus 8 modules are built into the kernel (not as .ko files), so this is rarely
an issue.

#### WiFi or Bluetooth doesn't work after flashing
**Cause:** The WLAN/BT driver is looking for a function that isn't compiled in.
**Fix:** The OPlus stubs file at `drivers/soc/oplus/oplus_stubs.c` provides two
missing functions:
```c
int cnss_get_restart_level(void) { return 0; }
void wl_android_wifi_bt_power_on(int on) {}
```
If these were accidentally removed, WiFi and Bluetooth will fail to initialize.
Re-add them and rebuild.

---

## 11. Technical Reference

### 11.1 All Manual Hooks (Already Applied)

These six hooks are physically inserted into the kernel source and verified by
`manual_hook_check.mk` at build time:

| # | Function | File | Where It's Called | Why |
|---|----------|------|-------------------|-----|
| 1 | `ksu_handle_execveat` | `fs/exec.c` | Inside `do_execve()` — every time a program is launched | Intercepts app launches so ReSukiSU can grant root before the app starts |
| 2 | `ksu_handle_execveat` | `fs/exec.c` | Inside `compat_do_execve()` — same as above but for 32-bit apps | 32-bit app support (legacy apps, some modules) |
| 3 | `ksu_handle_stat` | `fs/stat.c` | Inside `newfstatat()` and `fstatat64()` — when an app checks file metadata | Hides ReSukiSU files from non-root apps |
| 4 | `ksu_handle_newfstat_ret` | `fs/stat.c` | After `newfstat()` returns — reads the stat result | Post-processes stat results to hide ReSukiSU |
| 5 | `ksu_handle_fstat64_ret` | `fs/stat.c` | After `fstat64()` returns — same as above, 32-bit path | Same as #4 but for 32-bit stat64 calls |
| 6 | `ksu_handle_faccessat` | `fs/open.c` | Inside `faccessat()` — when an app checks file accessibility | Prevents non-root apps from detecting ReSukiSU files via access checks |
| 7 | `ksu_handle_sys_reboot` | `kernel/reboot.c` | Inside `SYSCALL_DEFINE4(reboot...)` — when anything triggers a reboot | Allows ReSukiSU to intercept and manage reboots (e.g., for Safe Mode) |

### 11.2 Auto Hooks via LSM (No Manual Code Needed)

These are enabled by `CONFIG_KSU_MANUAL_HOOK_AUTO_*` options and work through the
Linux Security Module framework. They require no source code modifications:

| Hook | Config Option | LSM Callback | Kernel Mechanism |
|------|---------------|--------------|------------------|
| setresuid | `AUTO_SETUID_HOOK` | `task_fix_setuid` | Intercepts UID changes — allows ReSukiSU to grant root when `su` is called |
| sys_read (init.rc) | `AUTO_INITRC_HOOK` | `file_permission` | Intercepts reads of `/init.rc` and related files — allows ReSukiSU to inject service definitions |
| input_event | `AUTO_INPUT_HOOK` | (input_handler) | Registers an `input_handler` that monitors all input events — enables key-combo detection |

These auto hooks are available on **all kernels < 6.8**. On 6.8+, the LSM API changed
(`security_operations` became `security_hook_list` with different function signatures),
so manual hooks must be used instead.

### 11.3 SELinux Compatibility (4.19 Specifics)

ReSukiSU needs to modify SELinux policies at runtime. The approach differs by kernel version:

| Kernel Version | SELinux Approach | Config Variables |
|---------------|------------------|------------------|
| **< 5.10** (our 4.19) | Uses `selinux_state` — a global struct containing the policy, sidtab, and status. ReSukiSU creates a `fake_state` and swaps it in at runtime. | `KSU_COMPAT_HAS_SELINUX_STATE` is set. `KSU_COMPAT_HAS_SELINUX_POLICY_STRUCT` is NOT set. |
| 5.10+ | Uses `struct selinux_policy` — a newer abstraction that wraps the policydb and sidtab together. | Both flags are set. Simpler API. |

Our 4.19 kernel uses the older approach. The `kernel_compat.mk` build script auto-detects
which structures exist and sets the appropriate `-D` flags. You can see the result in any
`.o.cmd` file:
```bash
grep "KSU_COMPAT_HAS_SELINUX" drivers/kernelsu/selinux/.rules.o.cmd
# Shows: -DKSU_COMPAT_HAS_SELINUX_STATE
# Does NOT show: KSU_COMPAT_HAS_SELINUX_POLICY_STRUCT
```

### 11.4 OPlus Stubs

Two stubs are provided in `drivers/soc/oplus/oplus_stubs.c` for functions that OPlus
drivers reference but whose implementations are not included in this kernel tree:

```c
int cnss_get_restart_level(void) { return 0; }
// Used by: WLAN driver (CNSS = Converged Network SubSystem)
// Without this: WiFi module load fails with "unknown symbol"

void wl_android_wifi_bt_power_on(int on) {}
// Used by: Bluetooth power management
// Without this: Bluetooth fails to initialize
```

These stubs are compiled by an additional line in `drivers/soc/oplus/Makefile`:
```makefile
obj-y += oplus_stubs.o
```

### 11.5 Repository Remotes

```
# Pull updates from LineageOS upstream (read-only, reference):
git remote add lineage https://github.com/LineageOS/android_kernel_oneplus_sm8250.git

# Your fork (read-write, where you push your builds):
git remote add origin https://github.com/Hotsteel2901/op8_sm8250_lineage23_resukisu_droidspaces.git
```

To check if LineageOS has released updates to the base kernel:
```bash
git fetch lineage lineage-23.2
git log lineage/lineage-23.2..HEAD --oneline    # shows what you have that lineage doesn't
git log HEAD..lineage/lineage-23.2 --oneline    # shows what lineage has that you don't
```

---

*Last updated: 2026-06-12. Kernel: 4.19.325. ReSukiSU: v4.1.0 (0dbd28e). Droidspaces: non-GKI config + cgroup prefix patch applied.*
