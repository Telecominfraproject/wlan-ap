cmake_minimum_required(VERSION 2.6)

PROJECT(udnssnoop C)
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c dns.c)
SET(LIBS ubox ubus resolv)

ADD_EXECUTABLE(udnssnoop ${SOURCES})
TARGET_LINK_LIBRARIES(udnssnoop ${LIBS})
INSTALL(TARGETS udnssnoop
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
