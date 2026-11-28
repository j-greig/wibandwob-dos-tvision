# CMake toolchain: a static aarch64 Linux wwdos, cross-built on Debian/Ubuntu.
# Used by unkernel/build.sh, which names the arm64 static libraries.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
