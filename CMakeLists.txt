cmake_minimum_required(VERSION 3.20)

# Configure with the cross-compilation toolchain:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake
# then build with:
#   cmake --build build
project(floss
    VERSION 0.1.0
    LANGUAGES C CXX ASM
)

# --- Bare-metal / freestanding options ---------------------------------------
# Applied to every target in the tree via directory-level properties.
add_compile_options(
    -ffreestanding          # We don't assume a hosted C environment / libc
    -fno-pie                # Fixed load address, not position-independent
    -fno-pic
    -mgeneral-regs-only     # Avoid FP/SIMD regs (you'd have to save/restore them)
    -Wall -Wextra
    -fno-exceptions
    -fno-rtti
)

add_link_options(
    -nostdlib               # Don't link the C standard library
    -nostartfiles           # Don't link crt0/startup files; we provide our own
    -no-pie                 # Produce a static, non-PIE executable
    -Wl,--build-id=none     # No .note.gnu.build-id section
)

# --- Kernel image ------------------------------------------------------------
add_executable(floss_kernel
    src/start.S
    src/kernel.cpp
    src/uart.cpp
)
target_link_options(floss_kernel PRIVATE
    -T ${CMAKE_SOURCE_DIR}/linker/aarch64.ld
)
set_target_properties(floss_kernel PROPERTIES LINK_DEPENDS
   ${CMAKE_SOURCE_DIR}/linker/aarch64.ld)

# --- QEMU run target ---------------------------------------------------------
# `cmake --build build --target run` boots the built kernel under QEMU via the
# script below. This target configures fine even before floss_kernel exists; it
# only fails at run time if the ELF hasn't been built yet.
#
# Once you've uncommented the kernel target above, also uncomment the
# add_dependencies line so `run` always triggers a rebuild first.
add_custom_target(run
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu.sh
            ${CMAKE_BINARY_DIR}/floss_kernel
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Booting floss_kernel under QEMU (virt / cortex-a53)"
)
add_dependencies(run floss_kernel)
