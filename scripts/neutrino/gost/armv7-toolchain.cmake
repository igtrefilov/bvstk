# Neutrino uses the QNX ABI and the qcc compiler interface supported by CMake.
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER "$ENV{KPDA_HOST}/usr/bin/qcc")
set(CMAKE_C_COMPILER_TARGET "8.3.0,gcc_ntoarmv7le" CACHE STRING "qcc variant")
set(CMAKE_AR "$ENV{KPDA_HOST}/usr/bin/ntoarmv7-ar" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "$ENV{KPDA_HOST}/usr/bin/ntoarmv7-ranlib" CACHE FILEPATH "Ranlib")

set(CMAKE_FIND_ROOT_PATH "$ENV{KPDA_TARGET}/armle-v7" "$ENV{KPDA_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Upstream skips its unaligned-access probe when cross-compiling. Cortex-A9
# requires the portable implementation for buffers with arbitrary alignment.
set(CMAKE_C_FLAGS_INIT "-DSTRICT_ALIGNMENT")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--no-undefined")
set(CMAKE_SKIP_RPATH TRUE)
