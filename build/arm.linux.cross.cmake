#set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

#set(TOOLCHAIN_DIR  "/data/rk356x/buildroot/output/rockchip_rk3568/host")
set(TOOLCHAIN_DIR  "/data/works/rk3568/rk356x-linux-20210809/buildroot/output/rockchip_rk3568/host")

set(SYSROOT_PATH  ${TOOLCHAIN_DIR}/aarch64-buildroot-linux-gnu/sysroot)
set(CMAKE_SYSROOT "${SYSROOT_PATH}")



SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/aarch64-linux-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/aarch64-linux-g++)
SET(CMAKE_FIND_ROOT_PATH  ${SYSROOT_PATH})



#set(SYSROOT_PATH  ${tools_path}/host/aarch64-buildroot-linux-gnu/sysroot)
#set(CMAKE_SYSROOT ${SYSROOT_PATH})




