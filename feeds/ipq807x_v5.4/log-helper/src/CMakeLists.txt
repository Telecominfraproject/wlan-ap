cmake_minimum_required(VERSION 2.6)

INCLUDE(CheckFunctionExists)

PROJECT(log-helper C)
ADD_DEFINITIONS(-Os -Wall -Werror)
ADD_DEFINITIONS(-std=gnu99 -g3 -Wmissing-declarations -Wno-unused-parameter)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
SET(SOURCES main.c)

FIND_LIBRARY(uci NAMES uci)
FIND_LIBRARY(ubus NAMES ubus)
FIND_LIBRARY(ubox NAMES ubox)

ADD_EXECUTABLE(log-helper ${SOURCES})

TARGET_LINK_LIBRARIES(log-helper ${uci} ${ubus} ${ubox})

INSTALL(TARGETS log-helper
	RUNTIME DESTINATION sbin
)
