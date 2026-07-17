# CMake toolchain file for aarch64 cross-compilation.
#
# Configure with:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake
# then build with:
#   cmake --build build

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Prefer the bare-metal target triple so we don't pick up a Linux libc by
# accident, but fall back to aarch64-linux-gnu if that's all the system has.
# Override with -DAARCH64_TRIPLE=... on the cmake command line.
if(NOT DEFINED AARCH64_TRIPLE)
    find_program(_AARCH64_NONE_ELF_GCC aarch64-none-elf-gcc)
    if(_AARCH64_NONE_ELF_GCC)
        set(AARCH64_TRIPLE aarch64-none-elf)
    else()
        set(AARCH64_TRIPLE aarch64-linux-gnu)
    endif()
    unset(_AARCH64_NONE_ELF_GCC CACHE)
    set(AARCH64_TRIPLE "${AARCH64_TRIPLE}" CACHE STRING "aarch64 toolchain triple")
endif()

# Tool discovery: let CMake find `aarch64-none-elf-gcc` and `-g++` on PATH.
set(CMAKE_C_COMPILER   ${AARCH64_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${AARCH64_TRIPLE}-g++)
set(CMAKE_ASM_COMPILER ${AARCH64_TRIPLE}-gcc)
set(CMAKE_AR           ${AARCH64_TRIPLE}-ar      CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB       ${AARCH64_TRIPLE}-ranlib  CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP        ${AARCH64_TRIPLE}-strip   CACHE FILEPATH "Strip")
set(CMAKE_OBJCOPY      ${AARCH64_TRIPLE}-objcopy CACHE FILEPATH "Objcopy")
set(CMAKE_OBJDUMP      ${AARCH64_TRIPLE}-objdump CACHE FILEPATH "Objdump")

# We're targeting a bare-metal environment, so don't try to use a sysroot or
# look for host libraries. Prevent CMake from doing test runs that assume a
# hosted environment.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Where to search for libraries / headers (none for a freestanding build).
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Target the ARMv8-A architecture. Override by setting AARCH64_MCPU on the
# cmake command line (e.g. -DAARCH64_MCPU=cortex-a72).
if(NOT DEFINED AARCH64_MCPU)
    set(AARCH64_MCPU "generic" CACHE STRING "Target -mcpu value")
endif()

set(AARCH64_ARCH_FLAGS -march=armv8-a -mcpu=${AARCH64_MCPU} -mlittle-endian)

add_compile_options(${AARCH64_ARCH_FLAGS})
add_link_options(${AARCH64_ARCH_FLAGS})
