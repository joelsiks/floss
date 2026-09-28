# Floss

A from-scratch, bare-metal kernel / operating system (OS) for AArch64 (ARMv8), built in freestanding C++ and AArch64 assembly with no libc and no external dependencies.

Core features:
 - Generic Interrupt Controller (GIC, implementing both v2 and v3), configure and route both PPI (per-core) and SPI (shared) interrupts via the GIC Distributor + Redistributor
 - SMP bring-up via PSCI or spin tables. Secondary cores are booted from the kernel (EL1) with an assembly trampoline entry into the kernel
 - PL011 UART, MMIO-driven serial output and RX interrupt-driven input, no libc (kstdio provides what printf/putc would)
 - Streaming DeviceTree parser, walks a flattened device tree node-by-node and dispatches to subsystems (GIC/PSCI/UART/CPU/Memory) to discover MMIO addresses instead of hardcoding them
 - Exception handling, AArch64 exception vectors (exceptions.S), exception-level detection, interrupt masking
 - Rudimentary virtual memory and translation support, currently only identity mapping virtual addresses to physical addresses, with different attributes for Normal and Device memory
 - Timer, a generic timer that fires predictable periodic interrupts

Much of the implementation of each feature/subsystem is based on ARM specifications, some linked under [Useful Documentation](#useful-documentation).

## Prerequisites

An AArch64 cross toolchain and CMake is needed to build and run on x86. To debug you also need `gdb-multiarch`.

On Ubuntu:
```sh
sudo apt-get install cmake make gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-system-aarch64 gdb-multiarch
```

## Building

Configure once (note the toolchain file), and select the platform you want to configure against. Floss currently targets the virt and rpi boards on QEMU, but the rpi target works for real Raspberry Pi hardware as well. The main difference is what start address the kernel is loaded at.
```sh
mkdir build-virt build-rpi

cmake -S . -B build-virt -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake \
    -DFLOSS_PLATFORM=virt \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE=Debug

cmake -S . -B build-rpi -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake \
    -DFLOSS_PLATFORM=rpi \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE=Debug
```

Then build with:
```sh
cmake --build build-qemu
cmake --build build-qemu -t clean
```

## Running

Two ways to boot under QEMU's machine:
```sh
# 1) Directly via the script (works without CMake's run target):
./scripts/run-qemu.sh                       # boots build/floss_kernel
./scripts/run-qemu.sh path/to/custom.elf    # or any ELF

# 2) Through CMake (rebuilds first):
cmake --build build-virt -t run
```

Exit QEMU with `Ctrl-A + X`.

To run QEMU's Raspberry Pi 4 target, you need to download a matching devicetree and put it inside the `devicetree/` folder. You can find the [bcm2711-rpi-4-b.dtb](https://github.com/raspberrypi/firmware/blob/master/boot/bcm2711-rpi-4-b.dtb) at Raspberry Pi's [raspberrypi/firmware](https://github.com/raspberrypi/firmware) repository.

## Debugging

A CMake target is exposed for a handy and common debug setup. If running in a tmux session, this target splits a GDB session into a new pane and pauses execution at the very start of execution in QEMU.
```
cmake --build build-virt -t run-debug
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

This is still work in progress. There are a lot of differences between QEMU's virt and real hardware that I haven't fully mapped out yet. Some main differences are:

1. The base for the kernel should be at `0x80000`. This is configured via the `-DFLOSS_PLATFORM` in the configure step, which aligns the linker script (aarch64.ld) to have the correct base set.
2. The Raspberry Pi 5 (and Raspberry Pi 4) are using GICv2. The main difference beteween GICv3 and GICv2 is that v2 does not have Redistributors, and the CPU interface is accessed via MMIO/MMDR instead of system registers.
3. The kernel image should be named `kernel_2712.img` (matching the Raspberry Pi 5's Broadcom 2712 chip). The firmware expects `arm_64bit=1` to be set to find this kernel image.
4. Raspberry Pi 5 enters from the firmarre to the kernel in EL2. This is similar to QEMU's virt if you pass `-M virtualization=on`.

## Useful Documentation

* [DeviceTree v0.4 Specification](https://github.com/devicetree-org/devicetree-specification/releases/tag/v0.4)
* [Arm Generic Interrupt Controller Specification](https://support.arm.com/documentation/ihi0069/hb/)
* [Arm Power State Coordination Interface Specification](https://support.arm.com/documentation/den0022/fb/)
* [SMC Calling Convention (SMCCC)](https://support.arm.com/documentation/den0028/h/)
* [Arm Architecture Reference Manual Armv8](https://support.arm.com/documentation/ddi0487/ga)
