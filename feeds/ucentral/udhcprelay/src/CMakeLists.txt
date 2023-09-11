cmake_minimum_required(VERSION 3.10)

PROJECT(udhcprelay C)
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations -Wno-address-of-packed-member -fwrapv -fno-strict-aliasing)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c ubus.c dev.c dhcp.c relay.c)
SET(LIBS ubox ubus)

ADD_EXECUTABLE(udhcprelay ${SOURCES})
TARGET_LINK_LIBRARIES(udhcprelay ${LIBS})
INSTALL(TARGETS udhcprelay
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
