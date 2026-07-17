#!/usr/bin/env bash
#
# Boot a bare-metal AArch64 kernel under QEMU's "virt" machine.
#
#   ./scripts/run-qemu.sh                # uses the default kernel path below
#   ./scripts/run-qemu.sh path/to.elf     # boot a specific ELF
#
# Requirements: qemu-system-aarch64 on your PATH.

set -euo pipefail

# Default location produced by the CMake build (see CMakeLists.txt).
KERNEL="${1:-build/floss_kernel}"

if [[ ! -f "$KERNEL" ]]; then
    echo "error: kernel image not found: $KERNEL" >&2
    echo "hint: build first with  cmake --build build" >&2
    exit 1
fi

# -M virt            : the generic "virt" board; load address 0x40000000 (see linker/aarch64.ld)
# -cpu cortex-a53    : a common ARMv8-A core (matches the linker/load assumptions)
# -m 128M            : 128 MiB of RAM — plenty for early bring-up
# -nographic         : serial + console go to this terminal (no GUI window)
# -kernel <elf>      : QEMU loads the ELF and jumps to its entry symbol (_start)
# -no-reboot         : halt instead of rebooting if the guest triggers a reset
# -gdb tcp:1234      : starts a GDB channel on TCP port 1234
exec qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -m 128M \
    -nographic \
    -no-reboot \
    -gdb tcp::1234 \
    -kernel "$KERNEL"
