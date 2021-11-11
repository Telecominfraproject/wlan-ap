cmake_minimum_required(VERSION 3.10)

PROJECT(atfpolicy C)

ADD_DEFINITIONS(-Os -Wall -Wno-unknown-warning-option -Wno-array-bounds -Wno-format-truncation -Werror --std=gnu99)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

find_library(nl NAMES nl-tiny)
ADD_EXECUTABLE(atfpolicy main.c ubus.c interface.c nl80211.c)
TARGET_LINK_LIBRARIES(atfpolicy ${nl} ubox ubus)

INSTALL(TARGETS atfpolicy
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
