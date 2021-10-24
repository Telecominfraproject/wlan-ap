cmake_minimum_required(VERSION 3.10)

PROJECT(qosify C)

ADD_DEFINITIONS(-Os -Wall -Wno-unknown-warning-option -Wno-array-bounds -Wno-format-truncation -Werror --std=gnu99)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

find_library(bpf NAMES bpf)
ADD_EXECUTABLE(qosify main.c loader.c map.c ubus.c interface.c)
TARGET_LINK_LIBRARIES(qosify ${bpf} ubox ubus)

INSTALL(TARGETS qosify
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
