set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(VCPKG_C_FLAGS "-Wno-error=format-truncation -march=armv8.3-a")
set(VCPKG_CXX_FLAGS "-Wno-error=format-truncation -march=armv8.3-a")

if(PORT MATCHES "mujoco|libsystemd")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
