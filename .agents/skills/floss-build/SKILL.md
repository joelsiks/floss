---
name: floss-build
description: This skill instructs the agent on how to build, run, and verify the **floss** kernel project.
---

## Configure

Configure the project with the cross-compilation toolchain for aarch64:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake
```

The build type defaults to **Debug** (which adds DWARF debug info for gdb).
Override at configure time if needed:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake -DCMAKE_BUILD_TYPE=Release
```

## Build

Build the kernel ELF:

```bash
cmake --build build
```

This produces `build/floss_kernel`.

## Verify a build

Use the follow command to verify that the project builds successfully (configure + build in one shot):

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake && cmake --build build
```

## Project overview

- **Language**: C, C++, ASM (aarch64, bare-metal / freestanding)
- **Output**: Static ELF (`floss_kernel`) linked with `linker/aarch64.ld`
- **Source layout**: All `.S`, `.cpp`, `.c` files under `src/` are picked up automatically.
- **Build system**: CMake >= 3.20
