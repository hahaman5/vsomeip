# this one is important
SET(CMAKE_SYSTEM_NAME Android)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH  /home/rtst/toolchain-4.9)

# specify the cross compiler
SET(CMAKE_C_COMPILER   "${CMAKE_FIND_ROOT_PATH}/bin/arm-linux-androideabi-gcc")
SET(CMAKE_CXX_COMPILER "${CMAKE_FIND_ROOT_PATH}/bin/arm-linux-androideabi-g++")
SET(CMAKE_LINKER "${CMAKE_FIND_ROOT_PATH}/bin/arm-linux-androideabi-ld")
#SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_LINKER} ${FLAGS} ${CMAKE_CXX_LINK_FLAGS} ${LINK_FLAGS} ${OBJECTS} -o ${TARGET} ${LINK_LIBRARIES}")

# we need to supply another libm since crystax's libm is empty
set(MATH_LIB "m_r13_ndk")

set(CRT_BEGIN "${CMAKE_FIND_ROOT_PATH}/sysroot/usr/lib/crtbegin_static.o")
set(CRT_END "${CMAKE_FIND_ROOT_PATH}/sysroot/usr/lib/crtend_android.o")
set(PLATFORM_LINK_LIB_PATH "${PLATFORM_LINK_LIB_PATH} -L${CMAKE_FIND_ROOT_PATH}/sysroot/usr/lib")
set(PLATFORM_LINK_LIB_PATH "${PLATFORM_LINK_LIB_PATH} -L${CMAKE_FIND_ROOT_PATH}/lib/gcc/arm-linux-androideabi/4.9/armv7-a")
set(PLATFORM_LINK_LIB_PATH "${PLATFORM_LINK_LIB_PATH} -L${CMAKE_FIND_ROOT_PATH}/arm-linux-androideabi/lib/armv7-a")
SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_LINKER} -dynamic-linker /system/bin/linker ${CRT_BEGIN} <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> ${PLATFORM_LINK_LIB_PATH} ${CMAKE_FIND_ROOT_PATH}/arm-linux-androideabi/lib/armv7-a/libcrystax.a --start-group -lstdc++ -l${MATH_LIB} -lgcc -lc --end-group ${CRT_END} -static -z muldefs")


# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(BOOST_ROOT boost)

SET(STATIC_BUILD ON)

SET(TOOLCHAIN_FLAGS "-march=armv7-a -DANDROID_BUILD=ON -DSTATIC_BUILD")
