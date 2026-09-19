# Floss

A from-scratch, bare-metal operating system (OS) for AArch64 (ARMv8), built in freestanding C++ and AArch64 assembly with no libc and no external dependencies. 

Core features:
 - Generic Interrupt Controller (GIC), configure and route both PPI (per-core) and SPI (shared) interrupts via the GIC Distributor + Redistributor
 - SMP bring-up via PSCI, secondary cores are booted from EL1 using the PSCI `CPU_ON` interface with an assembly trampoline entry (start.S, _secondary_start)
 - PL011 UART, MMIO-driven serial output and RX interrupt-driven input, no libc (kstdio provides what printf/putc would)
 - Streaming DeviceTree parser, walks a flattened device tree node-by-node and dispatches to subsystems (GIC/PSCI/UART) to discover MMIO addresses instead of hardcoding them
 - Exception handling, AArch64 exception vectors (exceptions.S), exception-level detection, interrupt masking
 - Timer, a generic timer that fires predictable periodic interrupts

Much of the implementation of each feature/subsystem is based on ARM specifications, some linked under [Useful Documentation](#useful-documentation).

## Prerequisites

An AArch64 cross toolchain and CMake is needed to build and run on x86. To debug you also need `gdb-multiarch`.

On Ubuntu:
```sh
sudo apt-get install cmake make gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-system-aarch64 gdb-multiarch
```

## Building

Configure once (note the toolchain file):
```sh
mkdir build
cmake -S . -B build -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE=Debug
```

Then build with:
```sh
cmake --build build
cmake --build build -t clean
```

## Running (once you have a kernel)

Two ways to boot under QEMU's `virt` machine:
```sh
# 1) Directly via the script (works without CMake's run target):
./scripts/run-qemu.sh                       # boots build/floss_kernel
./scripts/run-qemu.sh path/to/custom.elf    # or any ELF

# 2) Through CMake (rebuilds first):
cmake --build build -t run
```

Exit QEMU with `Ctrl-A + X`.

## Debugging

A CMake target is exposed for a handy and common debug setup. If running in a tmux session, this target splits a GDB session into a new pane and pauses execution at the very start of execution in QEMU.
```
cmake --build build -t run-debug
```

The CMake target uses the QEMU run script in `scripts/run-qemu.sh`, which exposes the port 1234 for GDB debugging.

Use `gdb-multiarch` to debug an already running QEMU instance by running:
```
gdb-multiarch build/floss_kernel
(gdb) target remote :1234
```

It might be useful to set scheduler locking to step mode so that multiple threads/cores are not executing simultaneously when debugging.
```
(gdb) set scheduler-locking step
```

# Running on a Raspberry Pi 5

A goal is to boot and run floss on a Raspberry Pi 5 with 1GB of memory.

1. The base for the kernel should be at `0x80000`. Make sure the aarch64.ld linker script matches this. (`BASE = 0x80000;`).
2. The Raspberry Pi 5 (and earlier models) seem to be using GICv2, so it must be working correctly. The main difference beteween GICv3 and GICv2 is that v2 does not have Redistributors, and the CPU interface is accessed via MMIO/MMDR instead of system registers.
3. The kernel image should be named `kernel_2712.img` (matching the Broadcom 2712 chip). The firmware expects `arm_64bit=1` to be set to find this kernel image.

## Useful Documentation

* [DeviceTree v0.4 Specification](https://github.com/devicetree-org/devicetree-specification/releases/tag/v0.4)
* [Arm Generic Interrupt Controller Specification](https://support.arm.com/documentation/ihi0069/hb/)
* [Arm Power State Coordination Interface Specification](https://support.arm.com/documentation/den0022/fb/)
* [SMC Calling Convention (SMCCC)](https://support.arm.com/documentation/den0028/h/)
* [Arm Architecture Reference Manual Armv8](https://support.arm.com/documentation/ddi0487/ga)
