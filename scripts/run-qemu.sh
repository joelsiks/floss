#!/usr/bin/env bash
#
# Boot a bare-metal AArch64 kernel under QEMU's "virt" machine.
#
#   ./scripts/run-qemu.sh                # uses the default kernel path below
#   ./scripts/run-qemu.sh path/to.elf    # boot a specific ELF
#
# Requirements: qemu-system-aarch64 on your PATH.

set -euo pipefail

# Default location produced by the CMake build (see CMakeLists.txt).
KERNEL="${1:-build/floss_kernel}"
shift || true   # drop the kernel arg so "$@" holds extra QEMU flags

if [[ ! -f "$KERNEL" ]]; then
    echo "error: kernel image not found: $KERNEL" >&2
    echo "hint: build first with  cmake --build build" >&2
    exit 1
fi

# Optionally open a gdb pane (inside tmux) that loads the kernel's symbols.
# Enabled by FLOSS_GDB=1 (set by the `run-pause` CMake target). QEMU is started
# with -S below, so the CPU stays frozen; once the pane is up, run
#   target remote :1234
# inside gdb to attach to QEMU's gdbserver and `continue`.
if [[ "${FLOSS_GDB:-0}" == "1" ]]; then
    if [[ -n "${TMUX:-}" ]]; then
        tmux split-window -h "exec gdb-multiarch '${KERNEL}' -ex 'target remote :1234' -ex 'layout src'"
    else
        echo "note: not inside tmux; skipping auto gdb pane." >&2
        echo "      attach manually:  gdb-multiarch ${KERNEL}" >&2
    fi
fi

aarch64-linux-gnu-objcopy -O binary "$KERNEL" "$KERNEL.bin"

# -M virt            : the generic "virt" board; load address 0x40000000 (see linker/aarch64.ld)
# -cpu cortex-a53    : a common ARMv8-A core (matches the linker/load assumptions)
# -m 128M            : 128 MiB of RAM — plenty for early bring-up
# -nographic         : serial + console go to this terminal (no GUI window)
# -kernel <elf>      : QEMU loads the ELF and jumps to its entry symbol (_start)
# -no-reboot         : halt instead of rebooting if the guest triggers a reset
# -gdb tcp:1234      : starts a GDB channel on TCP port 1234
# -d int             : prints exceptions to the terminal
# Any extra args passed to this script are forwarded to QEMU, e.g.
#   ./scripts/run-qemu.sh build/floss_kernel -S   # pause at startup for gdb
#
# Use the following options and commands to dump the DeviceTree to a plain text
# file:
#   QEMU Option(s): -M dumpdtb=dump.dtb
#   Command line dtb -> dts: dtc -I dtb -O dts -o device-tree-plain-text.dts dump.dtb
exec qemu-system-aarch64 \
    -M virt,gic-version=3 \
    -cpu cortex-a53 \
    -m 128M \
    -nographic \
    -no-reboot \
    -gdb tcp::1234 \
    -kernel "$KERNEL.bin" \
    -smp 2 \
    "$@"
