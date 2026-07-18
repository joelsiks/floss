# Floss

A toy OS for AArch64.

A from-scratch AArch64 (64-bit ARM) operating system.

## Prerequisites

An AArch64 cross toolchain and CMake is needed to build and run on x86.

```sh
sudo apt install gcc-aarch64-linux-gnu cmake qemu-system-aarch64
```

## Building

Configure once (note the toolchain file):

```sh
cmake -B build -S . -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
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
cmake --build build --target run
```

Exit QEMU with `Ctrl-A` then `X`.

### Tuning QEMU
Edit `scripts/run-qemu.sh` to change the board/CPU/RAM. Common swaps:
- `-cpu cortex-a72` for a beefier ARMv8-A core.
- `-m 512M` for more RAM.

## Debugging

The QEMU run script in `scripts/run-qemu.sh` exposes the port 1234 for GDB debugging.

Use `gdb-multiarch` to debug a running QEMU instance by running:
```
gdb-multiarch build/floss_kernel
(gdb) target remote :1234
```

If you want the debugger to start at the beginning of execution in the QEMU VM, add the `-S` flag to `scripts/run-qemu.sh`.

