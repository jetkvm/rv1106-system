SET(JK_TARGET_ARCH "arm-rockchip830-linux-uclibcgnueabihf")
SET(JK_BUILDKIT_PATH "/opt/jetkvm-native-buildkit")

SET(JK_TOOLCHAIN_PATH "${JK_BUILDKIT_PATH}/${JK_TARGET_ARCH}")
SET(JK_SYSROOT_PATH "${JK_TOOLCHAIN_PATH}/sysroot")

SET(JK_SYS_INCLUDE_PATH "${JK_TOOLCHAIN_PATH}/include/c++/8.3.0")
SET(JK_SYS_INCLUDE_PATH_ARCH "${JK_TOOLCHAIN_PATH}/include/c++/8.3.0/${JK_TARGET_ARCH}")
SET(JK_SYS_INCLUDE_PATH_SYSROOT "${JK_SYSROOT_PATH}/usr/include")

IF (NOT EXISTS "${JK_SYS_INCLUDE_PATH}")
    MESSAGE(FATAL_ERROR "JK_SYS_INCLUDE_PATH not found: ${JK_SYS_INCLUDE_PATH}")
ENDIF()

IF (NOT EXISTS "${JK_SYS_INCLUDE_PATH_ARCH}")
    MESSAGE(FATAL_ERROR "JK_SYS_INCLUDE_PATH_ARCH not found: ${JK_SYS_INCLUDE_PATH_ARCH}")
ENDIF()

IF (NOT EXISTS "${JK_SYS_INCLUDE_PATH_SYSROOT}")
    MESSAGE(FATAL_ERROR "JK_SYS_INCLUDE_PATH_SYSROOT not found: ${JK_SYS_INCLUDE_PATH_SYSROOT}")
ENDIF()

SET(MULTIARCH "$JK_TARGET_ARCH")
SET(ROOTFS "${JK_SYSROOT_PATH}")

# this is required
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR "armv7l")
 
# specify the cross compiler
SET(CMAKE_C_COMPILER ${JK_BUILDKIT_PATH}/bin/${JK_TARGET_ARCH}-gcc)
SET(CMAKE_CXX_COMPILER ${JK_BUILDKIT_PATH}/bin/${JK_TARGET_ARCH}-g++)

# This is very important, so that we find the right headers and libraries
# without explicitly listing the default include directories (e.g. JSC)
SET(CMAKE_SYSROOT "${ROOTFS}")

# Ensure that FIND_PACKAGE() functions and friends look in the rootfs
# only for libraries and header files, but not for programs (e.g perl)
SET(CMAKE_FIND_ROOT_PATH "${ROOTFS}")
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Add include directories from the rootfs matching the current toolchain
INCLUDE_DIRECTORIES(SYSTEM
  "${JK_SYS_INCLUDE_PATH}"
  "${JK_SYS_INCLUDE_PATH_ARCH}"
  "${JK_SYS_INCLUDE_PATH_SYSROOT}"
  )

SET(JK_CPPFLAGS "-D_GNU_SOURCE")
SET(JK_CPPFLAGS_DEPS "-I${JK_SYS_INCLUDE_PATH_SYSROOT} -isystem ${JK_SYS_INCLUDE_PATH} -isystem ${JK_SYS_INCLUDE_PATH_ARCH}")

# CMake does not pick CPPFLAGS, so we add it manually into CFLAGS and CXXFLAGS
# Note: I have no idea why the first include directory from the previous list
# gets ignored when building some components, so I pass it here as well.
SET(CPPFLAGS "-DG_DISABLE_CAST_CHECKS -DNDEBUG -Os -s")
SET(ENV{CFLAGS} "${CPPFLAGS} -fstack-protector-strong -Wall -Wformat -Werror=format-security ${JK_CPPFLAGS} ${JK_CPPFLAGS_DEPS}")
SET(ENV{CXXFLAGS} "${CPPFLAGS} -fstack-protector-strong -Wall -Wformat -Werror=format-security ${JK_CPPFLAGS} ${JK_CPPFLAGS_DEPS}")

# CMake does not pick LDFLAGS, so we add it manually too
SET(ENV{LDFLAGS} "-Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,--as-needed -Wl,-rpath-link,${ROOTFS}/usr/lib")

# Need to export this variables for pkg-config to pick them up, so that it
# sets the right search path and prefixes the result paths with the rootfs.
SET(ENV{PKG_CONFIG_PATH} "${ROOTFS}/usr/share/pkgconfig")
SET(ENV{PKG_CONFIG_LIBDIR} "${ROOTFS}/usr/lib/pkgconfig:${ROOTFS}/usr/lib")
SET(ENV{PKG_CONFIG_SYSROOT_DIR} "${ROOTFS}")

# These variables make sure that pkg-config does never discard standard
# include and library paths from the compile and linking flags.
SET(ENV{PKG_CONFIG_ALLOW_SYSTEM_CFLAGS} 1)
SET(ENV{PKG_CONFIG_ALLOW_SYSTEM_LIBS} 1)
SET(PKG_CONFIG_USE_CMAKE_PREFIX_PATH TRUE)