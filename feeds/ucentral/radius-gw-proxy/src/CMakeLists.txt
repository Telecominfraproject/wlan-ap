cmake_minimum_required(VERSION 2.6)

PROJECT(radius-gw-proxy C)

ADD_DEFINITIONS(-Wall -Werror)
IF(CMAKE_C_COMPILER_VERSION VERSION_GREATER 6)
	ADD_DEFINITIONS(-Wextra -Werror=implicit-function-declaration)
	ADD_DEFINITIONS(-Wformat -Werror=format-security -Werror=format-nonliteral)
ENDIF()
ADD_DEFINITIONS(-Os -std=gnu99 -g3 -Wmissing-declarations -Wno-unused-parameter -Wno-strict-aliasing)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c ubus.c)

FIND_LIBRARY(ubus NAMES ubus)
FIND_LIBRARY(ubox NAMES ubox)

FIND_PATH(ubox_include_dir libubox/uloop.h)
FIND_PATH(ubus_include_dir NAMES libubus.h)

INCLUDE_DIRECTORIES(${ubox_include_dir} ${ubus_include_dir})

ADD_EXECUTABLE(radius-gw-proxy ${SOURCES})

TARGET_LINK_LIBRARIES(radius-gw-proxy ${ubox} ${ubus})

INSTALL(TARGETS radius-gw-proxy
	RUNTIME DESTINATION sbin
)
