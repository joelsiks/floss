#!/usr/bin/env bash
#
# Boot a bare-metal AArch64 kernel under QEMU's "raspi3b" machine.

set -euo pipefail

# Default location produced by the CMake build (see CMakeLists.txt).
KERNEL="${1:-build/floss_kernel}"
shift || true   # drop the kernel arg so "$@" holds extra QEMU flags

GIC_VERSION="${FLOSS_GIC_VERSION:-3}"

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

exec qemu-system-aarch64 \
    -M raspi3b \
    -cpu cortex-a72 \
    -dtb bcm2710-rpi-3-b.dtb \
    -m 1G \
    -nographic \
    -no-reboot \
    -gdb tcp::1234 \
    -kernel "$KERNEL.bin" \
    -smp 4 \
    "$@"
